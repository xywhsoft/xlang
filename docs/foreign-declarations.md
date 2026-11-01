# 声明式 C 互操作

本功能沿用 AST → XLIR → C。声明中的 C 身份、物理表示和生命周期先成为
类型／调用合同；Cgen 不根据包名或方法名猜测转换，也不生成库专用转发函数。
普通 class 及既有 native-backed class 的对象语义不变。

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

同一 getter 的局部机器码对照中，GCC O2 与手写 C 均为一个尾跳转；TCC 仍有
通用 SSA 临时变量的额外栈读写（5 条指令、19 字节、16 字节栈空间）。这不构成
整个绑定或所有编译方式的零开销结论；后续需在通用 IR 值放置层消除这部分工作。

## 按值 C 记录

```xlang
extern "xui_rect_t" struct Rect @header("xui.h")
    c_int x
    c_int y
    c_int width
    c_int height
end

Rect rect = Rect()
rect.width = 640
Rect copy = rect
copy.width = 320
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
工厂函数值及可能失败的参数清理尚未闭合，因此这些用法明确拒绝。受管拥有、
上下文和回调环境是后续步骤；本节不代表完整 XGE/XUI 迁移已完成。

`tools/check_foreign_factories.py` 覆盖实际 C 提供方、相对模块导入及私有回滚
名称重写，验证 GCC O0/O2、TCC JIT／模块／独立 EXE 和完整状态码范围。
其 IR 破坏性测试要求状态比较、非空分支、回滚身份、入口及记录地址被修改后，
验证器在输出任何 C/header 前拒绝。
