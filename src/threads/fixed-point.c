#include "threads/fixed-point.h"


real int_to_real(int x)
{
	real y = x << 14;
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
	return x * real_to_int(y);
}

real real_divide(real x, real y)
{
	return x / real_to_int(y);
}