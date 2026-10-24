#define VP "xl6TlsVerificationPolicyDestroyV1"
#define VV "xl6TlsVerifierDestroyV1"
#define VG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsVerificationPolicyCreateV1",T,V,7,{I,I,I,I,I,I,B},.resultOwner=VP,VG},
{"xl6TlsVerificationPolicyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={VP},.nativeHandlePolicy="xl6TlsVerificationPolicyHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
#define XL6_TLS_VERIFY_LIMIT(Field,Name) {"xl6TlsVerificationPolicy" #Field "V1",T,I,1,{V},.parameterOwners={VP},VG},
#include "src/runtime/xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
{"xl6TlsVerificationPolicyAllowsSha1V1",T,B,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyUsesCurrentTimeV1",T,B,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyFixedTimeV1",T,M,1,{V},.parameterOwners={VP},VG},
{"xl6TlsVerificationPolicyWithTimeV1",T,V,2,{V,M},.parameterOwners={VP},.resultOwner=VP,VG},
{"xl6TlsVerificationPolicyWithCurrentTimeV1",T,V,1,{V},.parameterOwners={VP},.resultOwner=VP,VG},
{"xl6TlsVerifierCreateV1",T,V,1,{V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1"},.resultOwner=VV,VG},
{"xl6TlsVerifierCreatePolicyV1",T,V,2,{V,V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1",VP},.resultOwner=VV,VG},
{"xl6TlsVerifierDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={VV},.nativeHandlePolicy="xl6TlsVerifierHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsVerifierPolicyV1",T,V,1,{V},.parameterOwners={VV},.resultOwner=VP,VG},
#undef VG
#undef VP
#undef VV
