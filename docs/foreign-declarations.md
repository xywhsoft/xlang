# 声明式 C 互操作

类型安全的同步 Array 列投影、列式原生描述符输入及其明确开销见
[foreign-array-projections.md](foreign-array-projections.md)。

本功能沿用 AST → XLIR → C。声明中的 C 身份、物理表示和生命周期先成为
类型／调用合同；Cgen 不根据包名或方法名猜测转换，也不生成库专用转发函数。
普通 class 及既有 native-backed class 的对象语义不变。

## 精确长度的 C 文本视图

```xlang
extern "nativeTextSize" @pure @no_raise @nonblocking @c_text_view(0) func textSize(string text) as c_size
extern "nativeText" @no_raise @nonblocking @c_result("text_view") func text() as string
```

`@c_text_view(index)` 把一个非空 `string` 参数直接声明为 C 的 `xstrview`
（`const char* Data`、`size_t Size`）。索引不包含方法的隐式接收者。
这是声明作者提供的同步、只读、非逃逸合同；C 函数不能修改、释放或保留数据。
语言参数仍按现有值语义、求值和借用寿命计划处理，不增加语言层引用类型。
传输保留实际字节长度和内嵌 NUL，不执行 `strlen` 或终止符预检。
它不是接受 `const char*` 或分开的指针／长度参数的通用替代品，物理 C 签名必须匹配。

`@c_result("text_view")` 声明 C 生产者返回借用的 `xstrview`；语言返回类型
必须是 `string` 或 `string?`，不能与 `@owned` 或 `@factory` 混用。
生成代码在同一 CALL 边界内按精确长度复制，成功后才允许清理参数及接收者，
因此结果可以借用本次输入，包括临时字符串。C 提供的数据必须在这段复制完成前有效，
并满足现有字符串的文本合同；不能把任意二进制谎报为文本。

`{NULL,0}` 对可空结果表示缺省，对非空结果报错；非空指针＋零长度表示空字符串。
NULL＋非零长度、超过语言长度上限及复制失败均为 checked 失败。C 生产者已经报错时
不读取其返回指针；即使无法分配错误诊断，失败位也会保留。
`@no_raise` 只约束 C 生产者，不能抹掉结果物化所需的分配和错误合同。

此合同支持自由函数、直接方法、getter／setter、自由函数值及显式类型的视图绑定方法值。
C 可变参数函数值的视图结果尚未准入，不能退回动态装箱来隐藏缺口。
输入实测不增加 XRT 分配；借用结果通常需要一次精确长度复制和一次分配，
并保留 checked foreign scope 的开销，不宣称整条调用零成本。

`tools/check_foreign_text_views.py` 验证实际 C 签名、NUL／UTF-8 字节长度、
无终止符输入、临时参数、空结果、分配失败、生产者失败，以及独立 XLIR 破坏性检查。

## 调用方管理生命周期的原生视图

```xlang
extern "xui_widget" class Widget @header("xui.h") @lifetime("caller")
    native point handle
    extern "xuiWidgetGetChildCount" @no_raise @nonblocking get childCount() as c_int
    extern "xuiWidgetAddChild" @no_raise @nonblocking func addChild(Widget child) as c_int
end
```

这是显式的 C 指针视图，不是受引用计数保护的语言对象。`handle` 说明原生
接收者的来源，不另外申请字段存储。参数、返回值及局部变量使用 `xui_widget`
本身。方法直接调用 C 符号；读取接收者没有 xvalue 装箱、字段名查找或引用增减。

`@lifetime("caller")` 是 C 声明作者承担的生命周期合同：绑定不会保活、销毁
或延长 C 对象／代码的生命周期。它不应冒充受管资源能力，也不能用于隐藏语言
回调或异步拥有边。XUI 树、context 依赖和逃逸视图的 checked 合同另行接入。

声明必须提供一个合法的 C 指针类型拼写、可移植的相对头文件路径和显式生命周期。
可以使用指针 typedef，也可以使用 `const Vertex*`、`struct Node*` 等受限的普通
C 指针声明；不接受任意 C 表达式。指向元素的类型及 const 限定进入真实函数原型。
当前视图只接受一个 native point 字段及直接声明的 C 成员，不接受普通数据字段、
语言虚表、构造／析构函数。普通 class 不能继承这种视图。外部视图可以静态继承
具有相同 C 类型、头文件及生命周期的另一个外部视图，不增加运行时类型结构。
这里的限制不会改变普通类的既有继承或字段规则。

视图的类型别名共享规范化类型。`Widget?` 使用空指针表示缺省；外部声明作者
须把可能返回 NULL 的接口声明为可空类型。不同声明的视图仍保留语言的名义类型
身份，不允许通过相同指针大小把不相关类型互转。

别名链可以使用同一构造合同，例如 `WidgetAlias(raw)`；参数检查、成员访问
权限和规范类型身份与原名称一致。C 记录的零构造及普通类构造也沿用此规则。

### 类型化绑定方法值

```xlang
func(Widget) as c_int attach = parent.addChild
auto result = attach(child)
```

绑定时求值一次接收者，并把精确 C 指针复制到已有语言闭包环境。之后重新给
`parent` 赋值不改变绑定的目标。支持别名、静态继承和省略末尾默认参数的精确函数签名；
可空接收者须先通过现有控制流缩窄。这里要求显式 `func(...)` 类型，不扩展 `auto` 推导。

调用适配器沿用统一的类型化 XLIR CFG，直接从捕获槽读取 C 指针并调用声明的 C 符号，
没有接收者的 xvalue 装箱、动态方法查找、原生 retain／free 或额外的 View 控制对象。
文本视图参数和结果仍服从同一调用合同。普通语言类的方法绑定继续拥有语言引用，
不会因为新增此路径而退化成裸指针捕获。

闭包保留自身适配器的代码驻留合同，但不延长所指 C 对象或外部 C 提供方的寿命；
声明作者须保证它们覆盖绑定方法的全部调用。带析构的原生资源不能套用平凡指针捕获。
需要受管拥有、失效检查或长期原生订阅的接口仍须使用独立的受管合同。

绑定本身会分配语言闭包；调用仍有闭包间接调用、代码活动登记和环境读取成本，
不等同于手写 C 的直接函数调用。专项计数区分创建与稳定调用，不将此能力描述为零成本。
`tools/check_foreign_bound_views.py` 覆盖实际 C 提供方、绑定逃逸、接收者重赋值、
继承／默认参数、普通类拥有引用、创建 OOM 和既有绑定方法回归。
其独立 IR 检查会破坏捕获表示及调用目标；即使换成同签名方法，也须在输出任何 C/header 前拒绝。

## ABI 验证

生成 C 包含被选中类型的权威头文件，并保留真实 C 类型的函数声明。C11 编译器
检查指针表示及完整函数原型；把整数或结构体误声明为指针视图会在 C 编译阶段
拒绝。字段或函数参数不会回退为 `point` 来掩盖不支持的类型。

XLIR 类型保存 C 名称、声明头及精确父类型。子类型传参产生现有 VALUE 指令，
其父类型链和相同 C 表示均由 IR 校验；Cgen 只产生普通赋值。声明属性在 AST
交接时已成为类型事实，不作为未定类型的可执行字符串进入 IR。

`tools/check_foreign_declarations.py` 使用独立 C 提供方、真实头文件及当前源码
编译器，检查接收者、参数、返回值、可空、别名和继承的 C 生成及 GCC O0/O2
执行，同时通过 TCC 验证实际 JIT、模块加载和独立 EXE；也检查缺失／损坏合同和
不允许成员的拒绝。独立提供方使用不同文件名，避免将自动同名 sidecar 再次显式
加入构建。测试不创建 UI 或网络连接。

单次、精确、无适配／清理／异常的 C 调用采用 IR 层的直接参数放置计划。
只允许读取平凡参数，随后调用 C 函数并直接返回；debug 探针路径保留原有指令。
这项通用规则不按库名或方法名选中函数，也不修改生成后的 C 文本。

同一 getter 的机器码对照中，GCC O2 与手写 C 均为一个尾跳转；TCC 两者均为
32 字节，逐条指令字节一致。此前多出的 5 条指令、19 字节代码和 16 字节栈
空间已在该形态中消除。`tools/check_foreign_forwarding.py` 可重现此对照；
它仅证明这一精确形态，不构成受管资源、回调及整个 XGE/XUI 绑定的零开销结论。

## 按值 C 记录

```xlang
extern "xui_rect_t" struct Rect @header("xui.h")
    c_int fX
    c_int fY
    c_int fW
    c_int fH
end

Rect rect = Rect()
rect.fW = 640
Rect copy = rect
copy.fW = 320
```

这是独立的按值记录类型，`Rect()` 按 C 的 `{0}` 规则初始化。赋值复制内容，
修改副本不会改变原值。它可以按值传参、返回及嵌套，也支持可空、类型数组及
动态值往返。外部 C 返回及语言错误载体保留真实类型；const 指针不会经由
`void*` 错误载体丢失限定。

C 头拥有真实定义；生成器不重新定义该结构体，而是发出一个只用于静态核对的
字段模式，核对每个字段的类型、偏移，以及整体大小和对齐。因此 XUI 的整数
Rect 与 XGE 的浮点 Rect 不会被混为一谈。记录只能包含完整、平凡复制的 C
字段，不接受语言析构、资源拥有字段、字段默认表达式或递归按值布局。

XLIR 的 C record 与 union 保持不同类型／物理 ABI，共用明确的 aggregate
布局、ZERO／GET／SET 值操作及既有的平凡值传输。不会把 C struct 当作 C union
生成。布局池和相应操作扩展后，旧 union 的字段布局与按值行为保持不变。

`tools/check_foreign_records.py` 覆盖结构体往返、副本独立性、嵌套、动态值、
数组、可空及 const 连续数据指针。C 头独立于语言声明；错误字段类型、顺序或
数量必须被拒绝。`tools/check_foreign_parser_corpus.py` 为新旧解析器各自使用
匹配的生成 token 头和 lexer，避免新增 token 后错误地比较混装的旧解析器。

## 内联 C 数组

```xlang
extern "xge_mat4_t" struct Matrix @header("xge.h")
    c_float m[16]
end
Matrix value = Matrix()
value.m[0] = c_float(1.0)
auto copy = value.m
copy[0] = c_float(2.0) // value 的内容不变
value.m = copy
```

数组是内联、固定长度的平凡值，不分配堆对象。字段仍采用权威 C 定义的原生数组
布局；独立数组值用等长内联载体保存，复制后相互独立。当前接受一维、正整数
长度的字段，元素可以是精确 C 标量、指针或 C 记录。禁止语言资源元素和普通类
中的 C 数组字段。读取 `length` 不扫描数据。

