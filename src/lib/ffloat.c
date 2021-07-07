#include <ffloat.h>

ffloat
f_div (ffloat a, ffloat b)
{
  ffloat temp;
  temp.__VAL__ = ((int64_t) a.__VAL__) * __FRAC_BIT__ / b.__VAL__;
  return temp;
}



ffloat f_mul (ffloat a, ffloat b)
{
  ffloat temp;
  temp.__VAL__ = ((int64_t) a.__VAL__) * b.__VAL__ / __FRAC_BIT__;
  return temp;
}



ffloat f_add (ffloat a, ffloat b)
{
  ffloat temp;
  temp.__VAL__ = a.__VAL__ + b.__VAL__;
  return temp;
}



ffloat f_sub (ffloat a, ffloat b)
{
  ffloat temp;
  temp.__VAL__ = a.__VAL__ - b.__VAL__;
  return temp;
}



ffloat f_round (ffloat a)
{
  ffloat temp;
  if (a.__VAL__ > 0)
    temp.__VAL__ = a.__VAL__ + (__FRAC_BIT__ / 2);
  else
    temp.__VAL__ = a.__VAL__ - (__FRAC_BIT__ / 2);

  temp.__VAL__ = (temp.__VAL__ / __FRAC_BIT__) * __FRAC_BIT__;
  return temp;
}
