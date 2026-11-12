#include "xlir_scalar_numeric_closure.h"
#include <string.h>
#include <math.h>
float fixtureScalarFromBits(uint32_t bits)
{float value;memcpy(&value,&bits,sizeof(value));return value;}
uint32_t fixtureScalarFloatBits(float value)
{uint32_t bits;memcpy(&bits,&value,sizeof(bits));return bits;}
bool fixtureScalarIsInf(float value){return isinf(value)!=0;}
bool fixtureScalarIsNan(float value){return isnan(value)!=0;}
