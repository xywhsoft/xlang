#ifndef XL6_OWNERSHIP_SCOPE_H
#define XL6_OWNERSHIP_SCOPE_H
#include "lib/xrt/xrt.h"
#include <stdlib.h>

/* Resident ownership transitions, not execution/root admission. Enter before
 * any object/API/stack-list lock; leave after the owning-edge transition.
 * Terminal release must isolate the node/detach its owners in a short scope,
 * then run coordinated child releases/finalizers outside its OWN scope with
 * actual code/activity references kept through their return tails. These
 * convenience macros are not suitable for an entire such terminal release.
 * Opaque legacy callbacks retain their conservative scope; no operation may
 * suspend an enclosing caller scope. A fresh, stack-only scope cannot fail in
 * normal operation. Exhausting its counter is a runtime contract violation,
 * not an operation failure that may abandon a consuming argument.
 *
 * Do NOT enclose a normal invocation, wait, join or coroutine park. Activity
 * has real retained references; protect its edge transitions, not its duration.
 * This boundary alone does not certify arbitrary traced children or authorize
 * module retirement. Inspection still needs complete graph participation. */
#define XL6_OWNERSHIP_RETURN(Type, Call) do { \
    xrtownershipscope xl6Mutation = {0}; \
    Type xl6Result; \
    if (!xrtOwnershipMutationBegin(&xl6Mutation)) abort(); \
    xl6Result = (Call); \
    if (!xrtOwnershipScopeEnd(&xl6Mutation)) abort(); \
    return xl6Result; \
} while (0)
#define XL6_OWNERSHIP_RETURN_VOID(Call) do { \
    xrtownershipscope xl6Mutation = {0}; \
    if (!xrtOwnershipMutationBegin(&xl6Mutation)) abort(); \
    (Call); \
    if (!xrtOwnershipScopeEnd(&xl6Mutation)) abort(); \
    return; \
} while (0)
#endif
