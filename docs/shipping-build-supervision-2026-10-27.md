# 常规源码构建的完成判定

Windows / Linux 的源码验收工具不再以统一的 1800 秒墙钟上限终止构建。
调用方持续监督原来的构建进程，保留同一日志和会话，最终只接受实际退出 0、
所有输入哈希不变以及 CLI、两个动态库、源码 TCC / XRT 和两套 VFS 的完整回执。
此调整不修改功能用例的 600 秒期限，也不允许以现存产物替代构建退出回执。

Windows r3 因旧上限终止，证据继续保留为失败。r4 从同一份未修改的
freeze-r3 源码在 E 盘全新目录构建，没有复用旧产品或外部提供者，实际退出 0。
Linux r3 的常规源码构建同样实际退出 0。两个回执均经独立 reader 重新核对
全部输入与七个产物；这是构建来源证明，不代表标准库联合验收已完成。

证据根：`F:/xlang-validation/five-groups-20261027/p7-shipping/`；
Windows 实际构建和回执位于同名 E 盘目录的 `build-win-r4`，Linux 位于
`/home/ubuntu/xlang-validation/20261027-p7-shipping/build-linux-r3`。
