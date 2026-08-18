# xv6 课程项目答辩 PPT 大纲

- 总页数：14 页
- Main Slides：11 页
- Backup Slides：3 页
- 建议时长：5 分钟精简讲解，或 8～10 分钟完整讲解；现场 Demo 另计约 2 分钟
- 主线：完成范围 → 系统架构 → 内核路径 → COW → 并发与存储 → mmap → Demo 与总结

## MAIN SLIDES（11 页）

### Main 1：封面

- 标题：基于 MIT 6.S081 Fall 2021 xv6-riscv 的操作系统课程设计
- 副标题：10/10 Labs，Official Grader 846/846
- 姓名、学号、班级、课程、指导教师：由提交者按学校模板填写
- 视觉建议：RISC-V/xv6 标识配简洁架构线稿，不放代码截图
- 完整版讲解：15～20 秒

### Main 2：完成范围、10 Labs 与 846/846

- 版本：MIT 6.S081 Fall 2021 / xv6-riscv
- 完成：10/10 Labs，十个独立实验分支
- 覆盖：用户程序、系统调用、页表、Trap、COW、多线程、网络驱动、锁、文件系统、mmap
- 成绩：util 100、syscall 35、pgtbl 46、traps 85、cow 110、thread 60、net 100、lock 70、fs 100、mmap 140
- 总分：846/846
- 工作量：按课程说明中“全部实验内容完成”的口径体现 A 级工作量
- 视觉建议：一张紧凑覆盖矩阵，突出 10/10 与 846/846
- 完整版讲解：40～45 秒

### Main 3：xv6-riscv 总体架构

- User mode：shell、用户程序、用户库和系统调用 stub
- Trap boundary：trampoline、trapframe、`usertrap/usertrapret`
- Kernel：process/VM、scheduler/locks、file system、device drivers
- Hardware/QEMU：RISC-V CPU、VirtIO disk、E1000
- 视觉建议：复用 `docs/architecture.md` 的 xv6 架构图，强调 user/kernel 边界
- 完整版讲解：40～45 秒

### Main 4：System Call、Trap 与 Page Table

- 系统调用路径：用户函数 → stub → `ecall` → `uservec` → `usertrap` → `syscall` → handler
- Trap：exception、interrupt 与 system call 通过 `scause` 区分
- trapframe 保存用户现场；trampoline 跨页表切换保持入口/返回代码可执行
- Sv39：三级页表、4 KiB 页面、PTE 权限位
- 实验落点：trace/sysinfo、USYSCALL、vmprint、pgaccess、alarm
- 源码：`syscall:kernel/syscall.c`、`pgtbl:kernel/vm.c`、`traps:kernel/trap.c`
- 视觉建议：控制流箭头配 Sv39 小图，避免大段源码
- 完整版讲解：55～60 秒

### Main 5：COW 原理

- 原始 fork：为全部用户页立即分配并复制，成本与地址空间大小成正比
- COW：父子共享只读物理页，真正写入时才复制
- 关键状态：`PTE_W`、`PTE_COW`、物理页引用计数
- 两条写路径：用户 store page fault 与内核 `copyout`
- 视觉建议：父/子页表共同指向一个 PA 的“写前”图
- 完整版讲解：40～45 秒

### Main 6：COW 实现与不变量

- `uvmcopy()`：共享 PA、清写权限、设置 COW、`krefinc`
- `cowalloc()`：引用数为 1 时恢复写；大于 1 时分配、复制、替换并减少旧引用
- `usertrap()`：处理 store page fault
- `copyout()`：主动拆分 COW，不能等待用户态 fault
- 三个不变量：共享页不可直接写、引用计数准确、所有写路径规则一致
- 结果：cow 110/110；simple、three、file、usertests 全部 OK
- 视觉建议：写故障前后两幅映射图，配少量关键伪代码
- 完整版讲解：65～75 秒

### Main 7：Multithreading 与 Lock

