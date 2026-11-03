# 声明式 C 互操作

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
插入 output。普通参数序号仍按显式语言参数计算，不包含接收者。

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
