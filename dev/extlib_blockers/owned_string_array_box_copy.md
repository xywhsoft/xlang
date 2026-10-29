# 拥有型字符串临时值进入列表时仍复制

## 定性

2026-10-28 的通用性能边界，不是结果错误，不增加普通包发布矩阵的
M/R 计数。INI 的节名/键名枚举已通过内容、快照和 OOM 恢复验证，
但不能把原生接管或 `xlir_string_copy` 点数减少解释成整条路径不复制。
本任务不修改主线、不替换 XRT，也不在每个扩展包新增装箱适配层。

## 最小复现与生成证据

从 `demo6` 目录运行同名 `.xl`：

```powershell
release\xl.exe run dev\extlib_blockers\owned_string_array_box_copy.xl
release\xl.exe cgen dev\extlib_blockers\owned_string_array_box_copy.xl --emit build\test\owned_string_array_box_copy.c
```

当前 Windows xl.exe SHA256 为
`CCF98FF05BAAA71C1827EAA0C8B7A011A9A17EAB1AECD593CD89848D227C5A2B`。
程序输出 `owned string array boxing: PASS`，退出 0；结果及输入重绑后的
快照语义正确。生成 C 将 `xrtStrDup` 的返回指针作为拥有型结果接管，
随后仍调用 `xrtValueString(view)`，再释放原字符串。XRT 的该 API
明确复制字节，因而直接调用临时值也不能消除此处的正文副本。
这项最小复现只记录该 Windows JIT/cgen；不据此声称其他版本或架构
相同。INI 同组操作的两平台包门禁另有独立验收记录。

INI 试过直接 `result.append(ownedCall() ?? raiseError())`：Windows
固定配置的节名/具名键/全局键预算仍为 17/18/13，与原可空局部写法
相同。该改写没有收益，已撤回，未留下额外错误辅助函数。

## 主线改进方向

当前 XRT 已有 `xrtValueStringTake(str*, size_t)`，可作为唯一拥有型
临时字符串的统一装箱入口；借用实参、别名和仍在使用的局部值仍应
保留快照复制，不能全局替换 `xrtValueString`。

需要由主线同时处理接管后的源槽清空、异常清理、OOM 时的所有权
回滚、容器追加失败及动态模块租约，避免优化变成悬空引用或双重
释放。不要只在 INI 修改一条生成路径。

Take 接口本身还会做槽位检查、创建 xvalue、执行 `xrtRealloc` 以保证
末尾零；必须测量当前分配器是否原地保持指针，不能因为 API 名称含
Take 就宣称任意输入零分配、零复制或整个操作零开销。INI 的原生
名称查询本身返回借用视图，其声明层为独立快照而做的首次复制也要
与这里的后续装箱副本分开计算。
