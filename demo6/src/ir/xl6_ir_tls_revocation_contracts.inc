#define RP "xl6TlsRevocationPolicyDestroyV1"
#define RQ "xl6TlsRevocationRequestDestroyV1"
#define RC "xl6X509RevocationCheckDestroyV1"
#define RG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsRevocationPolicyCreateV1",T,V,6,{XL6_IR_C_ABI_CLOSURE_PTR,B,I,I,I,I},.resultOwner=RP,.parameterEnums={NULL,NULL,NULL,&xl6IrX509CrlPolicyFlagsContract},.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1,.nativeClassCallbackParameter=1,.nativeClassCallbackArgumentOwner=RQ,.nativeClassCallbackResultOwner=RC,RG},
{"xl6TlsRevocationPolicyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RP},.nativeHandlePolicy="xl6TlsRevocationPolicyHandlePolicyV1Get",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TlsRevocationPolicyAllowsUnknownV1",T,B,1,{V},.parameterOwners={RP},RG},
{"xl6TlsRevocationPolicySkewV1",T,I,1,{V},.parameterOwners={RP},RG},
{"xl6TlsRevocationPolicyFlagsV1",T,I,1,{V},.parameterOwners={RP},.resultEnum=&xl6IrX509CrlPolicyFlagsContract,RG},
{"xl6TlsRevocationPolicyMaxSourcesV1",T,I,1,{V},.parameterOwners={RP},RG},
{"xl6TlsRevocationPolicyMaxSignerCertificatesV1",T,I,1,{V},.parameterOwners={RP},RG},
{"xl6TlsRevocationRequestDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RQ},.nativeHandlePolicy="xl6TlsRevocationRequestHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsRevocationRequestPeerV1",T,V,1,{V},.parameterOwners={RQ},.resultOwner="xl6TlsVerifiedPeerDestroyV1",RG},
{"xl6TlsRevocationRequestTargetV1",T,V,1,{V},.parameterOwners={RQ},.resultOwner="xl6X509CertificateDestroyV1",RG},
{"xl6TlsRevocationRequestIssuerV1",T,V,1,{V},.parameterOwners={RQ},.resultOwner="xl6X509CertificateDestroyV1",RG},
{"xl6TlsRevocationRequestIndexV1",T,I,1,{V},.parameterOwners={RQ},RG},
{"xl6TlsRevocationRequestTimeV1",T,M,1,{V},.parameterOwners={RQ},RG},
{"xl6TlsRevocationRequestSkewV1",T,I,1,{V},.parameterOwners={RQ},RG},
{"xl6TlsRevocationRequestFlagsV1",T,I,1,{V},.parameterOwners={RQ},.resultEnum=&xl6IrX509CrlPolicyFlagsContract,RG},
{"xl6TlsVerifierCreateRevocationV1",T,V,3,{V,V,V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1","xl6TlsVerificationPolicyDestroyV1",RP},.resultOwner="xl6TlsVerifierDestroyV1",RG},
{"xl6TlsVerifierCreateCallbackRevocationV1",T,V,4,{V,V,XL6_IR_C_ABI_CLOSURE_PTR,V},.parameterOwners={"xl6X509StoreSnapshotDestroyV1","xl6TlsVerificationPolicyDestroyV1",NULL,RP},.resultOwner="xl6TlsVerifierDestroyV1",.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_PREDICATE_V1,.nativeClassCallbackParameter=3,.nativeClassCallbackArgumentOwner="xl6TlsVerifiedPeerDestroyV1",RG},
#undef RG
#undef RC
#undef RQ
#undef RP
