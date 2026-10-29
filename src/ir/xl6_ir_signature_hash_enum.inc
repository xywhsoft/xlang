/* The entire nominal vocabulary is part of the resident signing ABI. */
static const XL6_IrNumericEnumMemberContract xl6IrSignatureHashMembers[]={
    {"sha256",3,0},{"sha384",4,0},{"sha512",5,0}
};
static const XL6_IrNumericEnumContract xl6IrSignatureHashEnumContract={
"SignatureHash",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,
    xl6IrSignatureHashMembers,3
};
