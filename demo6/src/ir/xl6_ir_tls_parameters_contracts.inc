#define TP "xl6TlsPolicyDestroyV1"
#define TL "xl6TlsLimitsDestroyV1"
#define TC "xl6TlsParametersDestroyV1"
#define TG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsLimitsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TL},.nativeHandlePolicy="xl6TlsLimitsHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsPolicyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TP},.nativeHandlePolicy="xl6TlsPolicyHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsParametersDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC},.nativeHandlePolicy="xl6TlsParametersHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsLimitsCreateV1",T,V,6,{I,I,I,I,I,I},.resultOwner=TL,TG},
#define XL6_TLS_LIMIT(Field,Name) {"xl6TlsLimits" #Field "V1",T,I,1,{V},.parameterOwners={TL},TG},
#include "src/runtime/xl6_tls_limits_fields.inc"
#undef XL6_TLS_LIMIT
{"xl6TlsPolicyDefaultV1",T,V,0,{0},.resultOwner=TP,TG},
#define XL6_TLS_POLICY_LIST(Singular,Plural,Type,Limit) {"xl6TlsPolicy" #Singular "CountV1",T,I,1,{V},.parameterOwners={TP},TG},
#include "src/runtime/xl6_tls_policy_lists.inc"
#undef XL6_TLS_POLICY_LIST
{"xl6TlsPolicyVersionAtV1",T,I,2,{V,I},.parameterOwners={TP},.resultEnum=&xl6IrTlsVersionContract,TG},
{"xl6TlsPolicyCipherAtV1",T,I,2,{V,I},.parameterOwners={TP},.resultEnum=&xl6IrTlsCipherSuiteContract,TG},
{"xl6TlsPolicyGroupAtV1",T,I,2,{V,I},.parameterOwners={TP},.resultEnum=&xl6IrTlsGroupContract,TG},
{"xl6TlsPolicySignatureAtV1",T,I,2,{V,I},.parameterOwners={TP},.resultEnum=&xl6IrTlsSignatureContract,TG},
{"xl6TlsPolicyKeyShareV1",T,I,1,{V},.parameterOwners={TP},.resultEnum=&xl6IrTlsKeySharePolicyContract,TG},
{"xl6TlsParametersDefaultV1",T,V,0,{0},.resultOwner=TC,TG},
{"xl6TlsParametersCreateV1",T,V,2,{V,V},.parameterOwners={TP,TL},.resultOwner=TC,TG},
{"xl6TlsParametersPolicyV1",T,V,1,{V},.parameterOwners={TC},.resultOwner=TP,TG},
{"xl6TlsParametersLimitsV1",T,V,1,{V},.parameterOwners={TC},.resultOwner=TL,TG},
#undef TG
#undef TP
#undef TL
#undef TC
