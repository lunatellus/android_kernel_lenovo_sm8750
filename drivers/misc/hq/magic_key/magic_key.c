// SPDX-License-Identifier: GPL-2.0
/*
 * Huaqin "magic key" driver.
 *
 * Debug helper that watches a short key sequence on the keyboard cover's
 * input device.  When the sequence completes the driver either panics
 * ("magic trigger") or dumps every task that is stuck uninterruptible, which
 * is how the vendor used it to bisect a suspected input/lockup problem.
 *
 * There is no DT, GPIO or extcon binding at all: the driver only registers an
 * input handler whose id_table matches devices that report EV_KEY together
 * with one of the three "magic" keys.
 */
#include <linux/bitops.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/panic.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>

/* Key codes that take part in a magic sequence. */
#define MAGIC_KEY_DOWN		KEY_VOLUMEDOWN	/* 0x72 */
#define MAGIC_KEY_UP		KEY_VOLUMEUP	/* 0x73 */
#define MAGIC_KEY_POWER		KEY_POWER	/* 0x74 */

/*
 * Key codes key_event() acts on.
 *
 * Stock is "cmp w1,#1 / b.ne ret ; sub w8,w2,#0x75 ; cmn w8,#3 ; b.hs body":
 * b.hs selects the in-range codes 0x72..0x74 = KEY_VOLUMEDOWN..KEY_POWER,
 * the three codes the id_table matches and KEYS_SEQ1/2 are built from.
 */
#define MAGIC_EV_FIRST		KEY_VOLUMEDOWN	/* 0x72 */
#define MAGIC_EV_LAST		KEY_POWER	/* 0x74 */

#define MAGIC_TIMER_JIFFIES	25
#define MAGIC_RETRY_JIFFIES	8
#define MAGIC_STEP_JIFFIES	3
/* Windows measured from the last key event (MAGIC_STAMP), in jiffies. */
#define MAGIC_IDLE_JIFFIES	250	/* stock 0x468: cmn #0xfa */
#define MAGIC_CHANGE_JIFFIES	50	/* stock 0x53c: cmn #0x32 */
#define MAGIC_STABLE_JIFFIES	8	/* stock 0x51c: cmn #0x8 */
#define MAGIC_BLOCK_NS		0x4e3b29200ULL
#define MAGIC_BLOCK_S		20

/* One step of a magic sequence: up to three keys plus a step timeout. */
struct magic_key_seq {
	u32 key[3];
	u32 timeout;
};

struct magic_data {
	int state;
	struct timer_list timer;
	int index;
	unsigned long time;
	const struct magic_key_seq *seq;
	int last;
	void (*trigger)(struct magic_data *);
};
#define magic_key_instance magic_data

/* Stamp of the last key event and the currently pressed keys. */
struct magic_common {
	unsigned long last_time;
	unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
};

struct magic_common common;
#define MAGIC_STAMP	common.last_time

static const struct magic_key_seq KEYS_SEQ1[] = {
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_UP, MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_UP, MAGIC_KEY_DOWN } },
	{ },
};

static const struct magic_key_seq KEYS_SEQ2[] = {
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_UP } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_DOWN } },
	{ { MAGIC_KEY_UP, MAGIC_KEY_DOWN }, 4000 },
	{ },
};

static const struct input_device_id ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT(EV_KEY) },
		.keybit = { [1] = BIT(MAGIC_KEY_UP - BITS_PER_LONG) },
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT(EV_KEY) },
		.keybit = { [1] = BIT(MAGIC_KEY_DOWN - BITS_PER_LONG) },
	},
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT(EV_KEY) },
		.keybit = { [1] = BIT(MAGIC_KEY_POWER - BITS_PER_LONG) },
	},
	{ 0 },
};

static void trigger_func1(struct magic_data *data)
{
	panic("magic trigger");
}

static inline void _show_state(struct task_struct *p)
{
	u64 delta = sched_clock() - p->sched_info.last_arrival;

	if (delta >= MAGIC_BLOCK_NS) {
		pr_info("[magic_key][%s:%d] block time %llus > %d\n", __func__, 137,
			delta / 1000000000ULL, 20);
		sched_show_task(p);
	}
}

static void trigger_func2(struct magic_data *data)
{
	struct task_struct *g, *t;
	int old_level = console_printk[0];

	console_printk[0] = 15;
	pr_err("[magic_key][%s:%d] =====TASK_UNINTERRUPTIBLE=====\n", __func__, 151);

	rcu_read_lock();
	for_each_process_thread(g, t) {
		touch_softlockup_watchdog();
		if ((t->__state & (TASK_UNINTERRUPTIBLE | TASK_NOLOAD)) == TASK_UNINTERRUPTIBLE)
			_show_state(t);
	}
	rcu_read_unlock();

	console_printk[0] = old_level;
}

struct magic_key_instance datas[2] = {
	{
		.seq = KEYS_SEQ1,
		.trigger = trigger_func1,
	},
	{
		.seq = KEYS_SEQ2,
		.trigger = trigger_func2,
	},
};

