# xlang 扩展库建设 Spec

## 1. 总则与目标

本 Spec 定义 xlang demo6 扩展库批量建设任务的总体规范。目标：新增 **162 个**扩展库，覆盖 17 个应用领域，按依赖复杂度递增顺序分批实现。

### 1.1 设计原则

- 复用现有扩展库系统：source import + `xlang.package.json` + C companion
- 优先使用 native-backed class 模式（新特性完整应用）
- 第三方 C 源码以 vendor 方式内嵌，不依赖外部安装
- 每个库独立可测、独立可用，无隐式全局状态
- API 设计遵循：工厂函数优先、默认参数、错误即异常、命名空间隔离

### 1.2 领域覆盖（17 个）

数据库与存储 / 序列化与数据交换 / 消息队列与事件流 / 网络协议与服务 / 音频与语音 / 图形与GUI / 文档与办公 / 科学计算与数值 / 人工智能与机器学习 / 数据处理与可视化 / 系统工具与终端 / 数据结构与算法 / 安全与认证 / 文本与NLP / 嵌入式与IoT / DevOps与工程 / 金融与专业领域

### 1.3 排除范围

- 与 XGE 直接竞争的图形/游戏库（cairo, sdl2, raylib 等）
- XRT 已覆盖能力（x509, libsodium 等）
- 标准库已涵盖（crypto 全套, http, tcp/udp, regex, template 等）
- 已有扩展库（gzip, csv, sqlite, redis, opencv, onnxruntime 等 48 个）
- 竞争力不足/已衰退的库（espeak, tflite, disque 等）

---

## 2. 每库标准交付物

每个扩展库必须包含以下文件：

| 文件 | 必须 | 说明 |
|------|------|------|
| `release/extlibs/<name>/xlang.package.json` | 是 | 包清单 |
| `release/extlibs/<name>/src/<name>.xl` | 是 | xlang API 层 |
| `release/extlibs/<name>/<name>_native.c` | 是 | C 桥接层 |
| `release/extlibs/<name>/vendor/` 或 `third_party/` | 视需要 | 第三方 C 源码 |
| `release/extlibs/<name>/VENDOR.md` | 视需要 | 第三方来源/版本/许可证 |
| `test/cases/extlib_<name>.xl` | 是 | xlang assert 测试 |
| `release/examples/extlibs/<name>.xl` | 是 | 范例代码 |
| `release/extlibs/<name>/test/test_<name>.c` | 可选 | C 层单元测试 |

---

## 3. 目录结构规范

```
release/extlibs/<name>/
├── xlang.package.json
├── src/
│   └── <name>.xl
├── <name>_native.c
├── [vendor/<upstream>/]       # 第三方源码（如需）
│   ├── LICENSE
│   └── *.c / *.h
├── [VENDOR.md]                # 来源、版本、许可证说明
└── [test/
    └── test_<name>.c]
```

### 3.1 Package Manifest 模板

```json
{
    "format": "xlang-package-1",
    "name": "<name>",
    "version": "1.0.0",
    "abiVersion": "1",
    "kind": "demo6-extlib",
    "artifact": {
        "kind": "source",
        "cSources": ["<name>_native.c"],
        "xlSources": ["src/<name>.xl"]
    },
    "links": [],
    "loadlibs": [],
    "dependencies": [],
    "native": {
        "singleton": false,
        "loadName": "",
        "version": ""
    }
}
```

含第三方源码时，`cSources` 列出所有 `.c` 文件（含 vendor 路径）。
含私有依赖包时，使用 `_` 前缀包名，`dependencies` 引用 `"_xxx@version#abi1"`。

### 3.2 xlang API 层规范

