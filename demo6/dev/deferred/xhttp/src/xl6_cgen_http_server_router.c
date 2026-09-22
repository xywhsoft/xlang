static const char xl6HttpServerCoreSource[] = XL6_HTTP_SERVER_SOURCE(
static xstrview xl6_http_view(const char* text)
{
    xstrview view;
    view.Data = text;
    view.Size = text != 0 ? strlen(text) : 0u;
    return view;
}

static xbytesview xl6_http_bytes(const void* data, size_t size)
{
    xbytesview view;
    view.Data = (const unsigned char*)data;
    view.Size = size;
    return view;
}

static bool xl6_http_view_equal(xstrview left, xstrview right)
{
    return (left.Size == right.Size) && ((left.Size == 0u) || (memcmp(left.Data, right.Data, left.Size) == 0));
}

static bool xl6_http_view_equal_text(xstrview left, const char* right)
{
    return xl6_http_view_equal(left, xl6_http_view(right));
}

static bool xl6_http_ascii_equal(const char* left, const unsigned char* right, size_t size)
{
    size_t index;
    if ( left == 0 || strlen(left) != size || (size != 0u && right == 0) ) { return false; }
    for ( index = 0u; index < size; index++ ) {
        unsigned char a = (unsigned char)left[index];
        unsigned char b = right[index];
        if ( a >= 'A' && a <= 'Z' ) { a = (unsigned char)(a + ('a' - 'A')); }
        if ( b >= 'A' && b <= 'Z' ) { b = (unsigned char)(b + ('a' - 'A')); }
        if ( a != b ) { return false; }
    }
    return true;
}

static char* xl6_http_copy_view(xstrview view)
{
    if ( view.Data == 0 && view.Size != 0u ) { return xl6_xrt_string_dup("", 0u); }
    return xl6_xrt_string_dup(view.Data != 0 ? view.Data : "", view.Size);
}

static uint64_t xl6_http_timeout_us(uint32_t milliseconds)
{
    return ((uint64_t)milliseconds) * UINT64_C(1000);
}

static void xl6_http_retain_function(XL6_Function* function)
{
    if ( function != 0 && function->retain != 0 && function->env != 0 ) { function->retain(function->env); }
}

static void xl6_http_release_function(XL6_Function* function)
{
    if ( function != 0 && function->release != 0 && function->env != 0 ) { function->release(function->env); }
    if ( function != 0 ) { memset(function, 0, sizeof(*function)); }
}

static bool xl6_http_function_valid(const XL6_Function* function)
{
    return function != 0 && (function->invoke_var != 0 || function->invoke_xcall != 0);
}

static XL6_Http_Router* xl6_http_router_create(void)
{
    XL6_Http_Router* shared = (XL6_Http_Router*)xrtCalloc(1u, sizeof(*shared));
    if ( shared == 0 ) { return 0; }
    shared->Router = xrtHttpServerRouterCreate(0);
    if ( shared->Router == 0 ) { xrtFree(shared); return 0; }
    shared->References = 1u;
    return shared;
}

static XL6_Http_Router* xl6_http_router_retain(XL6_Http_Router* shared)
{
    if ( shared != 0 ) { shared->References++ ; }
    return shared;
}

static void xl6_http_router_release(XL6_Http_Router* shared)
{
    if ( shared == 0 || shared->References == 0u ) { return; }
    shared->References--;
    if ( shared->References != 0u ) { return; }
    xrtHttpServerRouterDestroy(shared->Router);
    xrtFree(shared);
}

static void xl6_http_route_release(void* data)
{
    XL6_Http_Route* route = (XL6_Http_Route*)data;
    if ( route == 0 ) { return; }
    xl6_http_release_function(&route->Handler);
    xl6_http_release_function(&route->BeginHandler);
    xl6_http_release_function(&route->ChunkHandler);
    xl6_http_release_function(&route->EndHandler);
    if ( route->Static != 0 ) {
        if ( route->Static->Root != 0 ) { (void)xrtRootClose(route->Static->Root); }
        xrtFree(route->Static->Mount);
        xrtFree(route->Static->IndexStorage);
        xrtFree(route->Static->Indexes);
        xrtFree(route->Static->CacheControl);
        xrtFree(route->Static);
    }
    xrtFree(route);
}

static XL6_Http_Route* xl6_http_route_create(int kind)
{
    XL6_Http_Route* route = (XL6_Http_Route*)xrtCalloc(1u, sizeof(*route));
    if ( route != 0 ) { route->Kind = kind; }
    return route;
}

static void xl6_http_router_add_owned(XL6_Http_Router* shared, XL6_Http_Route* route)
{
    route->Next = shared->Routes;
    shared->Routes = route;
}

static void xl6_http_router_bind(XL6_Http_Router* shared, XL6_Http_Server* owner)
{
    XL6_Http_Route* route;
    if ( shared == 0 ) { return; }
    route = shared->Routes;
    while ( route != 0 ) { route->Owner = owner; route = route->Next; }
}

static bool xl6_http_method_part(const char* methods, size_t* offset, xstrview* output)
{
    size_t begin;
    size_t end;
    xstrview method;
    if ( methods == 0 || offset == 0 || output == 0 || *offset == SIZE_MAX ) { return false; }
    begin = *offset;
    end = begin;
    while ( methods[end] != 0 && methods[end] != '|' ) { end++; }
    if ( end == begin ) { return false; }
    method.Data = methods + begin;
    method.Size = end - begin;
    *offset = methods[end] == '|' ? end + 1u : SIZE_MAX;
    if ( xl6_http_view_equal_text(method, "ANY") ) { *output = xl6_http_view("*"); return true; }
    if ( !xrtHttpTokenValid(method) ) { return false; }
    *output = method;
    return true;
}

static bool xl6_http_methods_valid(const char* methods)
{
    size_t offset = 0u;
    size_t count = 0u;
    bool any = false;
    xstrview method;
    do {
        if ( !xl6_http_method_part(methods, &offset, &method) ) { return false; }
        count++;
        any = any || xl6_http_view_equal_text(method, "*");
    } while ( offset != SIZE_MAX );
    return count != 0u && (!any || count == 1u);
}

static long long xl6_http_method_id_view(xstrview method)
{
    if ( xl6_http_view_equal_text(method, "GET") ) { return 1; }
    if ( xl6_http_view_equal_text(method, "HEAD") ) { return 2; }
    if ( xl6_http_view_equal_text(method, "POST") ) { return 3; }
    if ( xl6_http_view_equal_text(method, "PUT") ) { return 4; }
    if ( xl6_http_view_equal_text(method, "DELETE") ) { return 5; }
    if ( xl6_http_view_equal_text(method, "PATCH") ) { return 6; }
    if ( xl6_http_view_equal_text(method, "OPTIONS") ) { return 7; }
    if ( xl6_http_view_equal_text(method, "CONNECT") ) { return 8; }
    if ( xl6_http_view_equal_text(method, "TRACE") ) { return 9; }
    return 0;
}

static xvalue* xl6_http_text_value(xstrview text)
{
    return xl6_xrt_value_text_create((void*)(text.Data != 0 ? text.Data : ""), (uint32_t)text.Size, false);
}
    ) "\n";



/* Request 只在回调期间借用连接和协议快照，解析容器按需创建并统一释放。 */