普通索引显式检查上下界，失败抛出 RangeError；XLIR 的实际读写指令必须携带
同一数组和索引的成功分支证明。C/header 输出前会重新验证该证明，不依靠
“前端已检查”的假设。常量值、临时返回值和只读 length 不能修改。安全索引
尚未定义，语义检查明确拒绝。异步帧和生成器保存相同的平凡值，使用统一的
aggregate 渲染实现。

`tools/check_foreign_arrays.py` 检查矩阵、记录数组、副本、越界、异步和生成器，
也以错误的 C 字段长度／元素类型验证权威 ABI。传入 `--source-build` 时，还
执行破坏性 IR 测试，要求不合法的证明、分支和类型在输出任何 C/header 前拒绝。

## C 描述结构和状态／输出工厂

```xlang
extern "Config" struct Config @header("provider.h")
    c_int value
end
extern "readConfig" @pure @no_raise @nonblocking @c_ref(0) func read(Config config) as c_int
extern "destroyWidget" @no_raise @nonblocking func dispose(Widget widget)
extern "createWidget" @no_raise @nonblocking @factory(1, 0, dispose) @c_ref(1) func create(c_int mode, Config config) as Widget
```

`@c_ref(index)` 按零起始的显式语言参数序号，将平凡 C 记录映射到调用期间的
`const Config*`。它不改变参数的值语义，不允许 C 保存或修改这个临时地址，
也不提供语言层引用。不同参数可分别声明；重复序号、越界序号和资源类型会拒绝。

上面的工厂对应 `int createWidget(int mode, Widget* output, const Config*)`。
`@factory` 的输出序号按零起始的物理 C 参数计算；第二个参数是成功状态码，
支持整个 C int 范围；第三个参数是具有精确结果类型的、无返回值且 no-raise 的
原生回滚函数。它必须能唯一解析为模块级声明。工厂本身为静态／自由声明。

XLIR 用一条 NATIVE_STATUS_OUT 表示原始 C 调用，并得到 `(status, raw)`。
后续 CFG 明确检查状态及非空输出。失败先释放部分输出，再创建诊断和转入
语言异常处理；诊断分配失败也不会跳过回滚。Cgen 不建立一层工厂包装函数，
不将原生状态码误当成对象指针，也不根据库名补写错误处理。

这里 `@no_raise` 描述 C 生产者；状态／非空结果的准入仍可失败，作为独立的
求值效果保存到 IR，不会被该声明压掉。Cgen 的原始调用不需要额外的 foreign
error scope。声明属性不作为可执行表达式跨入后端；IR 仅保留数值化合同。

当前原生输出仍是 caller 管理的视图：成功后须显式交给原生生命周期管理者。
工厂函数可以作为普通语言函数值使用；其适配器复用同一状态／输出准入 CFG，
函数值的 checked 结果不因 C 生产者标记 no-raise 而丢失。可能失败的参数清理
仍明确拒绝，待原生结果的清理事务闭合后启用。受管拥有、上下文和回调环境
是后续步骤；本节不代表完整 XGE/XUI 迁移已完成。

`tools/check_foreign_factories.py` 覆盖实际 C 提供方、相对模块导入及私有回滚
名称重写，验证 GCC O0/O2、TCC JIT／模块／独立 EXE 和完整状态码范围。
其 IR 破坏性测试要求状态比较、非空分支、回滚身份、入口及记录地址被修改后，
验证器在输出任何 C/header 前拒绝。

## 精确 C 回调与静态函数地址

`c_callback` 可以按真实 C ABI 传递 caller-lifetime 指针视图和按值 C 记录，
包括可空指针结果。普通回调继续使用现行代码活动保护和 checked 错误合同。

```xlang
extern "apply" @no_raise @nonblocking @c_static_callback(0) func apply(c_callback func(Widget, Rect) as Rect callback, Rect input) as Rect
func adjust(Widget widget, Rect input) as Rect
    input.fW = 640
    return input
end
auto output = apply(adjust, Rect())
```

`@c_static_callback(index)` 按零起始显式参数序号声明：原生 API 接收的是
精确、无捕获、不会抛错的 C 函数地址。声明作者负责保证该函数的模块代码及
它访问的全局状态存活至最后一次原生调用。此合同不会自动保活模块，不能替代
需要运行时管理的回调订阅。函数变量、捕获闭包、需要参数转换、可能抛错或
签名不匹配的函数均拒绝。

XLIR 使用私有 C 函数指针类型和 C_FUNCTION_ADDRESS，不先创建语言闭包再
提取入口。验证器复核来源、精确类型和 no-raise 合同，并限制其只能进入相同
声明的静态 C 回调参数；该指针不能装箱、保存到语言变量或伪装成普通函数值。
Cgen 直接传递已有函数地址，不生成额外回调入口或闭包分配。

`tools/check_foreign_callbacks.py` 检查 GCC O0/O2、TCC JIT／模块／独立 EXE，
并覆盖坏签名、错误参数序号和 fallible／间接函数来源。提供 `--source-build`
时另行破坏 IR，要求在输出任何 C/header 之前拒绝。带捕获 userdata、长期
注册的拥有边和原生退役后的回调清理仍须使用独立的受管合同，不能套用静态
地址声明后声称已解决。

静态回调夹具在 C 提供方直接读取 `xrtMemDebugSnapshot`，检查调用区间的
分配、释放、重分配、活动内存及无效释放计数均不变化。记录必须在宿主创建
模块前启用；已有活动分配时再次调用 `xrtMemDebugEnable` 会被拒绝，不能据此
判断计数器不可用。该测量仅覆盖夹具内的静态回调调用，不包含模块初始化或
后续受管回调注册的成本。

## 精确二进制视图

```xlang
extern "inspectBytes" @pure @no_raise @nonblocking @c_bytes_view(0) func inspect(bytes input) as c_size
extern "borrowBytes" @no_raise @nonblocking @c_bytes_view(0) @c_result("bytes_view") func copyResult(bytes input) as bytes
```

对应 C 声明为 `size_t inspectBytes(xbytesview)` 和
`xbytesview borrowBytes(xbytesview)`。`@c_bytes_view(index)` 使用零起始的显式
语言参数序号，只接受非空 `bytes`；接收者不占该序号。多个参数可分别声明。
它不能与同参数的文本视图、记录地址或静态回调合同重叠。

输入在调用处通过一次 `xrtValueGetBytes` 取得栈上的指针／长度记录；不复制数据，
不扫描终止符，不创建临时 Buffer 或转发函数。原生函数只能在当前调用期间读取
该视图，不能保存地址、修改数据或在返回后使用。它保留任意字节及内嵌 NUL，
不执行 UTF-8 检查。可变共享 Buffer 应使用下述显式 `c_span` 合同，不能套用此 bytes 合同。

`@c_result("bytes_view")` 接受 `bytes`／`bytes?` 结果。原生生产者返回借用
`xbytesview`；统一 CALL 在清理输入及接收者前将它复制成语言拥有值。
因此 C 生产者的 `@no_raise` 不会掩盖结果复制的分配失败。

结果的 `{NULL, 0}` 表示缺失：可空结果成为 `null`，非空结果报 checked 错误。
非 NULL 地址加零长度表示存在的空字节值。NULL 地址加非零长度或超过语言长度
上限的视图被拒绝。生产者已失败时不读取返回地址；复制分配失败即使不能再分配
诊断，也会保留失败状态。此合同不检查任意 C 地址是否有效，提供方仍须交付有效
且至少存活至本次结果复制结束的内存。

自由函数、函数值、直接方法、属性、类型化绑定方法、协议／动态方法、异步和
生成器中的调用均复用该合同。结果有必要的复制、分配和 checked scope 成本；
输入也有一次不透明值访问，不能将整个调用宣称为零开销。

`tools/check_foreign_bytes_views.py` 提供公开头、原生 C 提供方、GCC O0/O2、
TCC JIT／模块／独立程序、声明负例和独立 IR 篡改验收。

## 受管原生视图

```xlang
extern "UiContext*" class Context @header("ui.h") @lifetime("owned") @policy("contextPolicy") @scope("UiScope") @watch("UiWatch")
    native point handle
end
extern "UiWidget*" class Widget @header("ui.h") @lifetime("owner") @owner(Context) @policy("widgetPolicy") @watch("UiWatch")
    native point handle
end
extern "uiRollbackContext" @no_raise func rollback(Context context)
extern "uiCreateContext" @no_raise @c_result("owned_view") @factory(0,0,rollback) func createContext() as Context
```

语言槽保存 `XL6_ForeignView*`，方法仍以声明中的精确 C 指针调用。
Root 拥有原生对象；`owner` 子视图由 Root 管辖，失去最后一个子视图引用不会单独
销毁原生子对象。方法准备检查线程、关闭状态与调用作用域。显式关闭和原生销毁
使既有别名失效；复制关闭后的值仍允许，后续原生访问返回 checked 错误。

`@c_result("owner_view", source)` 的 source 包含接收者；0 是实例方法的 self，
自由函数则是首个实参。Root 结果只能借回同一 Root；子结果必须属于声明的 Root。
`@c_control(index)` 显式传控制记录，供 checked close 等驻留生命周期操作使用。
`@c_result("bool_status")` 把原生 bool 转为语言 void，false 独立表示失败。

字段、可空值、动态装箱、Array／Stack、闭包／cell、Future、异步及生成器都保存
实际拥有槽和图合同。原生描述符必须准确追踪实际拥有引用，不能把含语言闭包的
payload 声明为 leaf。native watch 必须在释放内存前通知，且自身不分配、不执行
用户代码。工厂观察记录覆盖结果登记及其失败回滚；代码驻留准入重入后须重新
确认输出存活和 parent 仍开放，再安装新观察者。

这些规则不替代原生库的资源合同。caller 视图仍由声明作者负责生命周期；原生
共享资源、借用参数的长期保存及尚无拥有入口的回调，必须先补足相应原生合同。

## C 终止符字符串与分离的指针／长度

```xlang
extern "readPath" @no_raise @c_cstr(0) func readPath(string path) as c_int
extern "drawPoints" @no_raise @c_span(0,"const Point*","int","draw.h") func drawPoints(Buffer input)
extern "consumeBytes" @no_raise @c_span(0,"const uint8_t*","size_t") func consume(bytes input)
```

`@c_cstr(index)` 检查内嵌 NUL，拒绝静默截断；调用原生 C 函数时不创建字符串
副本，但有一次检查扫描。需要精确长度时优先使用 `c_text_view`、`c_bytes_view`
或 `c_span`。

`c_span` 将一个逻辑参数展开为相邻的 C 指针和元素数量。指针要求一个显式 `*`，
计数类型为 `int` 或 `size_t`；可选第四个字符串指定权威 C 头文件。
string／bytes 和显式 C record 只提供 const 输入，Buffer 可以提供 const 或可写
存储。元素数量由实际字节长度除以 `sizeof(element)` 得出；长度必须整除，地址
必须满足元素对齐要求，int 计数不得溢出。void 元素按字节计数。

