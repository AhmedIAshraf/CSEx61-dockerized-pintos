// FIXED-POINT REAL ARITHMATIC IMP
// added by Hager Melook
#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

typedef int real;

#define P 17
#define Q 14
#define F 16384

#define CONVERT_N_TO_FP(n) (n)*(F)
#define CONVERT_X_TO_INT_ROUND_ZERO(x) (x)/(F)
#define CONVERT_X_TO_INT_ROUND_NEAREST(x) ((x)<0 ? ((x)-(F)/2)/(F):((x)+(F)/2)/(F))

#define ADD(x,y) (x)+(y)
#define SUB(x,y) (x)-(y)
#define MUT(x,y) ((int64_t)(x))*(y)/(F)
#define DIV(x,y) ((int64_t)(x))*(F)/(y)

#define ADD_FP_INT(x,n) (x)+(n)*(F)
#define SUB_FP_INT(x,n) (x)-(n)*(F)
#define MUT_FP_INT(x,n) (x)*(n)
#define DIV_FP_INT(x,n) (x)/(n)

#endif