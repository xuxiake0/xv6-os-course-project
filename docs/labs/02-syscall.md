# Lab 02：System Calls

## 1. 实验目的

理解 xv6 系统调用从用户态存根、`ecall`、trap 处理到内核分发函数的完整路径，并通过 `trace` 与 `sysinfo` 掌握进程状态、用户/内核数据复制和内核共享数据的加锁读取。

## 2. 实验环境

- 分支：`syscall`
- 官方原始提交：`1e6e6cafbb26a898b8c3f90e819fc5e7227dc8af`
- WSL2 Ubuntu 26.04、RISC-V GCC 15.2.0、QEMU 10.2.1
- 公共工具链与兼容处理见 `docs/environment.md`

## 3. 相关 xv6 原理

用户程序调用 `read()` 等函数时，实际进入 `user/usys.pl` 生成的汇编存根。存根把系统调用号写入寄存器 `a7` 并执行 `ecall`。CPU 切换到 supervisor mode，经 trampoline 保存用户寄存器后进入 `usertrap()`，再由 `syscall()` 根据 `a7` 查询函数表。

参数保存在 trapframe 的 `a0` 至 `a5`，`argint` 和 `argaddr` 从这些寄存器读取。系统调用返回值写回 trapframe 的 `a0`，回到用户态后表现为 C 函数返回值。

内核不能直接信任用户指针。`sysinfo` 先在内核栈构造结果，再使用 `copyout` 按当前进程页表验证并复制到用户虚拟地址。

## 4. 实验任务

1. `trace(mask)`：按位掩码跟踪指定系统调用，输出 PID、调用名和返回值，且子进程继承掩码。
2. `sysinfo(info)`：返回当前空闲物理内存字节数和非空闲进程数，对非法地址返回错误。

## 5. 原始代码分析

原始分支已经提供 `user/trace.c`、`user/sysinfotest.c` 和 `kernel/sysinfo.h`，但未加入 `UPROGS`，也没有系统调用号、用户存根、内核处理函数或进程字段。

基线 `make grade` 结果为 `5/35`：未启用跟踪时无额外输出这一项通过，其余测试因程序不存在而失败。原始输出保存于 `results/syscall/baseline-grade.txt`。

## 6. 设计思路

- 新增系统调用号 22（trace）和 23（sysinfo），保持现有编号不变。
- 在 `struct proc` 中保存 `trace_mask`，`fork` 时复制，释放进程时清零。
- 在统一 `syscall()` 分发点获得返回值后再打印，使所有系统调用共享同一跟踪逻辑。
- 空闲内存统计遍历 `kmem.freelist` 并持有 allocator 锁。
- 进程统计逐个持有 `p->lock`，只统计状态不为 `UNUSED` 的槽位。
- `sys_sysinfo` 使用 `copyout`，因此无效用户指针自然返回 `-1`。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/syscall.h` | 新增系统调用号 |
| `user/user.h`、`user/usys.pl` | 用户原型与 `ecall` 存根 |
| `kernel/syscall.c` | 处理函数表、名称表和跟踪输出 |
| `kernel/sysproc.c` | `sys_trace`、`sys_sysinfo` |
| `kernel/proc.h`、`kernel/proc.c` | 掩码存储/继承、进程计数 |
| `kernel/kalloc.c`、`kernel/defs.h` | 空闲内存统计及声明 |
| `user/trace.c` | 给传入 `exec` 的参数数组补空指针终止项 |
| `Makefile` | 加入 `_trace` 与 `_sysinfotest` |

对应提交：`4d9b6f5`（trace）、`0d66b09`（sysinfo）。

### 7.2 关键数据结构

```c
struct proc {
  // ...
  int trace_mask;
};

struct sysinfo {
  uint64 freemem;
  uint64 nproc;
};
```

掩码的第 `n` 位对应系统调用号 `n`。`sysinfo` 使用固定宽度字段，用户态和内核态共享同一头文件定义。

### 7.3 关键代码

文件：`kernel/syscall.c`；函数：`syscall`；作用：完成调用后按掩码打印。

```c
int retval = syscalls[num]();
p->trapframe->a0 = retval;
if(p->trace_mask & (1 << num))
  printf("%d: syscall %s -> %d\n",
         p->pid, syscall_names[num], retval);
```

文件：`kernel/proc.c`；函数：`fork`；作用：让后代继承跟踪策略。

```c
np->trace_mask = p->trace_mask;
```

文件：`kernel/sysproc.c`；函数：`sys_sysinfo`；作用：安全写回用户空间。

```c
info.freemem = freemem();
info.nproc = nproc();
if(copyout(p->pagetable, address,
           (char *)&info, sizeof(info)) < 0)
  return -1;
```

## 8. 执行流程分析

以 `trace 32 grep hello README` 为例：`trace` 用户程序首先调用 `trace(32)`；`sys_trace` 把掩码写入当前进程。随后 `exec` 用 `grep` 替换地址空间，但仍使用同一个 `struct proc`，因此掩码保留。`grep` 的系统调用号 5（read）对应 `1 << 5 == 32`，每次 read 返回后由统一分发点打印结果。

`sysinfo` 先通过 `argaddr` 取得用户虚拟地址，然后分别锁定 allocator 和进程表元素读取一致状态。最后 `copyout` 走页表翻译并检查目标页是否为合法用户映射。

## 9. 测试方法

单项测试：

```bash
make clean
make
./grade-lab-syscall trace
./grade-lab-syscall sysinfo
```

完整测试与结果保存：

```bash
./scripts/run-current-lab.sh
```

手动测试：

```text
trace 32 grep hello README
sysinfotest
```

## 10. 测试结果

2026-08-18 完整 grader：

```text
trace 32 grep: OK
trace all grep: OK
trace nothing: OK
trace children: OK
sysinfotest: OK
time: OK
Score: 35/35
```

完整输出为 `results/syscall/grade.txt`，对应测试前提交 `5e9c98cc6cc36875c84825711ddd04d0e70a7717`。手动输出保存在 `results/syscall/manual.txt`。

## 11. 遇到的问题

1. `trace` 的掩码必须在 `exec` 后保留，并由 `fork` 复制给子进程。
2. 官方 `trace.c` 组装的新参数数组没有显式空指针终止项。
3. 统计 allocator 链表和进程状态时存在并发读写。
4. 用户传入 `sysinfo` 的地址可能完全非法。

## 12. 问题原因

`exec` 不创建新进程，而是替换当前进程地址空间；`fork` 才创建新的 `struct proc`。内核共享链表和进程状态会被其他 CPU 修改，用户地址也不能作为内核指针直接解引用。

## 13. 解决方法

- 将掩码放在 `struct proc` 中，`fork` 显式复制，`freeproc` 清零。
- 在 `trace.c` 中设置 `nargv[i-2] = 0`。
- 读取 freelist 时持有 `kmem.lock`，读取进程状态时持有对应 `p->lock`。
- 使用 `copyout` 完成验证和复制，错误直接返回 `-1`。

## 14. 实验结果分析

`trace all` 验证了名称表、返回值和 trace 自身调用；`trace nothing` 验证默认状态不会污染输出；`trace children` 验证 fork 继承。`sysinfotest` 通过耗尽/恢复物理页、fork/wait 和非法地址三类场景验证两个统计值及边界处理。

## 15. 实验总结

本实验串联了用户 API、汇编存根、trapframe、系统调用分发表与具体内核服务。系统调用边界的核心原则是：寄存器只传递原始参数，用户地址必须经过页表感知的复制函数，读取共享内核状态必须遵守相应锁规则。
