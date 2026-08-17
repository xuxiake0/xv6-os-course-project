# Lab 10：mmap

## 1. 实验目的

为 xv6 实现文件支持的 `mmap/munmap`，综合运用虚拟地址空间、页故障、文件引用、共享回写与进程生命周期管理。

## 2. 实验环境

- 分支：`mmap`
- 官方原始提交：`2255a4ca32a31faeacd902855b77080e3ccbdd8f`
- E 盘 WSL2 Ubuntu、3 CPU QEMU、RISC-V GCC 15.2.0
- 官方 `mmaptest` 与完整 `usertests`

## 3. 相关 xv6 原理

页表可以先保留虚拟地址范围而不建立有效 PTE。用户首次访问时产生 load/store page fault，内核依据 VMA 找到文件和偏移，分配物理页、读取文件并建立映射。这样映射成本与实际访问页数相关，而不是与声明长度相关。

VMA 还必须持有独立的 file 引用，因为用户可以在 `mmap()` 后关闭原 fd。`MAP_PRIVATE` 修改只留在私有物理页；`MAP_SHARED` 在解除映射或进程退出时写回文件。

## 4. 实验任务

1. 添加 `mmap`、`munmap` 系统调用与用户接口。
2. 用固定大小 VMA 表记录地址、长度、权限、模式、文件与偏移。
3. 在用户页故障中延迟分配并载入文件页。
4. 支持整个、前缀和后缀范围的 `munmap` 及共享回写。
5. 在 `fork`、`exit` 和 `exec` 中正确管理映射与文件引用。

## 5. 原始代码分析

原始分支没有把 `mmaptest` 加入镜像，也没有系统调用和 VMA 数据结构。八个 mmap 子项全部失败，原版 `usertests` 通过；未填写时间项时基线为 `19/140`，见 `results/mmap/baseline-grade.txt`。

## 6. 设计思路

- 每进程设置 16 个 VMA 槽，从 `TRAPFRAME` 下方向低地址分配，避免改变普通堆的 `p->sz`。
- `mmap()` 只登记区域并 `filedup()`，不分配任何用户页。
- 页故障按 `scause` 校验读/写/执行权限，按页读取文件，不足部分保持为零。
- `munmap()` 只处理整段或首尾部分；未触页的 PTE 直接跳过。
- 子进程复制 VMA 元数据与 file 引用，但不复制高地址 mmap 物理页，访问时自行 fault-in。
- `exit/exec` 在释放旧页表前解除全部 VMA。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/proc.h` | `struct vma`、16 槽 VMA 表 |
| `kernel/sysfile.c` | `sys_mmap()` 参数/权限校验与 `sys_munmap()` |
| `kernel/proc.c` | fault-in、回写/unmap、fork/exit 生命周期 |
| `kernel/trap.c` | 12/13/15 页故障转入 VMA 处理 |
| `kernel/exec.c` | 替换地址空间前清理旧 VMA |
| `kernel/syscall.*`、`user/*` | 系统调用号、分发表和 stub |
| `Makefile` | 将 `mmaptest` 加入镜像 |

实现提交：`054a84f`。

### 7.2 关键数据结构

VMA 保存 `used/addr/length/prot/flags/file/offset`。映射区域使用页对齐长度；file 指针来自 `filedup()`，直到 VMA 完全删除才由 `fileclose()` 释放。

### 7.3 关键代码

`vma_fault()` 用故障地址查找所属 VMA，检查故障类型与 `prot`，分配并清零一页，从 `offset + page_offset` 读取最多 4096 字节，再以相应 PTE 权限映射。`vma_unmap()` 对已存在的页执行共享回写和 `uvmunmap(..., do_free=1)`，最后缩短或清空 VMA。

## 8. 执行流程分析

`mmap` 返回高地址但不产生 I/O。用户解引用该地址后，CPU 因无有效 PTE 陷入 `usertrap()`；内核加载文件页并返回，原指令自动重试。`MAP_PRIVATE` 的物理页只属于当前进程。`MAP_SHARED` 的可写页在 unmap 时按页进入日志事务并调用 `writei()`。

`fork` 的 `uvmcopy()` 只覆盖低于 `p->sz` 的普通内存，高地址 VMA 不在其中；子进程复制 VMA 后首次访问自行载入。退出与成功 exec 均先解除映射，避免 `freewalk()` 遇到未清理的高地址叶子 PTE。

## 9. 测试方法

```bash
make -j4 kernel/kernel user/_mmaptest
./grade-lab-mmap -v mmaptest
./grade-lab-mmap -v
```

## 10. 测试结果

2026-08-18：定向八项全部 OK；完整评分中八项 mmaptest、`usertests`（163.1 秒）和 time 全部 OK，最终 `Score: 140/140`。输出见 `results/mmap/targeted.txt`、`results/mmap/grade.txt`。

## 11. 遇到的问题

1. 若把 mmap 区域计入 `p->sz`，局部 unmap 形成的空洞会破坏原版连续释放假设。
2. fd 关闭后映射仍必须有效。
3. 从未访问的页没有 PTE，不能直接调用要求映射存在的 `uvmunmap()`。
4. `fork`、`exit` 之外，成功 `exec` 同样会销毁旧地址空间。

## 12. 问题原因

原 xv6 用单个 `sz` 描述从地址 0 开始的连续用户内存，不适合带空洞的 VMA。file descriptor 与 `struct file` 生命周期不同；只保留 fd 无法抵抗 close。惰性分配意味着 VMA 和 PTE 的存在性也不同。页表递归释放要求所有叶子映射事先移除。

## 13. 解决方法

- VMA 从 `TRAPFRAME` 下方向下分配，与低地址连续内存分离。
- 每个 VMA 调用 `filedup/fileclose` 管理独立引用。
- unmap 前用 `walk()` 检查 PTE，只解除实际存在的页。
- 把统一的 `vma_unmap_all()` 接入 `exit` 和 exec 成功提交点。
- fork 复制 VMA 元数据和文件引用，让子进程惰性载入独立物理页。

## 14. 实验结果分析

基线从 19 分提升为 140 分。只读与读写测试验证 PTE/文件权限；dirty 测试验证共享回写；not-mapped 测试验证 VMA 与 PTE 解耦；two-files 验证地址范围和文件引用隔离；fork_test 验证跨进程元数据复制与引用计数。完整 usertests 通过说明高地址映射未破坏原有堆、栈和进程管理。

## 15. 实验总结

`mmap` 把文件系统、页表、trap 和进程生命周期连接在一起。正确实现的核心是区分“虚拟区域已登记”和“物理页已存在”，并让 VMA、PTE、物理页与 file 引用在 fault、unmap、fork、exit、exec 各路径上成对获取和释放。

