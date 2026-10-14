// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - procfs node operations
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"


/* Global variables matching stock */
unsigned int hyper_log_ctl;

static int last_stathyper_pid = -1;
static int glo_pid = -1;


void reinit_lenovo_task_struct(void *ltsk)
{
	if (init_lenovo_static_task_struct(ltsk) != 0)
		printk(KERN_ERR "reinit_lenovo_task_struct: failed!!!");
}

static int proc_hyper_log_open(struct inode *inode, struct file *file);
static ssize_t proc_hyper_log_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
static ssize_t proc_hyper_log_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int proc_hyper_log_show(struct seq_file *s, void *v);

static int proc_static_hyper_ux_open(struct inode *inode, struct file *file);
static ssize_t proc_static_hyper_ux_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
static ssize_t proc_static_hyper_ux_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int proc_static_hyper_ux_show(struct seq_file *s, void *v);

static int proc_hyper_prio_open(struct inode *inode, struct file *file);
static ssize_t proc_hyper_prio_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
static ssize_t proc_hyper_prio_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int proc_hyper_prio_show(struct seq_file *s, void *v);

static int proc_hyper_stat_open(struct inode *inode, struct file *file);
static int proc_hyper_stat_show(struct seq_file *s, void *v);

static int proc_ux_runtime_open(struct inode *inode, struct file *file);
static ssize_t proc_ux_runtime_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int proc_ux_runtime_show(struct seq_file *s, void *v);

static int proc_ux_period_open(struct inode *inode, struct file *file);
static ssize_t proc_ux_period_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int proc_ux_period_show(struct seq_file *s, void *v);

/* /proc/lenovo_hyperschedule node operations */

static __always_inline long read_user_input(const void __user *user, size_t count, char buf[128])
{
	size_t n = min_t(size_t, count, 127);
	memset(buf, 0, 128);
	if (copy_from_user(buf, user, n))
		return -EFAULT;
	buf[n] = '\0';
	return (long)n;
}

static __always_inline int parse_three(char buf[128], char token[3][8], int *pid, int *value)
{
	char *cursor = strim(buf);
	char *part;
	unsigned int i;
	int ret;

	part = strsep(&cursor, " ");
	for (i = 0; i < 3; i++) {
		size_t n;
		if (!part || !*part)
			break;
		n = min_t(size_t, strlen(part), 7);
		memcpy(token[i], part, n);
		token[i][n] = '\0';
		part = strsep(&cursor, " ");
	}

	ret = kstrtoint(strim(token[1]), 10, pid);
	if (ret)
		return ret;
	return kstrtoint(strim(token[2]), 10, value);
}

static __always_inline char *parse_one(char buf[128], char token[64])
{
	char *cursor = strim(buf);
	char *part;

	part = strsep(&cursor, " ");
	if (part && *part) {
		size_t n = min_t(size_t, strlen(part), 63);
		memcpy(token, part, n);
		token[n] = '\0';
		(void)strsep(&cursor, " ");
	}
	return strim(token);
}

static ssize_t proc_hyper_log_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char kbuf[128], token[64] = "0";
	int value = 0;
	long n = read_user_input(buf, count, kbuf);
	if (n < 0)
		return n;

	{
		int ret = kstrtoint(parse_one(kbuf, token), 0, &value);
		if (ret)
			return ret;
	}

	mutex_lock(&hyper_proc_node_mutex);
	hyper_log_ctl = value;
	mutex_unlock(&hyper_proc_node_mutex);
	return n;
}

static ssize_t proc_hyper_log_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char b[13];
	int len = snprintf(b, sizeof(b), "0x%x\n", hyper_log_ctl);
	return simple_read_from_buffer(buf, count, pos, b, len);
}

static int proc_hyper_log_show(struct seq_file *s, void *v)
{
	seq_printf(s, "0x%x\n", hyper_log_ctl);
	return 0;
}

static int proc_hyper_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_hyper_log_show, inode);
}

