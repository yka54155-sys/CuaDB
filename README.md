# 当前分支：feature/btree

⚠️ 本分支正在把存储层从「列表版」迁移到「B+ 树」。**这里不是稳定版本**，
   稳定版在 `main`。

## 目标
补完 B+ 树分裂：内部节点访问器 → 根分裂 → 父节点挂接 → 内部节点分裂 → 递归查找

## 已完成

- 内部节点访问器、`leaf_node_insert`、`internal_node_insert`
- `Table::create_new_root()`：根分裂（14 行插入不再崩溃，页 0 已变为 INTERNAL）
- `Table::find()`：改为循环下降，支持任意层数的内部节点
  （`idx == num_keys` 时走 `right_child`，否则走 `child[idx]`）
- `Table::end()`：沿 `right_child` 下到最右叶子，不再误读内部节点的 `num_keys`
- 连续插入 100 条通过，无 assert 中断

## 下一步（按顺序）

1. 迭代器跨页：`begin()` 下到最左叶子；`operator++` 走到 `num_cells` 后跳下一页
   （两种方案：叶子加 `next_leaf` 字段，或迭代器内维护路径栈）
2. `select` 验证：顺序严格递增、条数等于插入条数
3. 非根分裂：新增 `update_internal_node_key()`，分裂后更新父节点里的 key 并插入新 cell；
   父节点满则继续分裂内部节点（最终递归回 `create_new_root`）
4. `.btree` 按节点类型递归打印每一层

## 当前卡点

- [X] `Table.hpp`、`Node.hpp` 契约注释
- [X] `Table::create_new_root()` 根分裂
- [X] `find()` 对内部节点的下降查找（15 行起插入已通过，100 行验证）
- [X] `end()` 下到最右叶子
- [ ] `select` 结果不正确：`begin()` 仍假设根是叶子，迭代器不跨页
- [ ] 三层结构未验证：扇出 511，100 行仍只有 2 层
- [ ] `.btree` 只打印根页，无法验证多层结构
- [ ] 未验证：插入的 key 超过某子树当前最大值时，祖先节点里的 max key 没有更新
      （内部键是「快照」，不改会导致 find 走错分支、插入重复数据）
- [ ] 非根分裂未实现：叶子分裂后父节点没有收到新的 (max_key, child) cell

## 已知隐患

- `find()` 的插入位置与 `end()` 恰好相等的性质，依赖 **没有删除 / 节点合并**。
  做了 delete 之后，`num_cells` 位置会出现在非最右叶子，届时需要改用哨兵
  `INVALID_PAGE_NUM` 表示 `end()`。
- `leaf_node_find_idx` / `internal_node_find_idx` 返回的是「第一个 >= k」的位置，
  不是「命中」；找到叶子后必须再做一次相等判断，才算命中。
- 内部节点里 `idx == num_keys` 时不能用 `read_child(node, idx)`，那会读到陈旧字节，
  必须显式走 `right_child`。

## 验收标准（满足才算完成）
- [ ] INTERNAL_NODE_MAX_CELLS=3 时插 200 行不崩
- [ ] `.btree` 打印每一层的 keys
- [ ] `select` 顺序严格递增、条数正确
- [ ] 改回 510 后同样通过

## 注意事项（仅本分支）

- 本分支是开发版，**不稳定**：`select` 仍不可用（迭代器不跨页）
- 不要在 `main` 上直接改代码，稳定版在 `main`
- 每次提交前先 `git status` 确认工作区干净
- 完成一个小项就 commit，不要攒到最后
- 切分支前必须先把改动提交或还原，未提交的改动会被带到另一条分支

## 合并到 main 的步骤（验收通过后再做）

1. 对照文首的「验收标准」，四项全部通过
2. `git checkout main && git pull`
3. `git merge feature/btree`
4. 遇到 `CONFLICT (add/add): README.md` 是正常的，保留 main 的版本：
   `git checkout --ours README.md && git add README.md && git commit`
5. `cmake -S . -B build && cmake --build build`，确认 main 上也能编译
6. `git push`
7. 打标签：`git tag -a v0.3.0 -m "B+ 树三层验证通过"` 然后 `git push origin v0.3.0`