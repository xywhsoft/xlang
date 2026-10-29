/* Native code owns compiled syntax only. Callbacks, include owners and code
 * leases use the ordinary language object/closure graph, not opaque handles. */
#define XL6_TEMPLATE_RENDER_STATE \
    "        array<Template> owners = []\n" \
    "        Template owner = self\n" \
    "        (func(string) as Template?)? resolver = null\n" \
    "        int maxOwners = 4096\n" \
    "        if options != null then\n" \
    "            resolver = options.resolver\n            maxOwners = options.maxResolvedTemplates\n        end\n" \
    "        if maxOwners < 0 then throw RangeError \"invalid template include owner limit\" end\n"
const char* xl6StdlibTemplateClassSource =
    "func __xl_data_public(var input) as var\n"
    "    throw RuntimeError \"public-data intrinsic requires a direct verified call\"\nend\n"
    "func __xl_template_scopes(var data, TemplateRenderOptions? options) as var\n"
    "    var root = data\n    var current = data\n    var global = null\n"
    "    if options != null then\n"
    "        if options.root != null then root = options.root end\n"
    "        current = root\n"
    "        if options.current != null then current = options.current end\n"
    "        global = options.global\n    end\n"
    "    var scopes = __xl_data_public([root, current, global])\n"
    "    auto prepared = TemplateRenderOptions()\n"
    "    prepared.root = scopes[0]\n    prepared.current = scopes[1]\n    prepared.global = scopes[2]\n"
    "    if options != null then\n"
    "        prepared.maxOutputBytes = options.maxOutputBytes\n"
    "        prepared.maxSteps = options.maxSteps\n"
    "        prepared.maxDepth = options.maxDepth\n"
    "        prepared.maxLoopIterations = options.maxLoopIterations\n"
    "        prepared.maxIncludeDepth = options.maxIncludeDepth\n"
    "        prepared.strictUndefined = options.strictUndefined\n"
    "        prepared.escapeHtml = options.escapeHtml\n    end\n"
    "    return prepared\nend\n"
    "class TemplateCompileOptions\n"
    "    string open = \"{\"\n    string close = \"}\"\n"
    "    int maxSourceBytes = 16777216\n    int maxNodes = 1000000\n"
    "    int maxPathSegments = 1000000\n    int maxPathDepth = 64\n"
    "    int maxExpressions = 1000000\n    int maxBlockDepth = 64\n"
    "    int maxExpressionDepth = 128\n    int maxArguments = 1000000\n"
    "    int maxCallArguments = 256\nend\n"
    "class TemplateRenderOptions\n"
    "    var root = null\n    var current = null\n    var global = null\n"
    "    int maxOutputBytes = 16777216\n    int maxSteps = 10000000\n"
    "    int maxDepth = 64\n    int maxLoopIterations = 1000000\n    int maxIncludeDepth = 32\n"
    "    int maxResolvedTemplates = 4096\n    bool strictUndefined = true\n    bool escapeHtml = false\n"
    "    (func(string) as Template?)? resolver = null\nend\n"
    "class TemplateLocation\n"
    "    private int byteOffset\n    private int byteSize\n    private int sourceLine\n    private int sourceColumn\n"
    "    create(int offset, int size, int line, int column)\n"
    "        self.byteOffset = offset\n        self.byteSize = size\n        self.sourceLine = line\n        self.sourceColumn = column\nend\n"
    "    get offset() as int\n        return self.byteOffset\nend\n"
    "    get size() as int\n        return self.byteSize\nend\n"
    "    get line() as int\n        return self.sourceLine\nend\n"
    "    get column() as int\n        return self.sourceColumn\nend\nend\n"
    "class TemplateNode\n    private var snapshot\n"
    "    create(var snapshot)\n        self.snapshot = snapshot\nend\n"
    "    get type() as int\n        return int(self.snapshot[\"type\"])\nend\n"
    "    get output() as int\n        return int(self.snapshot[\"output\"])\nend\n"
    "    get offset() as int\n        return int(self.snapshot[\"offset\"])\nend\n"
    "    get size() as int\n        return int(self.snapshot[\"size\"])\nend\n"
    "    get line() as int\n        return int(self.snapshot[\"line\"])\nend\n"
    "    get column() as int\n        return int(self.snapshot[\"column\"])\nend\n"
    "    get source() as string\n        return string(self.snapshot[\"source\"])\nend\n"
    "    get expression() as string\n        return string(self.snapshot[\"expression\"])\nend\n"
    "    get format() as string\n        return string(self.snapshot[\"format\"])\nend\n"
    "    get name() as string\n        return string(self.snapshot[\"name\"])\nend\nend\n"
    "class Template\n    private native point handle\n"
    "    private array<func(TemplateCall) as bool> handlers = []\n"
    "    private bool handlersBound = false\n"
    "    extern \"xl6TemplateCreateV1\" create(string text, var options = null, int size = -1)\n"
    "    extern \"xl6TemplateCreateExtensionsV1\" create(string text, var options, var definitions, int size)\n"
    "    extern \"xl6TemplateDestroyV1\" destroy()\n"
    "    extern \"xl6TemplateSourceV1\" get source() as string\n"
    "    extern \"xl6TemplateNodeCountV1\" get nodeCount() as int\n"
    "    extern \"xl6TemplateIdentityV1\" get registryIdentity() as uint64\n"
    "    private extern \"xl6TemplateExtensionCountV1\" get extensionCount() as int\n"
    "    private extern \"xl6TemplateNodeV1\" func nodeNative(int index) as var\n"
    "    private extern \"xl6TemplateRenderV1\" func renderNative(var data, var options, func() as string dispatch) as string\n"
    "    func node(int index) as TemplateNode\n        return TemplateNode(self.nodeNative(index))\nend\n"
    "    func __bindHandlers(array<func(TemplateCall) as bool> handlers)\n"
    "        if self.handlersBound then throw RuntimeError \"template handlers already bound\" end\n"
    "        if self.handlers.length != 0 or handlers.length != self.extensionCount then throw ValueError \"template handler snapshot mismatch\" end\n"
    "        for handler in handlers\n            self.handlers.append(handler)\n        end\n"
    "        self.handlersBound = true\nend\n"
    "    func __dispatch(TemplateCall call, array<Template> owners, (func(string) as Template?)? resolver, int maxOwners)\n"
    "        if call.isInclude then\n"
    "            if resolver == null then return end\n"
    "            auto included = resolver(call.name)\n            if included == null then return end\n"
    "            if included.registryIdentity != 0 then\n"
    "                bool found = included.registryIdentity == self.registryIdentity\n"
    "                for owner in owners\n                    if owner.registryIdentity == included.registryIdentity then found = true end\n                end\n"
    "                if not found then\n"
    "                    if owners.length >= maxOwners then throw RangeError \"template include owner limit\" end\n"
    "                    owners.append(included)\n                end\n            end\n"
    "            call.resolve(included)\n            return\n        end\n"
    "        Template? owner = null\n"
    "        if call.registryIdentity == self.registryIdentity then owner = self end\n"
    "        for candidate in owners\n            if candidate.registryIdentity == call.registryIdentity then owner = candidate end\n        end\n"
    "        if owner == null then throw RuntimeError \"template callback owner unavailable\" end\n"
    "        if call.extensionIndex < 0 or call.extensionIndex >= owner.handlers.length then throw RuntimeError \"template callback index out of range\" end\n"
    "        auto handler = owner.handlers[call.extensionIndex]\n"
    "        if not handler(call) then throw RuntimeError \"template callback rejected invocation\" end\nend\n"
    "    func render(var data = null, TemplateRenderOptions? options = null) as string\n"
    XL6_TEMPLATE_RENDER_STATE
    "        var prepared = __xl_template_scopes(data, options)\n"
    "        return self.renderNative(null, prepared, func() as string\n"
    "            owner.__dispatch(TemplateCall(), owners, resolver, maxOwners)\n            return \"\"\n        end)\nend\n";
