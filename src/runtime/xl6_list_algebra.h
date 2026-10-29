#ifndef XL6_LIST_ALGEBRA_H
#define XL6_LIST_ALGEBRA_H
#include "xl6_list_methods.h"
/* Compiler-private borrowed inputs and independent owned result. Genuine
 * projected List handles use typed storage; IntMap is the explicit migration
 * carrier, never an arbitrary HANDLE. Right-biased merge keeps the left domain
 * and complete int64 keys; mixed carriers use destination admission. Missing
 * subtraction keys are a successful no-op. Failure is checked, including a
 * NULL diagnostic, and never publishes a partial result or modifies inputs. */
XL6_LIST_METHODS_API xvalue* xl6ListValueCloneV1(const xvalue*);
XL6_LIST_METHODS_API xvalue* xl6ListValueMergeRightV1(const xvalue*,const xvalue*);
XL6_LIST_METHODS_API xvalue* xl6ListValueWithoutKeyV1(const xvalue*,int64_t);
#endif
