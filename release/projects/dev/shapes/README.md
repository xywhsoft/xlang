# shapes - 继承体系验证工具

通过 `Shape > Circle extends Shape > Cylinder extends Circle` 三层继承链，验证 xlang class
的访问控制、static、virtual/override/final、base 调用和 destroy 析构能力。

## 运行

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run projects\dev\shapes\shapes.xl
xl build projects\dev\shapes\shapes.xl -o projects\dev\shapes\shapes.exe
shapes.exe
```

三条路径都必须 EXIT=0。

## 覆盖范围（42 条断言 PASS）

| 用例 | 被测能力 | 说明 |
|---|---|---|
| 1 | 继承链 + `base()` 链式构造 | `Circle.base(name)` / `Cylinder.base(name,r)` 跨两层到达 Shape |
| 2 | public / private / protected | private `id` 经 Shape 方法读；protected `name` 在 Circle/Cylinder 方法内可见；public 字段外部读写 |
| 3 | static 字段与方法 | `Shape.live` / `Shape.total()` / `Shape.madeCount()` 经类名访问，随实例增减 |
| 4 | virtual / override 方法 | `area()` 在 Shape/Circle/Cylinder 各自实现，直接调用结果正确 |
| 5 | virtual / override 属性 (get) | `type` 在三层分别返回 `shape:`/`circle:`/`cylinder:` 前缀 |
| 6 | vtable 动态派发 | `Shape s = Circle(...)` 向上转型后 `s.area()`/`s.type` 按实例真实类型派发到 override（核心）；并辅以基类方法 `describe()` 内 `self.xxx()` 内部派发 |
| 7 | final 方法 | `Circle.density()` 标记 final；Cylinder 继承并可调用，不再 override |
| 8 | destroy 析构 | 作用域内创建实例，函数返回时 destroy 触发，`made/destroyed/live` 计数吻合 |
| 9 | var 装箱 downcast + 参数多态 | 无 destroy 的 `ProbeBase`/`ProbeDerived` 镜像层级：`var -> as ProbeBase -> b.area()` 保留 override；`probeArea(ProbeBase)` 传 Derived 派发到 override |

## 设计要点

- **vtable 派发路径**：xlang 现已支持派生类实例向基类类型的隐式向上转型与按值形参多态
  （`Shape s = Circle(...)`、`func f(Shape s)` 传 Circle 均合法），vtable 经向上转型与
  函数参数两条路径均按实例真实类型派发到 override。本工具用例 6 即采用向上转型写法
  验证动态分派，并保留基类方法 `describe()` 内 `self.xxx()` 的内部派发对照。
- **destroy 与不可复制**：带 `destroy` 的 class 被视为不可按值传参资源（参见 dev/README
  「仍需注意的限制」）。本工具所有 Shape/Circle/Cylinder 实例都保留在 typed 局部变量，
  不按值进出函数。用例 9 的 var 装箱与参数多态探针另建一对**无 destroy** 的 `ProbeBase`/`ProbeDerived`
  镜像层级承载（经 var 往返后 vtable 派发仍保留）。
- 测试框架与 `vops`/`overload` 一致：`g_pass`/`g_fail` 计数 + `expectI`/`expectF`/`expectS`，
  每条断言包在 `try/catch` 内，`main(...)` 始终返回 0。
- class 方法体内一律显式 `self.<field>` 访问（规避已知 BUG-F1）。

## 已知限制

- **BUG-F1**：class 方法体内裸字段名作接收方/赋值目标不被解析为 `self.field`，须显式写 `self.`（详见上级 `../BUGS.md`）。本工具因此一律用 `self.<field>`。
