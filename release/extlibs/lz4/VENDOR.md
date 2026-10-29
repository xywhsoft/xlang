# LZ4 依赖来源

标准帧复用 LZ4F，不手写帧头、校验或解码状态机。`lz4_frame.c` 和 `lz4_hash.c`
仅给内部 xxHash 符号添加 `XL6_LZ4_` 前缀，避免与 xxhash 扩展库碰撞。
上游 LZ4F 编译单元也引用 HC 入口，因此同时编入 lz4hc.c；公开 API 仍只提供 acceleration。

## 本次引入的文件

以下文件取自官方 `lz4/lz4` 的 `v1.10.0/lib/`，SHA-256 如下。
原始地址形式：`https://raw.githubusercontent.com/lz4/lz4/v1.10.0/lib/<文件名>`。

| 文件 | SHA-256 |
| --- | --- |
| lz4frame.c | 44F421BEA199C7F11DA263C717F063228CD2C8C05A8384D327B49CC81CCFBAC4 |
| lz4frame.h | B845DB4B7EE1BFA64B8F641A94F62E7F636A8B5D673CC61766413782283AAAD1 |
| lz4hc.c | 126CAFAFDB91767E6E55238298A910903851B35B2CEE27CE80AE2280469EE232 |
| lz4hc.h | E43824E8A9BA16F54100C4CCBCCFA5782A858CA9AB83C48AAC303FEA3E76E21E |
| xxhash.c | B667033DC735FB5EA5648E0A61A2E065E5EF5BBDA53669730063BD856C643C48 |
| xxhash.h | AEFDD236F35130495C18764CABED3F7B216906855FC5E6A9025CD2040BC84444 |
| LICENSE | 8B58C446121A109CCF32EDC094BBA3010A3D85E4EE3702950DB55E4D3E87736C |

这些文件保留原文，适配在外部编译单元和公共实现中完成。
许可证以 [上游许可](third_party/LICENSE) 和各文件头为准；各文件中的著作权声明保留。

## 保留的基础文件

仓库原有 lz4.c/h 声明版本 1.10.0，但并非 v1.10.0 标签逐字一致的文件，包含后续补丁。
本次没有覆盖它们，也不声称已经确认其精确上游提交；后续供应链收口仍需补足来源。

| 文件 | 当前 SHA-256 |
| --- | --- |
| lz4.c | 9471935CE68A41A83D94436E2F232370209C63CF3DCDCF118D7F2541F5F502EA |
| lz4.h | E2337DE0ECF6AD9C168E1DDD45E9FCDDB9475E6D04D53E0285626FC871A71219 |

两份文件与新增帧实现的组合已经过限定互操作和 sanitizer 检查；运行测试不等于来源证明。
独立参照使用 WSL 系统 liblz4.so.1，不用本包再编译的第二份库充当独立实现。
验证脚本保存参照库版本、库哈希、驱动哈希及输入，便于再次复核。
