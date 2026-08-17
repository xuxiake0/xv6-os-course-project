# Lab 08：Locks

## 1. 实验目的

通过重构物理页分配器与 buffer cache，理解多核锁争用的来源、数据结构分片、原子引用计数和死锁规避。

## 2. 实验环境

- 分支：`lock`
- 官方原始提交：`281b66cf19660eb15c4542b63693c74a9ced0467`
- E 盘 WSL2 Ubuntu、3 CPU QEMU、RISC-V GCC 15.2.0
- 使用实验分支内置的 spinlock test-and-set 统计

## 3. 相关 xv6 原理

原始 allocator 的所有 CPU 共用 `kmem.lock` 与一条 freelist；原始 bcache 也用一把全局锁保护查找、引用数和 LRU 链。数据结构本身强制所有并行操作串行化，CPU 越多，spin test-and-set 次数越高。

优化的前提是不破坏不变量：每个空闲物理页只能位于一条 freelist；每个 `(dev, blockno)` 最多有一个缓存副本；refcnt 非零的 buffer 不能被换出。

## 4. 实验任务

1. 把 allocator 改为每 CPU freelist，空列表时从其他 CPU 偷取页面。
2. 把 bcache 改为哈希桶查找和每桶锁，把争用总数降到 500 以下。
3. 保持 `sbrkmuch`、bcache 大工作集与完整 usertests 正确。

## 5. 原始代码分析

基线 `kalloctest test1` 因单一 kmem 锁失败；`bcachetest test0` 的争用总数为 62783，也远高于阈值。容量测试、sbrkmuch、bcache test1 和 usertests 仍通过，基线得分 `49/70`。记录见 `results/lock/baseline-grade.txt`。

## 6. 设计思路

- `kmem[NCPU]` 每项拥有独立锁和 freelist，free 回当前 CPU，alloc 优先当前 CPU。
- 本地为空时逐个查看 donor，并一次偷走约一半链表，摊薄跨 CPU 锁成本。
- bcache 使用 13 个质数桶；命中只获取目标桶锁。
- miss 由 `bcache.evict` 串行化，并按固定顺序锁定所有桶，原子完成二次查找、LRU victim 选择和换桶，保证唯一副本且无锁序反转。
- `brelse/bpin/bunpin` 用原子 refcnt，避免共享元数据块在释放路径争 bucket lock。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/kalloc.c` | 每 CPU freelist、关中断取 cpuid、批量偷取 |
| `kernel/bio.c` | 13 桶哈希、桶锁、串行 eviction、原子 refcnt |
| `kernel/buf.h` | 增加最后闲置 tick 时间戳 |

提交：`30d31d8`（allocator）、`46c5619`（bcache）。

### 7.2 allocator 偷取

`kalloc()` 在 `push_off()` 区间固定 CPU ID。本地为空时持有一个 donor 锁，统计其链表并切下约一半；第一页直接返回，其余页接到本地 freelist。任一时刻只持有一个 kmem 锁，避免两个 CPU 互偷导致死锁。

### 7.3 bcache miss

快速命中不碰全局锁。miss 获取 eviction lock 后按编号升序获取全部桶锁，重新检查目标是否已被并发插入；否则选择 `refcnt==0` 且 timestamp 最小的 victim，从旧桶摘除并插入新桶。所有 miss 同序持锁，快速路径最多持一把桶锁。

## 8. 执行流程分析

常规页分配/释放只操作当前 CPU 链表，三个 kalloctest 进程因此并行。只有分配不均衡时才跨 CPU 偷页，而且一次转移一批，后续请求重新本地化。

`bread()` 的 bget 先哈希定位；命中后原子加 ref 并获取该 buffer 的 sleeplock。释放先放 sleeplock，再原子减 ref；最后一个引用记录 ticks。换出只在 miss 慢路径发生，且所有桶锁阻止查找与身份更新交错。

## 9. 测试方法

```bash
./grade-lab-lock "kalloctest: test1" "kalloctest: test2"
./grade-lab-lock "bcachetest: test0" "bcachetest: test1"
./scripts/run-current-lab.sh
```

## 10. 测试结果

2026-08-18 正式评分：kalloctest test1/test2、sbrkmuch、bcachetest test0/test1、usertests、time 全部 OK，`Score: 70/70`。完整输出在 `results/lock/grade.txt`，测试前提交 `f6f72f5357e1910bb26699572dbc23bd9551c2da`。

定向 bcache 测得总争用 `163`，相对基线 `62783` 下降约 99.7%，并低于 500 阈值。

## 11. 遇到的问题

1. `cpuid()` 只有在中断关闭、线程不会迁移时才可安全用于数组索引。
2. 单页偷取会频繁访问 donor，批量全偷又会让空热点在 CPU 间摆动。
3. bcache 首版 13 桶仍因 brelse 对热点桶加锁得到 1259 次争用。
4. miss 中同时移动旧桶与新桶容易产生重复缓存或锁序死锁。

## 12. 问题原因

per-CPU 结构要求“选择 CPU”与实际操作期间 ID 稳定。bcache refcnt 更新频率远高于换出，若释放也获取桶锁，共享 inode/metadata block 会形成热点。两个 miss 若仅在各自桶内查找后分配，可能同时建立相同 block。

## 13. 解决方法

- 用 `push_off/pop_off` 包住 cpuid 及 allocator 操作。
- donor 一次切半，且任何时刻只持一把 kmem 锁。
- refcnt 使用 `__sync_add_and_fetch/__sync_sub_and_fetch`；桶锁聚焦身份和链表。
- miss 统一进入 eviction lock，按固定顺序锁全桶并二次查找。

## 14. 实验结果分析

kalloctest test1 证明热点锁被拆散，test2 和 sbrkmuch 证明页面没有因分片而“困”在其他 CPU。bcache test0 验证常规并行读取，test1 使用超过 NBUF 的块验证换出与唯一副本。完整 usertests 通过说明日志 pin/unpin 和文件系统语义保持正确。

## 15. 实验总结

降低争用不是简单增加锁，而是让锁与可独立变化的数据对应。allocator 适合按 CPU 分片；bcache 的 block 真正共享，适合哈希桶快速路径加串行、低频 eviction 慢路径。
