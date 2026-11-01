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

声明必须提供一个合法的 C 类型标识符、可移植的相对头文件路径和显式生命周期。
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

本节说明当前已接通的原生视图基础，不代表 C record、拥有构造、回调环境、
XUI 生命周期或整个 XGE/XUI 包迁移已经完成；这些能力按相同合同逐项实现。
