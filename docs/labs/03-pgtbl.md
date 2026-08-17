# Lab 03：Page Tables

## 1. 实验目的

理解 RISC-V Sv39 三级页表、PTE 权限位以及 xv6 用户地址空间布局；通过共享只读页、页表遍历和访问位检测，掌握虚拟地址翻译、页表生命周期与用户/内核边界。

## 2. 实验环境

- 分支：`pgtbl`
- 官方原始提交：`1e6b2dec7d5ca49571e426ccc6cd686d009b6d07`
- WSL2 Ubuntu 26.04、RISC-V GCC 15.2.0、QEMU 10.2.1
- Ubuntu 发行版与工具链位于 E 盘，公共配置见 `docs/environment.md`

## 3. 相关 xv6 原理

Sv39 将 39 位虚拟地址拆成三级各 9 位的页表索引和 12 位页内偏移。非叶 PTE 指向下一级页表，叶 PTE 则包含物理页号及 `V/R/W/X/U` 等权限。`PTE_U` 决定用户态是否可访问，`PTE_A` 由硬件在页面被读写或取指后置位。

xv6 在用户地址空间顶部映射 `TRAMPOLINE` 和 `TRAPFRAME`。本实验再在其下方映射 `USYSCALL`，用户代码可以直接读取其中的 PID，因而省去 `ecall`、trapframe 保存和内核分发的开销。

## 4. 实验任务

1. 为每个进程分配只读的 `USYSCALL` 页，并让 `ugetpid()` 从该页读取 PID。
2. 实现 `vmprint()`，按层级打印 init 进程的有效页表项。
3. 实现 `pgaccess()`，返回最多 32 个页面的访问位并清除已观察到的 `PTE_A`。
4. 回答页表布局与共享页可扩展性的书面问题。

## 5. 原始代码分析

原始分支已提供 `USYSCALL` 地址、`struct usyscall`、用户测试和系统调用入口，但没有为共享页分配物理内存，也没有建立映射。`vmprint()` 尚不存在，`sys_pgaccess()` 仅返回 0。

未实现状态下完整评分为 `10/46`：只有原有 `usertests` 通过；三个新功能、问答和时间文件均未通过。输出保存于 `results/pgtbl/baseline-grade.txt`。

## 6. 设计思路

- 让 `struct proc` 持有共享页的内核地址，使其生命周期与 trapframe 一致。
- 在 `allocproc()` 中分配并初始化，在 `proc_pagetable()` 中以 `PTE_R | PTE_U` 映射，在失败路径和 `freeproc()` 中对称释放。
- `vmprint()` 递归遍历每一级的 512 个 PTE，只打印有效项；仅对没有叶权限位的 PTE 继续递归。
- `pgaccess()` 用 `walk()` 找到叶 PTE，验证映射与用户权限，读取并清除 `PTE_A`，最后用 `copyout()` 返回 32 位掩码。
- 清除访问位后执行 `sfence_vma()`，避免 TLB 中旧状态影响下一次观察。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 核心修改 |
| --- | --- |
| `kernel/proc.h` | 在进程结构中保存 `usyscall` 页指针 |
| `kernel/proc.c` | 分配、初始化、映射、取消映射并释放共享页 |
| `kernel/vm.c`、`kernel/defs.h` | 实现并声明递归页表打印；导出 `walk()` |
| `kernel/exec.c` | init 第一次 `exec` 后调用 `vmprint()` |
| `kernel/riscv.h` | 定义 `PTE_A` |
| `kernel/sysproc.c` | 实现 `sys_pgaccess()` |
| `answers-pgtbl.txt` | 回答共享页与 init 地址空间问题 |

对应提交：`3e98750`（USYSCALL）、`68351b7`（vmprint）、`5b7fa07`（pgaccess）、`38f7243`（问答）。

### 7.2 共享页权限与生命周期

```c
p->usyscall = (struct usyscall *)kalloc();
p->usyscall->pid = p->pid;

mappages(pagetable, USYSCALL, PGSIZE,
         (uint64)(p->usyscall), PTE_R | PTE_U);
```

映射没有 `PTE_W`，所以用户程序能读取 PID，却不能修改它。释放时先由 `proc_freepagetable()` 删除用户映射，再由 `freeproc()` 释放对应物理页，避免重复释放。

### 7.3 页表递归打印

