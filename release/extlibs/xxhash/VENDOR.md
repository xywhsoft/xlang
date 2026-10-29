# xxHash 来源

- 上游：[Cyan4973/xxHash](https://github.com/Cyan4973/xxHash)。
- 固定 tag：`v0.8.3`，commit：`e626a72bc2321cd320e953a0ccf1584cad60f363`。
- [xxhash.h](https://github.com/Cyan4973/xxHash/blob/e626a72bc2321cd320e953a0ccf1584cad60f363/xxhash.h)
  为该提交原始文件，没有修改算法或声明。
- `third_party/xxhash.c` 是实例化入口，额外指定标量 XXH3 并包含独立的 TCC 适配头。
- `include/xrt_xxhash_tcc_compat.h` 仅对 TCC 定义缺少的乘法 intrinsic；
  不依赖 `_M_X64` 提前出现，因为 TCC 的 `stdlib.h` 也会定义目标宏。
  核心 C 测试先包含适配再包含运行库，防止包含顺序回退；低位拼接不重复累加
  进位，以边界常量和可用时的 128 位编译器算术差分验证。
- 许可证为包根 [LICENSE](LICENSE) 中的 BSD-2-Clause。
- 文件哈希见 [third_party/manifest.json](third_party/manifest.json)，构建不联网、不自动升级。

`test/vectors.json` 的 124 项数值取自同一提交的
[sanity_test_vectors.h](https://github.com/Cyan4973/xxHash/blob/e626a72bc2321cd320e953a0ccf1584cad60f363/tests/sanity_test_vectors.h)。
每种算法使用 31 个长度、两个种子，覆盖空值、小输入、块边界及 4096 字节输入。
输入字节生成规则记录在 JSON，`vectors.h` 是机械转换的数据表，不由待测实现计算期望值。
Windows 门禁逐项核对 JSON/头文件一致性；C 测试逐项消费这些答案。

本包核心状态直接内嵌 XXH64_state_t，不调用上游 createState 的 CRT 分配器。
这样 XRT 的所有权、分配故障注入和 C/xlang 生命周期一致，不需要另造内存系统。
哈希不是密码学完整性校验，测试通过不构成安全或吞吐性能认证。
