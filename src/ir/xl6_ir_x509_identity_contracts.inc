{"xl6X509MatchesDnsV1",T,XL6_IR_C_ABI_BOOL,4,{S,S,I,I},.pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6X509CertificateMatchesHostV1",T,XL6_IR_C_ABI_BOOL,3,{V,S,I},.parameterOwners={"xl6X509CertificateDestroyV1"},.pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6X509CertificateMatchHostV1",T,V,3,{V,S,I},.resultOwner="xl6X509GeneralNameDestroyV1",.parameterOwners={"xl6X509CertificateDestroyV1"},.nullableResult=1,.pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
