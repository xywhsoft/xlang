#ifndef XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION
/* 输出地址、代理和显式 Engine 的语言绑定。 */
static void xl6CgenEmitNetObjects(XL6_Output* out, const XL6_CgenNetNeeds* needs)
{
    if ( needs->address ) {
    xl6OutputPuts(
        "typedef struct xl6_net_address { xnetaddr Value; } xl6_net_address;\n\n"
        "void* xl6_xrt_net_address_create(void)\n"
        "{\n"
        "    xl6_net_address* address = (xl6_net_address*)xrtCalloc(1u, sizeof(*address));\n"
        "    if ( (address != 0) && !xrtNetAddrAny(&address->Value, XL6_XNET_FAMILY_IPV4, 0u) ) {\n"
        "        xrtFree(address);\n"
        "        return 0;\n"
        "    }\n"
        "    return address;\n"
        "}\n\n"
        "void* xl6_xrt_net_address_resolve(const char* host, long long port)\n"
        "{\n"
        "    xl6_net_address* address;\n\n"
        "    if ( (host == 0) || (host[0] == '\\0') || (port < 0) || (port > 65535) ) { return 0; }\n"
        "    address = (xl6_net_address*)xrtCalloc(1u, sizeof(*address));\n"
        "    if ( (address == 0) || !xl6_net_address_parse(&address->Value, host, (uint16_t)port, false) ) {\n"
        "        xrtFree(address);\n"
        "        return 0;\n"
        "    }\n"
        "    return address;\n"
        "}\n\n"
        "long long xl6_xrt_net_address_destroy(void* value) { xrtFree(value); return 0; }\n"
        "void* xl6_xrt_net_address_text(void* value) { return xl6_net_address_string(value != 0 ? &((xl6_net_address*)value)->Value : 0); }\n"
        "long long xl6_xrt_net_address_family(void* value) { return value != 0 ? ((xl6_net_address*)value)->Value.Family : 0; }\n"
        "long long xl6_xrt_net_address_port(void* value) { return value != 0 ? ((xl6_net_address*)value)->Value.Port : 0; }\n"
        "long long xl6_xrt_net_address_scope(void* value) { return value != 0 ? ((xl6_net_address*)value)->Value.Scope : 0; }\n"
        "void* xl6_xrt_net_address_list_resolve(const char* host, long long port, long long family)\n"
        "{\n"
        "    if ( (host == 0) || (host[0] == '\\0') || (port < 0) || (port > 65535) || ((family != 0) && (family != 4) && (family != 6)) ) { return 0; }\n"
        "    return xrtNetResolve(host, (uint16_t)port, (xnetfamily)family);\n"
        "}\n"
        "long long xl6_xrt_net_address_list_destroy(void* value) { xrtNetAddrListDestroy((xnetaddrlist*)value); return 0; }\n"
        "long long xl6_xrt_net_address_list_count(void* value) { return value != 0 ? (long long)xrtNetAddrListCount((xnetaddrlist*)value) : 0; }\n"
        "void* xl6_xrt_net_address_list_text(void* value, long long index)\n"
        "{\n"
        "    const xnetaddr* address = (value != 0) && (index >= 0) ? xrtNetAddrListGet((xnetaddrlist*)value, (size_t)index) : 0;\n"
        "    return xl6_net_address_string(address);\n"
        "}\n"
        "long long xl6_xrt_net_address_list_family(void* value, long long index) { const xnetaddr* address = (value != 0) && (index >= 0) ? xrtNetAddrListGet((xnetaddrlist*)value, (size_t)index) : 0; return address != 0 ? address->Family : 0; }\n"
        "long long xl6_xrt_net_address_list_port(void* value, long long index) { const xnetaddr* address = (value != 0) && (index >= 0) ? xrtNetAddrListGet((xnetaddrlist*)value, (size_t)index) : 0; return address != 0 ? address->Port : 0; }\n\n",
        out
    );
    }

    if ( needs->interfaces ) {
    xl6OutputPuts(
        "/* 接口列表、接口和地址视图共享一份不可变系统快照。 */\n"
        "typedef struct xl6_net_interface_owner {\n"
        "    xatomic32 References;\n"
        "    xnetinterfacelist List;\n"
        "} xl6_net_interface_owner;\n\n"
        "typedef struct xl6_net_interface_view {\n"
        "    xl6_net_interface_owner* Owner;\n"
        "    size_t Index;\n"
        "} xl6_net_interface_view;\n\n"
        "typedef struct xl6_net_interface_address_view {\n"
        "    xl6_net_interface_owner* Owner;\n"
        "    size_t InterfaceIndex;\n"
        "    size_t AddressIndex;\n"
        "} xl6_net_interface_address_view;\n\n"
        "/* 增加共享接口快照的引用。 */\n"
        "static void xl6_net_interface_owner_retain(xl6_net_interface_owner* owner)\n"
        "{\n"
        "    if ( owner != 0 ) { (void)xrtAtomic32FetchAdd(&owner->References, 1u, XMEMORY_RELAXED); }\n"
        "}\n\n"
        "/* 释放最后一个视图时归还系统接口快照。 */\n"
        "static void xl6_net_interface_owner_release(xl6_net_interface_owner* owner)\n"
        "{\n"
        "    if ( (owner != 0) && (xrtAtomic32FetchSub(&owner->References, 1u, XMEMORY_ACQ_REL) == 1u) ) {\n"
        "        xrtNetInterfacesFree(&owner->List);\n"
        "        xrtFree(owner);\n"
        "    }\n"
        "}\n\n"
        "/* 校验并取得接口视图借用的条目。 */\n"
        "static const xnetinterface* xl6_net_interface_value(const xl6_net_interface_view* view)\n"
        "{\n"
        "    if ( (view == 0) || (view->Owner == 0) || (view->Index >= view->Owner->List.Count) ) { return 0; }\n"
        "    return &view->Owner->List.Items[view->Index];\n"
        "}\n\n"
        "/* 校验并取得接口地址视图借用的条目。 */\n"
        "static const xnetinterfaceaddress* xl6_net_interface_address_value(const xl6_net_interface_address_view* view)\n"
        "{\n"
        "    const xnetinterface* interfaceValue;\n\n"
        "    if ( (view == 0) || (view->Owner == 0) || (view->InterfaceIndex >= view->Owner->List.Count) ) { return 0; }\n"
        "    interfaceValue = &view->Owner->List.Items[view->InterfaceIndex];\n"
        "    return view->AddressIndex < interfaceValue->AddressCount ? &interfaceValue->Addresses[view->AddressIndex] : 0;\n"
        "}\n\n"
        "/* 创建当前系统接口的一致快照。 */\n"
        "void* xl6_xrt_net_interfaces_create(void)\n"
        "{\n"
        "    xl6_net_interface_owner* owner = (xl6_net_interface_owner*)xrtCalloc(1u, sizeof(*owner));\n\n"
        "    if ( owner == 0 ) { return 0; }\n"
        "    xrtAtomic32Init(&owner->References, 1u);\n"
        "    if ( !xrtNetInterfaces(&owner->List) ) { xrtFree(owner); return 0; }\n"
        "    return owner;\n"
        "}\n\n"
        "/* 释放列表持有的共享快照。 */\n"
        "long long xl6_xrt_net_interfaces_destroy(void* raw)\n"
        "{\n"
        "    xl6_net_interface_owner_release((xl6_net_interface_owner*)raw);\n"
        "    return 0;\n"
        "}\n\n"
        "/* 返回当前快照中的接口数量。 */\n"
        "long long xl6_xrt_net_interfaces_count(void* raw)\n"
        "{\n"
        "    xl6_net_interface_owner* owner = (xl6_net_interface_owner*)raw;\n"
        "    return owner != 0 ? (long long)owner->List.Count : 0;\n"
        "}\n\n"
        "/* 创建可在列表销毁后继续使用的接口视图。 */\n"
        "void* xl6_xrt_net_interfaces_get(void* raw, long long index)\n"
        "{\n"
        "    xl6_net_interface_owner* owner = (xl6_net_interface_owner*)raw;\n"
        "    xl6_net_interface_view* view;\n\n"
        "    if ( (owner == 0) || (index < 0) || ((uint64_t)index >= (uint64_t)owner->List.Count) ) { return 0; }\n"
        "    view = (xl6_net_interface_view*)xrtMalloc(sizeof(*view));\n"
        "    if ( view == 0 ) { return 0; }\n"
        "    xl6_net_interface_owner_retain(owner);\n"
        "    view->Owner = owner;\n"
        "    view->Index = (size_t)index;\n"
        "    return view;\n"
        "}\n\n"
        "/* point 构造函数接管已经持有引用的接口视图。 */\n"
        "void* xl6_xrt_net_interface_wrap(void* raw) { return raw; }\n\n"
        "/* 释放接口视图。 */\n"
        "long long xl6_xrt_net_interface_destroy(void* raw)\n"
        "{\n"
        "    xl6_net_interface_view* view = (xl6_net_interface_view*)raw;\n\n"
        "    if ( view != 0 ) { xl6_net_interface_owner_release(view->Owner); xrtFree(view); }\n"
        "    return 0;\n"
        "}\n\n"
        "/* 复制接口规范名称。 */\n"
        "void* xl6_xrt_net_interface_name(void* raw)\n"
        "{\n"
        "    const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw);\n"
        "    return value != 0 ? xl6_xrt_string_dup(value->Name.Data != 0 ? value->Name.Data : \"\", value->Name.Size) : xl6_xrt_string_dup(\"\", 0u);\n"
        "}\n\n"
        "/* 复制接口显示名称。 */\n"
        "void* xl6_xrt_net_interface_display_name(void* raw)\n"
        "{\n"
        "    const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw);\n"
        "    return value != 0 ? xl6_xrt_string_dup(value->DisplayName.Data != 0 ? value->DisplayName.Data : \"\", value->DisplayName.Size) : xl6_xrt_string_dup(\"\", 0u);\n"
        "}\n\n"
        "long long xl6_xrt_net_interface_ipv4_index(void* raw) { const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw); return value != 0 ? (long long)value->IPv4Index : 0; }\n"
        "long long xl6_xrt_net_interface_ipv6_index(void* raw) { const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw); return value != 0 ? (long long)value->IPv6Index : 0; }\n"
        "long long xl6_xrt_net_interface_flags(void* raw) { const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw); return value != 0 ? (long long)value->Flags : 0; }\n"
        "long long xl6_xrt_net_interface_mtu(void* raw) { const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw); return value != 0 ? (long long)value->Mtu : 0; }\n\n"
        "/* 复制原始硬件地址，文本编码由调用方显式选择。 */\n"
        "void* xl6_xrt_net_interface_hardware(void* raw)\n"
        "{\n"
        "    const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw);\n"
        "    xbytesview bytes = value != 0 ? value->HardwareAddress : (xbytesview){ 0, 0u };\n\n"
        "    return xl6_xrt_buffer_from_view_bytes(bytes.Data, bytes.Size);\n"
        "}\n\n"
        "long long xl6_xrt_net_interface_address_count(void* raw) { const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw); return value != 0 ? (long long)value->AddressCount : 0; }\n\n"
        "/* 创建可独立存活的接口地址视图。 */\n"
        "void* xl6_xrt_net_interface_address(void* raw, long long index)\n"
        "{\n"
        "    xl6_net_interface_view* interfaceView = (xl6_net_interface_view*)raw;\n"
        "    const xnetinterface* value = xl6_net_interface_value(interfaceView);\n"
        "    xl6_net_interface_address_view* view;\n\n"
        "    if ( (value == 0) || (index < 0) || ((uint64_t)index >= (uint64_t)value->AddressCount) ) { return 0; }\n"
        "    view = (xl6_net_interface_address_view*)xrtMalloc(sizeof(*view));\n"
        "    if ( view == 0 ) { return 0; }\n"
        "    xl6_net_interface_owner_retain(interfaceView->Owner);\n"
        "    view->Owner = interfaceView->Owner;\n"
        "    view->InterfaceIndex = interfaceView->Index;\n"
        "    view->AddressIndex = (size_t)index;\n"
        "    return view;\n"
        "}\n\n"
        "/* 判断接口是否同时具备指定标志集合。 */\n"
        "int xl6_xrt_net_interface_has_flag(void* raw, long long flag)\n"
        "{\n"
        "    const xnetinterface* value = xl6_net_interface_value((xl6_net_interface_view*)raw);\n"
        "    return (value != 0) && (flag > 0) && ((uint64_t)flag <= UINT32_MAX) && ((value->Flags & (uint32)flag) == (uint32)flag);\n"
        "}\n\n"
        "/* point 构造函数接管已经持有引用的地址视图。 */\n"
        "void* xl6_xrt_net_interface_address_wrap(void* raw) { return raw; }\n\n"
        "/* 释放接口地址视图。 */\n"
        "long long xl6_xrt_net_interface_address_destroy(void* raw)\n"
        "{\n"
        "    xl6_net_interface_address_view* view = (xl6_net_interface_address_view*)raw;\n\n"
        "    if ( view != 0 ) { xl6_net_interface_owner_release(view->Owner); xrtFree(view); }\n"
        "    return 0;\n"
        "}\n\n"
        "void* xl6_xrt_net_interface_address_text(void* raw) { const xnetinterfaceaddress* value = xl6_net_interface_address_value((xl6_net_interface_address_view*)raw); char* text = value != 0 ? xrtNetAddrString(&value->Address) : 0; return text != 0 ? text : xl6_xrt_string_dup(\"\", 0u); }\n"
        "long long xl6_xrt_net_interface_address_family(void* raw) { const xnetinterfaceaddress* value = xl6_net_interface_address_value((xl6_net_interface_address_view*)raw); return value != 0 ? (long long)value->Address.Family : 0; }\n"
        "long long xl6_xrt_net_interface_address_prefix(void* raw) { const xnetinterfaceaddress* value = xl6_net_interface_address_value((xl6_net_interface_address_view*)raw); return value != 0 ? (long long)value->PrefixLength : (long long)XNET_INTERFACE_PREFIX_UNKNOWN; }\n"
        "long long xl6_xrt_net_interface_address_scope(void* raw) { const xnetinterfaceaddress* value = xl6_net_interface_address_value((xl6_net_interface_address_view*)raw); return value != 0 ? (long long)value->Address.Scope : 0; }\n\n"
		"/* 把接口名称转换为指定地址族的系统索引。 */\n"
		"int64_t xl6_xrt_net_interface_index(const char* name, int64_t family)\n"
		"{\n"
		"    if ( (name == 0) || (name[0] == '\\0') || ((family != 0) && (family != 4) && (family != 6)) ) { return 0; }\n"
		"    return (int64_t)xrtNetInterfaceIndex(name, (xnetfamily)family);\n"
		"}\n\n"
		"/* 把系统接口索引转换为规范名称。 */\n"
		"void* xl6_xrt_net_interface_name_by_index(int64_t index, int64_t family)\n"
        "{\n"
        "    size_t size;\n"
        "    size_t actual;\n"
        "    char* name;\n\n"
        "    if ( (index <= 0) || ((uint64_t)index > UINT32_MAX) || ((family != 0) && (family != 4) && (family != 6)) ) { return xl6_xrt_string_dup(\"\", 0u); }\n"
        "    size = xrtNetInterfaceName((uint32)index, (xnetfamily)family, 0, 0u);\n"
        "    if ( (size == XRT_NPOS) || (size == SIZE_MAX) ) { return xl6_xrt_string_dup(\"\", 0u); }\n"
        "    name = (char*)xrtMalloc(size + 1u);\n"
        "    if ( name == 0 ) { return 0; }\n"
        "    actual = xrtNetInterfaceName((uint32)index, (xnetfamily)family, name, size + 1u);\n"
        "    if ( actual != size ) { xrtFree(name); return xl6_xrt_string_dup(\"\", 0u); }\n"
        "    name[size] = '\\0';\n"
        "    return name;\n"
        "}\n\n",
        out
    );
    }

    if ( needs->resolverAsync ) {
        xl6OutputPuts(
            "xfuture* xl6_xrt_net_resolve_async(const char* host, long long port, long long family)\n"
            "{\n"
            "    if ( (xl6_net_resolver == 0) || (host == 0) || (host[0] == '\\0') || (port < 0) || (port > 65535) || ((family != 0) && (family != 4) && (family != 6)) ) { return 0; }\n"
            "    return xl6_net_future_map_arg(xrtNetResolveAsync(xl6_net_resolver, host, (xnetfamily)family), xl6_net_map_address_list, (void*)(uintptr_t)(uint16_t)port);\n"
            "}\n\n",
            out
        );
    }

    if ( needs->proxy ) {
    xl6OutputPuts(
        "void* xl6_xrt_net_proxy_create(long long type, const char* host, long long port, const char* user, const char* password)\n"
        "{\n"
        "    xnetproxyconfig config;\n\n"
        "    if ( (type < 1) || (type > 2) || (host == 0) || (host[0] == '\\0') || (port <= 0) || (port > 65535) ) { return 0; }\n"
        "    xrtNetProxyConfigInit(&config);\n"
        "    config.Type = (int)type;\n"
        "    config.Host = (xstrview){ host, strlen(host) };\n"
        "    config.Port = (uint16_t)port;\n"
        "    config.Username = (XL6_XrtBytesView){ (const unsigned char*)(user != 0 ? user : \"\"), user != 0 ? strlen(user) : 0u };\n"
        "    config.Password = (XL6_XrtBytesView){ (const unsigned char*)(password != 0 ? password : \"\"), password != 0 ? strlen(password) : 0u };\n"
        "    return xrtNetProxyCreate(&config);\n"
        "}\n\n"
        "long long xl6_xrt_net_proxy_destroy(void* value) { xrtNetProxyRelease((xnetproxy*)value); return 0; }\n"
        "void* xl6_xrt_net_proxy_native(void* value) { return value; }\n"
        "long long xl6_xrt_net_proxy_type(void* value) { xnetproxyinfo info; return (value != 0) && xrtNetProxyInfo((xnetproxy*)value, &info) ? info.Type : 0; }\n"
        "long long xl6_xrt_net_proxy_port(void* value) { xnetproxyinfo info; return (value != 0) && xrtNetProxyInfo((xnetproxy*)value, &info) ? info.Port : 0; }\n"
        "void* xl6_xrt_net_proxy_host(void* value) { xnetproxyinfo info; return (value != 0) && xrtNetProxyInfo((xnetproxy*)value, &info) ? xl6_xrt_string_dup(info.Host.Data, info.Host.Size) : xl6_xrt_string_dup(\"\", 0u); }\n"
        "void* xl6_xrt_net_proxy_user(void* value) { xnetproxyinfo info; return (value != 0) && xrtNetProxyInfo((xnetproxy*)value, &info) ? xl6_xrt_string_dup((const char*)info.Username.Data, info.Username.Size) : xl6_xrt_string_dup(\"\", 0u); }\n\n",
        out
    );
    }

    if ( needs->engineObject ) {
    xl6OutputPuts("typedef struct xl6_net_task { XL6_Function Callback; } xl6_net_task;\n\n", out);
    xl6OutputPuts(
        "static void xl6_net_task_release(xl6_net_task* task)\n"
        "{\n"
        "    if ( task == 0 ) { return; }\n"
        "    if ( (task->Callback.release != 0) && (task->Callback.env != 0) ) { task->Callback.release(task->Callback.env); }\n"
        "    xrtFree(task);\n"
        "}\n\n"
        "static xl6_net_task* xl6_net_task_create(XL6_Function callback)\n"
        "{\n"
        "    xl6_net_task* task;\n\n"
        "    if ( callback.invoke_var == 0 ) { return 0; }\n"
        "    task = (xl6_net_task*)xrtCalloc(1u, sizeof(*task));\n"
        "    if ( task == 0 ) { return 0; }\n"
        "    task->Callback = callback;\n"
        "    if ( (callback.retain != 0) && (callback.env != 0) ) { callback.retain(callback.env); }\n"
        "    return task;\n"
        "}\n\n"
        "static void xl6_net_task_run(xnetworker* worker, void* arg)\n"
        "{\n"
        "    xl6_net_task* task = (xl6_net_task*)arg;\n"
        "    xvalue* result = 0;\n"
        "    (void)worker;\n"
        "    if ( (task != 0) && (task->Callback.invoke_var != 0) ) { result = task->Callback.invoke_var(task->Callback.env, 0, 0); }\n"
        "    if ( result != 0 ) { xrtValueRelease(result); }\n"
        "    xl6_net_task_release(task);\n"
        "}\n\n"
        "static void xl6_net_timer_run(xnetworker* worker, uint64_t id, xnetresult resultCode, void* arg)\n"
        "{\n"
        "    if ( resultCode == XL6_XNET_OK ) { xl6_net_task_run(worker, arg); }\n"
        "    else { xl6_net_task_release((xl6_net_task*)arg); }\n"
        "    (void)id;\n"
        "}\n\n",
        out
    );

    xl6OutputPuts(
        "void* xl6_xrt_net_engine_create(long long workerCount)\n"
        "{\n"
        "    xnetengineconfig config;\n"
        "    if ( (workerCount < 0) || (workerCount > UINT32_MAX) ) { return 0; }\n"
        "    xrtNetEngineConfigInit(&config);\n"
        "    config.Workers = (uint32_t)workerCount;\n"
        "    return xrtNetEngineCreate(&config);\n"
        "}\n"
        "long long xl6_xrt_net_engine_destroy(void* value) { if ( value != 0 ) { (void)xrtNetEngineStop((xnetengine*)value); (void)xrtNetEngineDestroy((xnetengine*)value); } return 0; }\n"
        "void* xl6_xrt_net_engine_native(void* value) { return value; }\n"
        "int xl6_xrt_net_engine_start(void* value) { return (value != 0) && xrtNetEngineStart((xnetengine*)value); }\n"
        "void xl6_xrt_net_engine_stop(void* value) { if ( value != 0 ) { (void)xrtNetEngineStop((xnetengine*)value); } }\n"
        "long long xl6_xrt_net_engine_workers(void* value) { return value != 0 ? xrtNetEngineWorkerCount((xnetengine*)value) : 0; }\n"
        "bool xl6_xrt_net_engine_post(void* value, XL6_Function callback, int64_t affinity)\n"
        "{\n"
        "    xl6_net_task* task;\n"
        "    if ( (value == 0) || (affinity < 0) ) { return false; }\n"
        "    task = xl6_net_task_create(callback);\n"
        "    if ( task == 0 ) { return false; }\n"
        "    if ( !xrtNetEnginePost((xnetengine*)value, (uint64_t)affinity, xl6_net_task_run, task) ) { xl6_net_task_release(task); return false; }\n"
        "    return true;\n"
        "}\n"
        "bool xl6_xrt_net_engine_post_delayed(void* value, XL6_Function callback, int64_t delayMs, int64_t affinity)\n"
        "{\n"
        "    xl6_net_task* task;\n"
        "    if ( (value == 0) || (delayMs < 0) || (affinity < 0) ) { return false; }\n"
        "    task = xl6_net_task_create(callback);\n"
        "    if ( task == 0 ) { return false; }\n"
        "    if ( xrtNetEngineAfter((xnetengine*)value, (uint64_t)affinity, xl6_net_timeout_us(delayMs), xl6_net_timer_run, task) == 0u ) { xl6_net_task_release(task); return false; }\n"
        "    return true;\n"
        "}\n\n",
        out
    );
    }
}



