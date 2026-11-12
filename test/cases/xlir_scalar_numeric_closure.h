#ifndef XLIR_SCALAR_NUMERIC_CLOSURE_H
#define XLIR_SCALAR_NUMERIC_CLOSURE_H
#include <stdint.h>
#include <stdbool.h>
typedef struct ScalarFloatFields { float value; float samples[2]; } ScalarFloatFields;
#ifdef __cplusplus
extern "C" {
#endif
float fixtureScalarFromBits(uint32_t bits);
uint32_t fixtureScalarFloatBits(float value);
bool fixtureScalarIsInf(float value);
bool fixtureScalarIsNan(float value);
#ifdef __cplusplus
}
#endif
#endif
