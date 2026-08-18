# 项目答辩提纲与现场手册

## 1. 开场说明（约 1 分钟）

本项目基于 MIT 6.S081 Fall 2021 官方 xv6-riscv，按官方顺序完成十个独立分支。所有 Lab 都经历原始基线、实现、定向测试和完整 grader；十项成绩合计 `846/846`。重点展示 COW、页表/trap、文件系统与 mmap，因为它们最能体现内核资源生命周期和跨子系统协作。

## 2. 推荐现场流程

在 Windows Terminal 中进入 E 盘 WSL Ubuntu，并在仓库根目录执行：

```bash
wsl -d Ubuntu
git rev-parse --show-toplevel
git status --short
./scripts/check-env.sh
```

优先演示 mmap（冷编译、启动、现场运行）。脚本在 E 盘 WSL `/tmp` 中建立隔离快照，不切换或修改主工作树：

```bash
./scripts/defense-demo.sh mmap demo
```

进入 `$` 后执行 `mmaptest`，退出时按 `Ctrl-a x`。若需展示官方评分：

```bash
./scripts/defense-demo.sh mmap grade
```

备选：`util`、`syscall`、`cow`、`fs`。`fs` 现场只运行 `symlinktest`；完整 `bigfile` 需要写 65,803 块，在 D 盘挂载路径可能超过 180 秒，正式满分日志已保存在 `results/fs/grade.txt`。

`nettests` 的完整交互验证依赖 host-side `make server`，不作为现场交互 Demo。网络实验使用 `results/net/grade.txt` 和仓库根目录的 `packets.pcap` 作为结果证据，现场只讲 `kernel/e1000.c` 的 TX/RX descriptor ring。

`scripts/run-all-tests.sh` 会依次运行十个完整 grader，仅用于提交前离线复测，不在答辩现场执行。

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

`mmap()` 只登记虚拟区域，不立即分配页。首次数据访问主要触发 load page fault 或 store page fault，内核随后分配页、读取文件并建立映射。这样只为实际访问的页面分配物理内存并执行文件 I/O，减少未访问映射的资源开销。

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

## 15. 答辩必会 8 问

### 必会 1. xv6 是什么，为什么使用 xv6-riscv？

xv6 是 MIT 面向操作系统教学实现的小型 Unix 风格内核，包含进程、虚拟内存、系统调用、文件系统、锁和设备驱动等完整主干。采用 RISC-V 版本可以直接观察 privilege mode、`ecall`、`scause`、`satp` 和页表等体系结构机制，同时代码规模适合从用户程序一路跟踪到硬件接口。

- Branch：`mmap`（总体结构适用于各 Lab 分支）
- 文件：`kernel/main.c`、`kernel/proc.c`、`kernel/vm.c`
- 函数：`main`、`userinit`、`scheduler`

### 必会 2. system call 从用户态到内核的完整路径？

用户 C 接口由 `user/usys.pl` 生成汇编 stub，stub 把系统调用号装入 `a7` 并执行 `ecall`。trampoline 的 `uservec` 保存用户寄存器后进入 `usertrap()`；`syscall()` 从 trapframe 读取编号并调用 handler，返回值写入 `a0`，最后经 `usertrapret/userret` 恢复用户现场并执行 `sret`。

- Branch：`syscall`
- 文件：`user/usys.pl`、`kernel/trampoline.S`、`kernel/trap.c`、`kernel/syscall.c`
- 函数：`uservec`、`usertrap`、`syscall`、`usertrapret`

### 必会 3. Sv39 与 PTE 是什么？

Sv39 使用 39 位有效虚拟地址：三级页表各取 9 位索引，低 12 位是 4 KiB 页内偏移。每个 PTE 保存物理页号和 `V/R/W/X/U/A` 等标志；非叶 PTE 指向下一级页表，叶 PTE 定义最终映射与权限。xv6 的 `walk()` 逐级查找，`mappages()` 建立叶映射。

- Branch：`pgtbl`
- 文件：`kernel/riscv.h`、`kernel/vm.c`
- 函数：`walk`、`mappages`、`vmprint`