/* 输出 TCP/UDP 配置对象。 */
static void xl6CgenEmitNetConfigs(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->tcpConnectConfig ) {
        xl6OutputPuts("typedef struct xl6_tcp_connect_config { xnetdialconfig Config; char* Host; uint16_t Port; xnetproxy* Proxy; } xl6_tcp_connect_config;\n\n", out);
    xl6OutputPuts(
        "void* xl6_xrt_tcp_connect_config_create(const char* host, long long port)\n"
        "{\n"
        "    xl6_tcp_connect_config* value;\n"
        "    if ( (host == 0) || (host[0] == '\\0') || (port <= 0) || (port > 65535) ) { return 0; }\n"
        "    value = (xl6_tcp_connect_config*)xrtCalloc(1u, sizeof(*value));\n"
        "    if ( value == 0 ) { return 0; }\n"
        "    xrtNetDialConfigInit(&value->Config);\n"
        "    value->Host = xl6_xrt_string_dup(host, strlen(host));\n"
        "    value->Port = (uint16_t)port;\n"
        "    if ( value->Host == 0 ) { xrtFree(value); return 0; }\n"
        "    return value;\n"
        "}\n"
        "long long xl6_xrt_tcp_connect_config_destroy(void* raw) { xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw; if ( value != 0 ) { xrtNetProxyRelease(value->Proxy); xrtFree(value->Host); xrtFree(value); } return 0; }\n"
        "int xl6_xrt_tcp_connect_config_timeout(void* raw, long long ms) { xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw; if ( (value == 0) || (ms < 0) ) { return 0; } value->Config.Timeout = xl6_net_timeout_us(ms); value->Config.Stream.ConnectTimeout = value->Config.Timeout; return 1; }\n"
        "int xl6_xrt_tcp_connect_config_water(void* raw, long long high, long long low) { xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw; if ( (value == 0) || (high < 0) || (low < 0) || (low > high) ) { return 0; } value->Config.Stream.WriteHighWater = (size_t)high; value->Config.Stream.WriteLowWater = (size_t)low; return 1; }\n"
        "int xl6_xrt_tcp_connect_config_recv_limit(void* raw, long long size) { xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw; if ( (value == 0) || (size < 0) ) { return 0; } value->Config.Stream.ReadLimit = (size_t)size; return 1; }\n"
        "int xl6_xrt_tcp_connect_config_proxy(void* raw, void* proxy) { xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw; xnetproxy* next = (xnetproxy*)proxy; if ( value == 0 ) { return 0; } next = next != 0 ? xrtNetProxyRetain(next) : 0; xrtNetProxyRelease(value->Proxy); value->Proxy = next; return 1; }\n\n",
        out
    );

    xl6OutputPuts(
        "static void xl6_net_proxy_stream_free(void* value, void* data) { (void)data; xrtNetStreamDestroy((xnetstream*)value); }\n"
        "static void xl6_net_proxy_done(xnetproxydial* dial, xnetresult resultCode, xnetstream* stream, const xerror* error, void* data)\n"
        "{\n"
        "    xpromise* promise = (xpromise*)data;\n"
        "    bool done = false;\n"
        "    (void)dial;\n"
        "    if ( (resultCode == XL6_XNET_OK) && (stream != 0) ) { done = xrtPromiseResolveOwned(promise, stream, xl6_net_proxy_stream_free, 0); if ( !done ) { xrtNetStreamDestroy(stream); } }\n"
        "    else if ( resultCode == 5 ) { done = xrtPromiseCancel(promise); }\n"
        "    else if ( resultCode == XL6_XNET_CLOSED ) { done = xrtPromiseClose(promise); }\n"
        "    else if ( error != 0 ) { done = xrtPromiseReject(promise, error); }\n"
        "    if ( !done ) { xerror* fallback = xrtErrorCreate(17, \"xlang.net\", 1, \"proxy connection failed\"); if ( fallback != 0 ) { (void)xrtPromiseReject(promise, fallback); xrtErrorFree(fallback); } }\n"
        "    xrtPromiseDestroy(promise);\n"
        "}\n\n"
        "void* xl6_xrt_tcp_connect_config_connect(void* raw)\n"
        "{\n"
        "    xl6_tcp_connect_config* value = (xl6_tcp_connect_config*)raw;\n"
        "    xdeadline deadline;\n"
        "    if ( (value == 0) || (xl6_net_engine == 0) || (xl6_net_resolver == 0) ) { return 0; }\n"
        "    deadline = value->Config.Timeout == 0u ? XL6_DEADLINE_NEVER : xrtDeadlineAfter(value->Config.Timeout);\n"
        "    value->Config.Affinity = xl6_net_next_dial_worker();\n"
        "    if ( value->Proxy == 0 ) { return xrtNetConnect(xl6_net_engine, xl6_net_resolver, value->Host, value->Port, &value->Config, 0, 0, deadline, 0); }\n"
        "    {\n"
        "        xnetproxydialconfig config; xfuture* future = 0; xpromise* promise; xnetproxydial* dial; xnetstream* stream = 0;\n"
        "        xrtNetProxyDialConfigInit(&config); config.Transport = value->Config; config.Timeout = value->Config.Timeout;\n"
        "        promise = xrtPromiseCreate(&future, 0); if ( promise == 0 ) { return 0; }\n"
        "        dial = xrtNetProxyDial(xl6_net_engine, xl6_net_resolver, value->Proxy, value->Host, value->Port, &config, 0, 0, xl6_net_proxy_done, promise);\n"
        "        if ( dial == 0 ) { xrtPromiseDestroy(promise); xl6FutureDestroy(future); return 0; }\n"
        "        if ( xrtFutureWait(future) == 0 ) { stream = xrtNetStreamRef((xnetstream*)xrtFutureValue(future)); }\n"
        "        xl6FutureDestroy(future); xrtNetProxyDialDestroy(dial); return stream;\n"
        "    }\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpListenConfig ) {
    xl6OutputPuts("typedef struct xl6_tcp_listen_config { xnetlistenconfig Config; } xl6_tcp_listen_config;\n\n", out);
    xl6OutputPuts(
        "void* xl6_xrt_tcp_listen_config_create(const char* host, long long port)\n"
        "{\n"
        "    xl6_tcp_listen_config* value;\n"
        "    if ( (port < 0) || (port > 65535) ) { return 0; }\n"
        "    value = (xl6_tcp_listen_config*)xrtCalloc(1u, sizeof(*value));\n"
        "    if ( value == 0 ) { return 0; }\n"
        "    xrtNetListenConfigInit(&value->Config);\n"
        "    if ( !xl6_net_address_parse(&value->Config.Address, host, (uint16_t)port, true) ) { xrtFree(value); return 0; }\n"
        "    return value;\n"
        "}\n"
        "long long xl6_xrt_tcp_listen_config_destroy(void* raw) { xrtFree(raw); return 0; }\n"
        "int xl6_xrt_tcp_listen_config_backlog(void* raw, long long value) { xl6_tcp_listen_config* config = (xl6_tcp_listen_config*)raw; if ( (config == 0) || (value < 0) || (value > INT_MAX) ) { return 0; } config->Config.Backlog = (int)value; return 1; }\n"
        "int xl6_xrt_tcp_listen_config_water(void* raw, long long high, long long low) { xl6_tcp_listen_config* value = (xl6_tcp_listen_config*)raw; if ( (value == 0) || (high < 0) || (low < 0) || (low > high) ) { return 0; } value->Config.Stream.WriteHighWater = (size_t)high; value->Config.Stream.WriteLowWater = (size_t)low; return 1; }\n"
        "int xl6_xrt_tcp_listen_config_recv_limit(void* raw, long long size) { xl6_tcp_listen_config* value = (xl6_tcp_listen_config*)raw; if ( (value == 0) || (size < 0) ) { return 0; } value->Config.Stream.ReadLimit = (size_t)size; return 1; }\n"
        "void* xl6_xrt_tcp_listen_config_listen(void* raw) { xl6_tcp_listen_config* value = (xl6_tcp_listen_config*)raw; return (value != 0) && (xl6_net_engine != 0) ? xrtNetListen(xl6_net_engine, &value->Config, 0, 0, 0) : 0; }\n\n",
        out
    );
    }

    if ( needs->udp ) {
    xl6OutputPuts("typedef struct xl6_udp_config { xnetaddr Address; xnetudpconfig Config; } xl6_udp_config;\n\n", out);
    xl6OutputPuts(
        "void* xl6_xrt_udp_config_create(const char* host, long long port)\n"
        "{\n"
        "    xl6_udp_config* value;\n"
        "    if ( (port < 0) || (port > 65535) ) { return 0; }\n"
        "    value = (xl6_udp_config*)xrtCalloc(1u, sizeof(*value));\n"
        "    if ( value == 0 ) { return 0; }\n"
        "    xrtNetUdpConfigInit(&value->Config);\n"
        "    if ( !xl6_net_address_parse(&value->Address, host, (uint16_t)port, true) ) { xrtFree(value); return 0; }\n"
        "    return value;\n"
        "}\n"
        "long long xl6_xrt_udp_config_destroy(void* raw) { xrtFree(raw); return 0; }\n"
        "int xl6_xrt_udp_config_recv_batch(void* raw, long long value) { xl6_udp_config* config = (xl6_udp_config*)raw; if ( (config == 0) || (value <= 0) || (value > UINT32_MAX) ) { return 0; } config->Config.ReceiveBatch = (uint32_t)value; return 1; }\n"
        "int xl6_xrt_udp_config_queues(void* raw, long long sendLimit, long long recvLimit) { xl6_udp_config* value = (xl6_udp_config*)raw; if ( (value == 0) || (sendLimit < 0) || (recvLimit < 0) ) { return 0; } value->Config.SendPacketLimit = (size_t)sendLimit; value->Config.ReceiveQueueLimit = (size_t)recvLimit; return 1; }\n"
        "void* xl6_xrt_udp_config_bind(void* raw) { xl6_udp_config* value = (xl6_udp_config*)raw; return (value != 0) && (xl6_net_engine != 0) ? xrtNetUdpBind(xl6_net_engine, &value->Address, 0u, &value->Config, 0, 0) : 0; }\n\n",
        out
    );
    }
}



