# XRT Windows 进程树收尾覆盖层

匹配 SDK 与冻结编译器的 `__xrtProcessStop` 源函数相同。原实现遇到已退出的根进程就返回，Windows Job 中仍存活的子进程会继续持有输出管道。

此源补丁仅让 Windows 的 `KILL_TREE` 在根退出后继续调用已有平台 Job 清理，不改变已发布的根退出快照。其他停止种类和 POSIX 行为保留，避免对已回收的 POSIX 组标识增加新操作。

`process_stop.before.inc` 来自已核对的共享源码函数，`process_stop.inc` 是源修复，`process_stop.patch` 供审阅。`tools/xrt_process_tree_overlay.py` 只接受两个已审计的完整 XRT 输入摘要，将该源函数替换生成新的独立头，并证明其他字节未改。没有手工修改安装头、上游工作树或冻结编译器。待上游合入并重新审计资产后，移除此覆盖层。

GUI 仍由匹配的 `xge.dll` 持有 XRT；外部语言服务单独使用编译器 XRT 实现，分别从对应基线生成，不能在一个进程混用。公开声明、类型布局和导出应保持不变；两个运行配置都需执行根已退出／子进程仍持管道的回归检查。
