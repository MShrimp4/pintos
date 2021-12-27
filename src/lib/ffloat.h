#ifndef __LIB_FFLOAT_H
#define __LIB_FFLOAT_H

#include <stdint.h>

#define __FRAC_BIT__ 16384 //2^14

typedef struct
{
  int32_t __VAL__;
} ffloat;

ffloat f_div (ffloat a, ffloat b);
ffloat f_mul (ffloat a, ffloat b);
ffloat f_add (ffloat a, ffloat b);
ffloat f_sub (ffloat a, ffloat b);
ffloat f_round (ffloat a);

#define FFLOAT(a)  ((ffloat) {(int32_t)(a) * __FRAC_BIT__})
#define F_TOINT(a) (a.__VAL__ / __FRAC_BIT__)

#endif /* lib/ffloat.h */
