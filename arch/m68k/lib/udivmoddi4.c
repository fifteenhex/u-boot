// SPDX-License-Identifier: GPL-2.0+
/*
 * udivmoddi4.c extracted from libgcc2.c with some use of the linux asm-generic stuff
 */

#include <linux/compiler_attributes.h>
#include <asm-generic/bitops/builtin-fls.h>
#include <asm-generic/bitops/fls64.h>

typedef unsigned int USItype	__attribute__ ((mode (SI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));
#define UWtype	USItype
#define UDWtype	UDItype

int __clzsi2(int val) __attribute__((used));
int __clzsi2(int val)
{
	return 32 - fls(val);
}

int __clzdi2(u64 val) __attribute__((used));
int __clzdi2(u64 val)
{
	return 64 - fls64(val);
}

UDWtype
__udivmoddi4 (UDWtype n, UDWtype d, UDWtype *rp)
{
	UDWtype q = 0, r = n, y = d;
	UWtype lz1, lz2, i, k;

	/* Implements align divisor shift dividend method. This algorithm
	 * aligns the divisor under the dividend and then perform number of
	 * test-subtract iterations which shift the dividend left. Number of
	 * iterations is k + 1 where k is the number of bit positions the
	 * divisor must be shifted left to align it under the dividend.
	 * quotient bits can be saved in the rightmost positions of the dividend
	 * as it shifts left on each test-subtract iteration. */

	if (y <= r)
	{
		lz1 = __builtin_clzll (d);
		lz2 = __builtin_clzll (n);

		k = lz1 - lz2;
		y = (y << k);

		/* Dividend can exceed 2 ^ (width - 1) - 1 but still be less than the
		 * aligned divisor. Normal iteration can drops the high order bit
		 * of the dividend. Therefore, first test-subtract iteration is a
		 * special case, saving its quotient bit in a separate location and
		 * not shifting the dividend. */
		if (r >= y)
		{
			r = r - y;
			q =  (1ULL << k);
		}

		if (k > 0)
		{
			y = y >> 1;

		/* k additional iterations where k regular test subtract shift
		 * dividend iterations are done.  */
		i = k;
		do
		{
			if (r >= y)
				r = ((r - y) << 1) + 1;
			else
				r =  (r << 1);
			i = i - 1;
		} while (i != 0);

		/* First quotient bit is combined with the quotient bits resulting
		 * from the k regular iterations.  */
		q = q + r;
		r = r >> k;
		q = q - (r << k);
		}
	}

	if (rp)
		*rp = r;
	return q;
}

UDWtype
__udivdi3(UDWtype n, UDWtype d) __attribute__((used));
UDWtype
__udivdi3(UDWtype n, UDWtype d)
{
	return __udivmoddi4(n, d, (UDWtype *)0);
}
