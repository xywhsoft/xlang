#define VP "xl6TlsVerificationPolicyDestroyV1"
#define VV "xl6TlsVerifierDestroyV1"
#define VG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsVerificationPolicyCreateV1",T,V,7,{I,I,I,I,I,I,B},.resultOwner=VP,VG},
{"xl6TlsVerificationPolicyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={VP},.nativeHandlePolicy="xl6TlsVerificationPolicyHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
#define XL6_TLS_VERIFY_LIMIT(Field,Name) {"xl6TlsVerificationPolicy" #Field "V1",T,I,1,{V},.parameterOwners={VP},VG},
#include "src/runtime/xl6_tls_verification_policy_fields.h"
XL6_TLS_VERIFICATION_POLICY_FIELDS()
#undef XL6_TLS_VERIFY_LIMIT
{"xl6TlsVerificationPolicyAllowsSha1V1",T,B,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyUsesCurrentTimeV1",T,B,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyFixedTimeV1",T,M,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyWithTimeV1",T,V,2,{V,M},.parameterOwners={VP},.resultOwner=VP,VG},
{"xl6TlsVerificationPolicyWithCurrentTimeV1",T,V,1,{V},.parameterOwners={VP},.resultOwner=VP,VG},
{"xl6TlsVerifierCreateV1",T,V,1,{V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1"},.resultOwner=VV,VG},
{"xl6TlsVerifierCreatePolicyV1",T,V,2,{V,V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1",VP},.resultOwner=VV,VG},
{"xl6TlsVerifierCreateCallbackV1",T,V,3,{V,V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6X509StoreSnapshotDestroyV1",VP},.resultOwner=VV,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_PREDICATE_V1,.nativeClassCallbackParameter=3,.nativeClassCallbackArgumentOwner="xl6TlsVerifiedPeerDestroyV1",VG},
{"xl6TlsVerifierDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={VV},.nativeHandlePolicy="xl6TlsVerifierHandlePolicyV1Get",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TlsVerifierPolicyV1",T,V,1,{V},.parameterOwners={VV},.resultOwner=VP,VG},
#define VW "xl6TlsVerifiedPeerDestroyV1"
{"xl6TlsVerifiedPeerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={VW},.nativeHandlePolicy="xl6TlsVerifiedPeerHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsVerifiedPeerRoleV1",T,I,1,{V},.parameterOwners={VW},.resultEnum=&xl6IrTlsRoleContract,VG},
{"xl6TlsVerifiedPeerTimeV1",T,M,1,{V},.parameterOwners={VW},VG},
{"xl6TlsVerifiedPeerNameV1",T,S,1,{V},.parameterOwners={VW},VG},
{"xl6TlsVerifiedPeerCertificateCountV1",T,I,1,{V},.parameterOwners={VW},VG},
{"xl6TlsVerifiedPeerPathCountV1",T,I,1,{V},.parameterOwners={VW},VG},
{"xl6TlsVerifiedPeerCertificateAtV1",T,V,2,{V,I},.parameterOwners={VW},.resultOwner="xl6X509CertificateDestroyV1",VG},
{"xl6TlsVerifiedPeerPathAtV1",T,V,2,{V,I},.parameterOwners={VW},.resultOwner="xl6X509CertificateDestroyV1",VG},
{"xl6TlsVerifiedPeerAnchorV1",T,V,1,{V},.parameterOwners={VW},.resultOwner="xl6X509CertificateDestroyV1",VG},
#undef VW
#undef VG
#undef VP
#undef VV
#include "xl6_ir_tls_revocation_contracts.inc"
