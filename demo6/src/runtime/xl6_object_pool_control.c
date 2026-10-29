#include "xl6_text_value.h"
#include "xl6_text_thread.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Only thread identity and admission state. Owned objects are normal typed
 * language fields, already visible to the complete class/container graph.
 * This native leaf never hides language references behind a void pointer. */
typedef struct ObjectPoolControl {
    XL6_TextThreadOwner owner;
    bool active,closed;
} ObjectPoolControl;
static ObjectPoolControl* control(void* raw)
{
    if (!raw) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    ObjectPoolControl* self=raw;
    return xl6TextThreadCheck(&self->owner) ? self : NULL;
}
void xl6ObjectPoolControlDestroyV1(void* raw)
{
    ObjectPoolControl* self=raw;
    if (!self) return;
    xl6TextThreadDrop(&self->owner);
    xrtFree(self);
}
void* xl6ObjectPoolControlCreateV1(void)
{
    ObjectPoolControl* self=xrtCalloc(1,sizeof(*self));
    if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner)) {
        xl6ObjectPoolControlDestroyV1(self); return NULL;
    }
    return self;
}
void xl6ObjectPoolControlCheckV1(void* raw)
{
    ObjectPoolControl* self=control(raw);
    if (self && self->closed) xrtSetErrorKind(XERR_CLOSED);
}
void xl6ObjectPoolControlEnterV1(void* raw)
{
    ObjectPoolControl* self=control(raw);
    if (!self) return;
    if (self->closed || self->active) {
        xrtSetErrorKind(self->closed ? XERR_CLOSED : XERR_STATE); return;
    }
    self->active=true;
}
void xl6ObjectPoolControlLeaveV1(void* raw)
{
    /* Only emitted in the finally paired with a successful enter. It owns no
     * failure-prone cleanup and preserves any primary language exception. */
    ((ObjectPoolControl*)raw)->active=false;
}
void xl6ObjectPoolControlIdleV1(void* raw)
{
    ObjectPoolControl* self=control(raw);
    if (self && self->active) xrtSetErrorKind(XERR_STATE);
}
void xl6ObjectPoolControlCloseV1(void* raw)
{
    ObjectPoolControl* self=control(raw);
    if (!self) return;
    if (self->active) { xrtSetErrorKind(XERR_STATE); return; }
    self->closed=true;
}
bool xl6ObjectPoolControlClosedV1(void* raw)
{
    ObjectPoolControl* self=control(raw);
    return self && self->closed;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ObjectPoolControlHandlePolicyV1Get,xl6ObjectPoolControlDestroyV1)
