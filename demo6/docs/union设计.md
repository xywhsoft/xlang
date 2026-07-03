# union 设计

`union` 是 DEMO6 用于兼容 C ABI 和 C 结构体布局的基础类型声明。

它采用 C 风格语义：所有字段共享同一块存储空间，xlang 不记录当前激活字段，不生成 tag，也不做字段生命周期管理。

## 语法

顶层 union 必须具名，用于声明一个可在变量、函数参数、返回值和 C ABI 中使用的类型：

```xlang
union Payload
    int i
    c_double d
end

Payload p
p.i = 42
```

class 内可以声明内联 union，主要用于模拟 C 结构体中的匿名 union：

```xlang
class Packet
    int tag
    union
        int code
        c_double ratio
    end
end

Packet p
p.code = 200
```

第一版只支持顶层具名 union 和 class 内匿名 union。

class 内具名内联 union 暂不进入语言标准，因为它需要额外定义 `packet.payload.code` 的内联匿名类型访问规则。为了避免半可用语法，当前阶段直接拒绝：

```xlang
class Packet
    union payload
        int code
    end
end
```

## 规则

- 顶层匿名 `union ... end` 非法，因为它不能形成可引用类型。
- union 字段只能使用 `Type name` 声明。
- union 字段必须是平凡 C 兼容值类型。
  - 禁止 `function`、`var`、`array`、`list`、`set`、`dict` 等需要引用计数或动态生命周期的类型。
  - 禁止 `future`、`task`、`channel`、`generator`、`thread`、`mutex`、`cond`、`rwlock`、`sem` 等资源句柄类型。
  - 禁止带 `destroy` 的 native-backed class，以及包含非平凡字段或显式 `destroy` 的 `class`。
  - 这样做是为了保持 C 风格 union 的“只描述共享内存布局”语义，避免在没有活动字段 tag 的情况下误释放或泄漏资源。
- union 不能为空。
- union 字段名不能重复。
- union 字段不能带默认值。
- union 不支持方法、构造函数、析构函数、继承或隐性 tag。
- class 内匿名 union 的字段会暴露为 class 的直接字段，例如 `packet.code`。
- class 内匿名 union 的字段名不能和 class 其他字段、方法、create/destroy 或其他匿名 union 字段冲突。
- class 内具名 union 当前非法，后续若要支持，必须同时补齐嵌套访问、类型推断和 C ABI 元数据规则。

## C 代码生成

顶层 union 生成 C typedef：

```c
typedef union XL_Union_Payload {
    int64_t i;
    double d;
} XL_Union_Payload;
```

class 内匿名 union 生成 C 匿名 union 字段：

```c
typedef struct XL_Class_Packet {
    int64_t tag;
    union {
        int64_t code;
        double ratio;
    };
} XL_Class_Packet;
```

默认初始化使用 C 的零初始化：

```c
XL_Union_Payload p = {0};
```

## C ABI 和 XAPI

具名 union 是 C ABI 类型，可以用于 `extern` / `export` 函数参数和返回值。

导出头文件会输出 union typedef，并在函数签名中使用对应 C 类型：

```xlang
union Payload
    int i
    c_double d
end

export "xl6_union_pick" func pick(Payload p) as int
    return p.i
end
```

生成头文件中会包含：

```c
typedef union XL_Union_Payload {
    int64_t i;
    double d;
} XL_Union_Payload;

int64_t xl6_union_pick(XL_Union_Payload p);
```

XAPI 中 `union` 作为类型元数据导出：

```json
{"kind":"union","name":"Payload"}
```

## 边界

`union` 不是 `class`，也不是未来可能设计的带 tag 的安全联合类型。

如果后续需要表达“只能是若干类型之一，并且运行时知道当前是哪一种”的高级语义，应另行设计 tagged union / enum union，不应改变本语句的 C ABI 含义。
