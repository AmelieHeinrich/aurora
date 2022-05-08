#include "random_gen.h"

#include <stdlib.h>

f32 float_rand(f32 min, f32 max)
{
    f32 scale = rand() / (f32)RAND_MAX; /* [0, 1.0] */
    return min + scale * ( max - min );      /* [min, max] */
}