### 必会 4. trapframe 和 trampoline 为什么存在？

trap 发生时必须保存完整用户寄存器，trapframe 提供每进程的保存区域，并记录返回用户态所需的内核信息。进入内核和返回用户态时会切换页表，trampoline 因而映射在用户页表与内核页表的同一虚拟地址，保证切换 `satp` 前后入口和返回代码仍可连续执行。

- Branch：`traps`
- 文件：`kernel/trampoline.S`、`kernel/trap.c`、`kernel/proc.h`
- 函数：`uservec`、`usertrap`、`usertrapret`、`userret`

### 必会 5. COW fork 如何工作？

`uvmcopy()` 不立即复制全部用户页，而是让父子 PTE 指向同一物理页；对原可写页清除 `PTE_W`、设置 `PTE_COW` 并增加引用计数。进程写入时产生 store page fault，`cowalloc()` 在多引用时分配并复制新页，在单引用时直接恢复写权限，旧页引用降到零后才由 `kfree()` 回收。

- Branch：`cow`
- 文件：`kernel/vm.c`、`kernel/kalloc.c`、`kernel/trap.c`、`kernel/riscv.h`
- 函数：`uvmcopy`、`cowalloc`、`krefinc`、`kfree`、`usertrap`

### 必会 6. copyout 为什么也必须处理 COW？

用户指令写只读 COW PTE 会触发 store page fault，但内核的 `copyout()` 根据用户页表取得物理地址后从内核态写入，不会自动走同一用户故障路径。如果不主动拆分，就会写失败或修改仍被其他进程共享的物理页，因此 `copyout()` 必须识别 `PTE_COW` 并先调用 `cowalloc()`。

- Branch：`cow`
- 文件：`kernel/vm.c`
- 函数：`copyout`、`cowalloc`、`walk`

### 必会 7. double indirect block 如何扩展大文件？

inode 保持 13 个地址槽不变：11 个 direct、1 个 single indirect、1 个 double indirect。每个索引块可保存 256 个块号，因此最大数据块数为 `11+256+256²=65,803`。`bmap()` 按外层和内层索引按需分配，`itrunc()` 从数据块到内层、外层索引块逆序释放。

- Branch：`fs`
- 文件：`kernel/fs.h`、`kernel/file.h`、`kernel/fs.c`
- 函数：`bmap`、`itrunc`

### 必会 8. mmap 的 VMA、lazy fault、munmap、fork、exit 如何形成完整生命周期？

`sys_mmap()` 建立 VMA 并持有 file 引用，但不立即装页；首次数据访问主要触发 load 或 store page fault，`vma_fault()` 才分配页、读取文件并建立 PTE。`vma_unmap()` 对已装入的 `MAP_SHARED` 可写页回写并释放，未装入页直接跳过；fork 复制 VMA 和 file 引用，exit 与成功 exec 在销毁页表前调用 `vma_unmap_all()`。

- Branch：`mmap`
- 文件：`kernel/proc.h`、`kernel/sysfile.c`、`kernel/trap.c`、`kernel/proc.c`、`kernel/exec.c`
- 函数：`sys_mmap`、`vma_fault`、`vma_unmap`、`vma_unmap_all`、`fork`、`exit`、`exec`

## 16. 高频答辩问题（备用问题库）

### 1. 这个项目基于哪个版本，完成了哪些内容？

基于 MIT 6.S081 Fall 2021 的 xv6-riscv，按 `util`、`syscall`、`pgtbl`、`traps`、`cow`、`thread`、`net`、`lock`、`fs`、`mmap` 十个实验分支完成。保存的官方评分总计为 846/846。

### 2. 如何证明这些分数来自真实实现，而不是只保存了数字？

每个实验都有独立 Git 分支、实现提交和 `results/<lab>/grade.txt` 完整评分输出；`docs/labs/<lab>.md` 记录基线失败、修改位置、验证命令与最终结果。答辩时可随机选择分支，在隔离副本中重新运行对应官方评分器。

### 3. 一次用户系统调用如何到达内核函数？

