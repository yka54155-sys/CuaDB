# 当前分支：feature/btree

⚠️ 本分支正在把存储层从「列表版」迁移到「B+ 树」。**这里不是稳定版本**，稳定版在 `main`。

## 目标

补完 B+ 树：内部节点访问器 → 根分裂 → 递归查找 → 迭代器跨页 → 父节点挂接 → 内部节点分裂。

## 已完成

- 内部节点访问器、`leaf_node_insert`、`internal_node_insert`（含"新孩子当最右 → 原 right_child 降级"）
- `Table::create_new_root()`：根分裂（第 14 行插入不再崩溃，页 0 变为 INTERNAL）
- `Table::find()`：循环下降，支持任意层内部节点（`idx == num_keys` 走 `right_child`）
- `Table::begin()`：沿 `child[0]` 下到最左叶子
- `Table::end()`：沿 `right_child` 下到最右叶子
- `Table::next_leaf_page()`：靠父指针上溯、用分界键定位兄弟、再下潜到最左叶子
- `operator++`：页内前进 → 本叶末尾跳下一页 → 全表末尾把 `cell_num` 置为 `num_cells` 归位到 `end()`
- `get_max_key()`：叶子取最后一条，内部节点沿 `right_child` 递归到底

## 当前状态

`select` 只覆盖根节点可达的那部分叶子。原因是非根分裂尚未实现：

```
插入 100 行 → 12 次 "Need to implement updating parent after split"
             叶子分裂已完成、数据已落盘，但父节点没有收到新的 (max_key, child)
             → 这些叶子成为孤儿页，根里没有指向它们的 cell
```

`select` 的输出条数等于可达叶子的 cell 之和，说明遍历本身正确：

| 插入行数 | 物理 cell 和 | `select` 输出 | 升序 |
|---|---|---|---|
| 14 | 14 | 14 | ✓ |
| 40 | 40 | 19 | ✓ |
| 65 | 65 | 16 | ✓ |
| 130 | 130 | 18 | ✓ |

## 下一步

1. **父节点更新（非根分裂）** —— 当前唯一的数据正确性问题
   - 新增 `update_internal_node_key(parent, old_key, new_key)`
   - `leaf_node_split_and_insert`：`old_is_root` 走 `create_new_root`，
     否则先改旧页在父里的分界键，再 `internal_node_insert(parent, 新页最大键, 新页)`
   - 父节点满时先 `assert`，下一步再实现内部节点分裂
2. **内部节点分裂** + `.btree` 按节点类型递归打印
3. 验收：`INTERNAL_NODE_MAX_CELLS` 临时改为 3，插 200 行不崩，三层树 `select` 条数正确

## 当前卡点

- [X] `Table.hpp`、`Node.hpp` 契约注释
- [X] `create_new_root()` 根分裂
- [X] `find()` 内部节点下降
- [X] `begin()` / `end()` 定位到最左 / 最右叶子
- [X] 迭代器跨页（`operator++` + `next_leaf_page`）
- [X] 两层树 `select` 顺序与条数正确
- [ ] 非根分裂未实现 → 孤儿页、`select` 少行
- [ ] 内部节点分裂未实现 → 三层树无法构造
- [ ] `.btree` 只打印根页，无法验证多层结构
- [ ] 三层树中 `next_leaf_page` 的下潜路径尚未实测（代码已写，当前无法构造三层）

## 已知隐患

- **不变量**：内部节点 `cell[i].key` 恰好等于 `child[i]` 子树的最大 rowid。
  插入不会破坏它（按"第一个 ≥ k"路由，能落入某孩子的键必然不超过它的分界键；
  只有最右孩子会变大，而它不存键）。**分裂必须维护它**：旧页最大值变小要改键，
  新页要插入一条 `(新页最大键, 新页)` 的 cell。写错会导致 `find` 走错分支、
  `next_leaf_page` 跳到错误的兄弟页，且不会报错，只表现为少几行数据。
- `find()` 未命中的"插入位置"与 `end()` 恰好相等，这个等式依赖**没有删除**。
  实现删除后 `num_cells` 位置会出现在非最右叶子，必须改用 `INVALID_PAGE_NUM` 作哨兵。
- `next_leaf_page()` 依赖页头的 `parent` 指针；删除后可能出现空叶子
  （`get_max_key` 会对空叶子断言），届时定位方式需与删除/合并一起重做。
- 测试库必须新建。复用旧库会把上一轮的孤儿页当成这一轮的结果。

## 合并到 main 的步骤

1. 对照"下一步"三项全部通过
2. `git checkout main && git pull`
3. `git merge feature/btree`
4. 遇到 `CONFLICT (add/add): README.md` 属正常现象，保留 main 的版本：
   `git checkout --ours README.md && git add README.md && git commit`
5. `cmake -S . -B build && cmake --build build`，确认 main 上也能编译
6. `git push`
7. `git tag -a v0.3.0 -m "B+ 树三层验证通过"` 然后 `git push origin v0.3.0`