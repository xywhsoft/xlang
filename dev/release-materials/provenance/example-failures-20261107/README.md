# 范例失败修复验收

本轮单人完成指定的 44 个范例修复；Redis 排除。

- Windows：44 个 check/AOT 构建及运行通过，非图形 35 个 JIT 通过；9 个游戏另经 JIT/静态 AOT 三帧运行。
- Linux：35 个非图形范例通过；批量结果中的 Modbus LP64 问题修正后定点复测，保留原失败报告。
- 核心：两端各 3 项；CSV 四组 Windows JIT/AOT；Socks 合法最大长度两端 JIT/AOT；XGE 两种初始化入口 Windows JIT/AOT。
- 发布：本地 Windows/Linux 编译器、配套头文件和扩展源码已更新；IDE/XGE DLL 未在本轮重建；未执行外部发布。
- 边界：云端 live 范例仅验无凭据退出；游戏仅验三帧运行；未执行全项目回归或安全专项。

receipt.json 记录产品哈希和源码材料；summary.json 为聚合结论；evidence-index.json 索引原始证据。证据保留原报告字节及历史 scratch 路径；源码增量和验证依赖位于对应 sources 目录。
