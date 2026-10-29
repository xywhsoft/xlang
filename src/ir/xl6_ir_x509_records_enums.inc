static const XL6_IrNumericEnumMemberContract xl6IrX509SignatureKindMembers[]={{"rsaPkcs1",1,0},{"rsaPss",2,0},{"ecdsa",3,0},{"ed25519",4,0},{"ed448",5,0}};
static const XL6_IrNumericEnumContract xl6IrX509SignatureKindContract={"X509SignatureKind",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509SignatureKindMembers,5};
static const XL6_IrNumericEnumMemberContract xl6IrX509DigestMembers[]={{"none",0,0},{"sha1Legacy",1,0},{"sha224",2,0},{"sha256",3,0},{"sha384",4,0},{"sha512",5,0}};
static const XL6_IrNumericEnumContract xl6IrX509DigestContract={"X509Digest",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509DigestMembers,6};
static const XL6_IrNumericEnumMemberContract xl6IrX509KeyKindMembers[]={{"unknown",0,0},{"rsa",1,0},{"rsaPss",2,0},{"ec",3,0},{"ed25519",4,0},{"ed448",5,0},{"x25519",6,0},{"x448",7,0}};
static const XL6_IrNumericEnumContract xl6IrX509KeyKindContract={"X509KeyKind",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509KeyKindMembers,8};
static const XL6_IrNumericEnumMemberContract xl6IrX509CurveMembers[]={{"unknown",0,0},{"p256",1,0},{"p384",2,0},{"p521",3,0}};
static const XL6_IrNumericEnumContract xl6IrX509CurveContract={"X509Curve",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509CurveMembers,4};