static ssize_t proc_static_hyper_ux_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char kbuf[128], token[3][8] = {"0", "0", "0"};
	int pid = 0, value = 0;
	long n = read_user_input(buf, count, kbuf);
	if (n < 0)
		return n;

	{
		int ret = parse_three(kbuf, token, &pid, &value);
		if (ret)
			return ret;
	}

	mutex_lock(&hyper_proc_node_mutex);
	if (token[0][0] == 'p' && value >= 0 && ((u32)pid - 1) <= 32767) {
		struct task_struct *p;
		rcu_read_lock();
		last_stathyper_pid = pid;
		p = find_task_by_vpid(pid);
		if (!p || !LENOVO_TASK(p)) {
			rcu_read_unlock();
			printk(KERN_ERR "proc_static_hyper_ux_write: the pid[%d] is err,because task is NULL!!!", pid);
			mutex_unlock(&hyper_proc_node_mutex);
			return -EFAULT;
		}
		if (!LENOVO_TASK(p)->lsa) {
			reinit_lenovo_task_struct(LENOVO_TASK(p));
			if (!LENOVO_TASK(p)->lsa) {
				rcu_read_unlock();
				printk(KERN_ERR "proc_static_hyper_ux_write: the pid[%d] is err,because ltsk->lsa is NULL!!!", pid);
				mutex_unlock(&hyper_proc_node_mutex);
				return -ENOMEM;
			}
		}
		LENOVO_TASK(p)->lsa->static_hyper_ux = value;
		rcu_read_unlock();
	}
	mutex_unlock(&hyper_proc_node_mutex);
	return n;
}

static ssize_t proc_static_hyper_ux_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	struct task_struct *p;
	int val, pid_val, n;

	if (last_stathyper_pid == -1)
		return 0;

	char kbuf[128] = {0};

	rcu_read_lock();
	p = find_task_by_vpid(last_stathyper_pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(p);
	rcu_read_unlock();

	if (!LENOVO_TASK(p)->lsa) {
		put_task_struct(p);
		return -ESRCH;
	}

	val = LENOVO_TASK(p)->lsa->static_hyper_ux;
	pid_val = p->pid;
	put_task_struct(p);

	n = snprintf(kbuf, sizeof(kbuf), "p pid=%d static_hyper_ux=%d\n", pid_val, val);
	return simple_read_from_buffer(buf, count, pos, kbuf, n);
}

static int proc_static_hyper_ux_show(struct seq_file *s, void *v)
{
	struct task_struct *p;

	rcu_read_lock();
	p = find_task_by_vpid(last_stathyper_pid);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();
	if (!p)
		return -ESRCH;
	if (!LENOVO_TASK(p)->lsa) {
		put_task_struct(p);
		return -ESRCH;
	}
	spin_lock(&p->alloc_lock);
	seq_printf(s, "%d\n", LENOVO_TASK(p)->lsa->static_hyper_ux);
	spin_unlock(&p->alloc_lock);
	put_task_struct(p);
	return 0;
}

static int proc_static_hyper_ux_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_static_hyper_ux_show, inode);
}

static ssize_t proc_hyper_prio_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char kbuf[128], token[3][8] = {"0", "0", "0"};
	int pid = 0, value = 0;
	long n = read_user_input(buf, count, kbuf);
	if (n < 0)
		return n;

	{
		int ret = parse_three(kbuf, token, &pid, &value);
		if (ret)
			return ret;
	}

	printk("create hyper_ux_prio  proc node is :  %d,%d\n", pid, value);

	mutex_lock(&hyper_proc_node_mutex);
	if (token[0][0] == 'p' && value >= 0 && ((u32)pid - 1) <= 32767) {
		struct task_struct *p;
		int ret;
		rcu_read_lock();
		glo_pid = pid;
		p = find_task_by_vpid(pid);
		if (!p) {
			rcu_read_unlock();
			printk(KERN_ERR "proc_hyper_prio_write: the pid[%d] is err,because task is NULL!!!", pid);
			mutex_unlock(&hyper_proc_node_mutex);
			return -EFAULT;
		}
		ret = set_hyper_prio(p, (u32)value);
		rcu_read_unlock();
		mutex_unlock(&hyper_proc_node_mutex);
		return ret ? ret : n;
	}
	mutex_unlock(&hyper_proc_node_mutex);
	return n;
}

