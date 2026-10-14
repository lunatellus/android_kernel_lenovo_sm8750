/* SPDX-License-Identifier: GPL-2.0 */
/*
 * temperature_model.h - Lenovo virtual thermal zone model
 *
 * Recovered from the stock lenovo_sys_temp.ko (.text 0x838..0x9f0 model,
 * 0x9cc..0x9f0 and 0xab8..0xac0 maxima, 0xaf8..0xb74 name dispatch).
 * Checked against the stock object with emulated test vectors.
 *
 * Plain int arithmetic: every product and the running sum are 32-bit (the
 * kernel builds with -fno-strict-overflow, so they wrap), the division is
 * C's truncating signed "/ 1000".
 *
 * This header is deliberately free of kernel headers so that the very same
 * code can be compiled and executed on the host by verify_thermal.py.
 */
#ifndef _LENOVO_TEMPERATURE_MODEL_H_
#define _LENOVO_TEMPERATURE_MODEL_H_

struct lenovo_temp_result {
	int front;
	int back_candidate;
	int user_front;
	int user_backleft;
	int user_backright;
};

/* Stock .text 0x838..0x9f0. */
static inline void lenovo_calc_temp(const int t[7],
				    struct lenovo_temp_result *r)
{
	r->front = (228 * t[0] + 543 * t[1] + 15 * t[2] + 183 * t[3] -
		    38 * t[4] + 68 * t[5] - 136 * t[6] + 5319910) / 1000 + 1700;
	r->back_candidate = (-119 * t[0] + 1083 * t[1] + 16 * t[2] -
			     152 * t[3] + 26 * t[4] + 4696974) / 1000 + 1500;
	r->user_front = (-117 * t[0] + 352 * t[1] + 1191 * t[2] -
			 354 * t[3] - 18 * t[4] + 733673) / 1000;
	r->user_backleft = (81 * t[0] + 299 * t[1] - 105 * t[2] +
			    367 * t[3] + 215 * t[4] + 3341326) / 1000;
	r->user_backright = (167 * t[0] - 51 * t[1] + 265 * t[2] +
			     304 * t[3] + 180 * t[4] + 3167134) / 1000;
}

#endif /* _LENOVO_TEMPERATURE_MODEL_H_ */