#endif

#ifdef XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION
/* 输出 TCP 便捷层和 Stream/Listener 绑定。 */
void xl6CgenEmitNetTcpComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->tcpDial ) {
    xl6OutputPuts(
        "void* xl6_xrt_tcp_connect(const char* host, uint16_t port, int64_t timeoutMs)\n"
        "{\n"
        "    xnetdialconfig config;\n"
        "    if ( (xl6_net_engine == 0) || (xl6_net_resolver == 0) || (host == 0) || (host[0] == '\\0') || (port == 0u) ) { return 0; }\n"
        "    xrtNetDialConfigInit(&config);\n"
        "    config.Timeout = xl6_net_timeout_us(timeoutMs);\n"
        "    config.Stream.ConnectTimeout = config.Timeout;\n"
        "    config.Affinity = xl6_net_next_dial_worker();\n"
        "    return xrtNetConnect(xl6_net_engine, xl6_net_resolver, host, port, &config, 0, 0, xl6_net_deadline(timeoutMs), 0);\n"
        "}\n\n"
        "xfuture* xl6_xrt_tcp_connect_async(const char* host, uint16_t port, int64_t timeoutMs)\n"
        "{\n"
        "    xnetdialconfig config; xfuture* source;\n"
        "    if ( (xl6_net_engine == 0) || (xl6_net_resolver == 0) || (host == 0) || (host[0] == '\\0') || (port == 0u) ) { return 0; }\n"
        "    xrtNetDialConfigInit(&config); config.Timeout = xl6_net_timeout_us(timeoutMs); config.Stream.ConnectTimeout = config.Timeout; config.Affinity = xl6_net_next_dial_worker();\n"
        "    source = xrtNetDialAsync(xl6_net_engine, xl6_net_resolver, host, port, &config, 0, 0);\n"
        "    return xl6_net_future_map(source, xl6_net_map_stream);\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpListen ) {
    xl6OutputPuts(
        "void* xl6_xrt_tcp_listen(const char* host, uint16_t port, uint32_t backlog)\n"
        "{\n"
        "    xnetlistenconfig config;\n"
        "    if ( xl6_net_engine == 0 ) { return 0; }\n"
        "    xrtNetListenConfigInit(&config);\n"
        "    if ( !xl6_net_address_parse(&config.Address, host, port, true) ) { return 0; }\n"
        "    if ( backlog != 0u ) { config.Backlog = backlog > INT_MAX ? INT_MAX : (int)backlog; }\n"
        "    return xrtNetListen(xl6_net_engine, &config, 0, 0, 0);\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpStream ) {
    xl6OutputPuts(
		"void* xl6_xrt_tcp_stream_native(void* raw) { return raw; }\n"
        "void xl6_xrt_tcp_stream_destroy(void* raw) { xnetstream* stream = (xnetstream*)raw; if ( stream != 0 ) { (void)xrtNetStreamClose(stream); xrtNetStreamDestroy(stream); } }\n"
        "void xl6_xrt_tcp_stream_close(void* raw) { if ( raw != 0 ) { (void)xrtNetStreamClose((xnetstream*)raw); } }\n"
        "int xl6_xrt_tcp_stream_shutdown_write(void* raw) { return (raw != 0) && xrtNetStreamShutdownWrite((xnetstream*)raw); }\n"
        "int xl6_xrt_tcp_stream_abort(void* raw) { return (raw != 0) && xrtNetStreamAbort((xnetstream*)raw); }\n"
        "long long xl6_xrt_tcp_stream_send_text_result(void* raw, const char* text) { return (raw != 0) && (text != 0) ? xrtNetStreamSend((xnetstream*)raw, text, strlen(text)) : XNET_RESULT_ERROR; }\n"
        "long long xl6_xrt_tcp_stream_send_bytes_result(void* raw, void* data, size_t size) { return (raw != 0) && ((data != 0) || (size == 0u)) ? xrtNetStreamSend((xnetstream*)raw, data, size) : XNET_RESULT_ERROR; }\n"
        "int xl6_xrt_tcp_stream_send_text(void* raw, const char* text) { return (raw != 0) && (text != 0) && (xrtNetStreamSend((xnetstream*)raw, text, strlen(text)) == XL6_XNET_OK); }\n"
		"int xl6_xrt_tcp_stream_send_bytes(void* raw, void* data, size_t size) { return (raw != 0) && ((data != 0) || (size == 0u)) && (xrtNetStreamSend((xnetstream*)raw, data, size) == XL6_XNET_OK); }\n"
        "size_t xl6_xrt_tcp_stream_available(void* raw) { return raw != 0 ? xrtNetStreamAvailable((xnetstream*)raw) : 0u; }\n"
        "long long xl6_xrt_net_stream_pending_send(void* raw) { return raw != 0 ? (long long)xrtNetStreamPending((xnetstream*)raw) : 0; }\n"
        "long long xl6_xrt_net_stream_write_limit(void* raw) { return raw != 0 ? (long long)xrtNetStreamWriteLimit((xnetstream*)raw) : 0; }\n"
        "long long xl6_xrt_net_stream_writable(void* raw) { return raw != 0 ? (long long)xrtNetStreamWritable((xnetstream*)raw) : 0; }\n"
        "void xl6_xrt_net_stream_pause(void* raw) { if ( raw != 0 ) { xrtNetStreamPause((xnetstream*)raw); } }\n"
        "void xl6_xrt_net_stream_resume(void* raw) { if ( raw != 0 ) { (void)xrtNetStreamResume((xnetstream*)raw); } }\n\n",
        out
    );

    if ( needs->tcpFile ) {
    xl6OutputPuts(
        "/* 校验完整文件范围；size 为 -1 时取 offset 到文件尾。 */\n"
        "static bool xl6_net_stream_file_range(xfile file, long long offset, long long size, uint64_t* amount)\n"
        "{\n"
        "    uint64_t fileSize; uint64_t range;\n"
        "    if ( (file == 0) || (amount == 0) || (offset < 0) || (size < -1) ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"invalid stream file range\"); return false; }\n"
        "    if ( !xrtFileSize(file, &fileSize) ) { return false; }\n"
        "    if ( (uint64_t)offset > fileSize ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file offset exceeds file size\"); return false; }\n"
        "    range = size < 0 ? fileSize - (uint64_t)offset : (uint64_t)size;\n"
        "    if ( range > (fileSize - (uint64_t)offset) ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file range exceeds file size\"); return false; }\n"
        "    *amount = range;\n"
        "    return true;\n"
        "}\n\n"
        "/* 单次提交保留 XRT 的 OK、AGAIN、CLOSED 和 ERROR 结果。 */\n"
        "static xnetresult xl6_net_stream_send_file_range(void* raw, xfile file, long long offset, long long size)\n"
        "{\n"
        "    uint64_t amount;\n"
        "    if ( (raw == 0) || !xl6_net_stream_file_range(file, offset, size, &amount) ) { return XNET_RESULT_ERROR; }\n"
        "    if ( amount > (uint64_t)SIZE_MAX ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file range exceeds platform size limit\"); return XNET_RESULT_ERROR; }\n"
        "    return xrtNetStreamSendFile((xnetstream*)raw, file, (uint64_t)offset, (size_t)amount);\n"
        "}\n\n"
        "/* 完整发送按当前可写预算分段，不要求文件区间小于 WriteLimit。 */\n"
        "static int xl6_net_stream_send_file_all(void* raw, xfile file, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    xnetstream* stream = (xnetstream*)raw; uint64_t amount; uint64_t current; xdeadline deadline;\n"
        "    if ( (stream == 0) || !xl6_net_stream_file_range(file, offset, size, &amount) ) { return 0; }\n"
        "    current = (uint64_t)offset; deadline = xl6_net_deadline(timeoutMs);\n"
        "    while ( amount != 0u ) {\n"
        "        size_t writable = xrtNetStreamWritable(stream); size_t chunk; xnetresult resultCode;\n"
        "        if ( writable == 0u ) { if ( !xrtNetStreamWait(stream, XL6_XNET_WAIT_WRITE, deadline, 0) ) { return 0; } continue; }\n"
        "        chunk = amount < (uint64_t)writable ? (size_t)amount : writable;\n"
        "        resultCode = xrtNetStreamSendFile(stream, file, current, chunk);\n"
        "        if ( resultCode == XNET_RESULT_OK ) { current += (uint64_t)chunk; amount -= (uint64_t)chunk; continue; }\n"
        "        if ( resultCode != XNET_RESULT_AGAIN ) { return 0; }\n"
        "        if ( !xrtNetStreamWait(stream, XL6_XNET_WAIT_WRITE, deadline, 0) ) { return 0; }\n"
        "    }\n"
        "    return 1;\n"
        "}\n\n"
        "long long xl6_xrt_tcp_stream_send_file_handle_result(void* raw, void* fileRaw, long long offset, long long size)\n"
        "{\n"
        "    return (long long)xl6_net_stream_send_file_range(raw, (xfile)fileRaw, offset, size);\n"
        "}\n\n"
        "long long xl6_xrt_tcp_stream_send_file_result(void* raw, const char* path, long long offset, long long size)\n"
        "{\n"
        "    xfile file; xnetresult resultCode;\n"
        "    if ( (path == 0) || (path[0] == '\\0') ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"file path is empty\"); return XNET_RESULT_ERROR; }\n"
        "    file = xrtOpen(path, XFILE_READ);\n"
        "    if ( file == 0 ) { return XNET_RESULT_ERROR; }\n"
        "    resultCode = xl6_net_stream_send_file_range(raw, file, offset, size);\n"
        "    (void)xrtClose(file);\n"
        "    return (long long)resultCode;\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_send_file_handle(void* raw, void* fileRaw, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    return xl6_net_stream_send_file_all(raw, (xfile)fileRaw, offset, size, timeoutMs);\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_send_file(void* raw, const char* path, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    xfile file; int sent;\n"
        "    if ( (path == 0) || (path[0] == '\\0') ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"file path is empty\"); return 0; }\n"
        "    file = xrtOpen(path, XFILE_READ);\n"
        "    if ( file == 0 ) { return 0; }\n"
        "    sent = xl6_net_stream_send_file_all(raw, file, offset, size, timeoutMs);\n"
        "    (void)xrtClose(file);\n"
        "    return sent;\n"
        "}\n\n",
        out
    );
    }

    xl6OutputPuts(
        "static char* xl6_net_stream_recv_text(xnetstream* stream, size_t maxBytes, xdeadline deadline)\n"
        "{\n"
        "    xnetbytes* bytes; XL6_XrtBytesView view; char* text;\n"
        "    if ( stream == 0 ) { return 0; }\n"
        "    bytes = xrtNetStreamRecv(stream, maxBytes, deadline, 0);\n"
        "    if ( bytes == 0 ) { return 0; }\n"
        "    view = xrtNetBytesView(bytes); text = (char*)xrtMalloc(view.Size + 1u);\n"
        "    if ( text != 0 ) { if ( view.Size != 0u ) { memcpy(text, view.Data, view.Size); } text[view.Size] = '\\0'; }\n"
        "    xrtNetBytesDestroy(bytes); return text;\n"
        "}\n"
        "void* xl6_xrt_tcp_stream_read_text(void* raw, size_t maxBytes) { return xl6_net_stream_recv_text((xnetstream*)raw, maxBytes, xrtDeadlineAfter(0u)); }\n"
        "void* xl6_xrt_tcp_stream_recv_text(void* raw, size_t maxBytes, long long timeoutMs) { return xl6_net_stream_recv_text((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs)); }\n"
        "xfuture* xl6_xrt_tcp_stream_recv_text_async(void* raw, size_t maxBytes) { return raw != 0 ? xl6_net_future_map(xrtNetStreamRecvAsync((xnetstream*)raw, maxBytes), xl6_net_map_text) : 0; }\n"
        "void* xl6_xrt_tcp_stream_recv_owned(void* raw, size_t maxBytes, long long timeoutMs) { return raw != 0 ? xrtNetStreamRecv((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
        "xfuture* xl6_xrt_tcp_stream_recv_owned_async(void* raw, size_t maxBytes) { return raw != 0 ? xl6_net_future_map(xrtNetStreamRecvAsync((xnetstream*)raw, maxBytes), xl6_net_map_bytes) : 0; }\n"
        "int xl6_xrt_tcp_stream_wait_available(void* raw, long long minimum, long long timeoutMs) { return (raw != 0) && (minimum >= 0) && ((uint64_t)minimum <= (uint64_t)SIZE_MAX) && xrtNetStreamWaitAvailable((xnetstream*)raw, (size_t)minimum, xl6_net_deadline(timeoutMs), 0); }\n"
        "xfuture* xl6_xrt_tcp_stream_wait_available_async(void* raw, long long minimum) { return (raw != 0) && (minimum >= 0) && ((uint64_t)minimum <= (uint64_t)SIZE_MAX) ? xl6_net_future_map(xrtNetStreamWaitAvailableAsync((xnetstream*)raw, (size_t)minimum), xl6_net_map_bool) : 0; }\n"
        "int xl6_xrt_tcp_stream_drain(void* raw, long long timeoutMs) { return (raw != 0) && xrtNetStreamWait((xnetstream*)raw, XL6_XNET_WAIT_DRAIN, xl6_net_deadline(timeoutMs), 0); }\n"
        "xfuture* xl6_xrt_tcp_stream_drain_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetStreamWaitAsync((xnetstream*)raw, XL6_XNET_WAIT_DRAIN), xl6_net_map_bool) : 0; }\n"
        "xfuture* xl6_xrt_tcp_stream_writable_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetStreamWaitAsync((xnetstream*)raw, XL6_XNET_WAIT_WRITE), xl6_net_map_bool) : 0; }\n\n",
        out
    );

    xl6OutputPuts(
        "static char* xl6_net_stream_address(void* raw, bool remote) { xnetaddr address; bool ok = raw != 0 && (remote ? xrtNetStreamRemote((xnetstream*)raw, &address) : xrtNetStreamLocal((xnetstream*)raw, &address)); return ok ? xl6_net_address_string(&address) : xl6_xrt_string_dup(\"\", 0u); }\n"
        "void* xl6_xrt_tcp_stream_local_addr(void* raw) { return xl6_net_stream_address(raw, false); }\n"
        "void* xl6_xrt_tcp_stream_remote_addr(void* raw) { return xl6_net_stream_address(raw, true); }\n"
        "long long xl6_xrt_tcp_stream_local_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetStreamLocal((xnetstream*)raw, &address) ? address.Port : 0; }\n"
        "long long xl6_xrt_tcp_stream_remote_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetStreamRemote((xnetstream*)raw, &address) ? address.Port : 0; }\n"
        "int xl6_xrt_net_stream_wait(void* raw, long long kind, long long timeoutMs) { static const int waits[4] = { XL6_XNET_WAIT_READ, XL6_XNET_WAIT_WRITE, XL6_XNET_WAIT_DRAIN, XL6_XNET_WAIT_CLOSE }; return (raw != 0) && (kind >= 0) && (kind < 4) && xrtNetStreamWait((xnetstream*)raw, waits[kind], xl6_net_deadline(timeoutMs), 0); }\n"
        , out);
    if (needs->tcpVectors) xl6OutputPuts(
        "int xl6_xrt_net_stream_send_vec(void* raw, xvalue* buffers)\n"
        "{\n"
        "    xnetspan local[16]; size_t count = buffers != 0 ? xrtValueCount(buffers) : 0u; size_t i; xnetspan* spans; xnetresult resultCode;\n"
        "    if ( (raw == 0) || (count == 0u) ) { return 0; }\n"
        "    spans = count <= 16u ? local : (xnetspan*)xrtCalloc(count, sizeof(*spans)); if ( spans == 0 ) { return 0; }\n"
        "    for ( i = 0u; i < count; ++i ) { xbuffer* buffer = (xbuffer*)xl6ValueHandleData(xrtValueArrayGet(buffers, i)); if ( buffer == 0 ) { if ( spans != local ) { xrtFree(spans); } return 0; } spans[i].Data = buffer->Data; spans[i].Size = buffer->Size; }\n"
        "    resultCode = xrtNetStreamSendVec((xnetstream*)raw, spans, count); if ( spans != local ) { xrtFree(spans); } return resultCode == XL6_XNET_OK;\n"
        "}\n", out);
    xl6OutputPuts(
        "int xl6_xrt_net_stream_send_move(void* raw, void* bufferRaw) { xbuffer* buffer = (xbuffer*)bufferRaw; xnetresult resultCode; if ( (raw == 0) || (buffer == 0) ) { return 0; } if ( buffer->Size == 0u ) { return 1; } resultCode = xrtNetStreamSendTake((xnetstream*)raw, buffer->Data, buffer->Size); if ( resultCode == XL6_XNET_OK ) { buffer->Data = 0; buffer->Size = 0u; buffer->Capacity = 0u; } return resultCode == XL6_XNET_OK; }\n\n",
        out
    );
    }

    if ( needs->tcpServer ) {
    xl6OutputPuts(
        "void xl6_xrt_tcp_listener_destroy(void* raw) { xnetlistener* listener = (xnetlistener*)raw; if ( listener != 0 ) { (void)xrtNetListenerClose(listener); xrtNetListenerDestroy(listener); } }\n"
		"uint16_t xl6_xrt_tcp_listener_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetListenerLocal((xnetlistener*)raw, &address) ? address.Port : 0; }\n"
		"int xl6_xrt_tcp_listener_close(void* raw) { return (raw != 0) && xrtNetListenerClose((xnetlistener*)raw); }\n"
		"long long xl6_xrt_tcp_listener_state(void* raw) { return raw != 0 ? (long long)xrtNetListenerState((xnetlistener*)raw) : (long long)XNET_LISTENER_CLOSED; }\n"
        "void* xl6_xrt_tcp_accept(void* raw, long long timeoutMs) { return raw != 0 ? xrtNetListenerAcceptWait((xnetlistener*)raw, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
        "xfuture* xl6_xrt_tcp_accept_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetListenerAcceptAsync((xnetlistener*)raw), xl6_net_map_stream) : 0; }\n\n",
        out
    );
    }
}

/* Statistics are transport mechanics shared by both C backends.  The
 * language-facing NetStats class shape remains owned by the verified IR. */
void xl6CgenEmitNetTcpStatsComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( out == NULL || needs == NULL || !needs->stats ) { return; }
    xl6OutputPuts(
        "#ifndef XL6_NET_STATS_RUNTIME\n"
        "#define XL6_NET_STATS_RUNTIME\n"
        "typedef struct xl6_net_stats_snapshot { size_t Count; uint64_t Values[34]; } xl6_net_stats_snapshot;\n"
        "static xl6_net_stats_snapshot* xl6_net_stats_create(size_t count)\n"
        "{ xl6_net_stats_snapshot* value = (xl6_net_stats_snapshot*)xrtCalloc(1u, sizeof(*value)); if (value != NULL) value->Count = count; return value; }\n"
        "void xl6_xrt_net_stats_destroy(void* raw) { xrtFree(raw); }\n"
        "uint64_t xl6_xrt_net_stats_value(void* raw, int64_t index)\n"
        "{ xl6_net_stats_snapshot* value = (xl6_net_stats_snapshot*)raw; return value != NULL && index >= 0 && (size_t)index < value->Count ? value->Values[index] : UINT64_C(0); }\n"
        "#endif\n\n",
        out);
    if ( needs->tcpStream )
        xl6OutputPuts(
            "void* xl6_xrt_tcp_stream_stats(void* raw)\n"
            "{\n"
            "    xnetstreamstats value; xl6_net_stats_snapshot* snapshot;\n"
            "    if (raw == NULL || !xrtNetStreamStats((xnetstream*)raw, &value)) return NULL;\n"
            "    snapshot = xl6_net_stats_create(14u); if (snapshot == NULL) return NULL;\n"
            "    snapshot->Values[0] = (uint64_t)value.State; snapshot->Values[1] = (uint64_t)value.ReceivedBytes; snapshot->Values[2] = (uint64_t)value.SentBytes;\n"
            "    snapshot->Values[3] = (uint64_t)value.ReadEvents; snapshot->Values[4] = (uint64_t)value.WriteEvents; snapshot->Values[5] = (uint64_t)value.SendRejected;\n"
            "    snapshot->Values[6] = (uint64_t)value.BufferedBytes; snapshot->Values[7] = (uint64_t)value.QueuedBytes; snapshot->Values[8] = (uint64_t)value.PeakQueuedBytes;\n"
            "    snapshot->Values[9] = (uint64_t)value.ReadPaused; snapshot->Values[10] = (uint64_t)value.ReadBlocked; snapshot->Values[11] = (uint64_t)value.ReadEnded;\n"
            "    snapshot->Values[12] = (uint64_t)value.WriteEnded; snapshot->Values[13] = (uint64_t)value.WriteBackpressured; return snapshot;\n"
            "}\n\n",
            out);
    if ( needs->tcpServer )
        xl6OutputPuts(
            "void* xl6_xrt_tcp_listener_stats(void* raw)\n"
            "{\n"
            "    xnetlistenerstats value; xl6_net_stats_snapshot* snapshot;\n"
            "    if (raw == NULL || !xrtNetListenerStats((xnetlistener*)raw, &value)) return NULL;\n"
            "    snapshot = xl6_net_stats_create(9u); if (snapshot == NULL) return NULL;\n"
            "    snapshot->Values[0] = (uint64_t)value.State; snapshot->Values[1] = (uint64_t)value.Accepted; snapshot->Values[2] = (uint64_t)value.Rejected;\n"
            "    snapshot->Values[3] = (uint64_t)value.Errors; snapshot->Values[4] = (uint64_t)value.ActiveAccepts; snapshot->Values[5] = (uint64_t)value.ActiveDispatches;\n"
            "    snapshot->Values[6] = (uint64_t)value.QueuedAccepts; snapshot->Values[7] = (uint64_t)value.PeakQueuedAccepts; snapshot->Values[8] = (uint64_t)value.AcceptWaiters; return snapshot;\n"
            "}\n\n",
            out);
}



