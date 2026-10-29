# xvm - 栈式字节码虚拟机

栈式字节码虚拟机，包含自定义指令集（31 条）、两遍汇编器、反汇编器、虚拟机执行引擎和单步调试器。

## 用法

```
xvm run <file.asm>                汇编并执行
xvm asm  <file.asm> -o out.xb     仅汇编，输出字节码文件
xvm disasm <file.xb>              反汇编字节码文件
xvm debug <file.asm>              单步跟踪模式（打印每步指令+栈状态）
xvm test                          内置自测程序（计算 1+2+...+10 = 55）
xvm -h | --help                   帮助
```

## 指令集（31 条）

| 类别 | 指令 |
|------|------|
| 栈操作 | `PUSH n` `POP` `DUP` `SWAP` `OVER` |
| 算术 | `ADD` `SUB` `MUL` `DIV` `MOD` `NEG` |
| 比较 | `EQ` `NE` `LT` `LE` `GT` `GE` |
| 逻辑 | `AND` `OR` `NOT` |
| 控制流 | `JMP label` `JZ label` `JNZ label` `CALL label` `RET` |
| 内存 | `LOAD slot` `STORE slot` |
| IO | `PRINT` `PRINTLN` `READ` |
| 终止 | `HALT` |

- 二元运算从栈顶弹两个值：`PUSH a; PUSH b; OP` 等价于 `a OP b`。
- 比较结果压 1（真）或 0（假）。
- `CALL` 压返回地址到调用栈，`RET` 弹出返回。
- `LOAD`/`STORE` 使用扁平局部变量槽（0..255），非栈帧式。
- `READ` 无 stdin 时返回 0（可通过 `pushInput` 编程填充输入队列）。

## 汇编语法

```asm
; 注释以分号开头
label:              ; 标签定义
    PUSH 10         ; 指令 + 操作数
    ADD             ; 无操作数指令
    JMP label       ; 跳转到标签
```

- 助记符大小写不敏感（`push` = `PUSH`）。
- 两遍汇编：第一遍收集标签地址，第二遍生成字节码并解析标签引用。

## 字节码格式（.xb）

文本格式，每行一个整数（操作码或操作数）：
```
XVMBC1       ← magic
1            ← PUSH opcode
0            ← operand 0
...
```

## 示例

`fact.asm` — 递归阶乘 `fact(5) = 120`：
```asm
    PUSH 5
    CALL fact
    PRINTLN
    HALT
fact:
    DUP
    PUSH 1
    LE
    JNZ base
    DUP
    PUSH 1
    SUB
    CALL fact
    MUL
    RET
base:
    POP
    PUSH 1
    RET
```

## 验证

```
xl run  projects\dev\xvm\xvm.xl -- test    → 输出 55, EXIT=0
xl build projects\dev\xvm\xvm.xl -o projects\dev\xvm\xvm.exe → EXIT=0
```
