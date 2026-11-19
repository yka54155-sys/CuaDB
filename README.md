# CuaDB

用 C++20 从零实现的小型关系型数据库，作为毕业设计《基于 C++ 和 Lua 的小型关系型数据库管理系统的设计与实现》的代码主体。

目标是走完「页缓存 → B+ 树 → SQL 前端 → 事务日志」这条链路，最后把 **Lua 变成可热加载的扩展点**：标量函数、聚合函数、自定义排序规则。

## 目录结构

```
include/        头文件：页布局与节点、表与迭代器、SQL 语句、元命令
src/main.cpp    程序入口与 REPL
test/           独立测试工程
bin/            编译产物（已忽略）
build/          构建目录（已忽略）
```

## 构建

依赖：CMake ≥ 3.10、支持 C++20 的编译器（GCC 13 及以上）。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./bin/CuaDB test.db
```

## 使用

```
CuaDB > insert 1 ann a@b.c
CuaDB > insert 2 bob b@x.c
CuaDB > select
1 | ann | a@b.c
2 | bob | b@x.c
CuaDB > .btree
CuaDB > .constant
CuaDB > .exit
```

- 数据按页（4096 字节）组织，退出时写回文件
- 元命令：`.exit` 退出、`.btree` 打印根页结构、`.constant` 打印页布局常量

## 分支说明

| 分支 | 内容 | 状态 |
|---|---|---|
| `main` | 列表存储版（整表按行数组保存） | 可编译可运行，保留作为基线 |
| `feature/btree` | 迁移到 B+ 树存储 | 开发中 |

## 进度

> `- [x]` 已完成 · `- [ ]` 待完成 · 带 🚧 的是当前正在做的

### 第 0 步 · 工程基础

- [x] 仓库、分支与远端（GitHub）
- [x] CMake 构建（Debug 配置、生成 compile_commands.json）
- [x] README 与进度记录
- [ ] 提交规范与 issue / PR 流程
- [ ] `.clang-format` 与格式化检查

### 第 1 步 · 存储与页

- [x] 页缓存（定长 100 帧，按需载入）
- [x] 页落盘（退出时整体 flush）
- [x] 行序列化 / 反序列化（293 字节定长，不含 padding）
- [x] 头文件契约注释：Row、Pager、Statement、MetaCommand
- [ ] 头文件契约注释：Node、Table
- [ ] 100 字节文件头（魔数、页大小、页数、变更计数器）
- [ ] 页号越界与跳号检查

### 第 2 步 · B+ 树（🚧 进行中）

- [x] 叶子节点：初始化、有序插入、二分查找、打印
- [x] 表迭代器：`find` / `begin` / `end` / `write`
- [x] 叶子分裂（`leaf_node_split_and_insert`）
- [ ] 内部节点访问器（子页、分界键、键数、最右子、父指针）
- [ ] `create_new_root` 根分裂
- [ ] 父节点插入（分界键 + 右子页号）
- [ ] 内部节点分裂
- [ ] 递归查找（`find` 遇内部节点不再抛异常）
- [ ] 迭代器跨页（`next_leaf` + 表末尾标志）
- [ ] `.btree` 递归打印整棵树
- [ ] 验收：内部节点容量改 3，插 200 行不崩

> 🚧 当前卡点：`create_new_root()` 是空实现，插入第 14 行会崩溃。

### 第 3 步 · SQL 前端

- [x] `insert` / `select` 基本语句（按空格切分）
- [ ] 词法分析（关键字、标识符、数字、单引号字符串）
- [ ] 递归下降语法树
- [ ] 表达式求值与 NULL 三值逻辑
- [ ] 列投影 / WHERE / ORDER BY / LIMIT
- [ ] `UPDATE` / `DELETE`
- [ ] 语法错误带位置信息

### 第 4 步 · 存储引擎升级

- [ ] LRU 页缓存（淘汰、脏页、钉住）
- [ ] 禁止页号跳号（分配统一入口）
- [ ] 变长记录（serial type、NULL）
- [ ] 溢出页
- [ ] 系统目录（`cuadb_schema` / 多根页）
- [ ] 多表与 `CREATE TABLE` / `DROP TABLE`
- [ ] 验收：能打开真实 `chinook.db` 并列出表

### 第 5 步 · 事务与崩溃恢复

- [ ] 回滚日志
- [ ] 原子提交（日志落盘 → 主库落盘 → 删日志）
- [ ] `BEGIN` / `COMMIT` / `ROLLBACK`
- [ ] 启动时热日志恢复
- [ ] 验收：插入途中 kill 进程，重启后全有或全无

### 第 6 步 · Lua 插件

- [ ] M0 重构：求值上下文打包（行为不变）
- [ ] 函数注册表（FunctionRegistry）
- [ ] Lua 运行时与沙箱
- [ ] 值封送层（Value ↔ Lua，含 int64 精度）
- [ ] 能力分级（SAFE / EXTERNAL / UNSAFE + 允许矩阵）
- [ ] `.load` / `.funcs` / `.unload` 元命令
- [ ] 聚合函数（`lua_agg`）
- [ ] 自定义排序规则（`COLLATE`，下穿到 B+ 树）
- [ ] 注册持久化（已注册函数存目录表，启动自动加载）
- [ ] 验收：`.load ./udf/str.lua` 后 `SELECT lua('mask_phone', phone) FROM users;`

### 第 7 步 · 收尾

- [ ] Lua UDF vs 内建函数的性能对比实验
- [ ] 索引 / collation 一致性实验
- [ ] 论文各章草稿
- [ ] 5 分钟演示录屏
- [ ] release 版本与标签

## 已知问题

1. 插入第 14 行崩溃（`create_new_root` 未实现）
2. 元命令用整行原文精确匹配，首尾带空格时无法识别
3. `insert` 按空格切分，用户名不能含空格；输入超长时未做长度检查
4. `.btree` 只打印根页，且固定按叶子节点解释，树分裂后输出不正确

## 开发约定

- 提交信息前缀：`feat:` / `fix:` / `docs:` / `chore:`
- 功能开发在 `feature/` 分支进行，完成并通过验收后再合并 `main`
- `main` 分支始终保持可编译