```xlang
// 错误获取（固定模式）
extern "xrtGetError" func __<name>_error() as string

// native-backed class（推荐）
class <ClassName>
    native point handle
    extern "x<name>_<class>_create" create(...)
    extern "x<name>_<class>_destroy" destroy()
    extern "x<name>_<class>_method" func methodNative(...) as ...

    // 高层包装方法
    func method(...) as ...
        ...
        throw <Name>Error __<name>_error()
    end
end

// 工厂函数（export）
export func create<Thing>(...) as <ClassName>
    return <ClassName>(...)
end
```

### 3.3 C 桥接层规范

- 函数前缀：`x<name>_`（公开）/ `__xl_<name>_`（内部）
- 错误报告：`xrtSetError("message")`
- 字符串分配：`xrtAlloc(len)` + `__xl_std_runtime_take_cstr`
- 类型映射：point→void*, string→const char*, int→int64_t, bool→int, float→double

---

## 4. 实现分层与排序

按依赖复杂度分为 6 个 Tier，从 Tier 0 开始递增实现。

| Tier | 定义 | 库数量 | 特征 |
|------|------|--------|------|
| 0 | 纯算法/协议实现，零第三方依赖 | 24 | 自行编写 C 或纯 xlang |
| 1 | 平台 API 封装 / 极小 C 代码 | 24 | 直接调用 OS API 或 <2K 行 C |
| 2 | 小型多文件 C 库 vendoring | 28 | 2K-20K 行，无复杂构建 |
| 3 | 中型 C/C++ 库 | 32 | 20K-200K 行，有构建系统 |
| 4 | 大型 C/C++ 库 | 28 | 200K-1M 行，重依赖 |
| 5 | 超大型 / 重型工具链依赖 | 26 | >1M 行或需 CMake/C++17/GPU SDK |

### 4.1 Tier 内排序原则

1. 无外部依赖 → 有外部依赖
2. 纯计算 → 需要 I/O → 需要网络
3. 单平台 → 跨平台
4. 被其他库依赖的优先

### 4.2 类别间推进顺序（Tier 2-5）

序列化 → 数据结构补充 → 文本/NLP → 安全认证 → 嵌入式/IoT → 系统工具 → 网络协议 → 消息队列 → 数据库 → 音频 → 文档 → 科学计算 → 数据处理 → DevOps → 金融/专业 → AI/ML → 图形/GUI

---

## 5. 完整清单（162 条）

### 5.1 Tier 0 — 纯算法/协议实现（24 个）

| # | 包名 | 说明 | 实现方式 |
|---|------|------|---------|
| 1 | bloom | 布隆过滤器 | 纯 C 位数组 + 多哈希 |
| 2 | hyperloglog | 基数估计 | 纯 C 寄存器数组 |
| 3 | cuckoo | Cuckoo 过滤器 | 纯 C 指纹桶 |
| 4 | skiplist | 跳表 | 纯 C 链表 |
| 5 | trie | 前缀树/基数树 | 纯 C 树结构 |
| 6 | lru | LRU/LFU 缓存 | 纯 C 哈希+双链表 |
| 7 | consistent_hash | 一致性哈希 | 纯 C 哈希环 |
| 8 | geohash | 地理编码 | 纯 C 位交错 |
| 9 | aho_corasick | 多模式字符串匹配 | 纯 C 自动机 |
| 10 | roaring | 压缩位图 | CRoaring amalgamation (单文件) |
| 11 | singleinstance | 应用单实例互斥 | Win32 Mutex / POSIX flock |
| 12 | dotenv | .env 文件加载 | 纯 C 文本解析 |
| 13 | crontab | 定时任务调度 | 纯 C cron 表达式解析 |
| 14 | tabulate | 终端表格格式化 | 纯 xlang 字符串处理 |
| 15 | progress | 进度条/速率估计 | 纯 xlang + 终端 ANSI |
| 16 | totp | TOTP/HOTP 两步验证 | 纯 C (HMAC from XRT) |
| 17 | pinyin | 汉字转拼音 | 纯 C + 内嵌码表 |
| 18 | fuzzy | 模糊字符串匹配 | 纯 C (Smith-Waterman) |
| 19 | diff | Myers diff 算法 | 纯 C |
| 20 | json5 | JSON5 解析器 | 纯 C 递归下降 |
| 21 | tabulate_ext | 结构化数据美化 | 纯 xlang |
| 22 | geohash_ext | 地理邻接/边界 | 纯 C |
| 23 | s2 | S2 Cell ID 编解码 | 纯 C (Hilbert 曲线) |
| 24 | rtree | R-Tree 空间索引 | 纯 C |

