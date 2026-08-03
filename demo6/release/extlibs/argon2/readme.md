# argon2 — 密码哈希（RFC 9106）

将用户密码转换为不可逆的安全哈希，用于凭证存储。基于 Argon2 算法（2015 年密码哈希竞赛冠军），具备内存硬特性，有效抵抗 GPU/ASIC 暴力破解。

对标 Python `argon2-cffi` 全功能实现。

---

## 典型应用场景

| 场景 | 说明 |
|------|------|
| 用户密码存储 | 注册时 hash，登录时 verify，数据库只存 PHC 编码 |
| 会话令牌派生 | 从主密钥 + 用户上下文派生唯一令牌 |
| 密钥拉伸 | 将短密码拉伸为高强度加密密钥 |
| 参数升级 | 安全策略更新后，检测并重新哈希旧密码 |
| 防篡改绑定 | 通过 ad 参数将哈希绑定到特定用户/租户 |

---

## 快速开始

```xlang
import argon2

// 注册：一行搞定（自动生成随机盐）
string stored = argon2.hash("user_password")

// 登录：一行验证
if argon2.verify("user_password", stored)
    println("欢迎回来!")
end
```

---

## 核心概念

### PHC 编码字符串

`hash()` 返回标准 PHC 格式，包含算法、版本、参数、盐、哈希的全部信息：

```
$argon2id$v=19$m=65536,t=3,p=4$c29tZXNhbHQ$RdescudvJCsgt3ub+b+dWRWJTmaaJObG
 ──┬─── ─┬─ ─────┬──────── ─────┬──── ─────────┬────────────────
  算法  版本    参数(m,t,p)    盐(base64)     哈希(base64)
```

存储到数据库时只需保存这一个字符串。验证时 `verify()` 自动解析全部信息。

### 三种变体

| 变体 | 常量 | 特点 | 适用 |
|------|------|------|------|
| Argon2d | `argon2.ARGON2D` | 数据依赖寻址，抗 GPU 最强 | 加密货币、无侧信道风险场景 |
| Argon2i | `argon2.ARGON2I` | 数据无关寻址，抗侧信道 | 高安全环境 |
| **Argon2id** | `argon2.ARGON2ID` | 混合（推荐） | **密码存储（默认）** |

### 参数含义

| 参数 | 说明 | 增大效果 |
|------|------|----------|
| `timeCost` | 迭代次数 | 计算时间线性增长 |
| `memoryCost` | 内存消耗（KiB） | 抗 GPU/ASIC 能力增强 |
| `parallelism` | 并行度 | 利用多核，同时影响哈希值 |
| `hashLen` | 输出长度（字节） | 通常 32 即可 |
| `saltLen` | 自动盐长度（字节） | 通常 16 即可 |

---

## API 参考

### 核心函数

| 函数 | 返回值 | 说明 |
|------|--------|------|
| `hash(password, ...)` | `string` | PHC 编码（自动盐） |
| `hashRaw(password, ...)` | `string` | 原始 hex 字符串 |
| `verify(password, encoded)` | `bool` | 验证密码 |
| `checkNeedsRehash(encoded, ...)` | `bool` | 参数是否需要升级 |

### hash / hashRaw 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `salt` | `""` | 空 = 自动生成 CSPRNG 随机盐 |
| `secret` | `""` | 服务器端密钥（pepper） |
| `ad` | `""` | 附加数据（绑定上下文） |
| `timeCost` | `3` | 迭代次数 |
| `memoryCost` | `65536` | 内存 KiB（64 MiB） |
| `parallelism` | `4` | 并行度 |
| `hashLen` | `32` | 输出字节数 |
| `saltLen` | `16` | 自动盐字节数 |
| `type` | `ARGON2ID` | 变体选择 |

### 参数提取

| 函数 | 说明 |
|------|------|
| `extractType(encoded)` | 变体类型（0/1/2） |
| `extractTimeCost(encoded)` | 迭代次数 |
| `extractMemoryCost(encoded)` | 内存 KiB |
| `extractParallelism(encoded)` | 并行度 |

### 辅助函数

