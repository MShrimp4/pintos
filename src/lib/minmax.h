#ifndef __LIB_MINMAX_H
#define __LIB_MINMAX_H

#define MIN(a,b) (((a) > (b)) ? (b) : (a))
#define MAX(a,b) (((a) < (b)) ? (b) : (a))

#define MINF(a,b,alargerthanb) ((alargerthanb (a,b)) ? (b) : (a))
#define MAXF(a,b,alargerthanb) ((alargerthanb (a,b)) ? (a) : (b))

#define CLAMP(num,min,max) MIN(MAX(num, min), max)
#define CLAMPF(num,min,max,f) MINF(MAXF(num, min, f), max, f)

#endif