const char* xl6StdlibTemplateBufferSource =
    "    private extern \"xl6TemplateAppendV1\" func appendNative(Buffer output, var data, var options, func() as string dispatch) as int\n"
    "    func appendTo(Buffer output, var data = null, TemplateRenderOptions? options = null) as int\n"
    XL6_TEMPLATE_RENDER_STATE
    "        var prepared = __xl_template_scopes(data, options)\n"
    "        return self.appendNative(output, null, prepared, func() as string\n"
    "            owner.__dispatch(TemplateCall(), owners, resolver, maxOwners)\n            return \"\"\n        end)\nend\n";
const char* xl6StdlibTemplateFileSource =
    "    extern \"xl6TemplateFileCreateV1\" create(string path, var options, int encoding, int policy, int maxInputBytes)\n";
const char* xl6StdlibTemplateVfsSource =
    "    extern \"xl6TemplateVfsCreateV1\" create(Vfs filesystem, string path, var options, int encoding, int policy, int maxInputBytes)\n";
const char* xl6StdlibTemplateWriterSource =
    "    private extern \"xl6TemplateWriteToV1\" func writeNative(IoWriter output, var data, var options, func() as string dispatch) as uint64\n"
    "    func writeTo(IoWriter output, var data = null, TemplateRenderOptions? options = null) as uint64\n"
    XL6_TEMPLATE_RENDER_STATE
    "        var prepared = __xl_template_scopes(data, options)\n"
    "        return self.writeNative(output, null, prepared, func() as string\n"
    "            owner.__dispatch(TemplateCall(), owners, resolver, maxOwners)\n            return \"\"\n        end)\nend\n";
