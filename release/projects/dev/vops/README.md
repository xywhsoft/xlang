# vops - 运算符协议全集验证工具

定义二维向量 `Vector2`，实现并逐一测试 xlang class 的全部 `__xxx__` 运算符协议。

## 覆盖的协议

| 类别 | 协议 | 运算符/语法 |
|---|---|---|
| 二元算术 | `__add__` `__sub__` `__mul__` `__div__` `__mod__` `__pow__` | `+ - * / % **` |
| 一元 | `__neg__` `__pos__` `__not__` `__bitNot__` | `-a +a not a ~a` |
| 比较 | `__eq__` `__ne__` `__lt__` `__le__` `__gt__` `__ge__` `__approx__` | `== != < <= > >= ~=` |
| 容器/调用 | `__contains__` `__call__` `__len__` `__item__` `__setItem__` | `b in a` `v(2.0)` `v.length` `v[i]` `v[i]=x` |
| 迭代 | `__iter__` `__next__` | `for x in v` / `for i,x in v` |
| 动态字段 | `__field__` `__setField__` | `v.magnitude` / `v.w = 9.5` |
| 复制 | `derive copy` `derive clone` | `copy(v)` / `clone(v)` |
| 复合赋值 | `__add__` `__sub__` | `a += b` / `a -= b` |

## 运行与验证

```bat
cd /d D:\GIT\x-lang\release
xl run  projects\dev\vops\vops.xl
xl build projects\dev\vops\vops.xl -o projects\dev\vops\vops.exe
```

两条路径都会执行完整协议回归；全部通过时退出码为 0，任一断言失败时退出码非 0。

## 设计要点

- `Vector2`：`derive copy/clone`；`float x,y`；实现全部协议（含 `__field__`/`__setField__`，用 `var bag` dict 存动态字段）；`__iter__` 返回独立迭代器类 `Vector2Iter`。
- `main` 返回失败计数，可直接纳入自动化回归测试。
