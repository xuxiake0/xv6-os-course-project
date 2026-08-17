# Lab 04：Traps

## 1. 实验目的

理解 RISC-V 调用约定、栈帧、trap 入口和返回过程；实现内核栈回溯以及由时钟中断驱动的用户态周期处理函数。

## 2. 实验环境

- 分支：`traps`
- 官方原始提交：`219a8d7d70b6ac66b1447aeada079a1f8c3027f7`
- WSL2 Ubuntu 26.04、RISC-V GCC 15.2.0、QEMU 10.2.1
- Ubuntu 与工具链位于 E 盘，公共配置见 `docs/environment.md`

## 3. 相关 xv6 原理

RISC-V 使用 `a0` 至 `a7` 传递参数，`ra` 保存返回地址，`s0` 可作为帧指针。GCC 生成的内核栈帧在 `fp-8` 保存返回地址，在 `fp-16` 保存上一帧指针。

用户态发生系统调用或中断时，trampoline 把全部用户寄存器保存到 `trapframe`，再进入 `usertrap()`。返回时 `usertrapret()` 设置 `sepc`，trampoline 恢复寄存器并执行 `sret`。因此把 trapframe 的 `epc` 改成用户函数地址即可改变恢复位置，但必须在处理结束后恢复完整现场。

## 4. 实验任务

1. 阅读 `call.asm` 并回答寄存器、内联、地址和端序问题。
2. 实现 `backtrace()`，从 `sys_sleep()` 及 `panic()` 打印内核调用链。
3. 实现 `sigalarm(interval, handler)` 与 `sigreturn()`，周期调用用户处理函数并正确恢复被中断程序。

## 5. 原始代码分析

原始分支包含 `call.c`、`bttest.c` 和 `alarmtest.c`，但 alarmtest 未加入镜像，两个系统调用和进程 alarm 状态不存在，也没有栈回溯实现。

基线完整评分为 `19/85`，只有原有 `usertests` 通过；记录见 `results/traps/baseline-grade.txt`。

## 6. 设计思路

- 从 `s0` 取得当前帧指针，并把遍历限制在当前一页内核栈，避免越界。
- 每个进程保存 interval、已计时 tick、handler 地址、是否正在处理以及一份完整 trapframe 备份。
- 只在 `which_dev == 2` 的用户态时钟中断中累计 CPU tick。
- 到期且未在 handler 内时，复制 trapframe 并把 `epc` 改为 handler。
- `sigreturn()` 恢复整份 trapframe，解除重入保护，并返回被中断时的 `a0`，防止系统调用分发再次覆盖它。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/riscv.h` | 新增读取 `s0` 的 `r_fp()` |
| `kernel/printf.c`、`kernel/defs.h` | 实现/声明 `backtrace()`，panic 时调用 |
| `kernel/sysproc.c` | sleep 回溯、`sys_sigalarm`、`sys_sigreturn` |
| `kernel/proc.h`、`kernel/proc.c` | alarm 状态、初始化与清理 |
| `kernel/trap.c` | 时钟中断计数、保存现场并转向 handler |
| `kernel/syscall.h`、`kernel/syscall.c` | 注册两个系统调用 |
| `user/user.h`、`user/usys.pl` | 用户 API 与汇编存根 |
| `Makefile` | 把 `_alarmtest` 加入文件系统镜像 |

对应提交：`6481cc3`（alarm）、`3151a80`（backtrace）、`3d45e7a`（汇编问答）。

### 7.2 回溯核心

```c
uint64 fp = r_fp();
uint64 bottom = PGROUNDDOWN(fp);
while(fp > bottom && fp < bottom + PGSIZE){
  printf("%p\n", *(uint64 *)(fp - 8));
  fp = *(uint64 *)(fp - 16);
}
```

手动输出的三个地址经 `riscv64-linux-gnu-addr2line` 解析为 `sysproc.c:64`、`syscall.c:144` 和 `trap.c:76`。

### 7.3 alarm 现场保存与恢复

```c
p->alarm_saved = *(p->trapframe);
p->trapframe->epc = p->alarm_handler;
p->alarm_active = 1;
```

`sigreturn()` 执行反向复制。handler 地址可以为 0，所以是否启用由 interval 判断，而不是用 handler 是否非零判断。

## 8. 执行流程分析

进程调用 `sigalarm(2, periodic)` 后继续执行。每次用户态 timer trap 进入 `usertrap()`，内核递增该进程计数。第二个 tick 到来时保存现场并修改 `epc`；trap 返回后从 `periodic` 开始执行。handler 最后调用 `sigreturn`，系统调用恢复先前 PC 和全部寄存器，随后回到被中断指令继续运行。

handler 执行期间 `alarm_active` 保持为 1，即使再次发生时钟中断也不会保存第二份现场或重入。完成 `sigreturn` 后才允许下一周期触发。

## 9. 测试方法

```bash
make clean
make fs.img
./grade-lab-traps "backtrace test"
./grade-lab-traps alarmtest
./scripts/run-current-lab.sh
```

手动运行 `bttest` 和 `alarmtest`，再用 `addr2line -e kernel/kernel <地址>` 验证调用链。

## 10. 测试结果

2026-08-18 完整 grader：

```text
answers-traps.txt: OK
backtrace test: OK
alarmtest: test0: OK
alarmtest: test1: OK
alarmtest: test2: OK
usertests: OK
time: OK
Score: 85/85
```

完整输出在 `results/traps/grade.txt`，测试前提交为 `3d45e7ae64d1a2c70541a838c81802419df1edbe`；手动输出在 `results/traps/manual.txt`。

## 11. 遇到的问题

1. 新增系统调用后，`printf` 在 `call.asm` 中的地址发生变化。
2. handler 返回后必须恢复 PC 之外的全部用户寄存器。
3. `syscall()` 会把处理函数返回值重新写入 trapframe 的 `a0`。
4. 慢 handler 可能跨越多个 timer tick，产生重入风险。

## 12. 问题原因

用户程序最终链接布局取决于存根数量，所以书面题中的地址必须在完成分支上重新读取。alarm 是在任意指令边界插入的异步控制流，仅恢复 PC 会破坏循环变量、栈指针和临时寄存器。`sigreturn` 若固定返回 0，又会覆盖刚恢复的 `a0`。

## 13. 解决方法

- 从最终 `user/call.asm` 读取 `printf=0x64e` 和 `ra=0x40`。
- 复制整个 `struct trapframe`，而不是选择性保存寄存器。
- `sys_sigreturn()` 返回保存的 `a0`，使分发层写回同一值。
- 用 `alarm_active` 禁止重入，恢复完成后再清除。

## 14. 实验结果分析

`test0` 验证首次转向 handler；`test1` 比较循环变量以验证周期调用和完整寄存器恢复；`test2` 使用慢 handler 验证不可重入。完整 `usertests` 通过说明 trap 返回、调度与普通系统调用未被破坏。

## 15. 实验总结

本实验展示了普通函数调用、系统调用和硬件中断三类控制转移之间的联系。栈回溯依赖编译器约定，alarm 则依赖 trapframe 这一明确的体系结构现场；正确性关键是把异步插入的 handler 做到对原程序透明。