#endif

#if !defined(XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION) || \
    defined(XL6_CGEN_NET_BUFFER_COMPONENT_IMPLEMENTATION)
/* 输出 Buffer 所需的当前 TCP 字节桥。 */
#ifdef XL6_CGEN_NET_BUFFER_COMPONENT_IMPLEMENTATION
void xl6CgenEmitNetBufferComponent(
#else
static void xl6CgenEmitNetBufferBridge(
#endif
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->bytes ) {
    xl6OutputPuts(
        "void xl6_xrt_net_bytes_destroy(void* raw) { xrtNetBytesDestroy((xnetbytes*)raw); }\n"
        "long long xl6_xrt_net_bytes_len(void* raw) { return raw != 0 ? (long long)xrtNetBytesView((xnetbytes*)raw).Size : 0; }\n"
        "void* xl6_xrt_net_bytes_data(void* raw) { return raw != 0 ? (void*)xrtNetBytesView((xnetbytes*)raw).Data : 0; }\n"
        "void* xl6_xrt_net_bytes_buffer(void* raw) { XL6_XrtBytesView view = raw != 0 ? xrtNetBytesView((xnetbytes*)raw) : (XL6_XrtBytesView){ 0, 0u }; return xl6_xrt_buffer_from_view_bytes(view.Data, view.Size); }\n"
        "void* xl6_xrt_net_bytes_text(void* raw) { XL6_XrtBytesView view = raw != 0 ? xrtNetBytesView((xnetbytes*)raw) : (XL6_XrtBytesView){ 0, 0u }; return xl6_xrt_string_dup((const char*)view.Data, view.Size); }\n\n",
        out
    );
    }

    if ( needs->tcpStream || needs->tlsStream ) {
    xl6OutputPuts(
        "typedef const xnetbuf* (*xl6_net_buffer_view_proc)(void* transport);\n"
        "typedef size_t (*xl6_net_buffer_consume_proc)(void* transport, size_t size);\n\n"
        "typedef enum xl6_net_buffer_command_kind {\n"
        "    XL6_NET_BUFFER_PEEK = 1,\n"
        "    XL6_NET_BUFFER_CONSUME\n"
        "} xl6_net_buffer_command_kind;\n\n"
        "typedef struct xl6_net_buffer_command {\n"
        "    xevent Done;\n"
        "    xnetpost Post;\n"
        "    void* Transport;\n"
        "    xl6_net_buffer_view_proc View;\n"
        "    xl6_net_buffer_consume_proc Consume;\n"
        "    xl6_net_buffer_command_kind Kind;\n"
        "    size_t Size;\n"
        "    void* Buffer;\n"
        "    bool Boolean;\n"
        "    bool Signal;\n"
        "} xl6_net_buffer_command;\n\n"
        "/* 在传输所属 Worker 内复制快照或精确消费明文前缀。 */\n"
        "static void xl6_net_buffer_command_run(xnetworker* worker, void* data)\n"
        "{\n"
        "    xl6_net_buffer_command* command = (xl6_net_buffer_command*)data;\n"
        "    const xnetbuf* buffer;\n"
        "    size_t available;\n"
        "    size_t size;\n"
        "    void* bytes;\n\n"
        "    (void)worker;\n"
        "    buffer = command->View != 0 ? command->View(command->Transport) : 0;\n"
        "    available = buffer != 0 ? xrtNetBufSize(buffer) : 0u;\n"
        "    if ( command->Kind == XL6_NET_BUFFER_PEEK ) {\n"
        "        size = (command->Size == 0u) || (command->Size > available) ? available : command->Size;\n"
        "        if ( size == 0u ) {\n"
        "            command->Buffer = xrtBufferCreate();\n"
        "        } else {\n"
        "            bytes = xrtMalloc(size);\n"
        "            if ( (bytes != 0) && (xrtNetBufPeek(buffer, 0u, bytes, size) == size) ) {\n"
        "                command->Buffer = xl6_xrt_buffer_from_owned_bytes(bytes, size);\n"
        "            } else {\n"
        "                xrtFree(bytes);\n"
        "            }\n"
        "        }\n"
        "    } else if ( command->Kind == XL6_NET_BUFFER_CONSUME ) {\n"
        "        command->Boolean = (command->Size == 0u) ||\n"
        "            ((command->Consume != 0) && (command->Size <= available) &&\n"
        "             (command->Consume(command->Transport, command->Size) == command->Size));\n"
        "    }\n"
        "    if ( command->Signal ) { (void)xrtEventSet(&command->Done); }\n"
        "}\n\n"
        "/* 跨线程同步编组只覆盖内存操作，不在 Worker 内等待网络事件。 */\n"
        "static bool xl6_net_buffer_command_execute(xnetworker* worker, xl6_net_buffer_command* command)\n"
        "{\n"
        "    bool posted;\n"
        "    bool waited;\n\n"
        "    if ( (worker == 0) || (command == 0) ) { return false; }\n"
        "    if ( xrtNetWorkerIsCurrent(worker) ) {\n"
        "        command->Signal = false;\n"
        "        xl6_net_buffer_command_run(worker, command);\n"
        "        return true;\n"
        "    }\n"
        "    if ( !xrtEventInit(&command->Done, false, false) ) { return false; }\n"
        "    if ( !xrtNetPostInit(&command->Post) ) {\n"
        "        (void)xrtEventUnit(&command->Done);\n"
        "        return false;\n"
        "    }\n"
        "    command->Signal = true;\n"
        "    posted = xrtNetPost(worker, &command->Post, xl6_net_buffer_command_run, command);\n"
        "    waited = posted && (xrtEventWait(&command->Done) == XWAIT_OK);\n"
        "    /* Done is caller-owned embedded storage, not an EventCreate allocation. */\n"
        "    (void)xrtEventUnit(&command->Done);\n"
        "    return waited;\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpStream ) {
    xl6OutputPuts(
        "static void* xl6_net_stream_recv_buffer(xnetstream* stream, size_t maxBytes, xdeadline deadline)\n"
        "{\n"
        "    xnetbytes* bytes; XL6_XrtBytesView view; void* buffer;\n"
        "    if ( stream == 0 ) { return 0; }\n"
        "    bytes = xrtNetStreamRecv(stream, maxBytes, deadline, 0); if ( bytes == 0 ) { return 0; }\n"
        "    view = xrtNetBytesView(bytes); buffer = xl6_xrt_buffer_from_view_bytes(view.Data, view.Size); xrtNetBytesDestroy(bytes); return buffer;\n"
        "}\n"
        "void* xl6_xrt_tcp_stream_read_buffer(void* raw, size_t maxBytes) { return xl6_net_stream_recv_buffer((xnetstream*)raw, maxBytes, xrtDeadlineAfter(0u)); }\n"
        "void* xl6_xrt_tcp_stream_recv_buffer(void* raw, size_t maxBytes, long long timeoutMs) { return xl6_net_stream_recv_buffer((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs)); }\n\n"
        "static const xnetbuf* xl6_net_tcp_buffer_view(void* transport) { return xrtNetStreamBuffer((xnetstream*)transport); }\n"
        "static size_t xl6_net_tcp_buffer_consume(void* transport, size_t size) { return xrtNetStreamConsume((xnetstream*)transport, size); }\n\n"
        "void* xl6_xrt_tcp_stream_peek_buffer(void* raw, long long maxBytes)\n"
        "{\n"
        "    xnetstream* stream = raw != 0 && maxBytes >= 0 ? xrtNetStreamRef((xnetstream*)raw) : 0;\n"
        "    xnetworker* worker = stream != 0 ? xrtNetStreamWorker(stream) : 0;\n"
        "    xl6_net_buffer_command command = { 0 };\n\n"
        "    if ( (stream == 0) || ((uint64_t)maxBytes > (uint64_t)SIZE_MAX) ) { xrtNetStreamDestroy(stream); return 0; }\n"
        "    command.Transport = stream; command.View = xl6_net_tcp_buffer_view;\n"
        "    command.Kind = XL6_NET_BUFFER_PEEK; command.Size = (size_t)maxBytes;\n"
        "    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Buffer = 0; }\n"
        "    xrtNetStreamDestroy(stream);\n"
        "    return command.Buffer;\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_consume_exact(void* raw, long long size)\n"
        "{\n"
        "    xnetstream* stream = raw != 0 && size >= 0 ? xrtNetStreamRef((xnetstream*)raw) : 0;\n"
        "    xnetworker* worker = stream != 0 ? xrtNetStreamWorker(stream) : 0;\n"
        "    xl6_net_buffer_command command = { 0 };\n\n"
        "    if ( (stream == 0) || ((uint64_t)size > (uint64_t)SIZE_MAX) ) { xrtNetStreamDestroy(stream); return 0; }\n"
        "    command.Transport = stream; command.View = xl6_net_tcp_buffer_view;\n"
        "    command.Consume = xl6_net_tcp_buffer_consume;\n"
        "    command.Kind = XL6_NET_BUFFER_CONSUME; command.Size = (size_t)size;\n"
        "    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Boolean = false; }\n"
        "    xrtNetStreamDestroy(stream);\n"
        "    return command.Boolean;\n"
        "}\n\n",
        out
    );
    }

    if ( needs->udp ) {
        xl6OutputPuts(
            "void* xl6_xrt_udp_packet_buffer(void* raw) { xnetudppacket* packet = (xnetudppacket*)raw; return packet != 0 ? xl6_xrt_buffer_from_view_bytes(xrtNetUdpPacketData(packet), xrtNetUdpPacketSize(packet)) : 0; }\n\n",
            out
        );
    }
}



#endif

#ifndef XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION
/* 输出 UDP Socket 和 Packet 语言绑定。 */
static void xl6CgenEmitNetUdp(XL6_Output* out, int needsUdpBatch)
{
    xl6OutputPuts(
        "void* xl6_xrt_udp_bind(const char* host, uint16_t port)\n"
        "{\n"
        "    xnetaddr address; xnetudpconfig config;\n"
        "    if ( (xl6_net_engine == 0) || !xl6_net_address_parse(&address, host, port, true) ) { return 0; }\n"
        "    xrtNetUdpConfigInit(&config);\n"
        "    return xrtNetUdpBind(xl6_net_engine, &address, 0u, &config, 0, 0);\n"
        "}\n"
        "long long xl6_xrt_udp_destroy(void* raw) { xnetudp* udp = (xnetudp*)raw; if ( udp != 0 ) { (void)xrtNetUdpClose(udp); xrtNetUdpDestroy(udp); } return 0; }\n"
        "long long xl6_xrt_udp_local_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetUdpLocal((xnetudp*)raw, &address) ? address.Port : 0; }\n"
        "void* xl6_xrt_udp_local_addr(void* raw) { xnetaddr address; return (raw != 0) && xrtNetUdpLocal((xnetudp*)raw, &address) ? xl6_net_address_string(&address) : xl6_xrt_string_dup(\"\", 0u); }\n"
		"int xl6_xrt_udp_send_bytes(void* raw, const char* host, uint16_t port, void* data, size_t size) { xnetaddr address; return (raw != 0) && ((data != 0) || (size == 0u)) && xl6_net_address_parse(&address, host, port, false) && (xrtNetUdpSendTo((xnetudp*)raw, &address, data, size) == XL6_XNET_OK); }\n"
		"int xl6_xrt_udp_send_text(void* raw, const char* host, uint16_t port, const char* text) { return text != 0 && xl6_xrt_udp_send_bytes(raw, host, port, (void*)text, strlen(text)); }\n"
        "long long xl6_xrt_udp_send_bytes_result(void* raw, const char* host, uint16_t port, void* data, size_t size) { xnetaddr address; return (raw != 0) && ((data != 0) || (size == 0u)) && xl6_net_address_parse(&address, host, port, false) ? xrtNetUdpSendTo((xnetudp*)raw, &address, data, size) : XNET_RESULT_ERROR; }\n"
        "long long xl6_xrt_udp_pending(void* raw) { return raw != 0 ? (long long)xrtNetUdpPending((xnetudp*)raw) : 0; }\n"
        "long long xl6_xrt_udp_queued(void* raw) { return raw != 0 ? (long long)xrtNetUdpQueued((xnetudp*)raw) : 0; }\n"
        "long long xl6_xrt_udp_queued_bytes(void* raw) { return raw != 0 ? (long long)xrtNetUdpQueuedBytes((xnetudp*)raw) : 0; }\n"
        "int xl6_xrt_udp_writable(void* raw, size_t size, long long timeoutMs) { return (raw != 0) && xrtNetUdpWritable((xnetudp*)raw, size, xl6_net_deadline(timeoutMs), 0); }\n"
        "xfuture* xl6_xrt_udp_writable_async(void* raw, size_t size) { return raw != 0 ? xl6_net_future_map(xrtNetUdpWritableAsync((xnetudp*)raw, size), xl6_net_map_bool) : 0; }\n"
        "int xl6_xrt_udp_send_vec(void* raw, const char* host, uint16_t port, xvalue* buffers)\n"
        "{\n"
        "    xnetspan local[16]; xnetaddr address; size_t count = buffers != 0 ? xrtValueCount(buffers) : 0u; size_t i; xnetspan* spans; xnetresult resultCode;\n"
        "    if ( (raw == 0) || (count == 0u) || !xl6_net_address_parse(&address, host, port, false) ) { return 0; }\n"
        "    spans = count <= 16u ? local : (xnetspan*)xrtCalloc(count, sizeof(*spans)); if ( spans == 0 ) { return 0; }\n"
        "    for ( i = 0u; i < count; ++i ) { xbuffer* buffer = (xbuffer*)xl6ValueHandleData(xrtValueArrayGet(buffers, i)); if ( buffer == 0 ) { if ( spans != local ) { xrtFree(spans); } return 0; } spans[i].Data = buffer->Data; spans[i].Size = buffer->Size; }\n"
        "    resultCode = xrtNetUdpSendVecTo((xnetudp*)raw, &address, spans, count); if ( spans != local ) { xrtFree(spans); } return resultCode == XL6_XNET_OK;\n"
        "}\n"
        "int xl6_xrt_udp_send_move(void* raw, const char* host, uint16_t port, void* bufferRaw) { xbuffer* buffer = (xbuffer*)bufferRaw; xnetaddr address; xnetresult resultCode; if ( (raw == 0) || (buffer == 0) || !xl6_net_address_parse(&address, host, port, false) ) { return 0; } if ( buffer->Size == 0u ) { return 1; } resultCode = xrtNetUdpSendTakeTo((xnetudp*)raw, &address, buffer->Data, buffer->Size); if ( resultCode == XL6_XNET_OK ) { buffer->Data = 0; buffer->Size = 0u; buffer->Capacity = 0u; } return resultCode == XL6_XNET_OK; }\n"
        "long long xl6_xrt_udp_send_batch(void* raw, const char* host, uint16_t port, xvalue* buffers)\n"
        "{\n"
        "    xnetdgramsend local[16]; xnetaddr address; size_t count = buffers != 0 ? xrtValueCount(buffers) : 0u; size_t accepted = 0u; size_t i; xnetdgramsend* items; xnetresult resultCode;\n"
        "    if ( (raw == 0) || (count == 0u) || !xl6_net_address_parse(&address, host, port, false) ) { return 0; }\n"
        "    items = count <= 16u ? local : (xnetdgramsend*)xrtCalloc(count, sizeof(*items)); if ( items == 0 ) { return 0; }\n"
        "    for ( i = 0u; i < count; ++i ) { xbuffer* buffer = (xbuffer*)xl6ValueHandleData(xrtValueArrayGet(buffers, i)); if ( buffer == 0 ) { if ( items != local ) { xrtFree(items); } return 0; } items[i].Remote = &address; items[i].Data = buffer->Data; items[i].Size = buffer->Size; }\n"
        "    resultCode = xrtNetUdpSendBatch((xnetudp*)raw, items, count, &accepted); if ( items != local ) { xrtFree(items); } return (resultCode == XNET_RESULT_ERROR) ? -1 : (long long)accepted;\n"
        "}\n"
        "void* xl6_xrt_udp_recv(void* raw, long long timeoutMs) { return raw != 0 ? xrtNetUdpReceiveWait((xnetudp*)raw, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
        "xfuture* xl6_xrt_udp_recv_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetUdpReceiveAsync((xnetudp*)raw), xl6_net_map_packet) : 0; }\n\n",
        out
    );

    xl6OutputPuts(
        "long long xl6_xrt_udp_packet_destroy(void* raw) { xrtNetUdpPacketDestroy((xnetudppacket*)raw); return 0; }\n"
        "void* xl6_xrt_udp_packet_text(void* raw) { xnetudppacket* packet = (xnetudppacket*)raw; size_t size = packet != 0 ? xrtNetUdpPacketSize(packet) : 0u; char* text = (char*)xrtMalloc(size + 1u); if ( text == 0 ) { return 0; } if ( size != 0u ) { memcpy(text, xrtNetUdpPacketData(packet), size); } text[size] = '\\0'; return text; }\n"
        "void* xl6_xrt_udp_packet_addr(void* raw) { const xnetaddr* address = raw != 0 ? xrtNetUdpPacketRemote((xnetudppacket*)raw) : 0; return xl6_net_address_string(address); }\n"
        "long long xl6_xrt_udp_packet_port(void* raw) { const xnetaddr* address = raw != 0 ? xrtNetUdpPacketRemote((xnetudppacket*)raw) : 0; return address != 0 ? address->Port : 0; }\n"
        "int xl6_xrt_udp_packet_truncated(void* raw) { return (raw != 0) && xrtNetUdpPacketTruncated((xnetudppacket*)raw); }\n",
        out
    );
    if ( needsUdpBatch ) {
        xl6OutputPuts(
            "void* xl6_xrt_udp_recv_batch(void* raw, size_t capacity, long long timeoutMs) { return (raw != 0) && (capacity > 0u) ? xrtNetUdpReceiveBatchWait((xnetudp*)raw, capacity, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
            "xfuture* xl6_xrt_udp_recv_batch_async(void* raw, size_t capacity) { return (raw != 0) && (capacity > 0u) ? xl6_net_future_map(xrtNetUdpReceiveBatchAsync((xnetudp*)raw, capacity), xl6_net_map_udp_batch) : 0; }\n"
            "long long xl6_xrt_udp_batch_destroy(void* raw) { xrtNetUdpBatchDestroy((xnetudpbatch*)raw); return 0; }\n"
            "long long xl6_xrt_udp_batch_count(void* raw) { return raw != 0 ? (long long)xrtNetUdpBatchCount((xnetudpbatch*)raw) : 0; }\n"
            "void* xl6_xrt_udp_batch_take(void* raw, long long index) { return (raw != 0) && (index >= 0) ? xrtNetUdpBatchTake((xnetudpbatch*)raw, (size_t)index) : 0; }\n\n",
            out
        );
    }
    xl6OutputPuts("#endif\n\n", out);
}


/* 输出网络运行时；各层只依赖前面已经输出的最小公共声明。 */
static void xl6CgenEmitNetExtendedRuntime(
    XL6_CgenContext* ctx,
    const XL6_CgenNetNeeds* needs
)
{
    if ( ctx == NULL || ctx->out == NULL || needs == NULL ) {
        return;
    }

    xl6CgenEmitNetDecls(ctx->out, needs);
	xl6CgenEmitNetLifecycleSelective(ctx->out, needs);
	if ( needs->stats ) {
		xl6CgenEmitNetStats(ctx->out, needs);
	}
    xl6CgenEmitNetHelpers(ctx->out, needs);
    xl6CgenEmitNetObjects(ctx->out, needs);
    if ( needs->tcpConnectConfig || needs->tcpListenConfig || needs->udp ) {
        xl6CgenEmitNetConfigs(ctx->out, needs);
    }
    if ( needs->tcpStream || needs->tcpServer ) {
        xl6CgenEmitNetTcpComponent(ctx->out, needs);
    }
    if ( needs->bytes || needs->tcpStream || needs->udp ) {
        xl6CgenEmitNetBufferBridge(ctx->out, needs);
    }
    if ( needs->udp ) {
        xl6CgenEmitNetUdp(ctx->out, needs->udpBatch);
    } else {
        xl6OutputPuts("#endif\n\n", ctx->out);
    }
}

#endif
