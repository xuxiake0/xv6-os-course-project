# 基于 xv6-riscv 的操作系统实验与实现

## 摘要

本项目基于 MIT 6.S081 Fall 2021 的官方 `xv6-labs-2021`，依次完成 Utilities、System calls、Page tables、Traps、Copy-on-write、Multithreading、Network driver、Lock、File system 和 mmap 十个实验。项目在 Windows 11 + E 盘 WSL2 Ubuntu 环境中使用 RISC-V GCC 与 QEMU 实际编译运行。十个分支的官方 grader 均通过，总成绩 `846/846`。除实现代码外，项目保存了每个实验的基线、正式测试输出、Git 提交和 15 节实验记录，并提供全量复测与答辩演示脚本。

## 1. 项目背景

xv6 是对 Unix Version 6 思想的教学性重实现，代码量较小，但具备进程、虚拟内存、系统调用、文件系统、设备驱动与多核同步等完整操作系统机制。RISC-V 指令集开放、异常模型清晰，适合跟踪从用户指令到内核实现的完整路径。本项目严格使用 2021 RISC-V 实验分支，没有替换为旧版 xv6-x86，也没有修改官方 grader。

## 2. xv6 系统概述

xv6 内核运行在 supervisor mode，用户程序运行在 user mode。每个进程拥有私有页表、trapframe、内核栈和文件描述符表。系统调用、页故障与设备中断统一通过 trap 进入内核。文件系统以 inode 表示文件，通过 buffer cache 与 VirtIO 磁盘交互，并用日志维持崩溃一致性。多核共享结构由 spinlock/sleeplock 保护。

## 3. 实验环境

| 项目 | 实际环境 |
| --- | --- |
| 宿主 | Windows 11 build 22631.6199 |
| WSL | 2.7.11.0，内核 6.18.33.2 |
| Ubuntu | 26.04 LTS，`ext4.vhdx` 位于 `E:\WSL\Ubuntu` |
| QEMU | 10.2.1 |
| RISC-V GCC | 15.2.0 |
| Binutils | 2.46 |
| GDB multiarch | 17.1 |
| Make / Git | 4.4.1 / 2.53.0 |

GCC 15 环境下显式固定 `-march=rv64gc`，并对 2021 上游源码的两个新诊断做最小兼容；Binutils 的教学内核 RWX 段提示也只做定向抑制。Python 3.14 缺少 grader 依赖的 `pipes`，通过 E 盘 Ubuntu 内安装的兼容包恢复，grader 源码未改变。环境与启动原始记录见 `docs/environment.md`。

## 4. xv6 总体架构

### 4.1 用户态与内核态

硬件 privilege mode 隔离用户代码和内核资源。用户代码不能直接访问内核地址或设备，只能通过系统调用请求服务。trampoline 位于每个用户页表的相同高地址，使 trap 切换页表前后都能执行同一段入口/返回代码。

### 4.2 系统调用

用户 stub 将调用号写入 `a7` 并执行 `ecall`。`usertrap()` 调整 `sepc`，`syscall()` 按号调用 handler；参数从 trapframe 或用户地址空间安全复制，返回值写回 `a0`。

### 4.3 进程管理

`allocproc()` 分配进程结构、trapframe 和页表；`fork()` 建立子进程；`exec()` 从 ELF 重建地址空间；`exit()` 释放资源并进入 ZOMBIE；`wait()` 完成最终回收。scheduler 使用 context 保存内核线程的 `ra/sp/s0-s11`。

### 4.4 虚拟内存

Sv39 使用三级页表，4 KiB 页面。`walk()` 逐级查找 PTE，`mappages()` 建立映射。Page Tables Lab 使用共享只读页减少系统调用切换，COW 用只读共享页和引用计数延迟 fork 复制，mmap 用 VMA 和页故障延迟文件载入。

### 4.5 Trap 与 Interrupt

同步 exception 包括 ecall、非法访问和 page fault；异步 interrupt 来自时钟或设备。trapframe 保存完整用户寄存器，kernel context 保存调度所需的 callee-saved 寄存器。用户 alarm 通过修改返回上下文在固定 tick 后执行用户 handler。

### 4.6 调度

每 CPU scheduler 扫描 RUNNABLE 进程并 `swtch()`。sleep/wakeup 以 channel 协调等待条件，并通过锁序避免丢失唤醒。用户线程实验在用户态实现了类似的寄存器与栈切换。

### 4.7 并发与锁

spinlock 适合短临界区，获取期间关闭本 CPU 中断以避免同 CPU 重入。Lock Lab 将物理页 freelist 按 CPU 分片，并将 bcache 查找按 13 桶分片；低频换出路径统一串行并按固定锁序处理，保持唯一缓存副本。

### 4.8 文件系统

路径遍历从目录项得到 inode。inode 保存文件类型、大小和数据块地址。项目把原 12 个直接地址改为 11 个，腾出一槽用于双重间接块，同时保持磁盘 inode 大小不变。日志使块分配、目录修改与 inode 更新成为可恢复事务。

### 4.9 网络

E1000 使用 DMA descriptor ring。TX 填入 packet 地址和长度，设备完成后置 DD；RX 检查完成描述符，将 packet 交给网络栈并补入新缓冲。驱动必须正确处理 ring wrap-around 和 buffer 所有权。

## 5. Utilities 实验