Buffer 的存储在栈作用域中持有实际原生 handle；没有临时 Buffer、复制回写或
数据复制。存储视图存活时，冲突的常规 Buffer 操作和显式 destroy 被 checked 拒绝；
释放语言变量不能提前释放被使用的存储。它增加登记、引用和同步成本。
原有显式 point／裸指针 C 互操仍由调用者负责地址有效期。

工厂的 output 序号按实际 C 参数计算。指针／数量二元组和回调三元组不可从中间
插入 output。`c_span` 等普通参数序号按显式语言参数计算，不包含接收者；
`c_control` 和下述 `c_value` 的序号包含隐式接收者。

## 长期原生回调

```xlang
extern "uiSetHandlerOwned" @no_raise @c_owned_callback(1,0,"UiCallbackOwner",-1) func setHandler(Context context,func(Widget?,c_int) as c_int callback)
```

逻辑函数展开为精确类型的 C 函数指针、userdata 和 `const UiCallbackOwner*`。
owner source 0 指首个 Context 实参；实例方法的 0 则指 self。可选第五个字符串
`"first"`／`"last"` 选择 C 回调中 userdata 的位置，默认最后。回调结果支持平凡
C 值；托管输入只能来自同一 Root。字符串／bytes 的隐式跨回调转换不属于此合同。

原生 owner record 按顺序包含 `xrtownershipref`、bool Hold、bool Drop。发布者在
注册时持有实际引用，每次调用另持有一份快照，直到整个类型化 thunk 返回后才
释放。替换先提交新槽，再清理旧槽；追踪必须枚举这些实际拥有槽。
普通闭包可捕获，nullable 函数用空三元组清除注册；不通过 xvalue 装箱调用。

thunk 进入独立的 adapter／body 代码作用域及 Root 原生作用域，转换受管输入，
调用 checked 语言入口，按逆序清理，失败时返回声明的 C fallback，绝不跨 C 栈
展开语言异常。有即时 checked 边界时交付该边界；外部宿主触发而没有边界时，
错误和独立失败位保留在 Root，供下一次调用、显式检查或关闭时交付。
宿主的累计查询统计这类延迟交付失败，首个错误为空时也不会被后续错误覆盖。

原生 `@no_raise` 只描述 C 生产者。准备、结果复制／准入和输入清理各自的失败都
进入 XLIR，不能借此省略必要的 checked 路径。可写 span 和长期回调注册不能声明
为 pure。直接、绑定、命名函数、协议和动态分派共用同一份物理参数映射。

## 回调期视图

```xlang
extern "UiEvent*" class Event @header("ui.h") @lifetime("callback") @owner(Context)
    private native point handle
    extern "uiEventCode" @no_raise get code() as c_int
    extern "uiEventPart" @no_raise @c_result("scope_view",0) func part() as Event
end
```

回调期类型必须声明所属的 owned Root，不能带 policy／watch／scope，不能通过
拥有工厂或 `c_control` 创建／操作。长期回调的此类参数只在本次回调有效。
一个回调登记一次栈上 lifetime；所有传入及派生视图共享这一标识，不复制原生
payload，不逐个分配控制对象。`scope_view` 的非空来源必须已经是回调期视图；
从普通 Root 调用不能制造可在返回后使用的临时视图。`owner_view` 则可返回其
同一 Root 中的稳定对象，继续遵循已有的受管对象合同。

非空逻辑载体是地址整数和 uint64 标识；可空载体另带 hasValue。
赋值、捕获和字段／Array／Stack 保存可以保留这个值，但不会延长 Context、
原生数据或模块的生命周期。每次原生访问先在当前执行上下文检查标识，再恢复
原生指针；回调结束、跨线程／fiber 使用、相同地址后续复用都不能使旧值复活。
原生 owner scope 提前结束时也先使其视图过期。需要长期使用时显式复制数据。

普通 typed 路径不装箱；显式转为 var／普通对象存储时仍有动态值的正常分配成本。
访问存在执行上下文查询、有效期检查和 checked 边界；不宣称与无检查裸 C
指针完全相同。首次执行上下文初始化也是冷路径成本。

## 批量验证

使用同一次完整源码构建运行已落地能力的专项验收：

```text
python tools/check_foreign_declarative.py --root <repo> --driver <source-driver> --source-build <build.json> --out <fresh-evidence-directory>
```

脚本依次运行视图、记录、内联数组、工厂、回调、直接参数放置和初始化检查，
并对同一源码构建运行文本／二进制视图及类型化绑定方法专项，
保留各组及汇总报告。某组失败后继续执行其他独立组，最后统一报告失败。
本入口验证编译器能力，
XGE/XUI 包、受管拥有和长期回调的完整验收仍须在相应实现完成后加入。

## Native fields and checked C statuses

`@c_field` on an external instance getter/setter selects direct native field
access. The C name must be one identifier, the getter has no explicit arguments,
and the setter has one exact value argument. The authoritative header checks the
actual field type. A field cannot be static, an owning factory, or an arbitrary C
expression. Managed and callback receivers retain their normal access checks.

```xlang
extern "NativeEvent*" class Event @header("native.h") @lifetime("callback") @owner(Context)
    private native point handle
    extern "code" @c_field get code() as c_int
    extern "code" @c_field let code(c_int value)
end
```

A void language method can map an integer C status with
`@no_raise @c_result("status", successCode)`. The physical C result remains `int`.
Other values raise a checked error preserving the original native diagnostic,
or report the returned status and native operation if no diagnostic exists.
Failure is independent of whether an error object can be allocated. Successful
calls do not call the diagnostic helper.

`@no_raise @c_output(physicalIndex, successCode)` maps one trivial C output
parameter to the language result. It accepts exact scalars and C records and can
be used on free functions, instance methods and getters. The index includes the
native receiver and expanded pointer/count or callback/user/owner slots; an
output cannot split an expanded group. The native result is initialized on the
stack and committed only after success. Managed owning factories keep their
separate `@factory` rollback transaction. No parameter-reference syntax is added.

```xlang
extern "native_measure" @no_raise @c_output(1, 0) func measure(c_int constraint) as c_int
// C: int native_measure(int constraint, int* output);
```

## Counted native data fields

A native field getter can combine `@c_result("text_view")` or
`@c_result("bytes_view")` with `@c_length("nativeCountField", "int")` (or
`"size_t"`). Both names are C field identifiers on the same admitted receiver.
The authoritative header checks the byte pointer/array and count field types.
Byte fields accept char, unsigned-char, void and their const pointer forms;
text fields accept only char/const-char storage. Integer/object pointers are
not admitted as byte storage. The declared count still has the exact int or
size_t C type; no implicit narrowing changes its ABI.

```xlang
extern "text" @c_field @c_length("textSize", "int") @c_result("text_view") get text() as string
```

The count is in bytes and is read once. Negative counts and counts beyond a
fixed inline array's capacity raise checked errors before copying. Pointer
fields require the native publisher's valid-range contract; a nonzero count
with NULL data, or a result beyond the language length range, is rejected.
Flexible arrays need a separate explicit pointer/capacity API.

For a required bytes field, NULL plus an explicit zero count is an empty bytes
value. A nullable bytes field preserves NULL/zero as null. This field rule uses
the admitted authoritative count; it does not change NULL handling of ordinary
native xbytesview return values or pointer-valued text results. Materialization
still makes the language value copy; normalization adds no native allocation.

Embedded NUL bytes are preserved. A requested language string/bytes value is
copied before ending the receiver's native scope, so that value can be saved.
Saving the callback view itself does not extend its lifetime. No field wrapper
function, temporary Buffer or NUL scan is introduced for this projection.

## Native pointee projections

`@c_deref` on an external instance getter/setter reads or writes the exact
native pointee. It has no arguments and selects an ordinary C dereference,
with the same exact-value and borrowed-result rules as a native field. It
cannot be combined with `@c_field`, factories, status or output mappings.

```xlang
extern "int*" class IntOut @header("native.h") @lifetime("callback") @owner(Context)
    native point handle
    extern "value" @c_deref get value() as c_int
    extern "value" @c_deref let value(c_int next)
end
```

This provides callback output views without parameter-reference syntax. The
pointer is accessed only after the callback epoch has been checked; saved
views reject both reads and writes after the callback returns. Typed views
do not allocate per argument. Explicit dynamic boxing keeps its normal cost.
The authoritative header checks the pointee type; no forwarding C function
is emitted. A NUL-terminated character view can directly declare the existing
native `xrtStrView` primitive and request `@c_result("text_view")` when a
persistent string copy is needed.

## Managed instance factories and authoritative prototypes

A managed instance method can declare a status/output factory. Its output
index includes the implicit C receiver and any expanded argument groups. The
receiver and result must both have managed native lifetimes; caller-lifetime
factories retain the static/free restriction. The ordinary result probe,
rollback, input cleanup and code-residency transaction also applies to methods.

```xlang
// Native C: int xuiWidgetCreate(xui_context context, xui_widget* output);
extern "xuiWidgetCreate" @no_raise @c_result("owner_view",0) @factory(1,0,rollbackWidget) func widget() as Widget
```

When the physical signature uses header-owned C types, the included header
must declare the native function. Generated C checks the complete physical
function-pointer type with `_Static_assert` / `_Generic` instead of replacing
that declaration with another `extern`. This preserves attributes such as
Windows `dllimport` and adds no runtime check. Wrong return or parameter types
fail C/header acceptance. Native declarations without supplied header-owned
types keep their explicit C prototype.


## Nullable caller pointers and native-value ownership

A declared caller-lifetime C pointer may be an optional extern parameter.
Absent values use the existing NULL pointer sentinel; there is no tagged
wrapper at the C boundary. Other nullable families do not acquire that ABI.

Boxing a native owning rvalue borrows it until an independent xvalue has been
created, then releases the source on both success and failure. Container
insertion never silently loses the original owner.

Native storage Drop callbacks may still be running inside a container's
mutation. They enqueue resident deferred cleanup and must not reenter native
destruction there. Generated cleanup-end and completed foreign-call boundaries
drain one detached batch, preserve the first checked failure (including a NULL
diagnostic), and permit retries. This scheduling does not allocate. The
ordinary error-scope End remains available for native internals that still
hold storage addresses. These additions require the matching runtime build.


## Native-value protocols and callback pointer outputs

Declared managed and callback native views may implement runtime protocols.
XLIR records their exact native receiver preparation and nominal type IDs;
ordinary object implementations use the same identity domain. Dispatch unboxes
that sealed native value and emits the exact C operation with its checked
preparation/result/cleanup transaction. Caller-lifetime raw pointers may satisfy
compile-time constraints, but cannot acquire an owning runtime protocol carrier.
A non-null implementation may enter a nullable protocol target. Nullable source
values still require the ordinary explicit narrowing rules.