```c
for(int i = 0; i < 512; i++) {
  pte_t pte = pagetable[i];
  if(pte & PTE_V) {
    // 打印 depth 个 " .."
    if(PTE_FLAGS(pte) == PTE_V)
      vmprintwalk((pagetable_t)PTE2PA(pte), depth + 1);
  }
}
```

只有权限标志恰为 `PTE_V` 的项才是非叶页表指针；带 `R/W/X` 的有效项是叶映射，不能当作页表继续解引用。

### 7.4 访问位收集

```c
if(*pte & PTE_A) {
  mask |= 1U << i;
  *pte &= ~PTE_A;
}
```

系统调用限制页数为 0 至 32，逐页验证 `PTE_V | PTE_U`，并用 `copyout()` 写回用户掩码；非法范围、未映射页或非法目标地址均返回 `-1`。

## 8. 执行流程分析

创建进程时，内核依次分配 trapframe 和 usyscall 物理页，再建立 trampoline、trapframe 与 USYSCALL 三个特殊映射。用户调用 `ugetpid()` 时直接加载 `USYSCALL->pid`，整个过程不切换特权级。

调用 `pgaccess(base, n, &mask)` 时，系统调用读取三个参数，按页大小遍历 `[base, base+n*PGSIZE)`。硬件此前置位的 `PTE_A` 被编码到掩码相应位并清除，因此相同页面在没有再次访问时不会在下一次调用中重复报告。

## 9. 测试方法

定向测试：

```bash
make clean
make
./grade-lab-pgtbl ugetpid
./grade-lab-pgtbl pgaccess
./grade-lab-pgtbl "pte printout"
```

完整测试与结果保存：

```bash
./scripts/run-current-lab.sh
```

手动启动后运行：

```text
pgtbltest
```

## 10. 测试结果

2026-08-18 完整 grader：

```text
pgtbltest: ugetpid: OK
pgtbltest: pgaccess: OK
pte printout: OK
answers-pgtbl.txt: OK
usertests: all tests: OK
time: OK
Score: 46/46
```

完整输出为 `results/pgtbl/grade.txt`，对应测试前提交 `38f7243ef045c9c7c5c2df9c4c7ddb0a0d78643c`。手动 `pgtbltest` 输出保存在 `results/pgtbl/manual.txt`。

## 11. 遇到的问题

1. 新版 GCC 编译 `sys_pgaccess()` 时提示 `walk` 没有函数声明。
2. USYSCALL 既出现在用户页表中，又有内核直接访问的物理页指针，释放顺序容易出错。
3. 页表中的有效项既可能是下级页表，也可能是叶映射。
4. 读取后清除 `PTE_A` 需要考虑 TLB 中缓存的页表状态。

## 12. 问题原因

`walk()` 原本只在 `vm.c` 内部使用，因此 `defs.h` 没有导出声明。`uvmunmap(..., 1)` 会释放叶映射的物理页，而进程清理代码也会释放 usyscall 指针，若两处都释放便产生 double free。判断叶节点不能只看 `PTE_V`，还必须检查 `R/W/X` 权限位。

## 13. 解决方法

- 在 `defs.h` 增加 `walk()` 原型并保持实现唯一。
- 删除 USYSCALL 映射时使用 `do_free=0`，由 `freeproc()` 统一释放物理页。
- 仅在 `PTE_FLAGS(pte) == PTE_V` 时递归。
- 清除访问位后执行 `sfence_vma()`，并通过 `copyout()` 安全返回结果。

## 14. 实验结果分析

页表打印结果中，低地址的页 0 是 init 程序，页 1 是去掉 `PTE_U` 的 guard page，页 2 是用户栈。高地址末三页依次为 USYSCALL、TRAPFRAME 和 TRAMPOLINE。完整 `usertests` 通过说明新增映射没有破坏 fork、exec、sbrk 和进程退出时的内存管理。

`pgaccess_test` 第一次访问指定缓冲区后能观察相应位，第二次调用又能验证清零语义；这同时覆盖了硬件置位、内核收集和重新观测三个环节。

## 15. 实验总结

本实验把 Sv39 的抽象结构落实到进程地址空间和实际 PTE 位操作。共享只读页说明系统调用并非获取内核数据的唯一方式，但必须以严格权限和生命周期管理换取性能；访问位实验则展示了硬件与操作系统通过页表协同维护内存状态的机制。