实现 `sleep/pingpong/primes/find/xargs`。核心收获是 pipe EOF 依赖所有无用 fd 均关闭，递归进程流水线必须正确 wait，目录递归必须跳过 `.` 与 `..` 并限制路径长度。官方成绩 `100/100`。

## 6. System Calls 实验

实现 syscall tracing 与 `sysinfo`。trace mask 继承到子进程；系统调用返回后按编号输出参数关联结果。`sysinfo` 遍历空闲页链和进程表并用 `copyout` 返回。官方成绩 `35/35`。

## 7. Page Tables 实验

实现每进程 USYSCALL 只读共享页、递归 `vmprint` 和 `pgaccess`。`pgaccess` 查询并清除 PTE_A，向用户返回位图。实验验证 Sv39 多级结构、PTE_U 权限和硬件访问位语义。官方成绩 `46/46`。

## 8. Traps 实验

完成 RISC-V backtrace 与用户级 periodic alarm。alarm 保存 trapframe、防止 handler 重入，`sigreturn` 恢复中断前上下文。实验串联了时钟中断、trapframe、`sepc` 和用户返回路径。官方成绩 `85/85`。

## 9. Copy-on-write 实验

fork 不再复制全部物理页，而是让父子 PTE 清除 PTE_W、设置软件 COW 位并增加物理页引用。写故障时，若引用唯一则直接恢复可写，否则分配、复制并替换映射。`copyout` 同样走 COW 处理，引用计数受锁保护。官方成绩 `110/110`。

## 10. Multithreading 实验

实现用户线程 `thread_switch`，保存/恢复 `ra/sp/s0-s11`；修复哈希表并发插入的数据丢失；实现 barrier 的 round/generation 机制，防止不同轮次线程混淆。官方成绩 `60/60`。

## 11. Network Driver 实验

补全 E1000 transmit/receive descriptor ring。TX 回收已完成 buffer 并推进 tail；RX 把包交给协议栈、重新提供 buffer 并推进 tail。实际 grader 和 packet capture 证明收发路径工作，官方成绩 `100/100`。

## 12. Lock 实验

allocator 使用 per-CPU freelist，本地空时从 donor 批量偷取。bcache 使用 13 桶、原子 refcnt 与串行 eviction 慢路径。`bcachetest` 总争用由基线 62,783 降至 163，约下降 99.7%，低于 500 阈值；官方成绩 `70/70`。

## 13. File System 实验

双重间接索引把最大文件从 268 块扩大到 `11+256+256²=65,803` 块，`itrunc()` 按叶到根完整回收。符号链接以 inode 数据保存目标，`open()` 默认最多跟随 10 层，`O_NOFOLLOW` 可读取链接 inode。官方成绩 `100/100`。

## 14. mmap 实验

每进程 16 个 VMA 从高地址向下分配。`mmap()` 只登记并持有 file 引用；首次 load/store page fault 才分配页并读取文件。`munmap()` 支持整段和首尾部分，MAP_SHARED 可写页按页回写。fork 复制 VMA、不共享物理页，exit/exec 统一清理。官方成绩 `140/140`。

## 15. 综合测试

| Lab | 基线 | 正式成绩 |
| --- | ---: | ---: |
| util | 0/100 | 100/100 |
| syscall | 5/35 | 35/35 |
| pgtbl | 10/46 | 46/46 |
| traps | 19/85 | 85/85 |
| cow | 29/110 | 110/110 |
| thread | 10/60 | 60/60 |
| net | 0/100 | 100/100 |
| lock | 49/70 | 70/70 |
| fs | 19/100 | 100/100 |
| mmap | 19/140 | 140/140 |
| 合计 | — | **846/846** |

每个正式成绩均来自对应官方分支的真实 grader 输出，保存在 `results/<lab>/grade.txt`。`scripts/run-current-lab.sh` 可复测当前分支，`scripts/run-all-tests.sh` 在隔离克隆中顺序复测十个分支。

## 16. 遇到的问题与解决

1. 新工具链生成 QEMU 默认 CPU 不支持的压缩乘法指令：固定 ISA 为 `rv64gc`。
2. Python 3.14 移除 grader 所需模块：安装兼容模块，不改 grader。
3. COW 释放与并发引用：为每个物理页维护加锁引用计数，并让所有释放路径统一递减。
4. bcache 分桶后仍有热点：把高频 refcnt 改为原子操作，把全桶锁限制在低频 eviction。
5. fs 大镜像在 `/mnt/d` 小块 I/O 过慢：相同源码在 E 盘 WSL ext4 路径完成正式评分并保留说明。
6. mmap 高地址叶子页会影响页表销毁：在 exit 和成功 exec 之前显式解除全部 VMA。

## 17. 项目总结

十个实验形成了从用户工具到内核核心的完整链路。最重要的共同规律是资源生命周期与不变量：fd、inode、物理页、PTE、buffer、descriptor 和锁都必须在所有成功/失败/并发路径中成对管理。通过基线—实现—定向测试—完整 grader—文档—提交的固定流程，项目最终达到可运行、可复现、可解释的课程设计目标。

## 18. 项目仓库

- MIT 官方上游：`git://g.csail.mit.edu/xv6-labs-2021`
- 本地项目：当前 Git 仓库，十个同名实验分支均已提交
- 个人远程仓库：尚未配置；提交课程报告前应由项目所有者创建远程并在此补充链接