### 5.2 Tier 1 — 平台 API / 极小 C（24 个）

| # | 包名 | 说明 | 参考实现 |
|---|------|------|---------|
| 25 | chardet | 字符编码检测 | uchardet 简化 |
| 26 | langdetect | 自然语言检测 | 字符频率表 |
| 27 | serial | 串口通信 | Win32 COM / POSIX termios |
| 28 | gpio | GPIO 控制 | Linux sysfs / Windows |
| 29 | sharedmem | 跨进程共享内存 | Win32 MapView / POSIX shm |
| 30 | daemon | 守护进程/服务 | Win32 Service / POSIX daemon |
| 31 | sysinfo | 系统信息 | Win32 API / POSIX sysctl |
| 32 | procps | 进程监控 | Win32 ToolHelp / POSIX /proc |
| 33 | watch | 文件系统监听 | ReadDirectoryChanges / inotify |
| 34 | ntp | NTP 时间同步 | 协议实现 (RFC 5905) |
| 35 | mdns | mDNS/DNS-SD | 协议实现 (RFC 6762) |
| 36 | upnp | UPnP 端口映射 | miniupnpc (极小) |
| 37 | socks | SOCKS4/5 代理 | 协议实现 (RFC 1928) |
| 38 | argon2 | 密码哈希 | RFC 9106 参考实现 |
| 39 | jwt | JSON Web Token | 基于 stdlib crypto |
| 40 | pkcs | PKCS#7/#12 容器 | 小型 ASN.1 子集 |
| 41 | ncurses | 终端 UI | PDCurses (Win) / ncurses |
| 42 | libusb | USB 设备通信 | WinUSB / libusb 简化 |
| 43 | canbus | CAN 总线 | SocketCAN / Win32 |
| 44 | modbus | Modbus 协议 | 协议实现 (极简) |
| 45 | css | CSS 解析器 | 小型递归下降 |
| 46 | oauth2 | OAuth2 流程 | 基于 stdlib http |
| 47 | acme | ACME 自动证书 | 基于 stdlib http + crypto |
| 48 | vault | Vault 密钥管理 | 基于 stdlib http (REST) |

### 5.3 Tier 2 — 小型多文件 C 库（28 个）

| # | 包名 | 说明 | 上游 C 库 |
|---|------|------|----------|
| 49 | yaml | YAML 解析/生成 | libyaml |
| 50 | cbor | CBOR 编解码 | libcbor |
| 51 | msgpack | MessagePack | msgpack-c |
| 52 | hcl | HCL 配置语言 | 自行实现 (参考 Go hcl2) |
| 53 | bson | BSON 编解码 | libbson (子集) |
| 54 | avro | Avro 序列化 | avro-c |
| 55 | zeromq | 无 broker 消息 | libzmq (NULL 安全) |
| 56 | nanomsg | 轻量消息传输 | nng |
| 57 | mqtt | MQTT 协议 | paho.mqtt.c |
| 58 | coap | CoAP 协议 | libcoap |
| 59 | enet | 可靠 UDP 网络 | enet |
| 60 | lmdb | 嵌入式 KV | lmdb |
| 61 | leveldb | 嵌入式有序 KV | leveldb (C 接口) |
| 62 | unqlite | 嵌入式 KV+JSON | unqlite |
| 63 | influxdb | 时序数据库 | HTTP API 封装 |
| 64 | tdengine | 时序数据库 | taos.h |
| 65 | archive | 多格式归档 | libarchive |
| 66 | fftw | 快速傅里叶变换 | FFTW3 |
| 67 | gmp | 任意精度算术 | GMP |
| 68 | mpfr | 任意精度浮点 | MPFR |
| 69 | nlopt | 非线性优化 | NLopt |
| 70 | linalg | 向量/矩阵/四元数 | linalg.h (单头) |
| 71 | proj | 坐标投影转换 | PROJ |
| 72 | ta_lib | 金融技术指标 | TA-Lib |
| 73 | geos | 空间几何运算 | GEOS (C API) |
| 74 | pcap | 网络抓包 | libpcap / Npcap |
| 75 | snmp | SNMP 管理 | net-snmp (子集) |
| 76 | passkey | FIDO2/WebAuthn | 协议实现 + crypto |

