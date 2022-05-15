#include "random.h"

#include <stdlib.h>

f32 random_float(f32 min, f32 max)
{
    f32 scale = rand() / (f32)RAND_MAX; 
    return min + scale * ( max - min ); 
}

u32 random_uint(u32 min, u32 max)
{
    u32 scale = rand() / (u32)RAND_MAX;
    return min + scale * (max - min);
}