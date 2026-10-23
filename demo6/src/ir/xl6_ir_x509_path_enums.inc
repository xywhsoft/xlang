static const XL6_IrNumericEnumMemberContract xl6IrX509PathFlagsMembers[]={{"none",0,0},{"requireKeyUsage",1,0},{"requirePurpose",2,0},{"allowSha1",4,0}};
static const XL6_IrNumericEnumContract xl6IrX509PathFlagsContract={"X509PathFlags",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509PathFlagsMembers,4};
static const XL6_IrNumericEnumMemberContract xl6IrX509ValidationStatusMembers[]={{"valid",1,0},{"rejected",2,0},{"unsupported",3,0}};
static const XL6_IrNumericEnumContract xl6IrX509ValidationStatusContract={"X509ValidationStatus",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509ValidationStatusMembers,3};
static const XL6_IrNumericEnumMemberContract xl6IrX509PathReasonMembers[]={{"none",0,0},{"validityTime",7,0},{"signature",11,0},{"keyUsage",15,0},{"signatureAlgorithm",21,0},{"issuer",23,0},{"path",24,0},{"constraints",25,0},{"criticalExtension",26,0},{"purpose",27,0},{"nameConstraints",44,0}};
static const XL6_IrNumericEnumContract xl6IrX509PathReasonContract={"X509PathReason",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509PathReasonMembers,11};