### 5.4 Tier 3 — 中型 C/C++ 库（32 个）

| # | 包名 | 说明 | 上游 C 库 |
|---|------|------|----------|
| 77 | postgresql | PostgreSQL 客户端 | libpq |
| 78 | mysql | MySQL 客户端 | libmysqlclient |
| 79 | mongodb | MongoDB 驱动 | libmongoc |
| 80 | neo4j | 图数据库 | neo4j-client |
| 81 | etcd | 分布式 KV | gRPC 封装 |
| 82 | minio | S3 对象存储 | HTTP + AWS Sig V4 |
| 83 | clickhouse | 列式分析数据库 | clickhouse-cpp |
| 84 | kafka | Apache Kafka | librdkafka |
| 85 | rabbitmq | AMQP 消息代理 | rabbitmq-c |
| 86 | nats | NATS 消息 | nats.c |
| 87 | pulsar | Pulsar 消息 | pulsar-client-cpp |
| 88 | nsq | NSQ 消息 | 协议实现 |
| 89 | ssh | SSH2 + SFTP | libssh2 |
| 90 | ftp | FTP/FTPS | 协议实现 |
| 91 | dns | 异步 DNS | c-ares |
| 92 | quic | QUIC/HTTP3 | ngtcp2 + nghttp3 |
| 93 | ldap | LDAP 目录 | openldap |
| 94 | webrtc | 实时通信 | 协议子集 |
| 95 | rtmp | 直播推流 | librtmp |
| 96 | opus | Opus 音频编解码 | libopus |
| 97 | portaudio | 音频 I/O | portaudio |
| 98 | libsndfile | 音频文件读写 | libsndfile |
| 99 | sox | 音频效果 | libsox |
| 100 | taglib | 音频元数据 | taglib (C API) |
| 101 | xlsx | Excel 读写 | libxlsxwriter |
| 102 | docx | Word 生成/解析 | 自行实现 (zip+xml) |
| 103 | pptx | PowerPoint 生成 | 自行实现 (zip+xml) |
| 104 | epub | EPUB 电子书 | 自行实现 (zip+xml) |
| 105 | html5 | HTML5 解析 | gumbo |
| 106 | blas | 基础线性代数 | OpenBLAS |
| 107 | lapack | 线性方程/特征值 | LAPACKE |
| 108 | gsl | 数值计算全集 | GSL |

### 5.5 Tier 4 — 大型 C/C++ 库（28 个）

