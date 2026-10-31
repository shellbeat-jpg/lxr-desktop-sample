#pragma once
#include <stdint.h>
#include "main.h"
#define __CLZ(x) __builtin_clz(x)
#define __inline inline
typedef struct { int dummy; } RNG_HandleTypeDef;
typedef union { struct { uint32_t _r:27; uint32_t Q:1; uint32_t V:1; uint32_t C:1; uint32_t Z:1; uint32_t N:1; } b; uint32_t w; } APSR_Type;
static inline uint32_t __get_APSR(void){return 0;}
static inline int32_t __SSAT(int32_t v,int b){int32_t m=(1<<(b-1))-1; return v>m?m:(v<-m-1?-m-1:v);}
static inline int32_t __QADD16(int32_t a,int32_t b){return a+b;}
static inline int32_t __QSUB16(int32_t a,int32_t b){return a-b;}