Aliases of a physical C symbol are permitted only when their declared C pointer,
header, policy, owner root, watch and scope contracts agree. This grants no
implicit language downcast or relaxed lifetime.

`@c_deref` setters on a callback-lifetime pointer may write an owner-lifetime
native child (including null) from the same declared root. Generated C prepares
both views and checks their ACTUAL root control identity before writing. A
cross-context child leaves the output unchanged and reports a checked error.
The getter uses explicit `@c_result("owner_view",0)` admission. These are exact
pointer operations, with no per-function forwarding wrapper. Persisting the
callback output view does not extend its native lifetime: reads and writes after
the callback return are checked failures.

Optional declared managed inputs use their existing NULL sentinel; callback
inputs keep their tagged epoch carrier. This does not admit arbitrary nullable
C values. The native producer remains distinct from preparation and cleanup.
## In-place C record factories

An independently owned native resource can declare its actual initializer,
record type, storage adopter and failure finalizer:

```xl
extern "native_init" @no_raise @c_result("owned_view")
 @factory(0,0,release) @c_inplace("NativeRecord",adopt,0)
 func create(c_int option) as Resource
extern "native_adopt_storage" @no_raise func adopt(Resource value) as c_int
extern "native_release" @no_raise func release(Resource value)
```

The factory output index includes the native receiver and physical expansions.
The initializer receives `NativeRecord*`, rather than `NativeRecord**`. Generated
C allocates a zeroed, independently XRT-owned record, calls the initializer
directly, then adopts its storage. The authoritative header checks the exact
pointer type, complete record and supported heap alignment (16 bytes on the
currently supported targets). Over-aligned or incomplete records are rejected.

Before successful adoption, failure finalizes any initialized payload and frees
the record. After adoption, the native finalizer owns final record disposal,
including admission failure after transfer. An adopter's declared success means
it took storage even if it also reports a diagnostic; a failed adopter must not
consume storage. This is a trusted native API contract. The result is an
independent owned root, without a native watch or call-scope contract.

There is one shared checked transaction in Cgen. It does not synthesize a C
forwarding implementation for each constructor. Record/control allocation,
registration and checked ownership still have costs relative to a C stack
record; this feature does not establish universal zero overhead.

## Independent retained results

`owner_view` borrows a value within the same managed root. An independently
reference-counted borrowed result instead declares acquisition of its own
native reference:

```xl
extern "native_borrow" @no_raise @c_result("retained_view")
 @c_retain(retain,"positive",release) func borrow(Resource source) as Resource?
extern "native_addref" @no_raise func retain(Resource value) as c_int
extern "native_release" @no_raise func release(Resource value)
```

The success predicate is explicitly `"positive"` (result > 0) or `"zero"`
(result == 0). Both helpers must be exact top-level, non-pure, no-raise C
declarations taking the non-null resource type; release returns void. The
result uses an independent owned-root policy and authoritative header, without
native watches or scopes. A nullable NULL result acquires no native reference.
Properties may use the same contract, including direct `@c_field` reads.

The producer returns a borrowed pointer. Before input cleanup, generated C
acquires one reference and admits a language control. Failure before acquisition
does not release the borrowed pointer. Failure after acquisition releases only
the reference acquired by this transaction, including success accompanied by a
diagnostic. Separate controls may own separate references to the same native
pointer; this mechanism does not globally intern raw pointer addresses. It
requires one native retain and ordinary control admission, without copying the
resource payload.

## Bounded callback byte spans

A native callback's pointer/count pair can become one callback-epoch view:

```xl
extern "const xbytesview*" class Slice @header("native.h")
 @lifetime("callback") @owner(Context)
 native point handle
 extern "Size" @c_field get size() as c_size
 extern "Data" @c_field @c_length("Size","size_t")
  @c_result("bytes_view") get copy() as bytes
end
typedef Handler = func(Slice,c_int) as c_int
extern "native_set" @no_raise @c_owned_callback(1,0,"NativeOwner",-7)
 @c_callback_span(1,0,"const unsigned char*","size_t")
 func on(Context owner,Handler? callback)
```

The first index is the explicit callback parameter of the registration
declaration; the second is its logical callback argument. Physical expansion
preserves argument order and the declared first/last userdata convention.
Multiple logical arguments can each expand to pointer plus byte count. A
declaration permits at most 64 such mappings. Counts use `int` or `size_t`;
readonly pointers use `const void*`, `const uint8_t*`, `const unsigned char*` or
`const char*`. The header proves the logical view's exact const `xbytesview*`
type and complete native callback signature.

The thunk builds a stack `xbytesview` and uses the existing two-word epoch view.
It makes no payload copy or per-argument heap wrapper. NULL/zero is a present
empty slice and is normalized to a stable empty data address; NULL/nonzero,
negative and out-of-language-range counts fail before invoking language code.
Saving a slice does not extend its validity. Nested callbacks keep their own
epochs: an inner saved view expires while the outer view remains valid. A
declared copy property performs ordinary checked bytes materialization for
persistent data. Callback registration, activity, epoch bookkeeping, dynamic
boxing and explicit copies retain their respective costs.

### 非引用计数记录策略

`@c_inplace("RecordCName")` 使用同一原位工厂机制，但不调用原生 storage adopter。
成功准入后，类型策略的 Destroy 必须同时释放原生 payload 和 XRT 堆记录；
初始化或准入失败时，编译器先调用声明的 rollback，再释放记录。此模式仅用于
独立拥有根，不能让原生代码跨语言 close 保留该记录。需要原生引用计数逃逸的
Font／Texture 等资源使用三参数 `@c_inplace`，由最后一个原生引用释放记录。

### 原生回调返回拥有字符串

```xlang
extern "setLabelProviderOwned" @no_raise @c_owned_callback(1,0,"NativeOwner",0,"owned_text") func onLabel(Root root,func() as string callback)
```

`owned_text` 是显式的结果拥有合同：逻辑回调返回 `string` 或 `string?`，
实际原生回调返回 `xstrview`，将其 XRT 分配交给原生调用方。原生方消费后须
`xrtFree(Data)`，并在释放 callback snapshot 之前完成；此合同不能用于返回
借用 `char*` 的旧接口。`first`／`last` 仍控制 userdata 位置，指定两者时将
`owned_text` 放在第六个参数；省略位置时默认最后。

null 使用 `{NULL,0}`，存在的空字符串保持非空 Data；长度是精确 UTF-8 字节，
允许内嵌 NUL。回调抛错、分配失败或 Leave 清理失败时，thunk 先释放已经产生
的结果，再报告 checked 失败，返回空结果。失败码必须为 0。成功时直接交付语言
返回值，不增加桥接复制；返回共享的捕获字符串时仍须产生独立的拥有值。

### 原生视图的按值投影

```xlang
extern "nativeRead" @no_raise @c_value(0,"NativeRecord") func read(RecordView value) as c_int
// 类方法的 0 表示隐式接收者；自由函数的 0 表示第一个参数。
```

`c_value(index,"CType")` 从非空受管／回调期视图读取完整 pointee，按原生 C
值传入。序号包含隐式接收者；一项仍占一个物理参数，不与 `c_span` 等展开
合同混用。原始 caller 指针和 nullable 视图不能使用此合同。
长期回调的 owner 参数仍须保留原生指针 ABI，不能把它改为按值记录。

XLIR 保存稀疏的类型投影行和明确的物理 ABI；Cgen 复用普通视图的 checked
调用准备，参数直接生成为 `*((const NativeRecord*)native_view_0.raw)`，没有
逐方法转发函数、额外分配或字段装箱。原生头同时证明 pointee 类型、完整性和
整个函数签名；不能把同样大小的 `xstrview` 与 `xbytesview` 相互冒充。直接调用、
有明确函数类型的绑定方法、协议和动态投影使用同一准备规则，过期／关闭检查不变。

### C 与 C++ 的原生 ABI 证明

生成的公开头在 C11 和 C++11／17 中均执行签名、字段、尺寸和对齐检查。
`XLIR_NATIVE_*` 宏只组织编译期证明，不生成运行时代码。C++ 中对直接标量枚举
及函数签名使用底层整数兼容规则，指针仍保留精确枚举身份和 cv 限定；禁止用
`int*` 代替枚举指针。需要精确枚举指针时使用权威 C 头的 typedef。


### Void 原位初始化器

`@factory(outputIndex, rollback) @c_inplace("RecordCName")` 声明原生返回 `void` 的
原位初始化器；需要原生存储采用时同样可使用三参数 `@c_inplace`。初始化错误通过
checked 边界报告，部分初始化失败仍先 rollback payload，再释放独立记录。
已有 `@factory(outputIndex, successCode, rollback)` 保留精确的 C `int` 状态返回。
两参数形式必须配合原位存储，不允许用于普通指针输出；没有隐式成功码或虚构 int 返回。
XLIR 的 outputResultAbi 单独记录物理 producer 返回 ABI，与语言的拥有结果及 C 输出
参数分离；公开头持续核对真实原生声明。生成 C 直接调用初始化器，void 路径不产生
native_status 临时值。实际 XGE Material 已验证此合同；其纹理槽由原生代码 retain，
shader 是借用槽，不能误当成拥有资源公开。


### 同步捕获回调与独立 epoch

`@c_sync_callback(parameter, owner, failureCode)` 把一个语言 Closure 映射为原生
函数指针与 userdata 两个参数。parameter 不计隐式 receiver；owner 计入 receiver，
`-1` 明确表示没有受管 owner。可选第四参数为 `"first"`／`"last"`，指定 callback
内部 userdata 位置；第五参数可提供权威 C 头文件。具备该头的声明做完整函数类型
证明，不另行声明并覆盖原生导入／调用约定。一个函数可含多个独立同步回调，
`@c_callback_span` 可把 callback 的原生 pointer/count 组合为一个 callback 期字节视图。

这是声明作者对原生函数的严格合同：原生函数必须同步等待全部调用结束，不保留
函数地址或 userdata。编译器为每个声明参数生成一个类型化 thunk，调用上下文
保存在调用者栈上；闭包在整个 C 调用期间保留，adapter 代码活动登记复用已有
调用者的驻留能力。它不新建长期回调堆记录或获取另一个 adapter 代码 lease。
原生异步／逃逸回调必须使用 `@c_owned_callback`；返回后再使用过期 C userdata
属于违反原生合同，不能依靠语言 checked 保护修复任意 C 悬空指针。

