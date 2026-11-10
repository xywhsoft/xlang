#ifndef XL6_FIXTURE_POLICY_SAME_RESULT_H
#define XL6_FIXTURE_POLICY_SAME_RESULT_H
#include "foreign_policy_inputs.h"
static int policyInputCreateSibling(PolicyInput** out){return policyInputCreate(out,1);}
static int policyInputClone(PolicyInput* source,PolicyInput** out){return policyInputCreate(out,source->kind);}
static bool policyInputAllReleased(void){return !policyInputLive;}
#endif