以 `mmap` 为例：`user/user.h` 提供声明，`user/usys.pl` 生成带 `ecall` 的桩，`kernel/syscall.h` 分配编号，`kernel/syscall.c` 按编号分派到 `sys_mmap`，参数再由 `argint`、`argaddr`、`argfd` 等函数取出。

### 4. `trace` 为什么在 fork 后仍然生效？exec 后呢？

掩码保存在 `struct proc` 的 `trace_mask` 中，`fork` 显式复制该字段；`exec` 替换地址空间但不替换 proc，因此掩码仍属于同一进程。系统调用返回后，分派路径按系统调用号对应的位决定是否打印。

### 5. `sysinfo` 怎样安全地把结果交给用户？

内核先统计空闲内存和活跃进程，填入内核栈上的 `struct sysinfo`，再用 `copyout` 写到当前进程页表对应的用户地址。不能直接解引用用户指针，因为该地址需经过页表检查，也可能无效。

### 6. Sv39 地址为什么分为三级索引？

4 KiB 页提供 12 位页内偏移，剩余 27 位拆成三级、每级 9 位；每张 4 KiB 页表正好容纳 512 个 8 字节 PTE。`walk` 逐级取索引并找到或创建下一级页表。

### 7. `USYSCALL` 页解决了什么问题，为什么用户不能写？

它把只读的进程信息映射进用户空间，使 `ugetpid()` 不必陷入内核。映射带 `PTE_U|PTE_R` 而不带 `PTE_W`，避免用户篡改内核提供的数据。

### 8. `pgaccess` 为什么检查后还要清除 `PTE_A`？

`PTE_A` 由硬件在访问页面时置位。本次查询若不清除，下一次查询只能知道“过去某时访问过”，无法观察两个采样窗口之间的新访问；修改 PTE 后还需保证地址翻译缓存看到新状态。

### 9. trap、exception 和 interrupt 有什么区别？

trap 是进入内核的统一控制转移；exception 与当前指令同步，例如 ecall 和 page fault；interrupt 与当前指令异步，例如时钟或设备中断。`scause` 用来区分具体原因。

### 10. 为什么需要 trampoline 和 trapframe？

陷入时还在用户页表，返回时又要从内核页表切回用户页表，因此 trampoline 必须在两张页表的同一虚拟地址可执行。trapframe 保存用户寄存器及返回所需的内核信息，让汇编入口和 C 代码可以安全切换。

### 11. alarm 如何避免处理函数重入？

进程保存 alarm 周期、累计 tick、handler、备份 trapframe 和“正在处理”标志。计时到期且未在 handler 中时才备份上下文并改写 `epc`；`sigreturn` 恢复上下文并清除标志。

### 12. COW fork 的核心变化是什么？

`uvmcopy` 不再为每页分配并复制物理内存，而是让父子映射同一页、清掉可写位、对原可写页标记 `PTE_COW`，同时增加物理页引用计数。这样 fork 的成本与实际后续写入量相关。

### 13. COW 写故障怎样处理？

`usertrap` 识别用户 store page fault，`cowalloc` 验证 PTE 后检查引用计数。多人共享时分配新页并复制；仅剩一个引用时可直接把该映射恢复为可写，然后刷新相应映射状态。

### 14. COW 为什么必须有引用计数？

同一物理页可能同时被父进程和多个子进程映射。每增加映射就增加引用，每解除映射就减少引用，只有计数降到零才交还 allocator，否则会产生悬空映射或重复释放。

### 15. 为什么 `copyout` 也要识别 COW？

内核替用户写内存时不会走普通用户态 store page fault。若 `copyout` 直接写共享只读页，要么失败，要么破坏隔离，所以它必须先拆分 COW 页再复制数据。

### 16. 用户线程切换需要保存哪些寄存器？

本实验的 `thread_switch` 保存 `ra`、`sp` 和 RISC-V ABI 规定的 callee-saved 寄存器 `s0-s11`。caller-saved 寄存器由调用约定允许被函数调用破坏，无须放进最小线程上下文。

### 17. barrier 为什么需要 round 或 generation？

