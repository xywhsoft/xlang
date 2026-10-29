# deepclone - derive 全套验证工具

验证 xlang 全部 `derive` 能力。对每一项给出 PASS / FAIL 报告。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run  projects\dev\deepclone\deepclone.xl
xl build projects\dev\deepclone\deepclone.xl -o projects\dev\deepclone\deepclone.exe
```

两条路径均 EXIT=0。当前用例数：**58 PASS / 0 FAIL**。

## 覆盖矩阵

| derive | 声明 | 运行能力 | 备注 |
|---|---|---|---|
| `copy`        | OK | `copy(obj)` 浅拷贝 + 字段隔离 | 已实现 |
| `clone`       | OK | `clone(obj)` 深拷贝 + 字段隔离 | 已实现 |
| `moveOnly`    | OK | 独立声明合法；与 copy/clone 冲突被语义阶段拦截 | 已实现 |
| `value`       | OK | 经 `json` / `xlon` 往返验证 `__toValue__/__fromValue__` | 已实现 |
| `json`        | OK | `json.stringify` / `json.parse::<Product>` | 已实现 |
| `xlon`        | OK | `xlon.stringify` / `xlon.parse::<Product>` | 已实现 |
| `string`      | OK | 自动生成字符串表示，支持 `string(obj)` / `obj.toString()` | 已实现 |
| `hash`        | OK | 自动字段哈希，可直接用于类型化 set | 已实现 |
| `equals`      | OK | 自动生成 `==` / `!=` 值比较 | 已实现 |

字段级注解（全部已实现）：

| 注解 | 效果 |
|---|---|
| `@json("created_at")`     | 仅 JSON 侧重命名字段 |
| `@persist(name="x")`      | JSON/XLON 通用字段名 |
| `@persist(skip)`          | 跳过持久化（两侧生效） |
| `@json(omit=empty)`       | 空串字段不输出 |
| `@xlon(omit=null)`        | null 值字段不输出 |

## 测试类

```xlang
class Product
    derive copy
    derive clone
    derive value
    derive json
    derive xlon
    derive string
    derive hash
    derive equals

    int id = 0
    string name = ""
    float price = 0.0
    string createdAt @json("created_at") @persist(name="createdAt") = ""
    string internalNote @persist(skip) = ""
    string memo @json(omit=empty) @xlon(omit=null) = ""

end
```

## derive moveOnly 冲突检查

`derive moveOnly` 与 `derive copy` 或 `derive clone` 同列会被语义阶段拦截：

```xlang
class Bad
    derive copy
    derive moveOnly     // [sem] derive moveOnly 不能与 derive copy 或 derive clone 同时使用
end
```

实测 `xl check` 与 `xl run` 均返回 EXIT=1 并定位到对应行。因非法源码无法内联进本工具
（会破坏 EXIT=0 基线），冲突复现保留在本 README 中作为独立用例。

当前用例直接使用编译器派生实现，不包含历史手写兜底。