| 函数 | 说明 |
|------|------|
| `generateSalt(len = 16)` | CSPRNG 随机盐（hex） |
| `recommendedTime()` | 推荐迭代次数（3） |
| `recommendedMemory()` | 推荐内存（65536） |
| `recommendedParallelism()` | 推荐并行度（4） |
| `recommendedHashLen()` | 推荐哈希长度（32） |
| `recommendedSaltLen()` | 推荐盐长度（16） |

### 常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `ARGON2D` / `ARGON2I` / `ARGON2ID` | 0/1/2 | 变体 |
| `PROFILE_LOW_MEMORY_*` | t=3, m=19456, p=1 | RFC 9106 低内存推荐 |
| `PROFILE_HIGH_MEMORY_*` | t=4, m=2097152, p=4 | RFC 9106 高内存推荐 |

---

## 由浅入深

### 第 1 步：存储与验证

```xlang
string stored = argon2.hash("password")
bool ok = argon2.verify("password", stored)
```

### 第 2 步：调节安全强度

```xlang
// 高安全：增大内存和时间
string stored = argon2.hash("password", timeCost = 5, memoryCost = 262144)
```

### 第 3 步：密钥哈希（防数据库泄露）

```xlang
// secret 存在配置/环境变量中，不进数据库
string stored = argon2.hash("password", secret = serverSecret)
```

### 第 4 步：参数升级

```xlang
// 用户登录成功后
if argon2.checkNeedsRehash(storedHash)
    storedHash = argon2.hash(password)  // 用新默认参数重新哈希
    // 更新数据库
end
```

### 第 5 步：审计与迁移

```xlang
// 检查数据库中旧哈希的参数
int t = argon2.extractTimeCost(oldHash)
int m = argon2.extractMemoryCost(oldHash)
if m < 65536
    // 标记为需要升级
end
```

---

## 安全最佳实践

### 参数选择（OWASP 2024 推荐）

| 场景 | timeCost | memoryCost | parallelism |
|------|----------|------------|-------------|
| 一般 Web 应用 | 3 | 65536 (64 MiB) | 4 |
| 高安全（金融） | 4 | 262144 (256 MiB) | 4 |
| 低内存环境 | 3 | 19456 (19 MiB) | 1 |

**原则**：在不影响用户体验的前提下，尽量增大 `memoryCost`。

### 盐的策略

- **永远不要**复用盐（每个密码独立随机盐）
- **永远不要**自己实现随机数生成（用 `hash()` 的自动盐或 `generateSalt()`）
- 盐不需要保密，它随哈希一起存储在 PHC 编码中

### secret（pepper）的策略

- secret 是服务器端密钥，存在环境变量或 HSM 中
- 不随哈希存储，数据库泄露后攻击者无法离线暴力破解
- 更换 secret 需要重新哈希所有密码

---

## 注意事项

### verify 不含 secret

如果 `hash()` 时传了 `secret`，则 `verify()` 无法直接验证（因为 verify 不接受 secret 参数）。这是设计意图——需要 secret 的场景下，验证逻辑需要自行重新 hash 后比较。

### parallelism 影响哈希值

`parallelism` 不仅是性能参数，它**改变哈希结果**。同一密码用 p=1 和 p=4 产生不同哈希。verify 从 PHC 编码中自动读取 p 值，所以验证不受影响。

### 内存消耗

`memoryCost=65536` 表示每次哈希消耗 64 MiB 内存。高并发场景需评估服务器总内存：

```
总内存 ≈ 并发哈希数 × memoryCost
```

### 性能参考

| 参数 | 单次耗时（参考） |
|------|-----------------|
| t=3, m=65536, p=4 | ~200-500 ms |
| t=3, m=19456, p=1 | ~50-100 ms |
| t=1, m=4096, p=1 | ~5-10 ms |

首次集成时建议用目标硬件实测，确保登录延迟可接受。

### 与 bcrypt/scrypt 的互操作

本库仅处理 Argon2 格式。如需验证旧 bcrypt 哈希，需先用对应库验证，成功后用 `argon2.hash()` 重新哈希并迁移。

---

## 文件结构

```
argon2/
├── argon2_native.c      # C 引擎：Blake2b + Argon2 核心 + PHC 编解码
├── src/
│   └── argon2.xl        # xlang API 层
├── xlang.package.json   # 包清单
└── readme.md            # 本文件
```

---

## 完整范例

参见 `release/examples/extlibs/argon2.xl`，包含 11 节逐步教学，覆盖全部 API。
