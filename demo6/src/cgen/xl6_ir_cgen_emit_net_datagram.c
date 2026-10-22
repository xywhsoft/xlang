/* UDP uses exact resident declarations/calls. Only the unrelated legacy
 * Engine snapshot still needs this closed component selector. */
static int xl6IrCgenEmitNetDatagramRuntime(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    (void)diags;
    if (!needs->needsNetDatagramRuntime) return 1;
    XL6_CgenNetDatagramNeeds component = {0};
    for (size_t i=0; i<module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        if (target->reachability!=XL6_IR_REACHABILITY_UNREACHABLE &&
            target->runtimeBridge==XL6_IR_RUNTIME_BRIDGE_NET_ENGINE_STATS) component.engineStats=1;
    }
    xl6CgenEmitNetDatagramComponent(out,&component);
    return !xl6OutputHasFailed(out);
}
