#ifndef XL6_SET_KEY_POLICY_H
#define XL6_SET_KEY_POLICY_H
#include "xl6_text_value.h"

/* Private, immutable process-resident key capabilities. Borrowed by typed
 * storage and every clone/algebra result; no generated callback, source
 * module lease, context, or owning Value edge is stored in the policy.
 * Lifecycle/admission still belongs to the exact item type/owner schema.
 * Reference keys use XRT Value equality/hash (C NULL is the canonical null).
 * Future keys use control-block identity, never their eventual result.
 * Keys must remain hash/equality stable while stored. No ordering capability
 * is added to the retained-reference descriptor. */
XL6_TEXT_API const xtypedsetkeypolicy* xl6SetReferenceKeyPolicyV1(void);
XL6_TEXT_API const xtypedsetkeypolicy* xl6SetFutureKeyPolicyV1(void);
#endif