同步入口只允许在同一执行上下文进入闭包，其他线程及 Windows fiber 在触及
捕获状态之前 checked 拒绝。第一个回调失败单独保留，包括没有诊断对象的失败；
已有外层 checked 失败优先，其后才是回调失败、原生诊断和清理失败。
原生 `@no_raise` 不会消除闭包／准备的 checked 边界；动态语言回调可能阻塞，
这类声明不能标 `@pure` 或 `@nonblocking`。当前结果限于已支持的平凡 C 值或
void；拥有文本结果仍使用长期回调的显式 owned_text 合同。

`@lifetime("callback")` 类现在可省略 `@owner`，表示独立的 callback epoch，适用于
Event、DrawContext、原生快照和 C 字符串视图。带 owner 的声明继续验证根的名义
身份；无 owner 的视图也可出现在有受管根的回调中，此时实际 epoch 仍由该次原生
调用保护。保存／装箱只复制逻辑载体，不能延长原生 payload 生命周期；字段、方法
和派生视图都在读取地址前解析当前上下文中的 epoch，回调结束后 checked 拒绝。
需要长期保存的数据通过显式复制 API 获取。

回调控制记录升级为 XL6_ForeignScopeLifetimeV2，区分 root、rootless 与 expired；
配套 Begin／End／Make／Derive／Get／Alive／ExpireOwner 接口均为 V2。逻辑
XL6_ForeignScopeViewV1 仍为 address/identity 两字段，没有新增逐值拥有记录。
这些私有 runtime 头与编译器必须作为匹配 SDK 交付，不能混用旧版控制记录。

栈上下文与 epoch 并不消除所有成本：类型化间接调用、闭包引用、代码活动与所有权
登记、执行上下文／错误槽访问，以及需要时的 epoch 编号仍存在。预热循环的零新增
XRT 分配只能说明这些路径不再创建堆适配器，不能代替同工作负载的完整 C 性能比较。

同步调用在原生函数返回后先执行 producer Complete，再判断状态、采用拥有结果或
交付输出值；闭包与 adapter 的代码活动持续到最终 End 清理。Complete 不提前释放
这些能力，因此结果采用／清理阶段的拒绝或回调尾部不会访问已卸载代码。

### 平凡 C 输出与多返回值

`@c_output(physicalIndex)` 表示原生 `void` 函数写出一个平凡 C 值；
`@c_output(physicalIndex, successCode)` 表示原生 `int` 状态加一个平凡输出。
这两种形式与拥有型 `@factory` 分别建模。`@c_outputs(i, j, ...)` 把多个
独立输出槽映射为相同顺序的语言多返回值，默认原生返回 `void`；搭配
`@c_status(successCode)` 时原生返回 `int`。位置必须严格递增，计入 receiver，
不能插入 pointer/count 或 callback/userdata/owner 分组内部。

输出只接受明确的平凡 C 类型，包括完整的平凡记录。生成 C 先将输出值置零，
一次调用原生函数，确认 producer、回调和状态成功后再交付整个结果；失败不交付
部分值。多返回值的 product 只是语言结果载体，不能误声明成 C 函数直接返回
同名结构。`outputResultAbi` 区分物理 `INT_STATUS` 和 `VOID`，
`nativeSuccessCode` 保存该调用的状态成功谓词；Cgen 直接消费这些规范合同。

### 没有受管根的长期回调

`@c_owned_callback(parameter, -1, "NativeOwner", failureCode)` 明确声明由全局
发布者或独立原生记录持有的闭包。原生入口仍接收函数指针、userdata 和真实 owner
三元组，必须 Hold／Drop 并保护实际调用快照；`-1` 不会虚构 Root。一个函数可以
接收多个独立三元组，原生批量发布应先完成全部 Hold 和准备，失败回滚，最后一起
发布。此类回调可接收独立 epoch 视图；需要受管对象参数时仍须有对应的真实 owner。

回调失败优先交给当前 checked 调用，其次交给真实受管 owner。两者均不存在时，
驻留错误通道移动保留首个 xerror，并独立累计失败次数；没有诊断对象仍属于失败。
后续安全 checked 边界可以交付该批错误；已有主错误时保留到以后，不覆盖也不吞掉。
当前执行上下文仍处于原生回调时暂缓交付，不要求全进程其他线程都已静止。
宿主可通过匹配 SDK 的 `xl6CallbackFailureTakeV1`／`CountV1` 读取；Take 的返回值
表示输出参数检查成功，输出载体的 `ok` 才表示是否没有待交付失败。

错误通道只保存驻留 xerror 及其拥有的诊断数据，不保存生成代码或原生发布者。
它的可交付性不能充当 DLL 可卸载的证明。原生快照保护生成 thunk 的物理返回；
在原生回调／Drop 内请求模块销毁时，宿主另外保留模块的退役挂起状态，阻止自动
卸载发布者 DLL。宿主必须先停止并等待原生发布者返回，再显式调用
`xlRuntimeDrainRetiringChecked` 确认静止。任意线程的另一次 checked 调用不能
替代该确认。回调内排空或清理又产生新尾部时返回 BUSY，`ready` 和 `ok` 必须分开检查。


### 独立原生子图的声明与代码寿命

拥有型外部类可声明 `@domain("NativeDomain")`，其中标识符由权威 C 头提供，
类型必须是不可变的 `XL6_OwnershipDomainV1`。域必须没有 `Load` 回调，
`Resolve`／`AdmitsRead` 只按精确原生 Ops 身份查询，不依赖 catalog policy，
也不分配、不调用语言、不改变引用。caller／owner／callback 类不能声明此项；
可空类型和同一原生根的继承类型必须保留一致的域。

`TraceRaw` 对独立引用计数节点报告实际的一条拥有边，不能把该节点的子边摊平到
语言控制记录。节点自己的 Count、Trace、Claim、Clear、Finish 和活动状态继续
由原生库负责；普通原生引用和缓存持有的引用同样计入真实数量。

驻留根控制记录保存域指针及其代码 lease。catalog 在冷创建时登记物理根发现能力，
计划在 Freeze 内枚举所有物理根并取得真实 snapshot pin。原有 native boundary
的 Enter／Leave／Discover 和新增发现能力组合执行，不用另一个无关锁域替换它。
同一域指针去重，不同域全部参与查询；相互冲突的准入拒绝，不能采用第一个匹配。

计划的 native adapter 表可能来自可卸载 DLL。全部 adapter Drop 返回之前，
最后的 CodeLease release 只将真实宿主 acquisition 移入本次执行上下文的批次；
嵌套批次合并，在最外层结束时才执行宿主 release。该批次使用已经存在的 lease
记录作链表，不分配额外队列项。计划自身的 running 保护和记录也保留到所有
宿主 release 返回，避免重入时读到已经释放的计划。不能依赖图遍历顺序保住 DLL。

本合同不增加逐方法包装；域查询、发现和批次只服务创建／拥有图／清理。成本仍
包括根记录中的一个域指针、lease 中的一个退休链接、最后释放时的执行上下文查询，
以及冷期图查询和物理 pin；它们必须计入完整的 C 对照，不能称为整套绑定零成本。
`XL6_ForeignViewDescriptorV1` 和 factory ProbeV1 布局保持不变，有域的工厂使用
独立的 `xl6ForeignViewCreateRootWithDomainProbedV1` 入口。

私有 runtime SDK 同时提供 `__xl_runtime/xl_foreign_error.h`。原生清理结果中的
“已消耗 owning 引用”和“清理是否成功”必须分别传递；即使没有可分配的 xerror，
失败位仍须进入当前 checked scope。不能因为物理对象仍被其他引用保留而重复释放
已经消耗的语言 owner，也不能用返回成功来吞掉原生 finalizer 失败。

## Independent native function headers

A free extern function can name its authoritative C header independently of
its argument and result types. This also covers zero-argument and scalar-only
functions, whose signatures otherwise carry no external class header:

```xl
extern "nativeVersion" @header("native.h") @no_raise func version() as c_uint
```

The compiler emits the include and an exact function-pointer type assertion.
It preserves the header's DLL import and calling convention attributes instead
of redeclaring the symbol. The header path and declaration identity are sparse
XLIR contracts; malformed paths, duplicate contracts and mismatched call-target
indices are rejected before either C or public-header output.

## Exact const pointer inputs and borrowed dynamic results

`@c_const(index)` counts the implicit receiver, when present. For an external
pointer class it changes only that physical input to `const T*`; ownership,
epoch checks and the logical class remain the same. The source pointer must be
single-level and mutable; a class already declared `const T*` needs no annotation.
It cannot overlap a span, callback, `c_ref`, `c_value`, control or field mapping.

For a parameter whose exact logical type is `var`, the same annotation means a
nonescaping `const xvalue*` borrow. It passes the existing value pointer directly;
it does not clone, allocate, box or transfer ownership. Native code must acquire
its own reference before retaining that value beyond the call. The contract
does not recursively make the contents of mutable containers immutable.

```xl
extern "nativeSetValue" @no_raise @c_const(1) func setValue(Root owner,var value)
extern "nativeGetValue" @no_raise @c_result("borrowed_value") func getValue(Root owner) as var
```

`borrowed_value` declares an exact native `const xvalue*` result and a logical
`var` result. Before releasing input owners, callback epochs or native call
scopes, generated C obtains an owned value with `xrtValueRetain`. This follows
normal `var` assignment semantics, rather than an explicit container clone or
deep copy. Native NULL becomes the language null value. A producer failure is
observed before dereferencing or retaining its result, and a failed retain
remains a checked failure even if no diagnostic can be allocated. The native
pointer never escapes as the language result.

## Checked status rollback helpers

An owning factory's rollback helper has one exact, non-null resource parameter
and a logical void result. Its native C return may be void, or an explicitly
declared checked integer status:

```xl
extern "nativeReleaseChecked" @no_raise @c_result("status",0) func rollback(Resource value)
extern "nativeCreate" @no_raise @c_result("owned_view") @factory(0,0,rollback) func create() as Resource
```

The latter produces an exact `int nativeReleaseChecked(NativeResource*)` ABI.
An arbitrary language integer result, boolean status, hidden input adaptation,
or nullable/different owner type cannot act as rollback. This rule also applies
to the release helper of `c_retain`.

Rollback remains a direct native call on the cold failure path. The compiler
latches the original producer/admission failure before cleanup, then accumulates
both a failing cleanup status and a cleanup diagnostic through the same checked
scope. A first failure with a NULL diagnostic remains first; a later allocation
or cleanup error cannot replace it. Existing void rollback declarations keep
their native ABI.

## Callable native record fields

`@c_field_call` selects an exact function-pointer field of a native pointer
class, rather than a global C symbol. For example:

```xl
extern "NativeProxy*" class Proxy @header("native.h") @lifetime("caller")
    native point handle
    extern "measure" @c_field_call @no_raise func measure(c_int value) as c_int
end
```