void key_event(struct input_handle *handle, unsigned int type,
	       unsigned int code, int value)
{
	if (type != EV_KEY)
		return;

	if (code - MAGIC_EV_FIRST > MAGIC_EV_LAST - MAGIC_EV_FIRST)
		return;

	pr_info("[magic_key][%s:%d] type:%d code:%d value:%d\n", __func__, 296, type, code,
	       value);
	if (value)
		common.keybit[BIT_WORD(code)] |= BIT_MASK(code);
	else
		common.keybit[BIT_WORD(code)] &= ~BIT_MASK(code);
	if (!datas[0].index) {
		if (value == 1 && !datas[0].state)
			mod_timer(&datas[0].timer, jiffies + MAGIC_STEP_JIFFIES);
	}

	if (!datas[1].index) {
		if (value == 1 && !datas[1].state)
			mod_timer(&datas[1].timer, jiffies + MAGIC_STEP_JIFFIES);
	}

	MAGIC_STAMP = jiffies;
}

static void timer_function(struct timer_list *t);
static struct input_handler magic_handler;

static void magic_key_exit(void)
{
	input_unregister_handler(&magic_handler);
	timer_shutdown_sync(&datas[0].timer);
	timer_shutdown_sync(&datas[1].timer);
}

static inline void magic_reset(struct magic_data *data)
{
	data->state = 0;
	data->time = jiffies;
	data->index = 0;
}

static void timer_function(struct timer_list *t)
{
	struct magic_key_instance *data = container_of(t, struct magic_key_instance,
						       timer);
	const struct magic_key_seq *seq = data->seq + data->index;
	int keys = 0;
	int i;
	int hits = 0;

	for (i = 0; i < 3; i++) {
		if (seq->key[i]) {
			keys++;
			hits += !!(common.keybit[seq->key[i] >> 6] &
				   (1UL << (seq->key[i] & (BITS_PER_LONG - 1))));
		}
	}

	if (data->state == 0) {
		if (hits) {
			if (keys == hits) {
				data->state = 2;
				data->time = jiffies;
				mod_timer(t, jiffies + MAGIC_TIMER_JIFFIES);
			} else {
				data->state = 1;
				data->time = jiffies;
				mod_timer(t, jiffies + MAGIC_RETRY_JIFFIES);
			}
		} else if (data->index) {
			if (time_in_range(jiffies, MAGIC_STAMP,
					  MAGIC_STAMP + MAGIC_IDLE_JIFFIES))
				mod_timer(t, jiffies + MAGIC_TIMER_JIFFIES);
			else
				data->index = 0;
		}
	} else if (data->state == 1) {
		if (hits && hits >= data->last) {
			if (hits == data->last && hits != keys) {
				if (time_in_range(jiffies, MAGIC_STAMP,
						  MAGIC_STAMP + MAGIC_STABLE_JIFFIES))
					mod_timer(t, jiffies + MAGIC_RETRY_JIFFIES);
				else
					magic_reset(data);
			} else if (time_after(jiffies, MAGIC_STAMP + MAGIC_CHANGE_JIFFIES)) {
				magic_reset(data);
			} else if (hits == keys) {
				data->state = 2;
				data->time = jiffies;
				mod_timer(t, jiffies + MAGIC_TIMER_JIFFIES);
			} else {
				mod_timer(t, jiffies + MAGIC_RETRY_JIFFIES);
			}
		} else {
			magic_reset(data);
		}
	} else if (data->state == 2) {
		if (hits) {
			mod_timer(t, jiffies + MAGIC_TIMER_JIFFIES);
		} else if (!seq->timeout ||
			   time_after(jiffies, data->time + msecs_to_jiffies(seq->timeout))) {
			data->state = 0;
			data->time = jiffies;
			data->index++;
			mod_timer(t, jiffies + MAGIC_TIMER_JIFFIES);
		} else {
			magic_reset(data);
		}
	} else {
		pr_err("[magic_key][%s:%d] never enter\n", __func__, 275);
	}

	seq = data->seq + data->index;
	if (!seq->key[0]) {
		data->index = 0;
		if (!data->state) {
			pr_err("[magic_key][%s:%d] trigger magic key\n", __func__, 281);
			timer_delete(t);
			data->trigger(data);
		}
	}
	data->last = hits;
}

static int key_connect(struct input_handler *handler, struct input_dev *dev,
		       const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);

	pr_info("[magic_key][%s:%d] connect\n", __func__, 315);
	if (!handle) {
		pr_err("[magic_key][%s:%d] no mem\n", __func__, 317);
		return -ENOMEM;
	}

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "magic_key";

	ret = input_register_handle(handle);
	if (ret) {
		pr_err("[magic_key][%s:%d] register fail\n", __func__, 324);
		kfree(handle);
		return ret;
	}

	ret = input_open_device(handle);
	if (ret) {
		pr_err("[magic_key][%s:%d] open fail\n", __func__, 328);
		input_unregister_handle(handle);
		kfree(handle);
		return ret;
	}

	return ret;
}

static void key_disconnect(struct input_handle *handle)
{
	pr_info("[magic_key][%s:%d] disconnect\n", __func__, 341);
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static struct input_handler magic_handler = {
	.event = key_event,
	.connect = key_connect,
	.disconnect = key_disconnect,
	.name = "magic_sysrq",
	.id_table = ids,
};

static int __init magic_key_init(void)
{
	init_timer_key(&datas[0].timer, timer_function, 0, NULL, NULL);
	init_timer_key(&datas[1].timer, timer_function, 0, NULL, NULL);

	if (input_register_handler(&magic_handler))
		pr_err("[magic_key][%s:%d] register failed\n", __func__, 361);

	return 0;
}

module_init(magic_key_init);
module_exit(magic_key_exit);

MODULE_AUTHOR("lizongrui@huaqin.com");
MODULE_DESCRIPTION("magic key driver");
MODULE_LICENSE("GPL v2");