| # | 包名 | 说明 | 上游 C 库 |
|---|------|------|----------|
| 109 | ffmpeg | 音视频编解码 | FFmpeg libav* |
| 110 | pdf | PDF 生成 | libharu |
| 111 | pdfium | PDF 渲染/解析 | PDFium |
| 112 | eigen | 模板线性代数 | Eigen |
| 113 | ceres | 非线性最小二乘 | Ceres Solver |
| 114 | duckdb | 嵌入式 OLAP | DuckDB |
| 115 | rocksdb | 高性能嵌入式 KV | RocksDB |
| 116 | parquet | 列式文件 | Arrow Parquet |
| 117 | arrow | 列式内存格式 | Apache Arrow |
| 118 | dataframe | 表格数据分析 | 基于 arrow |
| 119 | plot | 2D 图表绘制 | 自行实现 (SVG/PNG) |
| 120 | icu | Unicode 全集 | ICU4C |
| 121 | jieba | 中文分词 | cppjieba |
| 122 | treesitter | 增量语法解析 | tree-sitter |
| 123 | git | Git 仓库操作 | libgit2 |
| 124 | docker | Docker API | HTTP REST 封装 |
| 125 | kubernetes | K8s API | HTTP REST 封装 |
| 126 | prometheus | 指标暴露 | 自行实现 (HTTP) |
| 127 | opentelemetry | 分布式追踪 | opentelemetry-cpp |
| 128 | terraform_state | TF 状态解析 | JSON 解析 |
| 129 | gdal | 地理空间数据 | GDAL |
| 130 | rdkit | 化学信息学 | RDKit |
| 131 | bio | 生物信息学 | htslib |
| 132 | fix | FIX 金融协议 | QuickFIX |
| 133 | xgboost | 梯度提升树 | XGBoost C API |
| 134 | lightgbm | 轻量 GBDT | LightGBM |
| 135 | libsvm | 支持向量机 | libsvm |
| 136 | faiss | 向量相似度搜索 | FAISS |

### 5.6 Tier 5 — 超大型 / 重型依赖（26 个）

| # | 包名 | 说明 | 上游 C 库 |
|---|------|------|----------|
| 137 | llama | 本地 LLM 推理 | llama.cpp |
| 138 | ggml | 张量库 | ggml |
| 139 | whisper | 语音识别 | whisper.cpp |
| 140 | tokenizers | HF 分词器 | tokenizers (Rust FFI) |
| 141 | sentencepiece | 语言无关分词 | sentencepiece |
| 142 | stablediffusion | 图像生成 | stable-diffusion.cpp |
| 143 | mnn | 移动端推理 | MNN |
| 144 | ncnn | 移动端推理 | NCNN |
| 145 | openvino | Intel 推理 | OpenVINO |
| 146 | tensorrt | NVIDIA 推理 | TensorRT |
| 147 | autodiff | 自动微分 | tinygrad 思路 |
| 148 | libtorch | PyTorch C++ | LibTorch |
| 149 | clip | CLIP 多模态 | clip.cpp |
| 150 | embeddings | 文本嵌入 | fastembed |
| 151 | vectordb | 向量数据库 | Qdrant/Milvus SDK |
| 152 | hnswlib | HNSW 近邻 | hnswlib |
| 153 | grpc | gRPC 调用 | grpc-c |
| 154 | sciter | HTML/CSS GUI | Sciter SDK |
| 155 | webview | WebView 嵌入 | webview.h |
| 156 | imgui | 即时模式 GUI | Dear ImGui |
| 157 | freetype | 字体渲染 | FreeType2 |
| 158 | harfbuzz | 文本整形 | HarfBuzz |
| 159 | glad | OpenGL 加载器 | glad |
| 160 | ratatui | 现代 TUI | ftxui |
| 161 | protobuf | Protocol Buffers | protobuf-c |
| 162 | flatbuffers | 零拷贝序列化 | flatcc |

---

## 6. 测试与范例规范

### 6.1 xlang 测试（必须）

路径：`test/cases/extlib_<name>.xl`

```xlang
import <name>

func main(...) as int
    // 基本功能断言
    assert <condition>, "<name> <description>"

    // 异常路径
    bool caught = false
    try
        // 触发错误的操作
    catch <Name>Error e
        caught = true
    end
    assert caught, "<name> error must throw"
    return 0
end
```

规则：
- 使用 `assert` 验证正确性，第二参数为描述文本
- 覆盖正常路径 + 边界条件 + 错误路径
- `return 0` 表示全部通过
- 不依赖外部服务（数据库/网络库使用 mock 或跳过标记）

