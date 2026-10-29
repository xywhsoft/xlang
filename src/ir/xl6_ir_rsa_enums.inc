/* Separate domains: RSA supports five native hashes; ECDSA's RFC6979
 * SignatureHash vocabulary remains unchanged. Weak SHA1 is explicit legacy. */
static const XL6_IrNumericEnumMemberContract xl6IrRsaHashMembers[]={
    {"sha1Legacy",1,0},{"sha224",2,0},{"sha256",3,0},{"sha384",4,0},{"sha512",5,0}
};
static const XL6_IrNumericEnumContract xl6IrRsaHashEnumContract={
    "RsaHash",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrRsaHashMembers,5
};
static const XL6_IrNumericEnumMemberContract xl6IrRsaPrivatePartMembers[]={
    {"privateExponent",0,0},{"prime1",1,0},{"prime2",2,0},{"exponent1",3,0},{"exponent2",4,0},{"coefficient",5,0}
};
static const XL6_IrNumericEnumContract xl6IrRsaPrivatePartEnumContract={
    "RsaPrivatePart",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrRsaPrivatePartMembers,6
};