The receiver is one native pointer, and the selected field's exact C function
type is proved against the authoritative header. XLIR records a distinct
FIELD_CALL operation and seals the field identity, receiver, physical inputs,
outputs and checked preparation. A field name never triggers a same-named
global symbol's bridge or CString rules. Direct, bound and protocol calls
prepare the receiver and arguments, read the typed callee once, and invoke it
with the native receiver as its first C argument. There is no forwarding C
function or operation-code dispatch. NULL receiver/callee admission is checked;
already transferred owned arguments are rolled back if admission fails.

Existing span, callback, factory and output contracts retain their meaning.
The native call is indirect because the authoritative C API is a function
table; safety admission and lifetime pins remain real binding costs.

## Optional clamped byte prefixes

```xl
extern "consume" @header("consume.h") @no_raise @c_span(0,"const void*","size_t",1) func consume(bytes input,int size=-1) as c_int
```

The numeric fourth argument of `c_span` identifies a distinct explicit language
parameter. That parameter must be `int`/`int64`; it supplies a byte limit and is
consumed by the span projection, rather than becoming another C argument. The
physical call remains `consume(pointer, count)`. The optional authoritative
header can follow as a fifth argument. The original three-argument span and
fourth-argument header spelling retain their complete-span behavior.

`-1` means the complete input, zero selects an empty prefix, and a nonnegative
limit is clamped to the available byte length. Other negatives raise a checked
range error before the native function runs. Clamping precedes the physical
`int` count overflow check; a small prefix of a larger input is representable.
Limits are bytes, so a UTF-8 prefix can end inside an encoded character. Embedded
NUL is ordinary data. Every logical argument is evaluated once.

The mapping supports readonly byte pointers (`const void*`, `const char*`,
`const unsigned char*`, `const uint8_t*`) with the existing string/bytes/Buffer
and POD span storage rules; only Buffer may be nullable. Null Buffer supplies
`NULL,0` even for a larger requested prefix. Writable spans and multi-byte
element counts do not acquire this byte-prefix meaning. A limit may precede or
follow its span, allowing required parameters before an optional `size=-1`.
It cannot carry another physical mapping. Several spans may share one limit;
each uses the smaller of that limit and its own byte length. The declaration's
prefix count authenticates span rows, while physical layout removes each unique
limit only once. Prefix declarations can also compose with rectangular regions.
The shared physical-layout walk omits the logical limit when resolving each
region pointer and extent selector; neither
the span pointer nor its native count can serve as an unrelated region selector.
Descriptor packs, array groups and inserted fixed input slots retain their
current separate restrictions with prefix declarations.

```xl
extern "rasterize" @header("raster.h") @no_raise @c_span(0,"const uint8_t*","int",5) @c_region(4,"write","2","3","5",4) @c_result("status",0) func rasterize(bytes input,c_int width,c_int height,Buffer rgba,c_int stride,int size=-1)
```

This declaration calls `rasterize(pointer, count, width, height, rgba, stride)`.
The optional language limit is evaluated once in normal argument order and
does not add a C parameter. Nonadjacent/shared limits are a later compiler
change; the historical adjacent-prefix cohorts below do not qualify it.

The sealed IR records the prefix index in its sparse parameter contract and an
exact count in existing declaration-fact padding. The declaration facts stay
88 bytes on the tested x64 ABI. Cgen emits bounds arithmetic at the direct call
site, with no per-function forwarding implementation or new runtime carrier.
Buffer access, error/code lifetime, ordinary value preparation and the limit
checks still have costs. Qualification is tracked separately from final SDK
and IDE delivery.

`foreign-span-prefix-preflight-20261112.json` records the O0 frontend-only
preflight over the complete r19 parent: 113 command checks, including direct
JIT/static/shared C++ clients and previous output-contract regressions, plus
130 damaged-IR rejections before C/header bytes. Sparse parameter rows grow
from 692 to 696 bytes on x64; the per-declaration facts remain 88 bytes and
runtime carriers do not grow. The 2,000-call hot string/bytes loop adds no XRT
heap operations. At that preflight stage, complete O2/TCC/XRT/VFS and embedded
compiler acceptance was still pending; the preflight installed no release or
IDE binaries.

The later Windows r20 complete O2 build passed 120 command checks, including
the new prefix fixture, nested JIT and an independently built embedded-compiler
host, native ABI headers/shared C++ clients, and borrowed/C-string/fixed output
regressions. `foreign-span-prefix-windows-20261112.json` records the exact full
build, source freeze and tests. The subsequent Linux r12 cohort also passes
120 command checks. `foreign-span-prefix-20261112.json` records the combined
240 checks, both complete O2/TCC/XRT/VFS/embedded builds and their exact source
mapping to `3958b1858`. Later prefix/region composition is a separate change;
none of these basic-prefix reports qualifies that composition, the whole SVG
package or final compiler/SDK/IDE delivery.

The later prefix/region composition preflight is recorded separately in
`foreign-span-region-prefix-preflight-20261112.json`: 17 O0 frontend command
checks and 55 damaged-IR rejections before C/header bytes. They cover exact
strided output, padding/length preservation, capacity/stride/prefix failure
before native entry, native field/bound/protocol calls, strict C/C++ ABI proofs
and an actual shared-library consumer. Two hundred warmed field calls succeed
with the XRT allocation budget set to zero. No new runtime carrier, output
staging buffer or business-method adapter is introduced; existing admission,
Buffer/lifetime and bounds checks remain. Complete O2 and nested-compiler gates
for this composition await the next integrated cohort. The basic-prefix r20/r12
cohorts must not be presented as qualification of the later composition.

`foreign-span-limit-positions-preflight-20261112.json` records the subsequent
nonadjacent/shared-limit change: 149 O0 frontend command checks and 96 damaged
IR rejections. It covers a leading limit, independent interleaved limits, a
shared optional trailing limit, native field/bound/protocol calls, a scalar
output before pixel dimensions, and strict C/C++ clients. A 1,000-iteration
loop makes 4,000 native calls with unchanged XRT allocation/free/reallocation
counters. Physical headers keep only the expected pointer/count pairs; no
carrier or per-method forwarding implementation is added. This preflight does
not qualify Linux, a rebuilt embedded compiler or final SDK/IDE delivery.

## Size-prefixed native record outputs

`@c_output_size(physicalOutput,"field")` initializes the named C member of an
existing `c_output` or `c_outputs` record before the single native call:

```xl
extern "nativeGetInfo" @header("native.h") @no_raise
    @c_output(0,0) @c_output_size(0,"size") func info() as NativeInfo
```

The position counts actual C slots, including a receiver and expanded input
groups. It must identify an output slot, and the member name is the exact C
name, including any record field `c_name` mapping. Multiple initializers may
select different outputs; duplicate slots are rejected. Attribute order does
not change the output kind or erase the initializer.

Only trivial C records with authoritative headers and an unsigned C integer
member are accepted. Fixed-width language integers, signed integers, booleans,
arrays and arbitrary initializer expressions do not establish this ABI. The
generated header proves the exact member type and that it can represent the
whole native record's `sizeof`; an unsigned byte in a larger record is rejected
even when its C layout otherwise matches.

The compiler stores only declarations that request this operation in sparse,
typed XLIR rows. Output position, return ordinal, record type and member
declaration/type are verified together. Generated C assigns
`native_raw.size = sizeof(native_raw)`, or the corresponding tuple element,
without an extra call, heap allocation or per-function wrapper. Direct,
function-value, bound and protocol calls use the same verified initializer.
The output-mode byte contains a closed kind/presence flag set; unknown bits or
a presence flag without its rows are rejected before C or header output.

## Borrowed owner pointer outputs

`@c_output(physicalOutput[,successCode]) @c_result("owner_view",ownerParameter)`
maps one native `Child**` output to an owner-bounded managed view. The output
position counts complete physical C parameters; the owner index counts logical
parameters including an instance receiver. The returned child and source must
have the same declared native root. Returning the existing root itself is also
valid. Nullable results admit NULL; required results reject it with a checked
value error. A void C producer omits the success code.

```xl
extern "nativeFind" @no_raise @c_output(2,0) @c_result("owner_view",0)
    func find(c_int index) as Child
```

This contract transfers no native reference. It has no rollback helper and
cannot be substituted for an owning `@factory`: a real constructor must still
declare cleanup for an output that it transfers before returning failure.
Borrowed outputs require `@no_raise`; preparation, status checking and managed
admission remain fallible. A failed producer may leave the pointer unspecified:
generated code checks the outcome before observing or admitting that pointer.
The owner/native call scope stays active through admission and failure cleanup.

The existing sparse output mapping and owner-view result ABI describe this
combination; no new runtime carrier or per-method forwarding body is introduced.
Cold exposure can allocate an observed view/control record. A repeated exposure
reuses the live view, but still performs root/thread/liveness checks, ownership,
code residency and checked call bookkeeping. No blanket zero-cost claim follows
from a warmed allocation counter.

## Borrowed text and byte output slots

`@c_output_view(physicalOutput,"text")` declares an exact native `xstrview*`
output and an owned logical `string` or `string?` result. The `"bytes"` form
declares `xbytesview*` and `bytes` or `bytes?`. It combines with `c_output` or
`c_outputs` and counts the same complete physical slots; it cannot overlap a
size initializer, input span or callback group.

The `"cstring"` form declares an exact `const char**` output and an owned
`string` or `string?` result. It reuses ordinary borrowed C-string return
admission: a terminating NUL scan and independent copy occur while native input
scopes are active. NULL is absence for a nullable result and a checked value
error for a required result; a non-NULL empty C string is an empty value. This
explicit terminating-text contract cannot preserve bytes after an embedded NUL.
Use `"text"` and native `xstrview*` for length-aware text. Neither form adopts or
frees the producer's borrowed storage. Failed status/callback outcomes skip the
scan and copy, since unsuccessful output slots need not contain valid pointers.

```xl
extern "nativeReadItem" @header("native.h") @no_raise
    @c_outputs(1,2) @c_status(0) @c_output_view(1,"text")
    func readItem(c_int index) as (string,c_int)
```

The native view is borrowed only until the input/native call scopes end.
Generated C first checks producer status and callback errors, then materializes
every logical result while those scopes remain active. Only after all copies
succeed does it deliver the complete result. A later copy failure releases
earlier copies and leaves an assignment's previous values unchanged; no partial
tuple is delivered. Input scope cleanup failures still follow the existing
checked call contract.

The NULL rules match `c_result("text_view")` and `c_result("bytes_view")`:
NULL data with zero size is absence for nullable results and a checked value
error for required results. Non-NULL data with zero size is an actual empty
value. NULL data with nonzero size, or a size above `INT64_MAX`, is a checked
range error. Embedded NUL and UTF-8 byte lengths are preserved without `strlen`.

