int xl6IrCgenVerifyEmittable(const XL6_IrModule* module, XL6_DiagList* diags)
{
    if ( module == NULL || module->stage != XL6_IR_STAGE_LOWERED ||
         !xl6IrVerifyModule(module, diags) ) return 0;
    return xl6IrCgenVerifyDeclarations(module, diags) &&
           xl6IrCgenVerifyInstructions(module, diags);
}