### 6.2 范例代码（必须）

路径：`release/examples/extlibs/<name>.xl`

```xlang
import <name>

func main(...) as int
    // 展示核心 API 典型用法
    ...
    println("<name> example: PASS")
    return 0
end
```

规则：
- 展示最常用的 3-5 个 API
- 代码可读性优先，无需覆盖全部功能
- 末尾固定输出 `"<name> example: PASS"`

### 6.3 测试套件（按类别）

路径：`test/suites/XX_<category>_extlibs.bat`

```bat
@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: <category> extension libraries...

for %%F in (
    extlib_<name1>
    extlib_<name2>
    ...
) do (
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

for %%F in (
    <name1>
    <name2>
    ...
) do (
    release\xl.exe run release\examples\extlibs\%%F.xl
    if errorlevel 1 exit /b 1
)

echo DEMO6 suite ok: <category> extension libraries
exit /b 0
```

### 6.4 需要外部服务的库

对于数据库、消息队列、网络服务等需要外部进程的库：
- 测试中使用条件跳过：检测服务是否可达，不可达时 `println("SKIP: ...")` + `return 0`
- 范例代码保持完整，注明运行前提条件
- 可选提供 docker-compose 或启动脚本

---

## 7. 验收标准

### 7.1 单库验收

- [ ] `xlang.package.json` 格式正确，`xl.exe run` 能解析
- [ ] `import <name>` 后可调用所有公开 API
- [ ] `test/cases/extlib_<name>.xl` 全部 assert 通过
- [ ] `release/examples/extlibs/<name>.xl` 输出 PASS
- [ ] C 桥接层无内存泄漏（create/destroy 配对）
- [ ] 错误路径正确抛出 `<Name>Error` 异常

### 7.2 批次验收

- [ ] 对应类别 `test/suites/XX_*.bat` 全部通过
- [ ] `release/extlibs/README.md` 索引已更新
- [ ] 无编译警告（TCC 编译通过）

### 7.3 整体验收

- [ ] 162 个库全部通过单库验收
- [ ] 全量回归测试通过（既有 48 个 + 新增 162 个）
- [ ] README 索引完整

---

## 附录 A：命名规范

| 项目 | 规则 | 示例 |
|------|------|------|
| 包名 | 小写，下划线分隔 | `consistent_hash` |
| C 函数前缀 | `x<pkg>_` | `xbloom_create` |
| xlang 异常类 | `<Pkg>Error` (PascalCase) | `BloomError` |
| native 方法后缀 | `*Native()` | `addNative()` |
| 内部函数前缀 | `__<pkg>_` | `__bloom_hash` |
| 测试文件 | `extlib_<pkg>.xl` | `extlib_bloom.xl` |
| 范例文件 | `<pkg>.xl` | `bloom.xl` |

## 附录 B：依赖关系图（关键路径）

```
arrow ← parquet ← dataframe
gmp ← mpfr
blas ← lapack ← eigen ← ceres
ggml ← llama ← whisper / clip / stablediffusion
ggml ← autodiff
opencv (已有) ← xfeat (已有)
 Zeromq ← kafka (librdkafka 内部可选)
ssh ← git (libgit2 可选 ssh 传输)
```

## 附录 C：预估工作量

| Tier | 单库平均代码量 | 单库预估时间 | 小计 |
|------|--------------|-------------|------|
| 0 | ~300 行 C + ~150 行 xl | 短 | 24 库 |
| 1 | ~500 行 C + ~200 行 xl | 短-中 | 24 库 |
| 2 | ~1000 行桥接 + vendor | 中 | 28 库 |
| 3 | ~2000 行桥接 + vendor | 中-长 | 32 库 |
| 4 | ~3000 行桥接 + 重 vendor | 长 | 28 库 |
| 5 | ~5000 行桥接 + 超重工件 | 很长 | 26 库 |
