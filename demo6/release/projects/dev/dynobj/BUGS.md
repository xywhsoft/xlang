# dynobj BUG 记录

`extends object` 动态字段验证工具运行过程中发现的 BUG 与手感限制。

## 当前用例结果

`xl run` 与 `xl build` + exe 运行结果一致：**passed=57 failed=8**。
8 条 FAIL 全部集中在「经 var 装箱后的字段访问」路径（BUG-D2）。
两条路径的退出码均为 0（程序内部捕获异常，仅记录 [FAIL]，不中断）。

---

### BUG-D1: extends object 子类无法覆盖 __field__ / __setField__

- 工具：dynobj
- 复现：
  ```xlang
  class TypedFieldHost extends object
      int core = 0
      func __field__(string name) as var      // 写法 A
          return null
      end
      func __setField__(string name, var val) // 写法 A
      end
  end
  ```
  或在两个协议方法前加 `override`（写法 B）。
- 现象：
  - 写法 A（不加 override）：
    `[sem] overriding a base member requires explicit override`
    （`dynobj.xl:103:5`）
  - 写法 B（加 override）：
    `[sem] override target must be virtual`
    （`dynobj.xl:105:14`）
- 期望：`class X extends object` 应允许子类覆盖基类 object 提供的字段协议
  方法，以便实现自定义计算字段或自定义动态存储。两条规则当前自相矛盾：
  不写 override 报「需要 override」，写了 override 又报「目标必须 virtual」，
  因此该组合无法编译。根因应是基类 object 的 `__field__`/`__setField__`
  未声明为 virtual，但语义层又把子类同名方法识别为 override。
- 绕过：自定义 `__field__`/`__setField__` 改放在「不继承 object 的普通 class」
  上（本工具的 FieldHost / FieldHostTyped 即如此，全部 PASS）。`extends object`
  子类只用 object 自带的动态字段字典，不再覆盖协议。
- 严重度：中（影响「自定义动态字段协议 + object 基类」组合，可用普通 class 规避）

---

### BUG-D2: 经 var 装箱的 extends object 实例字段访问返回默认值

- 工具：dynobj
- 复现：
  ```xlang
  class Actor extends object
      derive value
      int id = 0
      string name = ""
      create(int id, string name)
          self.id = id
          self.name = name
      end
  end

  Actor a = Actor(7, "hero")
  a.score = 42          // 动态字段（typed 路径下一切正常）

  var boxed = a
  int i = int(boxed.id)     // 期望 7，得到 0
  string s = string(boxed.name)  // 期望 "hero"，得到 ""
  int sc = int(boxed.score) // 期望 42，得到 0
  ```
- 现象：把 `extends object` 的实例装箱进 `var` 后，**所有字段访问都静默返回
  默认值且不抛异常**：
  - 静态字段读（dot / index）：`boxed.id` -> 0，`boxed.name` -> ""
  - 动态字段读（dot / index）：`boxed.score` -> 0，`boxed["level"]` -> 0
  - 动态字段写（dot / index）：`boxed.boost = 9` 后再读仍为 0
  - 引用可见性：`a.boost` 经 var 写入后仍读不到
  - 额外：在 var 接收者上调用 class 方法（如 `boxed.tag()`）在**编译期**即被拒：
    `[checked] 找不到 class方法`，连运行期都进不去。
- 期望：`var boxed = a` 后，`boxed.id` / `boxed.score` / `boxed["level"]`
  应分别返回静态字段值和动态字段值；写入也应反映到同一实例。
  对普通值类型（int/string/容器）的 var 装箱读写均正常，唯独 class
  （至少是 `extends object`）实例的字段路径失效。
- 绕过：不要把 `extends object` 实例装箱进 `var` 后再访问字段；始终用静态类型
  变量（`Actor a = ...`）操作。需要存入动态容器时，可先用 `derive value`
  序列化为 dict/var 再处理（本工具 testDynamicFieldTraversal / testJsonRoundtripRestore
  即走该路径，全部 PASS）。
- 严重度：高（静默错误最难排查；与 var 作为「动态值类型」的预期严重不符）

---

### LIMIT-D1: 复合 var 比较无法 lowering

- 工具：dynobj
- 复现：
  ```xlang
  var d = json.parse(js)   // d 为 dict 形态的 var
  for k, v in d            // 此处 k 推断为 var
      if k != "id" and k != "name" then   // 复合 var 比较
          ...
      end
  end
  ```
- 现象：`[checked] dynamic var comparison requires statement-level lowering`
  （`dynobj.xl:402:18`）。单个 `k == "id"`（出现在 if/elseif 链中）可正常编译，
  但 `k != "a" and k != "b"` 这类「复合 var 比较」当前无法 lowering。
- 期望：复合 var 比较应与单个 var 比较一视同仁，或在文档中明确标注该限制。
- 绕过：把复合比较拆成 if/elseif 链中的多个单比较，或先用 `string key = string(k)`
  归一化到静态类型再做复合比较。本工具 testDynamicFieldTraversal 改为
  「累计 allCount 与 staticCount，相减得到 dynCount」绕开。
- 严重度：低（有明显绕过，但与直觉不符）

---

## 验证命令与退出码

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\dynobj\dynobj.xl           :: EXIT=0
xl build projects\dev\dynobj\dynobj.xl -o projects\dev\dynobj\dynobj.exe  :: EXIT=0
projects\dev\dynobj\dynobj.exe                  :: EXIT=0
```

三条路径退出码均为 0；57 项 PASS，8 项 FAIL 全部为 BUG-D2 的如实记录。
