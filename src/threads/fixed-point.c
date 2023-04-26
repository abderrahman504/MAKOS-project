#include "threads/fixed-point.h"
#include <inttypes.h>

real int_to_real(int x)
{
	real y = x * 0x00004000;
	// real y = x << 14;
	return y;
}

int real_to_int(real x)
{
	x = x >= 0 ? x + 0x00002000 : x - 0x00002000;
	int y = x / 0x00004000;
	// int y = x >> 14;
	return y;
}

real real_add(real x, real y)
{
	return x + y;
}

real real_subtract(real x, real y)
{
	return x - y;
}

real real_multiply(real x, real y)
{
	return (int64_t)x * y / 0x00004000;
}

real real_divide(real x, real y)
{
	return (int64_t)x * 0x00004000 / y;
}