同一 barrier 会被循环重复使用。最后一个线程唤醒本轮等待者并递增 round，等待者检查 round 是否变化；否则下一轮到达可能与上一轮等待混在一起。

### 18. E1000 描述符环中谁拥有缓冲区？

发送时软件填 TX descriptor 并推进 `TDT`，设备完成后用 DD 位归还所有权；接收时设备填 RX descriptor，软件取出 mbuf、补充新缓冲区并推进 `RDT`。错误地提前释放 mbuf 会导致 DMA 使用已释放内存。

### 19. per-CPU freelist 怎样减少锁竞争？

正常分配和释放只操作当前 CPU 的空闲链表及其锁，不再让全部 CPU 争用一个全局锁。当前链表为空时才从其他 CPU 的链表窃取页面，同时保持明确的锁范围。

### 20. 哈希 buffer cache 如何避免同一块出现两个缓存副本？

块号映射到固定 bucket，同一 `(dev, blockno)` 的查找和插入在该 bucket 锁保护下完成；淘汰过程另有协调锁，避免跨桶选择 victim 时并发创建重复条目。

### 21. 双重间接块把最大文件扩展到多少？

本实现保留 11 个直接块、一个一级间接入口和一个二级间接入口。每个间接块容纳 256 个地址，因此最大数据块数是 `11+256+256*256=65,803`；`bmap` 分配，`itrunc` 对称释放。

### 22. 符号链接如何防止无限递归？

`open` 在未指定 `O_NOFOLLOW` 时读取 symlink 保存的目标并继续解析，但设置固定最大跟随深度 10。超过深度或目标无效就返回错误，从而终止环形链接。

### 23. mmap 为什么说是 lazy allocation？

`sys_mmap` 只验证参数、选择虚拟区间并登记 VMA，不分配物理页，也不读取文件。首次数据访问主要触发 load page fault 或 store page fault，由 `vma_fault` 分配、读取并建立 PTE。这样只为实际访问的页面分配物理内存并执行文件 I/O，减少未访问映射的资源开销。

### 24. `MAP_SHARED` 和 `MAP_PRIVATE` 在本项目中的区别？

两者的页面都是每个进程按需装入；区别主要发生在解除映射时：可写的 `MAP_SHARED` 页面回写文件，`MAP_PRIVATE` 的修改不回写。fork 后也不是让用户页共享同一物理页。

### 25. 部分 `munmap` 和从未 fault 的页面怎样处理？

实现允许从 VMA 头部、尾部或整个区域解除映射，并同步调整地址、长度和文件偏移。逐页检查 PTE；从未 fault 的页没有有效映射，直接跳过，不能把它当成内核错误。

### 26. fork、exit 和 exec 为什么都要考虑 VMA？

fork 要复制 VMA 元数据并 `filedup`，使子进程以后能独立 fault；exit 要回写并释放所有映射；成功 exec 替换旧地址空间前也要清理 VMA，否则会遗留文件引用或旧页表中的叶子映射。

### 高频问题源码与演示索引

上面每个答案按正常语速可在 30～90 秒内讲完。由于十个 Lab 位于独立分支，现场打开源码前先确认表中的 branch；无需切换主工作树，可用 `git show <branch>:<path>` 查看，或用对应 `defense-demo.sh` 命令在隔离副本演示。

