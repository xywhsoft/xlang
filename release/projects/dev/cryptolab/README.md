# cryptolab - AES-128/192/256 (ECB/CBC/CTR/GCM) from scratch

从零实现的 AES 加密工具，不使用 crypto 标准库的 AES，全部手写。

## 用法

```
cryptolab encrypt ecb  <key_hex> <data_hex>
cryptolab decrypt ecb  <key_hex> <data_hex>
cryptolab encrypt cbc  <key_hex> <iv_hex>  <data_hex>
cryptolab decrypt cbc  <key_hex> <iv_hex>  <data_hex>
cryptolab encrypt ctr  <key_hex> <iv_hex>  <data_hex>
cryptolab decrypt ctr  <key_hex> <iv_hex>  <data_hex>
cryptolab encrypt gcm  <key_hex> <iv_hex>  <plaintext_hex> [aad_hex]
cryptolab decrypt gcm  <key_hex> <iv_hex>  <ct_hex> <tag_hex> [aad_hex]
cryptolab test                              FIPS-197 + NIST GCM 测试向量
cryptolab sbox                              打印 S-Box / InvS-Box
cryptolab -h | --help                       帮助
```

密钥长度：16 字节（AES-128）/ 24（AES-192）/ 32（AES-256）。
GCM 的 tag 固定 128 位，IV 建议 12 字节（96 位）。
CBC/ECB 使用 PKCS#7 填充；CTR/GCM 为流模式（任意长度）。

## 实现要点

- **GF(2^8) 算术**：`gmul8`（俄罗斯农夫算法，AES 不可约多项式 0x11B）、`mul2`/`mul3`。
- **S-Box 生成**：对每个字节先求 GF(2^8) 乘法逆元（暴力搜索），再做仿射变换
  （`inv ^ rotl(inv,1) ^ rotl(inv,2) ^ rotl(inv,3) ^ rotl(inv,4) ^ 0x63`）。
  InvS-Box 由 `invSbox[sbox[i]] = i` 反向构建。
- **密钥扩展**：完整实现 FIPS-197 KeyExpansion（含 AES-256 的 SubWord 步骤和 Rcon 表）。
- **块变换**：AddRoundKey / SubBytes / ShiftRows / MixColumns 及其逆变换，列优先排列状态。
- **模式**：
  - ECB：逐块独立加密
  - CBC：PKCS#7 填充 + 链式 XOR
  - CTR：16 字节大端计数器全量递增
  - GCM：GHASH（GF(2^128) 逐位乘法 + 归约多项式 0xE1000...）+ GCTR（inc32）+ 128 位认证标签
- **数据结构**：所有字节用 `array<int>`（每元素 0..255）存储。大量位运算（`& | ^ << >>`）、
  查表、array 大循环——压测 xlang 在加密计算工作负载下的能力。

## 验证

### FIPS-197 单块测试向量

| 规格 | 密钥 | 明文 | 密文 |
|---|---|---|---|
| AES-128 (B) | `2b7e...4f3c` | `3243...0734` | `3925...0b32` |
| AES-128 (C.1) | `0001...0e0f` | `0011...eeff` | `69c4...c55a` |
| AES-192 (C.2) | `0001...1617` | `0011...eeff` | `dda9...7191` |
| AES-256 (C.3) | `0001...1e1f` | `0011...eeff` | `8ea2...6089` |

### NIST GCM 测试向量（McGrew & Viega）

| 测试 | 规格 | 密文 | 标签 |
|---|---|---|---|
| TC1 | AES-128, 空 | (空) | `58e2...455a` |
| TC2 | AES-128, 1 块 | `0388...fe78` | `ab6e...bddf` |
| TC3 | AES-128, 60B | `4283...e091` | `cc15...ac85` |
| TC4 | AES-128, 60B+AAD | `4283...e091` | `5bc9...1a47` |
| TC13 | AES-256, 空 | (空) | `530f...738b` |
| TC14 | AES-256, 1 块 | `cea7...9d18` | `d0d1...b919` |
| TC15 | AES-256, 60B | `522d...f662` | `eb9f...6f4f` |
| TC16 | AES-256, 60B+AAD | `522d...f662` | `76fc...551b` |

全部 16 项测试通过，EXIT=0。

## 架构说明

所有核心函数为**顶层独立函数**（非 class 方法），遵循 chip8/merkletree 的模式以规避
BUG-C1（class 方法不能安全接收 `array<int>` 参数）。函数接收 `array<int>` 参数（值拷贝，
读取安全），返回新的 `array<int>`。

多值返回的次返回值通过全局变量传递（`g_nr` / `g_tag` / `g_auth`），因为 xlang 的多值返回
会丢弃 `array<int>`（BUG-CL1，详见上层 `../BUGS.md`）。