static ssize_t proc_hyper_prio_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	struct task_struct *p;
	int val, pid_val, n;

	if (glo_pid == -1)
		return 0;

	char kbuf[64] = {0};

	rcu_read_lock();
	p = find_task_by_vpid(glo_pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(p);
	rcu_read_unlock();

	if (!LENOVO_TASK(p)->lda) {
		put_task_struct(p);
		return -ESRCH;
	}

	val = LENOVO_TASK(p)->lda->normal_hyper_ux_prio;
	pid_val = p->pid;
	put_task_struct(p);

	n = snprintf(kbuf, sizeof(kbuf), "p pid=%d hyper_ux_prio=%d\n", pid_val, val);
	return simple_read_from_buffer(buf, count, pos, kbuf, n);
}

static int proc_hyper_prio_show(struct seq_file *s, void *v)
{
	struct task_struct *p;

	rcu_read_lock();
	p = find_task_by_vpid(glo_pid);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();
	if (!p)
		return -ESRCH;
	if (!LENOVO_TASK(p)->lda) {
		put_task_struct(p);
		return -ESRCH;
	}
	spin_lock(&p->alloc_lock);
	seq_printf(s, "%d\n", LENOVO_TASK(p)->lda->normal_hyper_ux_prio);
	spin_unlock(&p->alloc_lock);
	put_task_struct(p);
	return 0;
}

static int proc_hyper_prio_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_hyper_prio_show, inode);
}

static int proc_hyper_stat_show(struct seq_file *s, void *v)
{
	long static_leaders = 0, dynamic_leaders = 0;
	struct task_struct *g, *t;
	int cpu;

	rcu_read_lock();
	for_each_process(g) {
		struct lenovo_task_struct *lt = LENOVO_TASK(g);

		if (!lt || !lt->lsa)
			continue;
		if (lt->lsa->static_hyper_ux) {
			seq_printf(s, "pid:%d,type:static_ux, value:%d, on_cpu:%d,comm:%s\n",
				   g->pid, lt->lsa->static_hyper_ux, task_cpu(g), g->comm);
			static_leaders++;
			for_each_thread(g, t) {
				struct lenovo_task_struct *thread_lt = LENOVO_TASK(t);

				if (t == g || !thread_lt || !thread_lt->lsa ||
				    !thread_lt->lsa->static_hyper_ux)
					continue;
				seq_printf(s, "  tid:%d,type:static_ux, value:%d, on_cpu:%d, comm:%s\n",
					   t->pid, thread_lt->lsa->static_hyper_ux, task_cpu(t), t->comm);
			}
		}
		if (atomic64_read(&lt->lsa->dynamic_hyper_ux)) {
			seq_printf(s, "pid:%d,type:boost_ux,on_cpu:%d,comm:%s\n",
				   g->pid, task_cpu(g), g->comm);
			dynamic_leaders++;
		}
	}

	seq_printf(s, "hyper_task:%ld;hyper_ux_boost_task:%ld\n", static_leaders, dynamic_leaders);
	seq_printf(s, "\n");

	for_each_possible_cpu(cpu) {
		if (cpu < 32)
			seq_printf(s, "cpu:%d,ux_list:%d\n", cpu,
				   LENOVO_RQ(cpu_rq(cpu))->num_hyper_ux_tasks);
	}
	rcu_read_unlock();
	return 0;
}

static int proc_hyper_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_hyper_stat_show, inode);
}

static ssize_t proc_ux_runtime_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char kbuf[128], token[64] = "0";
	u64 value = 0;
	long n = read_user_input(buf, count, kbuf);
	if (n < 0)
		return n;

	{
		int ret = kstrtoull(parse_one(kbuf, token), 0, &value);
		if (ret)
			return ret;
	}

	mutex_lock(&hyper_proc_node_mutex);
	def_ux_bandwidth.hyper_ux_runtime = value;
	mutex_unlock(&hyper_proc_node_mutex);
	return n;
}

static int proc_ux_runtime_show(struct seq_file *s, void *v)
{
	seq_printf(s, "hyper_ux_runtime: %lluns\n", (unsigned long long)def_ux_bandwidth.hyper_ux_runtime);
	return 0;
}

