/* Select pure transport components and synthesize only the object/Future ABI
 * adapters described by validated XLIR. No source spelling lookup here. */
static int xl6IrCgenEmitNetDatagramRuntime(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    XL6_CgenNetDatagramNeeds component = {0};
    size_t i, p;
    if (!needs->needsNetDatagramRuntime) return 1;
    for (i = 0; i < module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if (target->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        switch (target->runtimeBridge) {
        case XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST: component.addressList = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC: component.resolveAsync = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_UDP_SOCKET:
            component.socket = 1;
            for (p = 0; p < target->parameterCount; ++p)
                if (xl6IrTypeGet(&module->types, target->parameterTypes[p])->collectionAbi == XL6_IR_COLLECTION_ARRAY)
                    component.buffers = 1;
            break;
        case XL6_IR_RUNTIME_BRIDGE_NET_UDP_PACKET: component.packet = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_UDP_BATCH: component.batch = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_UDP_RECV_ASYNC: component.recvAsync = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_ENGINE_STATS: component.engineStats = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_RESOLVER_STATS: component.resolverStats = 1; break;
        case XL6_IR_RUNTIME_BRIDGE_NET_UDP_STATS: component.udpStats = 1; break;
        default: break;
        }
    }
    if (component.buffers) {
        const XL6_IrClassField* field = xl6IrRuntimeNetBufferField(module);
        const XL6_IrType* type = field != NULL ? xl6IrTypeGet(&module->types, field->ownerType) : NULL;
        if (type == NULL) return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID, "XLIR datagram Buffer projection is unresolved");
        xl6IrCgenEmitNetClassHandleProjection(out, field, "datagram_buffer");
        xl6OutputPrintf(out,
            "static xbuffer* xlir_datagram_buffer_borrow(const xvalue* value)\n"
            "{ return value != NULL && xrtValueTypeId(value) == UINT64_C(0x%016llX) ? (xbuffer*)xlir_net_datagram_buffer_borrow_handle(value) : NULL; }\n",
            (unsigned long long)type->typeValueId);
    }
    if (component.resolveAsync || component.recvAsync) {
        xl6OutputPuts(
            "static void xlir_datagram_publish(xpromise* output, xvalue* owned)\n"
            "{ xlir_native_future_publish_xvalue(output, owned); }\n"
            "static xfuture* xlir_datagram_future_map(xfuture* source, xfuturecontinueproc mapper, void* argument)\n"
            "{ xfuture* result; if (source == NULL) return NULL; result = xlir_native_future_then(source, mapper, (uintptr_t)argument); xrtFutureDestroy(source); return result; }\n", out);
    }
    if (component.resolveAsync) {
        XL6_IrTypeId type = xl6IrCgenNetFutureClassPayload(module, XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC);
        if (!xl6IrCgenEmitNetOwnedClassWrapper(out, module, type, "addresses", diags)) return 0;
        xl6OutputPuts(
            "static void xlir_datagram_map_addresses(const xfutureresult* input, xpromise* output, void* argument)\n"
            "{ xnetaddrlist* list = input != NULL ? (xnetaddrlist*)input->Value : NULL; void* owned = list != NULL ? xrtNetAddrListWithPort(list, (uint16_t)(uintptr_t)argument) : NULL; xlir_datagram_publish(output, xlir_net_addresses_wrap_owned(owned)); }\n", out);
    }
    if (component.recvAsync) {
        XL6_IrTypeId type = xl6IrCgenNetFutureClassPayload(module, XL6_IR_RUNTIME_BRIDGE_NET_UDP_RECV_ASYNC);
        if (!xl6IrCgenEmitNetClassWrapper(out, module, type, "packet", "xnetudppacket", "xrtNetUdpPacketRef", diags)) return 0;
        xl6OutputPuts(
            "static void xlir_datagram_map_packet(const xfutureresult* input, xpromise* output, void* argument)\n"
            "{ (void)argument; xlir_datagram_publish(output, xlir_net_packet_wrap_borrowed(input != NULL ? input->Value : NULL)); }\n", out);
    }
    xl6CgenEmitNetDatagramComponent(out, &component);
    return !xl6OutputHasFailed(out);
}