- 用户线程：`thread_switch` 保存 `ra/sp/s0-s11`，独立栈满足 ABI 对齐
- pthread hash：每 bucket mutex，避免同桶头插丢 key并保留跨桶并行
- barrier：mutex + condition variable + generation/round
- allocator：per-CPU freelist，本地为空时批量 steal
- bcache：13 个 bucket 快速路径 + 串行低频 eviction
- 本次保存结果：bcache 争用由 62,783 降至 163
- 视觉建议：左侧线程 context，右侧“全局锁 → 分片锁”对比
- 完整版讲解：55～60 秒

### Main 8：File System

- inode 布局：11 direct + single indirect + double indirect，总槽数保持不变
- 最大文件：`11 + 256 + 256² = 65,803` 个数据块
- `bmap()` 按需分配，`itrunc()` 由叶到根对称释放
- symlink：目标路径存入独立 inode；`open()` 默认跟随，`O_NOFOLLOW` 禁止跟随，最大深度 10
- 日志：新增索引地址进入事务，保持元数据一致性
- 结果：fs 100/100
- 视觉建议：三级块索引树配 symlink 跳转小图
- 完整版讲解：50～55 秒

### Main 9：mmap 设计

- 每进程 16 个 VMA：`addr/length/prot/flags/file/offset`
- `sys_mmap()` 只登记范围并 `filedup()`，不分配页、不读文件
- 地址从 `TRAPFRAME` 下方向下选择，与 `p->sz` 的低地址连续空间分离
- 首次数据访问主要触发 load page fault 或 store page fault，再由 `vma_fault()` 装页
- 收益：只为实际访问的页面分配物理内存并执行文件 I/O，减少未访问映射的资源开销
- 视觉建议：用户 VA 布局图，明确 VMA 已存在但 PTE 尚不存在
- 完整版讲解：50～60 秒

### Main 10：mmap 生命周期与综合验证

- fault：定位 VMA → 权限检查 → `kalloc`/清零 → `readi` → `mappages`
- unmap：跳过未 fault 页；`MAP_SHARED` 可写页回写；释放映射并调整 VMA
- fork：复制 VMA 元数据和 file 引用，子进程独立 fault-in
- exit/exec：页表销毁前统一 `vma_unmap_all`
- 关键区分：VMA 登记状态不等于 PTE/物理页存在状态
- 结果：mmap 140/140；八个 mmaptest 子项与 usertests 全部 OK
- 综合验证：10/10 `results/<lab>/grade.txt`，合计 846/846
- 视觉建议：四阶段生命周期图，右下角放简洁成绩结论
- 完整版讲解：70～80 秒

### Main 11：现场 Demo 与总结

- 演示命令：`./scripts/defense-demo.sh mmap demo`
- xv6 shell：运行 `mmaptest`
- 源码顺序：用户 stub → `sys_mmap` → VMA → `usertrap` → `vma_fault` → `vma_unmap` → fork/exit/exec
- 预期真实输出：`mmaptest: all tests succeeded`
- 总结：10/10 Labs，846/846；核心方法是维护资源生命周期和并发不变量
- 问答入口：COW 的共享/拆分/引用，mmap 的 VMA/fault/write-back/lifecycle
- 视觉建议：命令、源码导航和本人实际运行截图；不要在页内堆叠日志
- 完整版讲解：30 秒，现场演示另计约 2 分钟

## BACKUP SLIDES（3 页）

### Backup 1：Network Driver

- E1000 使用 DMA descriptor ring；TX/RX 通过 DD 位和 `TDT/RDT` 交接所有权
- `e1000_transmit()` 延迟释放上一次发送 mbuf，填写描述符后推进 tail
- `e1000_recv()` 先补充 replacement，再把完成包交给 `net_rx()`
- 在调用 `net_rx()` 前释放设备锁，避免 ARP reply 同步进入 transmit 时死锁
- memory barrier 保证设备看到完整描述符
- 结果：ping、single-process、multi-process、DNS 全部 OK，net 100/100
- 现场不运行交互式 `nettests`；完整验证依赖 host-side server，使用 `results/net/grade.txt` 与 `packets.pcap` 作为证据
- 用途：老师追问设备驱动、DMA、环形队列或网络 Lab 时打开