Sparse typed XLIR rows seal each conversion kind, physical output, logical
return ordinal and exact result type including nullability. C/header proof
uses the physical view pointer type, rather than treating a borrowed view as
a language string, a dynamic byte value or a user C record. Conversion and
copy costs are explicit; no per-library or per-method C wrapper is introduced.

## Fixed-pointer byte regions and typed observers

`@c_region(physicalInput,"read"|"write",width,height,stride,unitBytes)`
describes a strided byte region occupying exactly one native pointer slot.
Unlike `c_span`, it does not add a count argument. Selectors are quoted closed
references: `"2"` names a complete physical input, `"2.width"` names an exact
integer C-record member, and `"observer.width"` names an observer output member.
Positions include the receiver, factory output and expanded input groups.
An output slot, data pointer itself, partial span/callback group, arbitrary C
expression, float or unverified member cannot be used as an extent.

```xl
extern "surfaceUpdateRGBA" @c_field_call @no_raise
    @c_region(3,"read","2.width","2.height","4",4)
    func update(Surface surface,RectI rect,bytes pixels,c_int stride) as c_int

extern "surfaceReadRGBA" @c_field_call @no_raise @c_result("status",0)
    @c_region(2,"write","observer.width","observer.height","3",4)
    @c_region_observer(2,surfaceDesc,0,1)
    func read(Surface surface,Buffer output,c_int stride)
```

Read regions accept `bytes` or the actual managed `Buffer` resource. Write
regions accept only `Buffer`; they never write through a language byte value.
An optional final `"nullable"` is accepted for read `bytes?` or `Buffer?`, where
object absence supplies native NULL. A non-null empty Buffer/bytes value still
obeys the complete capacity check; NULL data inside an empty value is not
object absence. Write regions remain non-null. A matching create overload may
default its `bytes?` input to null; its `Buffer?` overload requires that argument
explicitly, avoiding an omitted-argument ambiguity. Bare null follows normal
overload rules and may need a typed nullable variable.
Strings and raw points are not Region storage carriers.

Width and height must be positive. Effective stride is the positive supplied
stride, or `width * unitBytes` when stride is zero or negative. The checked
extent is `(height - 1) * effectiveStride + width * unitBytes`: padding beyond
the final valid row is not required. Generated C rejects overflowing `size_t`
arithmetic, an undersized stride, and a default packed stride outside the
native stride type's range before calling the producer. Buffer bounds use
initialized logical size, including when capacity is larger. A write neither
grows nor changes length; failure may have modified a prefix, and length stays
unchanged. Existing scoped Buffer pinning protects the pointer for the call.

`c_region_observer` names a typed logical native method/function, not a string
to dispatch at run time. Its remaining arguments identify existing physical
inputs. It must be a nonrecursive no-raise single-record output producer with
compatible effects and exact input types/physical pointer qualification. The
query has no Region, callback or other hidden input expansion. Its native
status is checked before any output fields are read or any destination Buffer
is acquired. The same native input scopes remain active across query and main
call. An unavailable field, query failure, or allocation failure in reporting
that failure prevents the main call and leaves the destination unchanged.

Each observed Region adds one real native query per invocation. Other Region
work is checked integer arithmetic, a byte-value view or Buffer pin, and the
original direct native call. The compiler adds no content copy, per-method C
wrapper or pointer/count expansion. Existing view/lease/error bookkeeping is
still present and must be included in performance comparisons.

Sparse XLIR rows seal the physical input, exact logical storage, integer/member
identities and typed query edge. Reachability includes observer targets, so
their authoritative headers and field/function ABI proofs are emitted even
when no source expression directly calls the observer. Region presence shares
the closed input-mapping word with the independently counted callback-span
rows; missing/extra rows and invalid counts are rejected before C/header output.

## Multiple logical inputs for one native C target

Two logical overloads may bind the same native function or callable field only
when their complete physical, lifetime, output and effect contracts agree.
For Region data inputs, `bytes` and `Buffer` may provide the same
readonly pointer with identical extent selectors, units, stride and observer.
Matching `bytes?`/`Buffer?` forms use the same explicit nullable Region contract.
Their language types remain distinct; this does not enable nominal casts or
relax normal overload ambiguity rules. Constness, checked status, ownership,
native output shape, callback contracts and effects cannot differ between the
declarations. XLIR independently verifies this agreement, including coherent
but conflicting edits to one overload's otherwise valid Region contract.

## Readonly fixed C-array inputs

The `Vertex[4]` type suffix names the existing fixed C-array value type. The
extent is a static positive decimal integer up to `UINT32_MAX`; the element
must have a supported trivial scalar/C-record ABI. The earlier record-field
spelling `Vertex vertices[4]` retains the same AST/type/layout. Dynamic arrays,
nullable arrays/elements, nested C arrays and managed elements do not qualify
for this input contract.

```xl
extern "surfaceDrawQuad" @c_field_call @no_raise @c_ref(1)
    func drawQuad(Surface surface,SurfaceVertex[4] vertices,c_uint flags) as c_int
```

`c_ref` keeps its explicit-language-parameter index. For records it passes
`&value` as `const Record*`; for fixed arrays it passes `value.Elements` as
`const Element*`, with no pointer-to-array or count argument. Native code may
read the declared fixed elements only during the call and must not retain or
write them. The binding author must match the native API's documented fixed
cardinality: a C pointer signature alone cannot express that requirement.

Direct, bound, function-value and protocol calls share the same sealed
aggregate-borrow ABI. The type pool closes element type, extent and native
record layouts; native header proof checks the exact const element pointer.
There is no C forwarding wrapper, additional heap allocation or added content
copy in this projection. Ordinary language fixed-array assignment/value
semantics and callable preparation costs still apply.

## Fixed C-array output values

`c_output` and `c_outputs` also accept the existing inline fixed C-array
value type. The same positive static extent and trivial element rules apply;
record elements must describe the exact native record, without `c_projection`.
Dynamic, nullable, nested, pointer-element and managed-element arrays remain
excluded. This does not enable raw by-value C array returns.

```xl
extern "nativeGetCorners" @header("native.h") @no_raise
    @c_output(1,0) func corners(c_float tolerance) as Vertex[4]
```

The corresponding physical signature is `int(float, NativeVertex*)`, where
`Vertex` names `NativeVertex`. Cgen reserves the complete zero-initialized
inline value and passes `native_raw.Elements`. A mixed multi-output result
uses `native_raw.eN.Elements` for each array slot; scalar/record slots keep
their ordinary addresses. There is one native call and no dynamic Array,
Buffer, forwarding function or array-allocation adapter. Native code must
write no more than the declared extent and must not retain the output pointer.
The binding author must match the documented cardinality: C header proof can
check element type, pointer depth and mutability, but cannot infer an element
count from a pointer signature.

Status/producer/callback validation happens before committing any result;
failure leaves the destination language value unchanged even if the native
function has already written its temporary output. Ordinary fixed-array
value copies, zero initialization, callable preparation and managed receiver
lifetime/error checks still apply. Direct calls, function values, bound
methods, native function-pointer fields and protocol dispatch use the same
verified output contract. Sealed XLIR closes extent, element, layout, physical
output slots and status ABI before C or header bytes are emitted.

## Owned and callback-epoch variants of a native input

A same-target overload may also accept either an owned-root view or a
callback-epoch view of the exact same native pointee, qualifier and header.
Both inputs must use `BORROW` with the sealed `FOREIGN_VIEW_BORROW` physical
ABI. Each declaration keeps its own verified lifetime/policy/scope: the owned
view establishes its ordinary call scope, and the epoch view performs its
active-epoch check. Expired values fail before the producer is called.

This is input compatibility for a single native call, rather than an implicit
language conversion or a shared owning root. Raw caller pointers are excluded.
Result/producer ownership, effects, other argument mappings and authoritative
ABI contracts still have to agree. Ordinary overload resolution selects the
exact logical input type. This adds no forwarding C method or new scope kind.

## Borrowed native references in output tuples

`@c_output_retain(physical, retain, "zero"|"positive", rollback)` composes with
`c_output` or `c_outputs` and uses the same helper signatures and native success
rules as `c_retain`. The selected logical result must be an independent owned
foreign class, optionally nullable, with an exact authoritative header and
policy. Watch-based roots and caller/owner/callback views are not
accepted. Retain takes the exact non-null class and returns native `int`;
rollback takes that class and is logical void, with an optional checked native
status. Both helpers must be `no_raise` declarations without other adaptation.

An owned class may declare a validated `@scope("NativeScope")`. Scalar
`retained_view`, a single retained output, and retained tuple outputs all follow
this rule. The retained class keeps its normal input CallBegin/CallEnd and Busy
contract; acquiring the independent owning result does not invent a borrowed
lifetime or copy its payload. A scope does not make a watch-based root eligible.

The native producer receives its original `T**` output, with a separate raw
`T*` slot. All owning result roots are reserved before the producer; a failed
reservation skips the producer. After it succeeds, each non-null result acquires
one native reference and materializes its reserved language root without another
allocation, before native input scopes end. A nullable NULL
result does not invoke Retain. Required NULL is a checked error. Text/bytes
conversions can appear in the same tuple. No tuple is delivered until every
result has been admitted; a later failure releases all earlier acquired results
in reverse order and preserves the first failure even without an allocated
diagnostic. Rollback failures cannot replace that primary outcome.

This contract adds one native Retain and one ordinary root admission per
non-null owning output. It neither repeats the producer/getter nor copies the
native resource payload. Typed sparse output rows carry the physical slot,
logical type, ordinal and exact helper symbols. Same-target overloads must
preserve those rows and their effects; emission consumes the sealed IR.

## Synchronous native descriptor packs

`@c_pack(physical,"NativeRecord",first,count)` replaces a contiguous group of
explicit logical parameters with one `const NativeRecord*` at the selected
physical position. Receiver indices are included in physical positions; source
parameter indices exclude the receiver, as with `c_span`. Ordinary named and
default arguments remain language arguments and are evaluated once. Multiple
packs, interleaved outputs, callable fields, bound methods and protocols use the
same sealed physical-step plan.

Each `@c_pack_field(physical,"field",source,"mode")` assigns one exact native
field. `value` accepts an exact C scalar; `view_raw` borrows a checked managed
native view; `c_string` prepares checked terminated text. Paired `text_data` /
`text_size` fields borrow UTF-8 with its exact byte length, including NUL; the
size mode requires a final `"int"` or `"size_t"`. Paired `span_data` /
`span_count` fields use an existing readonly `c_span` contract, including its
element and count ABI. Nullable text distinguishes NULL from non-NULL empty
text. Nullable views preserve NULL. `int` text lengths are checked before the
native call. `@c_pack_size(physical,"size","uint32_t")` initializes an exact
unsigned size field with `sizeof(NativeRecord)` and proves representability.

