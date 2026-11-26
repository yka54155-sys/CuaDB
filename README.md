# 当前分支：feature/btree

⚠️ 本分支正在把存储层从「列表版」迁移到「B+ 树」。**这里不是稳定版本**，
   稳定版在 `main`。

## 目标
补完 B+ 树分裂：内部节点访问器 → 根分裂 → 父节点挂接 → 内部节点分裂 → 递归查找

## 下一步
- 实现 `Table::find()` 对 INTERNAL 节点的下降查找（沿 `internal_node_find_idx` 走 child / right_child）

## 当前卡点
- [X] `Table.hpp`、`Node.hpp` 契约注释
- [X] `Table::create_new_root()` 根分裂（14 行插入不再崩溃，页 0 已变为 INTERNAL）
- [ ] 第 15 行起插入返回 FAILED：`find()` 遇内部节点仍直接抛异常 `[DOING]`
- [ ] `select` 结果不正确：`begin()/end()` 仍假设根是叶子，迭代器不跨页
- [ ] `.btree` 只打印根页，无法验证多层结构

## 验收标准（满足才算完成）
- [ ] INTERNAL_NODE_MAX_CELLS=3 时插 200 行不崩
- [ ] `.btree` 打印每一层的 keys
- [ ] `select` 顺序严格递增、条数正确
- [ ] 改回 510 后同样通过
## 注意事项（仅本分支）

- 本分支是开发版，**不稳定**：第 15 行起插入会 FAILED（`find` 尚未实现内部节点下降）
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