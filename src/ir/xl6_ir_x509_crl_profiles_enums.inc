static const XL6_IrNumericEnumMemberContract xl6IrX509CrlVersionMembers[]={{"v1",1,0},{"v2",2,0}};
static const XL6_IrNumericEnumContract xl6IrX509CrlVersionContract={"X509CrlVersion",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509CrlVersionMembers,2};
static const XL6_IrNumericEnumMemberContract xl6IrX509CrlReasonMembers[]={{"unspecified",0,0},{"keyCompromise",1,0},{"caCompromise",2,0},{"affiliationChanged",3,0},{"superseded",4,0},{"cessationOfOperation",5,0},{"certificateHold",6,0},{"removeFromCrl",8,0},{"privilegeWithdrawn",9,0},{"aaCompromise",10,0}};
static const XL6_IrNumericEnumContract xl6IrX509CrlReasonContract={"X509CrlReason",XL6_IR_C_ABI_I64,XL6_IR_INTEGER_TARGET_SIGNED_64,xl6IrX509CrlReasonMembers,10};
