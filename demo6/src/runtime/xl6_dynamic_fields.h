#ifndef XL6_DYNAMIC_FIELDS_H
#define XL6_DYNAMIC_FIELDS_H
#include "xl6_text_value.h"
#include "xl6_native_handle.h"
/* Independent native field read capability shared by host and static entry.
 * The caller owns the source and whole-graph Freeze; this is not general
 * lifecycle admission or permission to invoke an unknown native trace. */
XL6_TEXT_API bool xl6DynamicFieldsReadAdmitsV1(struct xrtownershipref);

/* Resident private linkage for the existing language DynamicFields class.
 * Payloads are real XRT objects, not a second map or an estimated RC node.
 * Raw point adoption transfers one genuine native strong owner; arbitrary C
 * pointer validity remains the caller's FFI responsibility. */
#endif
