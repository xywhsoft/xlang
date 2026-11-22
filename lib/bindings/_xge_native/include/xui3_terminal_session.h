#ifndef XL6_XUI3_TERMINAL_SESSION_H
#define XL6_XUI3_TERMINAL_SESSION_H
#include <xge3_runtime.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>

static inline void xl6Xui3TerminalSessionFailure(int status, const char* operation)
{
 if (!xrtGetError()) xl6ForeignStatusFailureV1(status, operation);
 xl6_error_result failure = {false, xrtTakeError()};
 if (!xl6ErrorScopeTryPropagateFailureV1(&failure)) xl6CallbackFailureRecordV1(failure);
}

static inline bool xl6Xui3TerminalSessionDestroy(void* raw)
{
 bool consumed = false;
 int status = xuiTerminalSessionReleaseOwnerChecked((xui_terminal_session_t*)raw, &consumed);
 if (status != XUI_OK && status != XUI_STATUS_DEFERRED)
  xl6Xui3TerminalSessionFailure(status, "xuiTerminalSessionReleaseOwnerChecked");
 return consumed;
}

static inline bool xl6Xui3TerminalSessionTrace(const void* raw, XL6_ModuleOwnershipVisit visit, void* user)
{
 return visit(xuiTerminalSessionOwnership((xui_terminal_session_t*)raw), user);
}

static inline bool xl6Xui3TerminalSessionBusy(const void* raw)
{
 xrtownershipref reference = xuiTerminalSessionOwnership((xui_terminal_session_t*)raw);
 const xrtownershipquiescencev1* quiescence = xuiTerminalSessionOwnershipQuiescenceV1(reference);
 return !quiescence || quiescence->Busy(reference.Data);
}

static inline bool xl6Xui3TerminalSessionRequestClose(void* raw)
{
 if (!xl6Xui3TerminalSessionBusy(raw)) return true;
 xl6Xui3TerminalSessionFailure(XUI_ERROR_BUSY, "xui.TerminalSession.close");
 return false;
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xui3TerminalSessionPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy = {sizeof(policy), XL6_FOREIGN_VIEW_V1_TAG,
  "xui.terminalSession", 0, 1, NULL, NULL, xl6Xui3TerminalSessionDestroy, 0, 1,
  NULL, NULL, xl6Xui3TerminalSessionTrace, xl6Xui3TerminalSessionBusy,
  XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE | XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,
  NULL, xl6Xui3TerminalSessionRequestClose};
 return &policy;
}

static inline bool xl6Xui3TerminalSessionResolve(xrtownershipref reference, const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter, const xrtownershippreparationv1** preparation,
 const XL6_OwnershipFinishV1** finish)
{
 (void)policies;
 *adapter = NULL;
 *preparation = NULL;
 *finish = NULL;
 const xrtownershipadapterv1* exact = xuiTerminalSessionOwnershipAdapterV1(reference);
 if (!exact) return false;
 const xrtownershippreparationv1* ready = xuiTerminalSessionOwnershipPreparationV1(reference);
 const xrtownershipfinishv2* complete = xuiTerminalSessionOwnershipCompletionV2(reference);
 if (!ready || ready->size != sizeof(*ready) || ready->Adapter != exact || !complete ||
     complete->V1.size != sizeof(*complete) || complete->AbiTag != XRT_OWNERSHIP_FINISH_V2_TAG ||
     complete->V1.Adapter != exact || !complete->DropChecked || !complete->DropReady) return false;
 *adapter = exact;
 *preparation = ready;
 *finish = &complete->V1;
 return true;
}

static inline bool xl6Xui3TerminalSessionFinalization(xrtownershipref reference,
 const xrtownershipadapterv1* adapter, const xrtownershipfinalizationv1** output)
{
 *output = NULL;
 const xrtownershipadapterv1* exact = xuiTerminalSessionOwnershipAdapterV1(reference);
 if (!exact) return true;
 if (adapter != exact) return false;
 const xrtownershipfinalizationv1* value = xuiTerminalSessionOwnershipFinalizationV1(reference);
 if (!value || value->size != sizeof(*value) || value->AbiTag != XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
     value->Adapter != adapter || !value->CanStep || !value->Step) return false;
 *output = value;
 return true;
}

static inline const XL6_OwnershipDomainV1* xl6Xui3TerminalSessionDomainV1Get(void)
{
 static const XL6_OwnershipDomainV2 domain = {
  {sizeof(domain), NULL, xl6Xui3TerminalSessionResolve, xuiTerminalSessionOwnershipReadReady},
  XL6_OWNERSHIP_DOMAIN_V2_TAG, xl6Xui3TerminalSessionFinalization};
 return &domain.V1;
}
#define xl6Xui3TerminalSessionPolicy (*xl6Xui3TerminalSessionPolicyGet())
#define xl6Xui3TerminalSessionDomainV1 (*xl6Xui3TerminalSessionDomainV1Get())
#endif
