/* The same verified owning-slot vocabulary applies to async frames, generator
 * frames and the current yielded value. No type/name/AST ownership inference. */
static const char* xl6IrCgenOwnershipView(XL6_IrContinuationFrameTraceAbi abi)
{
    switch (abi) {
    case XL6_IR_FRAME_TRACE_XVALUE: return "xrtValueOwnership";
    case XL6_IR_FRAME_TRACE_FUTURE: return "xrtFutureOwnership";
    case XL6_IR_FRAME_TRACE_CLOSURE: return "xlir_closure_ownership";
    case XL6_IR_FRAME_TRACE_CELL: return "xlir_cell_ownership";
    case XL6_IR_FRAME_TRACE_ERROR: return "xrtErrorOwnership";
    case XL6_IR_FRAME_TRACE_VALUE_ITERATOR: return "xlir_xvalue_iterator_ownership";
    case XL6_IR_FRAME_TRACE_GENERATOR: return "xlir_generator_ownership";
    default: return NULL;
    }
}
static const char* xl6IrCgenCaptureKind(XL6_IrAsyncFrameDestroyAbi abi)
{
    switch (abi) {
    case XL6_IR_ASYNC_FRAME_DESTROY_NONE: return "XL6_CAPTURE_TRIVIAL";
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING: return "XL6_CAPTURE_STRING";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE: return "XL6_CAPTURE_XVALUE";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE: return "XL6_CAPTURE_FUTURE";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE: return "XL6_CAPTURE_CLOSURE";
    case XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR: return "XL6_CAPTURE_ERROR";
    case XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CELL: return "XL6_CAPTURE_CELL";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_VALUE_ITERATOR: return "XL6_CAPTURE_VALUE_ITERATOR";
    case XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_GENERATOR: return "XL6_CAPTURE_GENERATOR";
    default: return NULL;
    }
}
/* Serialize the already verified frame and action slices. The resident engine
 * implements tracing/cleanup once; a backend must not reconstruct lifetimes,
 * deduplicate apparent aliases, or invent a destruction order. */
static int xl6IrCgenEmitActivationLayout(XL6_Output* out, const char* prefix, XL6_SymbolId symbol,
    const XL6_IrModule* module,int nativeLayout,
    const XL6_IrContinuationFrameSlot* frames, size_t count, size_t first,
    const XL6_IrContinuationFrameAction* cleanup, size_t cleanupCount)
{
    if (count) {
        xl6OutputPrintf(out, "static const %s %s_slots_%u[] = {\n",
            nativeLayout ? "XL6_ActivationCaptureSlotV2" : "XL6_ActivationCaptureSlot", prefix, (unsigned int)symbol);
        for (size_t local = 0; local < count; ++local) {
            const XL6_IrContinuationFrameSlot* frame = &frames[local];
            const char* kind = xl6IrCgenCaptureKind(frame->destroyAbi);
            const XL6_IrNativeValuePlan* native=frame->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE ?
                xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,frame->type)) : NULL;
            if(native && nativeLayout)kind="XL6_CAPTURE_NATIVE_LEAF_V2";
            if (!kind) return 0;
            if(nativeLayout)xl6OutputPuts("    {",out);
            xl6OutputPrintf(out, "    {{%s, offsetof(%s_frame_%u, f%lu), sizeof(((%s_frame_%u*)0)->f%lu)}, ",
                kind, prefix, (unsigned int)symbol, (unsigned long)local, prefix, (unsigned int)symbol, (unsigned long)local);
            if(nativeLayout){
                if(native)xl6OutputPrintf(out,"&%s}, ",native->captureProfileSymbol);
                else xl6OutputPuts("NULL}, ",out);
            }
            if (frame->initBit == SIZE_MAX) xl6OutputPuts("SIZE_MAX},\n", out);
            else xl6OutputPrintf(out, "offsetof(%s_frame_%u, i%lu)},\n", prefix, (unsigned int)symbol, (unsigned long)frame->initBit);
        }
        xl6OutputPuts("};\n", out);
    }
    if (cleanupCount) {
        xl6OutputPrintf(out, "static const size_t %s_cleanup_%u[] = {", prefix, (unsigned int)symbol);
        for (size_t local = 0; local < cleanupCount; ++local) {
            const XL6_IrContinuationFrameAction* action = &cleanup[local];
            if (action->kind != XL6_IR_ASYNC_FRAME_ACTION_DESTROY_IF_INITIALIZED ||
                action->frameSlot < first || action->frameSlot - first >= count) return 0;
            xl6OutputPrintf(out, "%s%lu", local ? ", " : "", (unsigned long)(action->frameSlot - first));
        }
        xl6OutputPuts("};\n", out);
    }
    const char* layout=nativeLayout?"XL6_ActivationCaptureLayoutV2":"XL6_ActivationCaptureLayoutV1";
    xl6OutputPrintf(out, "static const %s %s_layout_%u = {\n"
        "    sizeof(%s), sizeof(%s_frame_%u), %lu, ",
        layout,prefix, (unsigned int)symbol,layout,prefix, (unsigned int)symbol, (unsigned long)count);
    if (count) xl6OutputPrintf(out, "%s_slots_%u", prefix, (unsigned int)symbol); else xl6OutputPuts("NULL", out);
    xl6OutputPrintf(out, ", %lu, ", (unsigned long)cleanupCount);
    if (cleanupCount) xl6OutputPrintf(out, "%s_cleanup_%u", prefix, (unsigned int)symbol); else xl6OutputPuts("NULL", out);
    xl6OutputPuts("\n};\n", out);
    return !xl6OutputHasFailed(out);
}