| 题号 | 建议时长 | 对应源码文件 | 关键函数/数据结构 | 可现场打开的位置 | 演示命令 |
| ---: | --- | --- | --- | --- | --- |
| 1 | 30 秒 | `README.md`、`docs/final-report.md` | 项目分支与成绩表 | 当前仓库文档首页 | `git branch -a` |
| 2 | 45 秒 | `results/<lab>/grade.txt`、`docs/labs/*.md` | Score、实现提交 | `results/` 与每份报告第 7/10 节 | `grep 'Score:' results/*/grade.txt` |
| 3 | 60 秒 | `mmap:user/user.h`、`user/usys.pl`、`kernel/syscall.c`、`kernel/sysfile.c` | `syscall`、`sys_mmap` | `git show mmap:kernel/syscall.c` | `./scripts/defense-demo.sh mmap demo` |
| 4 | 45 秒 | `syscall:kernel/proc.h`、`proc.c`、`syscall.c` | `trace_mask`、`fork`、`syscall` | `git grep -n trace_mask syscall -- kernel` | `./scripts/defense-demo.sh syscall demo` |
| 5 | 60 秒 | `syscall:kernel/sysproc.c`、`proc.c`、`kalloc.c` | `sys_sysinfo`、`nproc`、`freemem`、`copyout` | `git show syscall:kernel/sysproc.c` | `./scripts/defense-demo.sh syscall demo` |
| 6 | 45 秒 | `pgtbl:kernel/vm.c`、`riscv.h` | `walk`、`PX`、`PTE2PA` | `git show pgtbl:kernel/vm.c` | `./scripts/defense-demo.sh pgtbl demo` |
| 7 | 45 秒 | `pgtbl:kernel/proc.c`、`memlayout.h` | `proc_pagetable`、`USYSCALL`、`PTE_R/PTE_U` | `git grep -n USYSCALL pgtbl -- kernel` | `./scripts/defense-demo.sh pgtbl demo` |
| 8 | 45 秒 | `pgtbl:kernel/sysproc.c`、`riscv.h` | `sys_pgaccess`、`PTE_A`、`sfence_vma` | `git show pgtbl:kernel/sysproc.c` | `./scripts/defense-demo.sh pgtbl demo` |
| 9 | 30 秒 | `traps:kernel/trap.c`、`riscv.h` | `usertrap`、`scause`、`devintr` | `git show traps:kernel/trap.c` | `./scripts/defense-demo.sh traps demo` |
| 10 | 60 秒 | `traps:kernel/trampoline.S`、`trap.c`、`proc.h` | `uservec`、`usertrapret`、`trapframe` | `git show traps:kernel/trampoline.S` | `./scripts/defense-demo.sh traps demo` |
| 11 | 60 秒 | `traps:kernel/trap.c`、`proc.h`、`sysproc.c` | `alarm_active`、`sys_sigreturn`、trapframe 备份 | `git grep -n alarm_ traps -- kernel` | `./scripts/defense-demo.sh traps demo` |
| 12 | 60 秒 | `cow:kernel/vm.c`、`riscv.h`、`kalloc.c` | `uvmcopy`、`PTE_COW`、`krefinc` | `git show cow:kernel/vm.c` | `./scripts/defense-demo.sh cow demo` |
| 13 | 60 秒 | `cow:kernel/trap.c`、`vm.c` | `usertrap`、`cowalloc` | `git grep -n cowalloc cow -- kernel` | `./scripts/defense-demo.sh cow demo` |
| 14 | 45 秒 | `cow:kernel/kalloc.c` | 引用计数数组、`krefcount`、`kfree` | `git show cow:kernel/kalloc.c` | `./scripts/defense-demo.sh cow grade` |
| 15 | 45 秒 | `cow:kernel/vm.c` | `copyout`、`cowalloc` | `git grep -n copyout cow -- kernel/vm.c` | `./scripts/defense-demo.sh cow grade` |
| 16 | 45 秒 | `thread:user/uthread.c`、`uthread_switch.S` | `struct context`、`thread_switch` | `git show thread:user/uthread_switch.S` | `./scripts/defense-demo.sh thread demo` |
| 17 | 45 秒 | `thread:notxv6/barrier.c` | `bstate.round`、`barrier` | `git show thread:notxv6/barrier.c` | `git show thread:notxv6/barrier.c` |
| 18 | 60 秒 | `net:kernel/e1000.c`、`e1000_dev.h` | `e1000_transmit`、`e1000_recv`、`TDT/RDT` | `git show net:kernel/e1000.c` | 不现场交互；使用 `results/net/grade.txt`、`packets.pcap` |
| 19 | 45 秒 | `lock:kernel/kalloc.c` | `kmem[NCPU]`、`kalloc`、`kfree` | `git show lock:kernel/kalloc.c` | `./scripts/defense-demo.sh lock demo` |
| 20 | 60 秒 | `lock:kernel/bio.c`、`buf.h` | `bget`、bucket locks、`bcache.evict` | `git show lock:kernel/bio.c` | `./scripts/defense-demo.sh lock demo` |
| 21 | 60 秒 | `fs:kernel/fs.h`、`file.h`、`fs.c` | `NDIRECT`、`NDINDIRECT`、`bmap`、`itrunc` | `git grep -n 'NDINDIRECT\|MAXFILE' fs -- kernel` | 仅运行 `./scripts/defense-demo.sh fs demo` 中的 `symlinktest`；完整成绩见 `results/fs/grade.txt` |
| 22 | 45 秒 | `fs:kernel/sysfile.c`、`fcntl.h`、`stat.h` | `sys_symlink`、`sys_open`、`O_NOFOLLOW` | `git grep -n T_SYMLINK fs -- kernel` | `./scripts/defense-demo.sh fs demo` |
| 23 | 60 秒 | `mmap:kernel/sysfile.c`、`proc.h`、`proc.c` | `sys_mmap`、`struct vma`、`vma_fault` | `git grep -n 'sys_mmap\|vma_fault' mmap -- kernel` | `./scripts/defense-demo.sh mmap demo` |
| 24 | 45 秒 | `mmap:kernel/proc.c`、`fcntl.h` | `vma_unmap`、`MAP_SHARED`、`MAP_PRIVATE` | `git grep -n MAP_SHARED mmap -- kernel` | `./scripts/defense-demo.sh mmap demo` |
| 25 | 60 秒 | `mmap:kernel/sysfile.c`、`proc.c` | `sys_munmap`、`vma_unmap`、`walk` | `git grep -n 'sys_munmap\|vma_unmap' mmap -- kernel` | `./scripts/defense-demo.sh mmap grade` |
| 26 | 60 秒 | `mmap:kernel/proc.c`、`exec.c` | `fork`、`filedup`、`vma_unmap_all`、`exit`、`exec` | `git grep -n vma_unmap_all mmap -- kernel` | `./scripts/defense-demo.sh mmap demo` |

