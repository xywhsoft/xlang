# persistent - 持久化 (不可变) 数据结构库

用节点池 (id 引用) 实现三种持久化数据结构: **链表 / BST / HashMap**。
所有操作返回新版本, 旧版本保持不变 (structural sharing via path copying)。

## 设计

节点统一存放在模块级并行数组 (节点池) 中, 以 `int id` 引用; `NIL = -1` 表示空。
节点写入后不可变 (池只 `append`, 不原地改), 因此:

- 旧版本 (旧 root id) 指向的节点链永远不变;
- 新版本只在"变化路径"上分配新节点, 未变部分按 **id 共享**。

| 结构 | 表示 | 操作 |
|---|---|---|
| 链表 | root id | `cons / head / tail / map / filter / fold / reverse / append` |
| BST | root id | `insert / lookup / delete / inorder` |
| HashMap | version id (桶数组扁平存于池中) | `assoc / get / has / dissoc` |

HashMap 用分离链接处理冲突; `assoc`/`dissoc` 分配新 version (复制桶数组),
仅重建目标桶的链 (path copying), 其余桶按 id 共享。

## 运行

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\persistent\persistent.xl test
xl build projects\dev\persistent\persistent.xl -o projects\dev\persistent\persistent.exe
```

## 验证点

`persistent test` 验证持久化语义:

- 链表 `cons/map/filter/reverse/append` 后旧列表不变; `tail(cons(0,L))` 与 `L` 同一 id;
- BST `insert/delete` 后旧树查找结果与中序序列不变 (含删双子根用后继替换);
- HashMap `assoc/dissoc` (含替换 value) 后旧 map 内容与大小不变; 冲突链 20 key 全部 get 正确。
