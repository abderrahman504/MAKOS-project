

/*
This is a fixed-point representation of numbers
32 bits are split into to parts:
The integer part is the higher-order 17 bits. 
The fraction part is the lower order 14 bits.

To convert an int to real you multiply the int value by 2^14 (left-shift by 14) .
To convert a back to int you round first by adding 0x00002000 then divide by 2^14 (right-shift by 14).
*/
typedef int real;

real int_to_real(int x);
int real_to_int(real x);
real real_add(real x, real y);
real real_subtract(real x, real y);
real real_multiply(real x, real y);
real real_divide(real x, real y);
