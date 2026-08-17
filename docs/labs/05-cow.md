# Lab 05：Copy-on-Write Fork

## 1. 实验目的

通过写时复制优化 `fork()`，理解页表间接层、page fault、软件 PTE 位以及共享物理页的引用计数管理。

## 2. 实验环境

- 分支：`cow`
- 官方原始提交：`c9818915934504523e52a33e2755b7aff54c495e`
- WSL2 Ubuntu 26.04、RISC-V GCC 15.2.0、QEMU 10.2.1
- Ubuntu 与工具链位于 E 盘，公共配置见 `docs/environment.md`

## 3. 相关 xv6 原理

原始 `fork()` 为子进程逐页分配物理内存并复制父进程内容，时间和空间开销都与地址空间大小成正比。COW 让父子页表先指向同一物理页，并移除可写权限；真正写入时由 store page fault 延迟分配副本。

一个物理页可能同时被多个页表引用，因此 `kfree()` 不能在任一映射消失时立即回收，而应仅在引用计数降为 0 时放回 freelist。

## 4. 实验任务

1. 修改 `uvmcopy()`，共享而不是复制物理页。
2. 用 RISC-V RSW 位标记 COW 页并处理写 page fault。
3. 为可分配物理页维护并发安全的引用计数。
4. 让内核 `copyout()` 写 COW 用户页时执行同样的拆分逻辑。

## 5. 原始代码分析

原始 `uvmcopy()` 对每页调用 `kalloc()` 和 `memmove()`。当 `cowtest` 分配约三分之二物理内存后再 fork，子进程无法取得等量页面，立即出现 `simple: fork() failed`。

基线评分为 `29/110`：原有 `usertests` 及其 copyin/copyout 项通过，所有 COW 测试失败，且缺少时间文件。记录见 `results/cow/baseline-grade.txt`。

## 6. 设计思路

- 使用 RSW 的 bit 8 定义 `PTE_COW`，只把原本可写的映射改为只读 COW；真正只读的代码页保持普通只读。
- `uvmcopy()` 让父子共享 PA，清除父子 `PTE_W` 并增加引用数。
- `cowalloc()` 统一完成 COW 校验、复制、PTE 更新、TLB 刷新和旧页减引用。
- 引用数为 1 时不再复制，直接清除 COW 并恢复写权限。
- 用户写 fault 与内核 `copyout()` 都调用 `cowalloc()`，避免两套不一致逻辑。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/riscv.h` | 定义软件位 `PTE_COW` |
| `kernel/kalloc.c` | 引用计数数组、加引用和按计数释放 |
| `kernel/defs.h` | 导出引用计数及 COW 辅助函数 |
| `kernel/vm.c` | COW `uvmcopy`、`cowalloc`、COW-aware `copyout` |
| `kernel/trap.c` | 处理 scause 15 的 store page fault |

核心提交：`15ac3e6`。

### 7.2 引用计数

引用数组以 `(pa - KERNBASE) / PGSIZE` 为索引，只覆盖实际 128 MiB RAM。`kalloc()` 把新页计数置 1，`uvmcopy()` 每建立一个共享映射加 1，`kfree()` 先减 1，只有结果为 0 才清理并加入 freelist。所有操作复用 `kmem.lock` 串行化。

### 7.3 COW 拆分

```c
if(krefcount(pa) == 1){
  *pte = PA2PTE(pa) | writable_flags;
  sfence_vma();
  return 0;
}
mem = kalloc();
memmove(mem, (char *)pa, PGSIZE);
*pte = PA2PTE((uint64)mem) | writable_flags;
kfree((void *)pa);
```

无空闲页或 fault 不是合法 COW 映射时返回失败，用户 fault 路径随后杀死进程。

## 8. 执行流程分析

`fork()` 遍历父地址空间，父子 PTE 指向同一 PA。对原可写页，两者都清除 `PTE_W` 并设置 `PTE_COW`。任一进程写该 VA 时，CPU 产生 store page fault；内核读取 `stval`，调用 `cowalloc()` 分配/复制并把当前进程 PTE 恢复为可写，另一个进程仍指向原页。

当 pipe/read 等内核代码通过 `copyout()` 写用户缓冲区时，CPU 使用内核直接映射，硬件不会按用户 PTE 产生 fault。因此 `copyout()` 必须显式检查 PTE，遇到 COW 页先拆分再写物理地址。

## 9. 测试方法

```bash
make clean
make fs.img
./grade-lab-cow simple three file
./scripts/run-current-lab.sh
```

手动进入 xv6 后运行 `cowtest`，确认多轮测试结束后仍能回收内存。

## 10. 测试结果

2026-08-18 完整 grader：

```text
simple: OK
three: OK
file: OK
usertests: copyin: OK
usertests: copyout: OK
usertests: all tests: OK
time: OK
Score: 110/110
```

完整输出在 `results/cow/grade.txt`，测试前提交为 `e8c2c0275f99c3fd57eb06cda39bbfef2f57a6ee`；手动输出在 `results/cow/manual.txt`。

## 11. 遇到的问题

1. 初始化 allocator 时，freerange 本身也通过 `kfree()` 建立 freelist。
2. fork 失败回滚时，已经共享的子映射必须正确减引用。
3. 清除父 PTE 写权限后，CPU 可能仍缓存旧的可写 TLB 项。
4. 内核写用户页不会自然触发用户页表的写 fault。

## 12. 问题原因

引用计数改变了 `kfree()` 的前置条件，初始化自由页必须先赋一个临时引用再减到 0。页表更新与 CPU TLB 并非自动同步。`copyout()` 通过内核可访问的 PA 写入，绕过了用户态权限检查。

## 13. 解决方法

- `freerange()` 先把页面计数设为 1，再调用统一 `kfree()`。
- 子映射建立前加引用，失败时立刻 `kfree()`；整体失败由 `uvmunmap(..., 1)` 回滚已映射部分。
- 修改父 PTE 和 COW 拆分后执行 `sfence_vma()`。
- `copyout()` 验证 `MAXVA/V/U/W/COW` 并主动调用 `cowalloc()`。

## 14. 实验结果分析

`simple` 证明 fork 不再按地址空间大小立即复制；`three` 让三个进程写大量共享页并反复运行，覆盖引用数和退出回收；`file` 验证内核 copyout 不会污染父进程。完整 usertests 通过说明只读代码、非法地址、fork/exec/sbrk 等原有语义保持正确。

## 15. 实验总结

COW 把 fork 的成本从“复制所有页面”改为“只复制真正发生写入的页面”。优化成立的前提是三个不变量：COW 页不可直接写、所有映射都有准确引用计数、用户 fault 与内核写回使用同一个拆分规则。