#undef XL6_TEMPLATE_RENDER_STATE
const char* xl6StdlibTemplateEndSource =
    "end\n"
    "class TemplateArgument\n    private var snapshot\n"
    "    create(var snapshot)\n        self.snapshot = snapshot\nend\n"
    "    get index() as int\n        return int(self.snapshot[\"index\"])\nend\n"
    "    get name() as string\n        return string(self.snapshot[\"name\"])\nend\n"
    "    get source() as string\n        return string(self.snapshot[\"source\"])\nend\nend\n"
    "class TemplateCall\n    private native point handle\n"
    "    extern \"xl6TemplateCallCreateV1\" create()\n"
    "    extern \"xl6TemplateCallDestroyV1\" destroy()\n"
    "    extern \"xl6TemplateCallIsIncludeV1\" get isInclude() as bool\n"
    "    extern \"xl6TemplateCallRegistryV1\" get registryIdentity() as uint64\n"
    "    extern \"xl6TemplateCallIndexV1\" get extensionIndex() as int\n"
    "    extern \"xl6TemplateCallNameV1\" get name() as string\n"
    "    extern \"xl6TemplateCallResolveV1\" func resolve(Template included) as bool\n"
    "    extern \"xl6TemplateCallArgumentCountV1\" get argumentCount() as int\n"
    "    private extern \"xl6TemplateCallArgumentV1\" func argumentNative(int index) as var\n"
    "    func argument(int index) as TemplateArgument\n        return TemplateArgument(self.argumentNative(index))\nend\n"
    "    extern \"xl6TemplateCallFindV1\" func find(string name, int size = -1) as int\n"
    "    extern \"xl6TemplateCallEvalV1\" func eval(int index) as var\n"
    "    extern \"xl6TemplateCallWriteV1\" func write(string text, int size = -1) as bool\n"
    "    extern \"xl6TemplateCallRenderBodyV1\" func renderBody() as bool\n"
    "    private extern \"xl6TemplateCallRenderCurrentV1\" func renderCurrentNative(var current) as bool\n"
    "    func renderCurrent(var current) as bool\n"
    "        return self.renderCurrentNative(__xl_data_public(current))\nend\n"
    "    extern \"xl6TemplateCallRawV1\" get raw() as string\n"
    "    private extern \"xl6TemplateCallScopeV1\" func scopeNative(int scope) as var\n"
    "    get current() as var\n        return self.scopeNative(0)\nend\n"
    "    get root() as var\n        return self.scopeNative(1)\nend\n"
    "    get global() as var\n        return self.scopeNative(2)\nend\nend\n";
const char* xl6StdlibTemplateEngineSource =
    "class TemplateEngineGuard\n    private native point handle\n"
    "    extern \"xl6TemplateEngineGuardCreateV1\" create()\n"
    "    extern \"xl6TemplateEngineGuardDestroyV1\" destroy()\n"
    "    extern \"xl6TemplateEngineGuardCheckV1\" func check() as bool\n"
    "    extern \"xl6TemplateEngineValidateV1\" func validate(string name, int kind, int minArgs, int maxArgs) as bool\nend\n"
    "class TemplateEngine\n"
    "    private TemplateEngineGuard guard = TemplateEngineGuard()\n"
    "    private array<var> definitions = []\n"
    "    private array<func(TemplateCall) as bool> handlers = []\n"
    "    private func register(string name, int kind, func(TemplateCall) as bool handler, int minArgs, int maxArgs) as bool\n"
    "        self.guard.validate(name, kind, minArgs, maxArgs)\n"
    "        for definition in self.definitions\n            if string(definition[0]) == name then throw ValueError \"duplicate template extension\" end\n        end\n"
    "        self.definitions.append([name, kind, minArgs, maxArgs])\n"
    "        try\n            self.handlers.append(handler)\n"
    "        catch failure\n            self.definitions.remove(self.definitions.length - 1, 1)\n            throw\n        end\n"
    "        return true\nend\n"
    "    func registerFunction(string name, func(TemplateCall) as bool handler, int minArgs = 0, int maxArgs = 256) as bool\n"
    "        return self.register(name, 1, handler, minArgs, maxArgs)\nend\n"
    "    func registerStatement(string name, func(TemplateCall) as bool handler, int minArgs = 0, int maxArgs = 256) as bool\n"
    "        return self.register(name, 2, handler, minArgs, maxArgs)\nend\n"
    "    func registerBlock(string name, func(TemplateCall) as bool handler, int minArgs = 0, int maxArgs = 256) as bool\n"
    "        return self.register(name, 3, handler, minArgs, maxArgs)\nend\n"
    "    func registerRawBlock(string name, func(TemplateCall) as bool handler, int minArgs = 0, int maxArgs = 256) as bool\n"
    "        return self.register(name, 4, handler, minArgs, maxArgs)\nend\n"
    "    func compile(string text, TemplateCompileOptions? options = null, int size = -1) as Template\n"
    "        self.guard.check()\n"
    "        auto compiled = Template(text, options, self.definitions, size)\n"
    "        compiled.__bindHandlers(self.handlers)\n        return compiled\nend\n";
