# 项目答辩提纲与现场手册

## 1. 开场说明（约 1 分钟）

本项目基于 MIT 6.S081 Fall 2021 官方 xv6-riscv，按官方顺序完成十个独立分支。所有 Lab 都经历原始基线、实现、定向测试和完整 grader；十项成绩合计 `846/846`。重点展示 COW、页表/trap、文件系统与 mmap，因为它们最能体现内核资源生命周期和跨子系统协作。

## 2. 推荐现场流程

在 Windows Terminal 中进入 E 盘 WSL Ubuntu，再进入项目：

```bash
wsl -d Ubuntu
cd '/mnt/d/Users/zzb426/Desktop/资料/课程/专业课/大二下/操作系统课程设计/xv6-labs-2021'
git status --short
./scripts/check-env.sh
```

优先演示 mmap（冷编译、启动、现场运行）：

```bash
./scripts/defense-demo.sh mmap demo
```

进入 `$` 后执行 `mmaptest`，退出时按 `Ctrl-a x`。若需展示官方评分：

```bash
./scripts/defense-demo.sh mmap grade
```

备选：`util`、`syscall`、`cow`、`fs`。`fs` 现场只运行 `symlinktest`；完整 `bigfile` 需要写 65,803 块，在 D 盘挂载路径可能超过 180 秒，正式满分日志已保存在 `results/fs/grade.txt`。

## 3. xv6 基础

### 什么是 xv6？为什么选择 RISC-V？

xv6 是 MIT 用于教学的 Unix V6 风格操作系统重实现。它规模小、结构清楚，但具备进程、系统调用、页表、文件系统、多核同步和设备驱动。RISC-V 开放、特权级与 trap 规范清晰，`ecall/sret/satp/scause/sepc/stval` 都能直接对应源码。

## 4. System Call

### 系统调用完整路径是什么？

用户函数 → `usys.S` stub → `a7=系统调用号` → `ecall` → trampoline `uservec` → `usertrap()` → `syscall()` 分发 → `sys_xxx()` → 返回值写 `a0` → `usertrapret/userret` → `sret`。

### ecall 做了什么？

CPU 从 user mode 陷入 supervisor mode，记录异常原因和返回 PC，并跳到 `stvec` 指定入口。它不自动保存全部通用寄存器，所以 trampoline 要把用户寄存器写入 trapframe。

## 5. Process

### fork、exec、wait 分别做什么？

- `fork`：分配 proc、页表和 trapframe，复制或共享地址空间，复制文件引用，使子进程从相同位置返回 0。
- `exec`：读取 ELF，构造新地址空间和用户栈，成功后替换旧页表；PID 不变。
- `wait`：等待子进程进入 ZOMBIE，取回状态并最终释放 proc。

### process 与 thread 的区别？

进程通常拥有独立地址空间和资源表；同进程线程共享地址空间和大部分资源，但各自有栈、寄存器上下文和调度状态。用户线程切换至少保存 `ra/sp/s0-s11`。

## 6. Virtual Memory

### 什么是页表和 PTE？Sv39 是什么？

页表把虚拟页映射到物理页，PTE 保存物理页号与 V/R/W/X/U/A 等位。Sv39 的有效虚拟地址使用 39 位：三级各 9 位索引，加 12 位页内偏移；每页 4 KiB，每级 512 项。

## 7. Trap

### trap、exception、interrupt 的区别？

trap 是控制转移的总称。exception 与当前指令同步，例如 ecall、page fault；interrupt 与当前指令异步，例如时钟和设备中断。

### trapframe 和 trampoline 为什么存在？

trapframe 保存完整用户寄存器和内核返回信息。trampoline 在用户/内核页表中映射到同一虚拟地址，使切换 `satp` 前后入口代码仍可执行。

## 8. Copy-on-write

### 为什么 COW 更高效？何时复制？

普通 fork 立即复制全部用户页，即使子进程马上 exec。COW 让父子共享只读物理页，仅在写故障时复制。引用大于 1 就分配并复制；引用等于 1 可直接恢复写权限。映射增加引用、解除映射减少引用，降为 0 才释放。

### copyout 为什么也要处理 COW？

内核向用户地址写数据不会产生同样的用户态 store page fault，因此 `copyout` 必须主动识别 COW PTE 并拆分。

## 9. Thread 与同步

### 上下文切换为什么保存寄存器？

切换后要从原位置继续执行。`ra` 决定返回位置，`sp` 决定栈，callee-saved `s0-s11` 按 ABI 必须跨调用保持。

### race condition 和 spinlock 是什么？锁为什么影响多核性能？

操作交错使结果依赖时序就是竞态。spinlock 用原子 test-and-set 保证互斥；等待时忙等。全局锁让独立操作串行并使缓存行在 CPU 间转移，因此 Lock Lab 用 per-CPU freelist 和哈希桶拆散热点。

## 10. File system

### inode 是什么？

inode 保存类型、链接数、大小和数据块索引；文件名存在目录项中，因此多个名字可以链接同一 inode。

### direct、single indirect、double indirect？

直接地址指向数据块；一级间接指向一个地址数组块；双重间接先指向外层地址数组，再指向内层数组。本项目最大块数为 `11+256+256²=65,803`。

### 日志解决什么问题？

一次操作可能修改多个块。日志先记录事务再提交；崩溃恢复时重放已提交事务，避免目录、inode 和 bitmap 只更新一部分。

## 11. mmap

### mmap 为什么与 page fault 有关？lazy 有什么好处？

`mmap()` 只登记虚拟区域，不立即分配页。首次访问无有效 PTE，CPU 触发 page fault，内核才分配页、读取文件并映射。调用成本低，只为实际访问页消耗内存，也允许声明大于物理内存的映射。

### munmap 做什么？

查找 VMA，对 MAP_SHARED 的已加载可写页回写，清 PTE、释放物理页，缩短或删除 VMA，并在整段删除时释放 file 引用。

### fork、exit、exec 如何处理 mmap？

fork 复制 VMA 与 file 引用，子进程按需载入独立物理页；exit 统一 unmap。成功 exec 也先清理，否则旧页表仍有高地址叶子映射。

## 12. 网络驱动

### descriptor ring 和 DMA 是什么？

ring 是循环描述符数组，软件和设备通过 head/tail 协调所有权。DMA 让网卡直接读写内存 packet buffer，CPU 不逐字节搬运设备数据。

## 13. 结果证据

```text
util     100/100    syscall   35/35
pgtbl     46/46     traps     85/85
cow      110/110    thread    60/60
net      100/100    lock      70/70
fs       100/100    mmap     140/140
total    846/846
```

每项完整输出在 `results/<lab>/grade.txt`，实现分析在 `docs/labs/*.md`。

## 14. 现场故障备用命令

```bash
# 确认没有遗留 QEMU
ps -ef | grep '[q]emu-system-riscv64'

# 环境检查和当前分支冷构建
./scripts/check-env.sh
make clean && make -j4

# 当前分支官方评分器
./grade-lab-$(git branch --show-current) -v

# 查看保存的分数、状态和提交
grep 'Score:' results/*/grade.txt
git status --short
git log --oneline --decorate -10
```

若 QEMU 无响应，先按 `Ctrl-a x` 正常退出，再确认没有残留进程；不要修改测试或跳过失败项继续演示。