Paired `callback` / `callback_user` fields use the same logical source and an
existing `@c_sync_callback` contract. The function field must exactly match the
generated callback typedef; the userdata field must be `void*`. A nullable
closure writes NULL to both fields. Nested record paths and multiple independent
callback parameters use the same mapping; retained/escaping callbacks are not
allowed. See [packed synchronous callbacks](foreign-packed-callbacks.md) for
the declaration, failure ordering and qualification boundaries.

The native record is stack storage with zero-initialized unspecified fields.
It is a physical schema rather than a new managed record or public pointer.
The authoritative header proves a complete tagged struct and the exact type
of every mapped field. All logical sources must be covered; groups, native
fields and mappings must be disjoint. Writable spans, arbitrary expressions,
unions, nested managed records, arbitrary function-pointer fields, variadic functions
and escaping descriptor/data pointers are excluded. Pack and Region input
schemas cannot currently coexist in one declaration. A declaration uses
`no_raise` for its native producer; checked preparation failures still prevent
that producer from running.

The binding author must audit that the native implementation neither retains
the descriptor nor its borrowed memory. Managed native resources remain pinned
until after the call and result admission; native code that stores a resource
must acquire its own declared owning reference. For custom providers this is
part of the provider's explicit native contract. There is no descriptor heap
allocation, dynamic field lookup or forwarding C method. Costs are stack
initialization, scalar/pointer field stores, bounds checks and the preparation
already required for each input. Terminated-text validation and existing native
view/span scopes retain their normal costs. A packed logical function cannot
be passed as an unadapted flat C callback address.

## Readonly typed span source overloads

One native function or callable field may declare both `bytes` and `Buffer`
inputs for the same readonly `c_span`. The exact element type, pointer
qualifier, count ABI, native header, output, lifetime and effect contracts must
agree. Each source keeps its own verified preparation and pin; this does not
convert a Buffer into bytes, copy its contents or relax nominal casts. Writable
and nullable spans, strings, C records and fixed arrays are excluded from this
source substitution.

When the span feeds a descriptor pack, only its matching `span_data` and
`span_count` field rows may carry the corresponding different logical source
type. Physical field names/types, parameter positions and the whole descriptor
schema remain identical. XLIR checks this agreement independently for direct,
bound, protocol and global calls; invalid or conflicting safety metadata fails
before C/header emission. Ordinary overload resolution still selects an exact
logical source type. The bytes overload alone may supply an empty default;
the Buffer overload requires an explicit argument.

## Owning factory reservation and failed construction

Before an owning native producer or Retain can transfer a reference, the
runtime reserves its root control, code lease, directory entry and cleanup
storage. The producer runs only after reservation succeeds and the current
checked scope is healthy. Observe takes responsibility for every transferred
non-NULL owner even if the producer, Retain or a callback has already failed.
Successful materialization transfers that reservation to the language value
without allocating again. A failed construction retires the reserved owner
through the existing checked close queue; a non-consuming Busy result keeps
the native resource and its code lease available for retry.

Compiler-allocated `c_inplace` storage is owned by the reservation immediately
after allocation. Its policy must provide `DestroyUnadopted`: this cold hook
consumes the complete original record responsibility only when it returns
true. It must handle zero/partially initialized records, preserve storage on a
non-consuming failure, and account for any remaining native references.
Trace/Busy must cover that phase and its actual owning dependencies. Native
code must own the dependencies needed for deferred partial cleanup rather
than retain this call's borrowed text/view pointers. There is no unconditional
record free after a failed cleanup. An explicit native adopter switches to
normal destruction as soon as its native success status is reported, including
when a checked callback error is also present. Plain constructors switch only
after successful checked completion.

Owned policies with a physical Watch declare
`XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH` only after their native contract proves
that a valid one-owner output can always attach its observer. Watch's bool
reports physical attachment; a checked rejection uses the separate failure
channel while keeping observation installed. Constructor/Retain failure does
not mean that the owner was consumed. Borrowed child/root aliases retain
their existing observation and parent-ownership rules.

This moves the existing admission allocations before ownership transfer and
can fail before invoking a native producer. It can also allocate a reservation
for a producer that ultimately fails or returns nullable NULL. Materialization
adds no second root allocation. These cold lifetime operations do not add a C
forwarding method to ordinary UI or game calls.

## Borrowed native record outputs

One native output struct can project directly to a logical result or tuple.
The physical struct remains stack storage; it is not exposed as a language
record containing unmanaged resource pointers.

```text
extern "nativeGetPatch" @no_raise
    @c_output_record(1,"NativePatch","borrowed") @c_status(0)
    @c_output_field(0,"surface","retain",retainSurface,"zero",releaseSurface)
    @c_output_field(1,"source") @c_output_field(2,"mode")
    func getPatch() as (Surface?,Rect,c_int)
```

The output slot counts physical C arguments, including the native receiver.
Each `c_output_field` ordinal names one logical result and one direct C field
identifier. Every result must have exactly one mapping. The record must be a
complete tagged C struct defined by the authoritative header, with a trivial,
standard-layout C++ counterpart. C and C++ ABI
proofs check the exact producer signature and every mapped field's type.
`c_status(successCode)` denotes a native `int` status; omitting it denotes a
native `void` producer.

This contract is explicitly **borrowed**: the native output record itself
carries no producer-owned cleanup responsibility. Supported field projections
are trivial C scalars/records and independent owned native resources with
typed Retain/rollback helpers. Nullable resource fields preserve NULL and do
not Retain it. A non-nullable resource field rejects NULL. Raw pointer fields
without Retain, text/bytes conversions, nullable POD, unions, private fields,
nested field expressions and `c_output_view` on the same group
are rejected. Binding authors must audit the actual native lifetime; the
attribute cannot turn an unowned C pointer into a stable resource.

For size-prefixed physical output structs, compose
`@c_output_record(1,"NativeInfo","borrowed") @c_output_size(1,"iSize")`.
The size annotation names that same physical output slot, including the
receiver, rather than a logical tuple field. The compiler zero-initializes
the stack struct, assigns `raw.iSize = sizeof(raw)`, and invokes the producer
once. C/C++ header proofs require a complete trivial tagged struct, an unsigned
C integer member, and a member large enough to represent `sizeof(raw)`.
The sparse size row has ordinal zero and no logical record/field type identity:
those identities belong to the physical group and are proved against its
authoritative header. XLIR rejects a different slot, duplicate rows, nonzero
logical identities, a missing presence bit and disagreement between native
aliases. A failed status never projects or Retains fields, even if the native
producer left poison pointers in the output. This composition adds one stack
field assignment and no heap allocation or forwarding function.

All retained result roots are reserved before the native producer. Failure
to reserve any root prevents the call. The producer runs once, then each
resource is retained and admitted before input scopes end. Any status, Retain,
callback or admission failure releases previously acquired results and delivers
no partial tuple. The explicit failed bit preserves a first error even when
an error object cannot be allocated. Declarations of the same native target
must agree on the complete field, resource, effect and physical ABI contract.
XLIR verifies these sparse rows before emitting any C or public header.

Costs are zero-initialized stack record storage, field-to-result stores and
the normal checked scopes. Resource projections additionally use the existing
root reservation, one declared Retain per non-NULL resource, and lifetime
bookkeeping. POD projection performs no heap allocation or dynamic dispatch.
No forwarding C function, second getter call, runtime field-name lookup or
per-method marshalling wrapper is generated.

Descriptor-pack `value` fields accept the same pointer-free POD domain:
native C scalars and complete C records containing only those values,
recursively. Every record keeps its authoritative header, exact native field
types and layout proofs; nested native counterparts must also be tagged
structs and trivial/standard-layout in C++. Nullable records, unions, fixed
arrays, raw pointers, native resource handles and managed fields are excluded
from this value projection. Resource handles continue to require `view_raw`
and their separate native owning contract. A POD input is assigned by value
into the stack descriptor; it creates no heap object, runtime conversion,
reference alias, extra native call or forwarding C function.

### Nested descriptor packs and fixed physical NULL arguments

Descriptor packing accepts closed member paths with at most eight identifier
components. Each intermediate member needs its own exact native struct witness:

```xlang
@c_pack(1,"EditorDesc",0,2)
@c_pack_record(1,"view","ViewDesc")
@c_pack_record(1,"view.renderer","RendererDesc")
@c_pack_size(1,"size","uint32_t")
@c_pack_size(1,"view.size","uint32_t","view")
@c_pack_field(1,"view.document",0,"view_raw")
@c_pack_field(1,"view.renderer.zoom",1,"value")
```

`c_pack_record` is a compile-time C-layout witness, not a language record or
an owning object. Header proofs require a complete actual tagged struct and
the exact containing-field type; C++ additionally requires trivial, standard
layout. Pointer and union intermediate members are rejected. The optional
fourth `c_pack_size` argument must be precisely the containing registered
record path; it selects that record's `sizeof`, rather than the outer size.
All field paths, parent-row identities and types are sealed in IR. Calls
construct one zero-initialized outer stack descriptor; nested packing adds no
heap allocation, native forwarding function or dynamic member lookup. Existing
text validation, resource pinning and error/cleanup costs still apply.

`@c_null(physicalIndex,"const NativeType*")` inserts an exact typed NULL into
one physical C pointer slot without adding a logical argument. It is useful
when a native API explicitly allows an optional descriptor or output pointer
to be absent. It requires an authoritative header and `@no_raise`; the emitted
function-pointer proof includes this pointer type. It cannot replace a
receiver, another output/descriptor, or one half of a span/callback group.
Duplicate, unreachable, malformed and nonpointer slots are rejected. It does
not make native dereferencing of NULL safe, erase an output ownership duty,
or relax compatibility between different logical declarations of one symbol.

## 扩展中的受管 TCP 同步调用

语言声明 `xl6ForeignTcpOwnerV1(TcpStream) as var` 取得真实拥有令牌。
C 提供方包含 `xl_foreign_net.h`，使用 `xl6ForeignTcpVisitV1(owner,visit,data)`。
运行时检查精确策略，固定拥有者并进入原生调用作用域；回调只能在本次同步调用内
使用 xnetstream 指针，不能保存它。包的 `artifact.runtimeSymbols` 显式登记 C 使用的
驻留入口。普通原生错误继续走现有 checked 边界；额外调用、引用和作用域成本应计入评估。

### Complete Linux checked C integer cohort

The complete Linux r22 O2 compiler, TinyCC, XRT, fixed objects, VFS and embedded
source now pass the checked native integer qualification in CLI JIT, native
executables, actual nested executable/shared hosts and checked retirement.
The separate continuation follow-up covers async CLI/native and a generator
body in a real shared caller. It does not add async multi-return or await in
a synchronous export. The immutable index is
`dev/release-materials/provenance/checked-c-integer-linux-20261112.json`.
Owned UTF-8 output and the current Document allocator changes are later
candidates; this qualification does not certify them or final SDK/IDE delivery.