const char* xl6StdlibTemplateEngineEndSource = "end\n";
const char* xl6StdlibTemplateEngineFileSource =
    "    func compileFile(string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template\n"
    "        self.guard.check()\n"
    "        return self.compile(__xl_template_read_file(path, encoding, policy, maxInputBytes), options)\nend\n";
const char* xl6StdlibTemplateEngineVfsSource =
    "    func compileFileIn(Vfs filesystem, string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template\n"
    "        self.guard.check()\n"
    "        return self.compile(__xl_template_read_vfs(filesystem, path, encoding, policy, maxInputBytes), options)\nend\n";
const char* xl6StdlibTemplateReadFileSource =
    "extern \"xl6TemplateReadFileV1\" func __xl_template_read_file(string path, int encoding, int policy, int maxInputBytes) as string\n";
const char* xl6StdlibTemplateReadVfsSource =
    "extern \"xl6TemplateReadVfsV1\" func __xl_template_read_vfs(Vfs filesystem, string path, int encoding, int policy, int maxInputBytes) as string\n";
static const char xl6StdTemplateCompileBody[] =
    "func __xl_std_template_compile(string text, TemplateCompileOptions? options = null, int size = -1) as Template\n"
    "    return Template(text, options, size)\nend\n";
static const char xl6StdTemplateRenderBody[] =
    "func __xl_std_template_render(string text, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int size = -1) as string\n"
    "    return Template(text, compileOptions, size).render(data, options)\nend\n";
static const char xl6StdTemplateEngineBody[] =
    "func __xl_std_template_engine() as TemplateEngine\n    return TemplateEngine()\nend\n";
static const char xl6StdTemplateCompileFileBody[] =
    "func __xl_std_template_compile_file(string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template\n"
    "    return Template(path, options, encoding, policy, maxInputBytes)\nend\n";
static const char xl6StdTemplateCompileVfsBody[] =
    "func __xl_std_template_compile_file_in(Vfs filesystem, string path, TemplateCompileOptions? options = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as Template\n"
    "    return Template(filesystem, path, options, encoding, policy, maxInputBytes)\nend\n";
static const char xl6StdTemplateRenderFileBody[] =
    "func __xl_std_template_render_file(string path, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as string\n"
    "    return Template(path, compileOptions, encoding, policy, maxInputBytes).render(data, options)\nend\n";
static const char xl6StdTemplateRenderVfsBody[] =
    "func __xl_std_template_render_file_in(Vfs filesystem, string path, var data = null, TemplateRenderOptions? options = null, TemplateCompileOptions? compileOptions = null, int encoding = 0, int policy = 0, int maxInputBytes = 67108864) as string\n"
    "    return Template(filesystem, path, compileOptions, encoding, policy, maxInputBytes).render(data, options)\nend\n";
static const char xl6StdTemplateErrorLocationBody[] =
    "extern \"xl6TemplateErrorLocationV1\" @no_raise func __xl_template_location_value(error value, int field) as int\n"
    "func __xl_std_template_error_location(error value) as TemplateLocation?\n"
    "    int offset = __xl_template_location_value(value, 0)\n"
    "    if offset < 0 then return null end\n"
    "    return TemplateLocation(offset, __xl_template_location_value(value, 1), __xl_template_location_value(value, 2), __xl_template_location_value(value, 3))\nend\n";
