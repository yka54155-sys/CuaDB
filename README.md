# CuaDB

用 C++ 从零实现的小型关系型数据库，作为毕业设计《基于 C++ 和 Lua 的小型关系型数据库管理系统的设计与实现》的代码主体。

目标是走完「页缓存 → B+ 树 → SQL 前端 → Lua 扩展」这条链路，最后把 **Lua 变成可查询的扩展点**：
`WHERE lua('mask_phone', phone) = '138****1111'`，让脚本读外部资源、结果直接参与过滤。

## 目录结构

```
include/        头文件：页布局与节点、表与迭代器、SQL 语句、元命令
src/main.cpp    程序入口与 REPL
tool/           独立的落盘检查工具（read_db）
test/           测试工程
```

## 构建

依赖：CMake ≥ 3.10、支持 C++20 的编译器（GCC 13 及以上，Windows 可用 MSYS2 / MSVC）。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./bin/CuaDB test.db
```

> 任务书写作「C++17」；实际实现使用了 C++20 的指定初始化器（`{.is_root = true}`），
> 故以 C++20 构建。论文中如实说明该差异。

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
- 元命令：`.exit` 退出、`.btree` 打印 B+ 树、`.constant` 打印页布局常量

## 分支说明

| 分支 | 内容 | 状态 |
|---|---|---|
| `main` | 列表存储版（整表按行数组保存，无节点结构） | 可编译可运行，保留作为基线 |
| `feature/btree` | 迁移到 B+ 树（新增 `include/Node.hpp`） | 开发中：查找、迭代器跨页已打通；非根分裂待做 |

> B+ 树相关的进度都在 `feature/btree` 上，`main` 的代码仍是列表版。
> 下面的进度按**项目整体**记录。

## 进度

> `- [x]` 已完成 · `- [ ]` 待完成 · 带 🚧 的是当前阶段
> 最后更新：2026-11-28

### 阶段 0 · B+ 树跑通（🚧 进行中，代码在 `feature/btree`）

- [x] 叶子节点：初始化、有序插入、二分查找、打印
- [x] 叶子分裂（`leaf_node_split_and_insert`）
- [x] 内部节点访问器（子页、分界键、键数、最右子、父指针）
- [x] `create_new_root` 根分裂
- [x] 递归查找：`find` 循环下降，支持任意层
- [x] 迭代器跨页：`begin` 下潜最左叶、`end` 下潜最右叶、`operator++` 跳页、`next_leaf_page` 靠父指针上溯
- [x] 两层树 `select` 顺序与条数正确
- [ ] 父节点更新（非根分裂）：`update_internal_node_key` + `internal_node_insert`
- [ ] 内部节点分裂
- [ ] `.btree` 递归打印整棵树
- [ ] 验收：内部节点容量改 3，插 200 行不崩、三层树 `select` 正确

> 🚧 当前卡点：非根分裂未实现 → 叶子分裂后父节点收不到新的 `(max_key, child)` cell，
> 这些叶子成为孤儿页，`select` 只能看到根可达的那部分（数据没丢，但查不到）。

### 阶段 1 · 页格式与记录编码（按 SQLite 标准）

- [ ] 100 字节文件头（魔数、页大小、页数、freelist、schema cookie）
- [ ] 页类型 5/13，页头 12/8 字节，槽目录 2 字节/项
- [ ] 记录编码：varint + serial type（12 种）+ 头部自指
- [ ] payload 分数公式、overflow 溢出页链
- [ ] freeblock 链 + 碎片阈值 + 整理
- [ ] 去掉 `parent` / `is_root`，改游标路径栈
- [ ] 与真实 `sqlite3` 生成的库逐字段 hexdump 对照

### 阶段 2 · 缓冲区与删除

- [ ] 帧池 + LRU 淘汰 + 脏页写回 + 钉住（`PageGuard`）
- [ ] `--no-cache` 直通模式（用于"开关缓冲区"对比实验）
- [ ] 删除：叶子删 cell → 兄弟借用 → 节点合并 → 根塌陷
- [ ] freelist（trunk/leaf 两级）页回收
- [ ] 范围查询（游标 seek + next）

### 阶段 3 · SQL 前端

- [x] `insert` / `select` 基本语句（按空格切分）
- [ ] 词法分析（关键字、标识符、数字、字符串、注释）
- [ ] LL(1) 递归下降语法分析 → AST（CREATE / INSERT / SELECT / DELETE / SHOW）
- [ ] 表达式求值与三值逻辑
- [ ] WHERE 五种比较 + 等值条件自动走 B+ 树索引
- [ ] 结果表格化输出、语法错误带位置

### 阶段 4 · 系统目录与多表

- [ ] `cuadb_master` 目录表（自举）、schema blob
- [ ] `CREATE TABLE` / `SHOW TABLES` / `SHOW INDEX`
- [ ] 约束：主键唯一、NOT NULL、VARCHAR 长度、类型检查
- [ ] `Value`/`Tuple` 替换定长 `Row`

### 阶段 5 · Lua 扩展引擎（论文核心）

- [ ] M0 求值上下文打包（行为不变）
- [ ] M1 嵌入 Lua 5.4 + 函数注册表 + `.load` / `.funcs`
- [ ] M1 `WHERE lua('func', col)` 谓词与取值两种用法
- [ ] 值封送（int64 精度、UTF-8 长度、NULL/nil）
- [ ] M2 能力分级 SAFE / EXTERNAL / UNSAFE + 沙箱（禁用 `os.execute`、写模式 `io`、`debug`、`package`）
- [ ] M2 指令预算与超时
- [ ] M3 聚合函数 / 自定义排序规则（时间紧可裁剪）

### 阶段 6 · 测试、实验与论文

- [ ] `SHOW INDEX` 树形打印 + 页数/深度/填充率
- [ ] 功能测试用例（等价类、边界值、异常注入）
- [ ] 性能实验：有无索引、有无缓冲区、Lua UDF vs 内建
- [ ] 论文初稿（2027-03-15）→ 定稿（2027-04-18）→ 答辩（2027-05-15）

## 关键设计决策

| 决策 | 选择 | 理由 |
|---|---|---|
| 页号 | 0 基（页 0 含 100 字节文件头） | 沿用现有代码；SQLite 叫 page 1，文档注明差异 |
| 页格式 | 按 SQLite 官方格式实现 | 每处细节有出处、能与真实 SQLite 逐字段对照 |
| 记录编码 | serial type + varint 变长 | 空间效率远高于定长字段 |
| 跨页遍历 | 先父指针上溯，后改游标路径栈 | 阶段 0 少改动，阶段 1 与 SQLite 对齐 |
| 事务 | 不做 | 任务书未要求；局限在论文第 7 章说明 |

## 已知问题

1. `main` 上仍是列表存储版，B+ 树代码在 `feature/btree`
2. 列表中最多 `TABLE_MAX_ROWS`（100 × 13 = 1300）行，超出后页号越界无检查，会写坏内存
3. `feature/btree` 上非根分裂未实现，插入第 21 行起部分语句 FAILED 并产生孤儿页
4. 元命令用整行原文精确匹配，首尾带空格时无法识别
5. `insert` 按空格切分，用户名不能含空格；输入超长时未做长度检查
6. `.btree` 只打印根页，树分裂后输出不正确

## 开发约定

- 提交信息前缀：`feat:` / `fix:` / `docs:` / `chore:`
- 功能开发在 `feature/` 分支，通过验收后合并回 `main`
- `main` 分支始终保持可编译
- 每次改动前 `git status` 确认工作区干净；测试库一律新建（页格式一改旧库即作废）