## 17. mmap 现场演示讲解路线

建议先在 `mmap` 分支执行 `make qemu`，或从干净工作树执行 `./scripts/defense-demo.sh mmap demo`，进入 xv6 后运行 `mmaptest`。测试输出出现 `mmaptest: all tests succeeded` 后，按以下顺序打开源码讲解：

1. **用户接口**：从 `user/user.h` 的 `mmap`、`munmap` 声明开始，再指向 `user/usys.pl`。说明脚本生成的桩把参数留在约定寄存器中，装载系统调用号并执行 `ecall`。
2. **系统调用注册**：查看 `kernel/syscall.h` 的编号和 `kernel/syscall.c` 的分派表。说明 `ecall` 经过 trap 路径后，最终按编号调用 `sys_mmap` 或 `sys_munmap`。
3. **建立 VMA**：进入 `kernel/sysfile.c:sys_mmap`，依次讲参数与权限检查、文件可读写条件、空闲 VMA 选择、虚拟地址安排和 `filedup`。
4. **证明延迟分配**：指出 `sys_mmap` 只填写 VMA 的地址、长度、权限、标志、文件与偏移；这里没有 `kalloc`、`readi` 或 `mappages`，所以调用返回时页面尚未装入。
5. **触发首次数据访问**：回到 `user/mmaptest.c`，找到映射后的第一次读或写。首次数据访问主要触发 load page fault 或 store page fault；当前实现统一处理对应的 RISC-V page-fault scause。
6. **trap 分流**：在 `kernel/trap.c:usertrap` 中展示 page-fault scause 的统一分流，数据访问重点说明 load fault 与 store fault，并把 fault address 交给 `vma_fault`；非法地址或加载失败时进程被标记 killed。
7. **分配物理页**：进入 `kernel/proc.c:vma_fault`，先定位覆盖 fault address 的 VMA，再 `kalloc` 并清零一页。清零保证文件末尾不足一页的剩余字节不会泄漏旧内存。
8. **从文件装入**：讲 `readi` 使用 VMA 保存的 file/inode，以及“页地址相对 VMA 起点 + 映射文件 offset”计算读取位置；同时限制读取量不越过映射长度。
9. **建立用户映射**：根据 `PROT_READ`、`PROT_WRITE` 组合 PTE 权限，再调用 `mappages`。fault 指令返回用户态后重新执行，此时访问成功。
10. **解除映射与回写**：进入 `vma_unmap`。对已装入且可写的 `MAP_SHARED` 页面，在文件系统事务中 `writei` 回写；`MAP_PRIVATE` 不回写。随后 `uvmunmap` 清 PTE 并释放物理页。
11. **维护 VMA 生命周期**：说明头部/尾部解除映射怎样调整 `addr`、`length` 和 `offset`；整段删除时清空槽位并 `fileclose`。未 fault 的页面没有 PTE，解除时安全跳过。
12. **跨进程与退出清理**：在 `fork` 中展示 VMA 元数据复制和 `filedup`，在 `exit` 与 `exec` 路径展示 `vma_unmap_all`。最后回到 `mmaptest` 的 fork、shared/private、unmap 等用例，把测试现象与这些路径逐一对应。