static int proc_ux_runtime_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ux_runtime_show, inode);
}

static ssize_t proc_ux_period_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char kbuf[128], token[64] = "0";
	u64 value = 0;
	long n = read_user_input(buf, count, kbuf);
	if (n < 0)
		return n;

	{
		int ret = kstrtoull(parse_one(kbuf, token), 0, &value);
		if (ret)
			return ret;
	}

	raw_spin_lock(&def_ux_bandwidth.hyper_ux_runtime_lock);
	def_ux_bandwidth.hyper_ux_period = value;
	def_ux_bandwidth.hyper_ux_max_runtime = value - (value / 5);
	raw_spin_unlock(&def_ux_bandwidth.hyper_ux_runtime_lock);
	return n;
}

static int proc_ux_period_show(struct seq_file *s, void *v)
{
	seq_printf(s, "hyper_ux_period: %lluns\n", (unsigned long long)def_ux_bandwidth.hyper_ux_period);
	return 0;
}

static int proc_ux_period_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_ux_period_show, inode);
}

static const struct proc_ops proc_hyper_log_ops = {
	.proc_open    = proc_hyper_log_open,
	.proc_read    = proc_hyper_log_read,
	.proc_write   = proc_hyper_log_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_static_hyper_ux_operations = {
	.proc_open    = proc_static_hyper_ux_open,
	.proc_read    = proc_static_hyper_ux_read,
	.proc_write   = proc_static_hyper_ux_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_hyper_prio_operations = {
	.proc_open    = proc_hyper_prio_open,
	.proc_read    = proc_hyper_prio_read,
	.proc_write   = proc_hyper_prio_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_hyper_stat_operations = {
	.proc_open    = proc_hyper_stat_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_ux_runtime_operations = {
	.proc_open    = proc_ux_runtime_open,
	.proc_read    = seq_read,
	.proc_write   = proc_ux_runtime_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_ux_period_operations = {
	.proc_open    = proc_ux_period_open,
	.proc_read    = seq_read,
	.proc_write   = proc_ux_period_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

int lenovo_hypernode_proc_init(void)
{
	struct proc_dir_entry *proc_hyper_dir;
	proc_hyper_dir = proc_mkdir("lenovo_hyperschedule", NULL);
	if (!proc_hyper_dir) {
		printk("failed to create proc dir lenovo hyperschedule\n");
		return -ENOENT;
	}

	if (!proc_create("ux_logctl", 0666, proc_hyper_dir, &proc_hyper_log_ops)) {
		printk("failed to create ux_logctl proc node \n");
		goto err_logctl;
	}

	if (!proc_create("static_ux", 0666, proc_hyper_dir, &proc_static_hyper_ux_operations)) {
		printk("failed to create static_ux proc node \n");
		goto err_static;
	}

	if (!proc_create("ux_prio", 0666, proc_hyper_dir, &proc_hyper_prio_operations)) {
		printk("failed to create ux_prio proc node \n");
		goto err_prio;
	}

	if (!proc_create("ux_stat", 0444, proc_hyper_dir, &proc_hyper_stat_operations)) {
		printk("failed to create ux stat proc node \n");
		goto err_stat;
	}

	if (!proc_create("ux_runtime", 0666, proc_hyper_dir, &proc_ux_runtime_operations)) {
		printk("failed to create ux_runtime proc node \n");
		goto err_runtime;
	}

	if (!proc_create("ux_period", 0666, proc_hyper_dir, &proc_ux_period_operations)) {
		printk("failed to create ux_period proc node \n");
		goto err_period;
	}

	return 0;

err_period:
	remove_proc_entry("ux_runtime", proc_hyper_dir);
err_runtime:
	remove_proc_entry("ux_stat", proc_hyper_dir);
err_stat:
	remove_proc_entry("ux_prio", proc_hyper_dir);
err_prio:
	remove_proc_entry("static_ux", proc_hyper_dir);
err_static:
	remove_proc_entry("ux_logctl", proc_hyper_dir);
err_logctl:
	remove_proc_entry("lenovo_hyperschedule", NULL);
	return -ENOENT;
}

