# XUI Proxy 与资源拥有合同的设计记录

本文保留实施前的审计和设计决策。现行接口及其验收以 [XGE／XUI 原生能力映射](xge3-xui3-api-map.md) 为准。

这是原生接口审计发现的新增前置，不属于已经通过验收的声明覆盖。
用户已批准补齐原生共享拥有协议；以下原实现事实用于说明改动的必要性。
新的原生资源、Context／Widget 槽和后端能力分别保留真实引用，编译器直接调用函数指针字段。

## 原实现审计事实

- `include/xui.h` 的 `xui_proxy_t` 用函数指针字段提供 Font、Surface、绘制等能力。
  `fontLoadFile`／`fontDestroy` 不是可链接的全局 C 符号。
- `src/xui/core/xui_proxy_xge.c` 的 `xuiProxyXge()` 返回按值函数表。
  Context 复制该表；不能把它误当成全局 singleton 指针。
- `xui_font_t` 是独立后端 wrapper，并非 `xge_font_t` 的类型别名。
  当前 XGE 代理的 paint pins 只保护 paint 使用期，没有通用 Font AddRef 合同。
- `src/xui/core/xui_core.c` 的 SetDefaultFont／RegisterFont 保存裸指针，
  ClearFonts 只释放注册项和名称；Context 不负责销毁这些 Font。
  多个 Widget 的 SetFont 同样只保存裸指针。
- 自定义 proxy 的销毁函数可以使用 `pUser` 和表中其他字段。
  不能因为默认 XGE 实现忽略 proxy 参数，就让通用资源销毁丢弃它。

因此，单纯导出一个 owned Font，再在 setter 返回时释放调用参数，会给 Widget
留下悬空字体；把 GetDefaultFont 标成 Context owner view 也不能修复它。

## 建议的语义

原生层补齐真实共享拥有协议，语言层继续保持声明式调用：

1. Font／Surface 的语言 owning 引用、Context 注册槽、Widget 槽和临时 paint
   使用都必须有真实 Native retain／release；不把裸指针当成拥有引用。
2. setter 先取得新引用，失败不改槽；成功发布后再释放旧引用。清理失败保持独立
   状态，不能覆盖更早错误，也不能二次释放已经消耗的引用。
3. `resource.close()` 消耗这个语言 owner；其他真实拥有者继续使用资源。
   Context 关闭释放自身拥有的槽，不能销毁其他变量或 Context 仍持有的资源。
4. 资源保留不可变的后端能力与其实际 userdata owner，确保最后销毁时仍能调用
   正确的 proxy。没有真实 userdata／代码寿命合同的自定义 C 后端只进入显式 C
   互操，不伪装成安全语言对象。
5. 涉及语言引用的原生 owning slot 必须 Trace；独立 RC 节点保持独立 Count／
   admission。不能借用 xvalue 外壳来绕过真实拥有图。

具体 Native ABI 在开发前单独确定。目标是保留必要的后端能力，而不是每个资源
复制一整份 proxy，也不是引入逐函数 C 转发层。

## 编译器接入

补充通用 callable-field 合同。权威头检查函数指针字段及完整签名，IR 明确记录
callee 字段与物理参数，Cgen 直接生成 `proxy->field(proxy, ...)`。
缺失的可选字段 checked 报不支持；不生成操作码分派或逐方法包装。
已有 span、status/output、拥有结果、效果和回调合同继续复用。

默认后端、冷初始化、销毁和绘制热路径分别报告复制、分配、引用、间接调用及
同步成本。直接声明不等于已经证明整套绑定零开销。

## 可选的范围收缩

如果暂不升级原生资源拥有协议，这些接口只能保留为显式 C 互操，并明确列为
安全 OOP 接入缺口；不能把它们计入本轮“完整接入”。其他已获安全拥有合同的
XGE／XUI 声明、回调与验收可以独立继续。