### Backup 2：完整成绩与证据表

| Lab | Grade | Result File | Status |
| --- | ---: | --- | --- |
| util | 100/100 | `results/util/grade.txt` | PASS |
| syscall | 35/35 | `results/syscall/grade.txt` | PASS |
| pgtbl | 46/46 | `results/pgtbl/grade.txt` | PASS |
| traps | 85/85 | `results/traps/grade.txt` | PASS |
| cow | 110/110 | `results/cow/grade.txt` | PASS |
| thread | 60/60 | `results/thread/grade.txt` | PASS |
| net | 100/100 | `results/net/grade.txt` | PASS |
| lock | 70/70 | `results/lock/grade.txt` | PASS |
| fs | 100/100 | `results/fs/grade.txt` | PASS |
| mmap | 140/140 | `results/mmap/grade.txt` | PASS |
| **Total** | **846/846** | `results/` | **PASS** |

- 证据链：Lab branch → implementation commit → final grade output → detailed lab report
- 用途：老师询问成绩构成、测试真实性或某项 Lab 状态时打开

### Backup 3：详细源码与测试定位

| Topic | Branch | Source entry | Test / demo |
| --- | --- | --- | --- |
| System Call | `syscall` | `kernel/syscall.c:syscall`、`kernel/sysproc.c` | `defense-demo.sh syscall demo` |
| Page Table | `pgtbl` | `kernel/vm.c:vmprint`、`kernel/sysproc.c:sys_pgaccess` | `defense-demo.sh pgtbl demo` |
| Trap | `traps` | `kernel/trap.c:usertrap`、`kernel/sysproc.c:sys_sigreturn` | `defense-demo.sh traps demo` |
| COW | `cow` | `kernel/vm.c:uvmcopy/cowalloc/copyout` | `defense-demo.sh cow grade` |
| Thread / Lock | `thread` / `lock` | `user/uthread_switch.S`、`kernel/kalloc.c`、`kernel/bio.c` | `defense-demo.sh thread demo` 或 `defense-demo.sh lock demo` |
| File System | `fs` | `kernel/fs.c:bmap/itrunc`、`kernel/sysfile.c:sys_symlink` | 现场仅 `defense-demo.sh fs demo` 运行 `symlinktest`；完整成绩见 `results/fs/grade.txt` |
| mmap | `mmap` | `sys_mmap`、`vma_fault`、`vma_unmap_all` | `defense-demo.sh mmap demo` |

- 完整答辩索引：`docs/defense.md`
- 用途：老师要求现场打开函数、解释测试对应关系时快速定位

## 5 分钟使用路线

建议主讲 Main 1 → 2 → 3 → 4（压缩）→ 5 → 6 → 9 → 10 → 11。Main 7～8 各用一句话概括，Backup 不主动展示。若包含现场 Demo，口头部分控制在约 4 分钟。

## 10 分钟使用路线

按 Main 1～11 顺序完整讲解，COW 与 mmap 分配最多时间。Backup 1～3 不计入主讲页数，只在老师追问网络、完整成绩或源码位置时打开。

## 制作与导出检查

1. 使用统一 16:9 模板、字体和配色，每页保留一个主要结论。
2. Main Slides 保持 11 页；Backup Slides 放在总结页之后，不主动翻讲。
3. COW 与 mmap 合计 4 页，突出难度、跨模块联系和完整生命周期。
4. 成绩、性能数字和命令必须能在 `results/` 或源码中定位。
5. 源码截图显示 branch、file 和函数名，字号保证投影可读。
6. 终端截图由提交者在最终机器实际运行后获取。
