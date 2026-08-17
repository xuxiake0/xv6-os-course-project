# Lab 06：Multithreading

## 1. 实验目的

理解用户级线程上下文切换、pthread 互斥锁的粒度选择，以及用条件变量实现可复用 barrier 的方法。

## 2. 实验环境

- 分支：`thread`
- 官方原始提交：`7e0a45c6e73c5552d913ae49d090b9a11bbd95f9`
- xv6 部分运行于 QEMU；`ph` 与 `barrier` 直接运行于 E 盘 WSL2 Ubuntu
- RISC-V GCC 15.2.0、宿主 GCC、pthread

## 3. 相关 xv6 原理

协作式用户线程共享同一进程地址空间，但各自需要独立栈和寄存器上下文。按照 RISC-V ABI，调用者会自行保护 caller-saved 寄存器，所以切换函数只需保存 `ra`、`sp` 和 `s0-s11`。

并行程序的锁粒度同时影响正确性和性能。哈希桶头更新是 read-modify-write 临界区；不同桶互不影响，因此每桶锁比全表锁有更高并行度。条件变量则允许线程在释放 mutex 的同时休眠，并在唤醒后重新持锁检查条件。

## 4. 实验任务

1. 补全 `uthread` 的线程创建、调度和汇编上下文切换。
2. 解释并修复并发哈希表丢 key，且保留两线程加速。
3. 实现支持连续 20000 轮的 pthread barrier。

## 5. 原始代码分析

原始 `thread_switch` 与线程初始上下文为空，`uthread` 无法产生预期序列。`ph` 的并发 put 会覆盖同一桶头，实测两线程缺失 16638 个 key。`barrier()` 为空，线程首轮即可能触发断言。

基线评分 `10/60`，只有不保证正确性的 `ph_fast` 通过。输出见 `results/thread/baseline-grade.txt`。

## 6. 设计思路

- 为每个线程保存 14 个 ABI callee-saved 寄存器；新线程的 `ra=func`、`sp=独立栈顶`。
- 调度器先更新状态与 `current_thread`，再把旧/新 context 地址交给汇编切换。
- `ph` 为 5 个 bucket 各建一把 mutex，put/get 只锁目标桶。
- barrier 在锁内记录当前 generation；最后到达者清零计数、推进 round 并 broadcast，其余线程在 while 中等待 round 改变。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `user/uthread.c` | context、独立栈、线程创建和调度 |
| `user/uthread_switch.S` | 保存/恢复 `ra/sp/s0-s11` |
| `notxv6/ph.c` | 每桶 pthread mutex |
| `notxv6/barrier.c` | mutex + condition variable + generation |
| `answers-thread.txt` | 丢 key 交错过程分析 |

提交分别为 `01a8c8e`、`ccc99ad`、`7d7a7f9`。

### 7.2 线程初始现场

```c
t->context.ra = (uint64)func;
t->context.sp = ((uint64)t->stack + STACK_SIZE) & ~15;
```

首次恢复该 context 后，`ret` 跳到线程函数；16 字节栈对齐满足 ABI。

### 7.3 可复用 barrier

```c
int this_round = bstate.round;
if(++bstate.nthread == nthread){
  bstate.nthread = 0;
  bstate.round++;
  pthread_cond_broadcast(&bstate.barrier_cond);
} else {
  while(this_round == bstate.round)
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
}
```

## 8. 执行流程分析

`thread_yield()` 把当前线程改为 RUNNABLE；调度器循环选择下一个线程，设置 RUNNING 后切换 context。旧线程之后再次被选中时，恢复的 `ra/sp` 使 `thread_switch()` 像普通函数返回一样回到原调用点。

哈希表两个 put 若命中同一桶，必须在同一把 bucket lock 下完成查找与头插；命中不同桶时使用不同锁，可在两个 CPU 上并行。barrier 的 round 则区分相邻批次，防止快线程离开后进入下一轮污染上一轮计数。

## 9. 测试方法

```bash
./grade-lab-thread uthread
./grade-lab-thread ph_safe
./grade-lab-thread ph_fast
./grade-lab-thread barrier
./scripts/run-current-lab.sh
```

另手动运行 `./ph 1`、`./ph 2`、`./barrier 1/2/4` 和 xv6 `uthread`。

## 10. 测试结果

完整 grader 于 2026-08-18 得分 `60/60`，所有 `uthread / answers / ph_safe / ph_fast / barrier / time` 项均为 OK。正式输出在 `results/thread/grade.txt`，测试前提交为 `0f6fa0596b25ced58f6211e0c88f24c8073dc8e4`。

手动实测两线程 put 为 27364 次/秒，单线程为 18123 次/秒，约 1.51 倍，且两个 get 线程均缺失 0 个 key；barrier 在 1、2、4 线程下均通过。摘要见 `results/thread/manual.txt`。

## 11. 遇到的问题

1. 新线程从未执行过，必须人工构造第一次 `ret` 所需现场。
2. 全表锁虽能消除丢 key，却可能无法满足加速要求。
3. 条件变量允许伪唤醒，快线程还可能提前进入下一轮。

## 12. 问题原因

上下文切换不是新函数调用，恢复侧必须已有合法栈与返回地址。同桶头插由读取旧头和写新头组成，缺锁时两个写会互相覆盖。barrier 若只等待计数等于线程数，计数复用会混淆相邻轮次。

## 13. 解决方法

- 按 ABI 保存必要寄存器，并给新线程设置对齐栈顶与入口 `ra`。
- 使用每桶锁，把临界区限制在单桶，同时保留跨桶并行。
- 用 `round` 作为 generation，条件等待放在 while 循环中，最后到达者推进轮次后广播。

## 14. 实验结果分析

`uthread` 的固定交错序列验证每次 yield 都能精确回到旧线程位置。`ph_safe` 验证线性化正确性，`ph_fast` 验证锁粒度没有抹掉并行收益。20000 轮 barrier 测试覆盖了重复使用和调度抖动。

## 15. 实验总结

本实验从单核协作切换延伸到真实多核共享内存同步。核心经验是：上下文必须遵守 ABI，临界区必须覆盖完整不变量，而锁粒度和 generation 设计决定并发程序能否同时正确、可复用且具备性能。