现场讲解的核心句是：“`mmap` 先登记范围，首次数据访问再由 load/store page fault 装页，`munmap` 根据共享属性决定是否回写，进程生命周期路径负责复制或清理 VMA。”

## 18. 5 分钟项目介绍

### 0:00–0:35 项目范围

“我的项目基于 MIT 6.S081 Fall 2021 的 xv6-riscv，共完成十个实验，覆盖用户程序、系统调用、虚拟内存、trap、写时复制、线程同步、网卡驱动、锁优化、文件系统和 mmap。十个分支的官方评分结果全部保存，总分 846/846。”

### 0:35–1:15 验证方法

“我先在每个官方实验分支保存基线失败，再做最小实现并运行对应官方 grader。每个实验都有独立提交、完整 `grade.txt` 和实现说明。演示脚本使用临时隔离克隆，不会切换或污染主工作目录，因此可以现场随机复测。”

### 1:15–2:05 系统调用、页表与 trap

“系统调用路径是用户桩执行 ecall，trap 保存寄存器并切到内核，syscall 表按编号分派，返回值写回 trapframe。页表实验让我直接处理 Sv39 PTE、访问位和用户只读共享页；alarm 实验则要求正确保存并恢复完整用户上下文，还要阻止 handler 重入。”

### 2:05–3:05 COW 重点

“COW fork 不立即复制所有用户页。父子共享物理页，原可写页被标成只读 COW，并增加引用计数。发生写故障时，多引用页面才复制，单引用页面直接恢复写权限。内核的 copyout 也必须主动拆分 COW，否则会绕过用户态 page fault。这部分把页表权限、trap 和物理页生命周期串联起来。”

### 3:05–4:15 mmap 重点

“mmap 采用 lazy 策略：系统调用只登记 VMA，第一次访问触发 page fault，内核才分配页、从文件读取并映射。munmap 对 MAP_SHARED 的已加载可写页回写，对 MAP_PRIVATE 不回写，并处理头部、尾部和整段解除。fork 复制 VMA 与文件引用，exit 和 exec 负责统一清理。”

### 4:15–5:00 其他实验与结论

“并发部分用 per-CPU freelist 和哈希 buffer cache 降低热点锁竞争；文件系统加入双重间接块和符号链接；网络部分完成 E1000 描述符环的收发与 mbuf 生命周期。项目的重点不是单个补丁，而是用相同的方法验证接口、并发不变量、资源释放和失败路径。完整结果和源码映射都在提交文档中。”

## 19. 10 分钟项目介绍

### 0:00–0:50 背景与完成情况

“项目基于 MIT 6.S081 Fall 2021 的 xv6-riscv。xv6 代码量小，但包含进程、页表、trap、文件系统、锁和设备驱动的完整主干。我完成十个实验分支，官方评分依次为 100、35、46、85、110、60、100、70、100、140，总计 846/846。”

### 0:50–1:35 工作与证据组织

