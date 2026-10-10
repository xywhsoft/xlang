#ifndef XL6_SET_KEY_POLICY_H
#define XL6_SET_KEY_POLICY_H
#include "xl6_text_value.h"

/* Private, immutable process-resident key capabilities. Borrowed by typed
 * storage and every clone/algebra result; no generated callback, source
 * module lease, context, or owning Value edge is stored in the policy.
 * Lifecycle/admission still belongs to the exact item type/owner schema.
 * Boxed basic values use content equality/hash (C NULL is canonical null).
 * Mutable container shells use their instance identity; native handles use
 * the owner/producer/context/type domain, never semantic hash/equal callbacks.
 * Ordinary mutation cannot change a retained key's identity. Destructive
 * native handle extraction/reinterpretation is outside the language contract.
 * Future keys use control-block identity, never their eventual result.
 * No recursive content walk or key freezing is needed. No ordering capability
 * is added to the retained-reference descriptor. */
XL6_TEXT_API const xtypedsetkeypolicy* xl6SetReferenceKeyPolicyV1(void);
XL6_TEXT_API const xtypedsetkeypolicy* xl6SetFutureKeyPolicyV1(void);
#endif