“每个实验先保存未实现时的基线，再按官方分支开发，最终运行原始 grader。Git 分支保存实现历史，`results` 保存逐项测试输出，`docs/labs` 说明关键改动和失败修复。答辩复测通过临时克隆完成，当前工作树始终留在 mmap 分支。”

### 1:35–2:30 系统调用路径

“用户 API 声明和 usys 脚本生成汇编桩，ecall 进入 trampoline，寄存器写入 trapframe；usertrap 识别系统调用并推进 epc，syscall 表根据 a7 中的编号分派。trace 在 proc 中保存掩码并随 fork 继承；sysinfo 在内核统计资源后通过 copyout 安全返回用户结构体。这说明新增系统调用既涉及接口，也涉及进程状态和用户指针边界。”

### 2:30–3:25 页表与 trap

“Sv39 使用三级页表，每级 9 位索引和 12 位页内偏移。USYSCALL 把只读进程信息映射到用户空间，pgaccess 读取并清除硬件访问位。trap 实验中，backtrace 沿 frame pointer 找返回地址；sigalarm 在时钟中断计数，到期后备份 trapframe、跳转到用户 handler，sigreturn 再完整恢复，并用状态位禁止重入。”

### 3:25–4:55 COW fork

“普通 fork 立即复制全部地址空间，子进程若马上 exec 会浪费大量工作。COW 的 uvmcopy 让父子共享页面，清 PTE_W、设置 PTE_COW，并维护物理页引用计数。写故障进入 usertrap 后，如果引用数大于一就分配、复制并替换映射；等于一则恢复写权限。uvmunmap 递减引用，降到零才释放。另一个容易漏掉的路径是 copyout：内核写用户页不会产生普通用户 store fault，因此也要先调用 COW 拆分页。最终 cowtest 和 usertests 全部通过。”

### 4:55–5:50 用户线程与网络

“用户线程切换按 RISC-V ABI 保存 ra、sp 和 s0 到 s11；哈希表用 bucket 锁提高并发；barrier 用 round 区分重复轮次。网络实验中，TX/RX descriptor ring 是软件与 E1000 的所有权协议，DD 位表示完成，TDT/RDT 推进边界，mbuf 必须在 DMA 不再使用后才能释放或替换。”

### 5:50–6:50 锁优化

“内存分配器从全局 freelist 改成 per-CPU freelist，通常只拿本 CPU 的锁，缺页时才向其他 CPU 窃取。buffer cache 按块号分成 13 个 bucket，减少无关块之间的竞争，同时用淘汰协调锁避免同一块并发产生两个缓存副本。这里的目标不是消除锁，而是缩小共享范围并保持唯一性不变量。”

### 6:50–7:45 文件系统

“大文件支持把 inode 地址布局改为 11 个直接块、一级间接和二级间接，最大为 65,803 个数据块；bmap 的分配与 itrunc 的释放必须完全对称。符号链接新增 inode 类型和 symlink 系统调用，open 默认跟随目标，O_NOFOLLOW 可禁止跟随，并用深度 10 防止链接环无限递归。”

### 7:45–9:15 mmap

“mmap 是最综合的实验。sys_mmap 验证参数并登记 VMA，但不分配页；第一次访问触发 load 或 store page fault，vma_fault 定位区域、分配清零页、按文件偏移 readi，再按 PROT 权限建立映射。sys_munmap 支持区域头部、尾部和整体解除；对 MAP_SHARED 的已加载可写页回写，对 MAP_PRIVATE 不回写，未 fault 的页直接跳过。fork 复制 VMA 并增加文件引用，exit 和 exec 统一解除映射。这个实现同时考察系统调用、trap、页表、文件 I/O 和资源生命周期。”

### 9:15–10:00 总结与演示入口

“十个实验让我形成了统一的检查方式：先明确状态归属，再检查并发或引用不变量，最后覆盖成功、失败和清理路径。现场我可以运行 mmaptest，并沿 user API、sys_mmap、usertrap、vma_fault、vma_unmap、fork/exit 的顺序展示完整路径；也可以从保存的 grader 结果中随机复核任一实验。”
