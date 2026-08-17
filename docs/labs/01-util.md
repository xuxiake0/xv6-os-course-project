# Lab 01：Utilities

## 1. 实验目的

熟悉 xv6 用户程序的构建与运行方式，掌握 `fork`、`exec`、`wait`、`pipe`、文件描述符、目录项和基本系统调用的组合使用。

## 2. 实验环境

- MIT 6.S081 Fall 2021，`xv6-labs-2021` 的 `util` 分支
- 原始提交：`f654383cdec479c9d53a02bffa1ab5526f6c3ca4`
- WSL2 Ubuntu 26.04 LTS
- RISC-V GCC 15.2.0，QEMU 10.2.1
- 完整版本与兼容处理见 `docs/environment.md`

## 3. 相关 xv6 原理

xv6 用户程序由 Makefile 编译为无标准库依赖的 RISC-V ELF 文件，再由 `mkfs` 写入 `fs.img`。shell 通过 `fork` 创建子进程、在子进程中 `exec` 用户程序，并由父进程 `wait` 回收。

`pipe` 返回读端和写端两个文件描述符。只有所有写端都关闭后，读端才会获得 EOF，因此并发程序必须明确关闭不再使用的描述符。

目录在 xv6 中也是文件，其内容是连续的 `struct dirent`。目录名固定占 `DIRSIZ` 字节，读取后需要手动补 `\0` 才能作为 C 字符串使用。

## 4. 实验任务

实现以下五个用户程序：

1. `sleep`：按 tick 暂停当前进程。
2. `pingpong`：父子进程通过 pipe 双向传递一个字节。
3. `primes`：用进程流水线实现并发素数筛。
4. `find`：递归查找指定名称的文件或目录。
5. `xargs`：将标准输入逐行转换为命令参数并执行。

## 5. 原始代码分析

原始 `util` 分支没有上述五个源文件，Makefile 的 `UPROGS` 也未将它们写入文件系统镜像。第一次功能基线测试因此得到 `Score: 0/100`，所有 xv6 shell 调用均报告 `exec ... failed`。

此外，第一次 grader 启动被 Python 3.14 移除 `pipes` 模块阻塞。通过环境兼容包解决后再次运行，才得到真实的功能基线。两次输出分别保存在：

- `results/util/baseline-grade.txt`
- `results/util/baseline-functional-grade.txt`

## 6. 设计思路

- 每个功能保持为独立、短小的 xv6 用户程序。
- 对 `pipe`、`fork`、`read`、`write` 和 `exec` 的失败进行显式处理。
- 父进程负责 `wait`，每个进程只保留必要文件描述符。
- `find` 在递归前跳过 `.` 和 `..`，并检查路径缓冲区长度。
- `xargs` 同时限制单行长度与 `MAXARG`，避免覆盖缓冲区或越过内核参数上限。

## 7. 具体实现

### 7.1 修改文件

| 文件 | 作用 | 提交 |
| --- | --- | --- |
| `user/sleep.c` | 参数转换并调用 `sleep` 系统调用 | `099f6fb` |
| `user/pingpong.c` | 两个 pipe 完成父子往返通信 | `c60d4a2` |
| `user/primes.c` | 递归创建过滤进程链 | `07a7581` |
| `user/find.c` | 读取目录项并递归遍历 | `4fd28b3` |
| `user/xargs.c` | 逐行组装参数并 `exec` | `b5b2fb9` |
| `Makefile` | 将五个程序加入 `UPROGS` | 随各功能提交 |
| `time.txt` | 记录 grader 要求的整小时投入 | `1cd9b3f` |

### 7.2 关键数据结构

- `int pipefd[2]`：`pipefd[0]` 为读端，`pipefd[1]` 为写端。
- `struct dirent`：包含 inode 编号和 `DIRSIZ` 字节文件名。
- `struct stat`：用于区分 `T_FILE` 与 `T_DIR`。
- `char *args[MAXARG]`：`xargs` 传给 `exec` 的参数指针数组。

### 7.3 关键代码

文件：`user/primes.c`；函数：`sieve`；作用：每个进程负责一个素数过滤级。

```c
if(read(input, &prime, sizeof(prime)) != sizeof(prime)){
  close(input);
  exit(0);
}
printf("prime %d\n", prime);
```

输入是上一级 pipe 的整数流；首个整数一定是本级素数。剩余非倍数写入下一级 pipe，关闭写端后以 EOF 通知下一级结束。

文件：`user/find.c`；函数：`find`；作用：安全递归目录树。

```c
memmove(p, de.name, DIRSIZ);
p[DIRSIZ] = 0;
if(strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
  continue;
find(buf, target);
```

固定长度目录名被转换为 C 字符串，随后跳过两个特殊目录项，避免无限递归。

文件：`user/xargs.c`；函数：`run_line`；作用：执行一行输入形成的命令。

```c
pid = fork();
if(pid == 0){
  exec(args[0], args);
  fprintf(2, "xargs: exec %s failed\n", args[0]);
  exit(1);
}
wait(0);
```

父进程逐行等待，使每行输入对应一次完整命令执行；子进程仅在 `exec` 失败时返回错误。

## 8. 执行流程分析

`pingpong` 使用两个单向通道：父进程向第一个 pipe 写入字节，子进程读取并打印 `ping`；子进程再向第二个 pipe 写入，父进程读取并打印 `pong`。

`primes` 的第一级输入为 2 到 35。每一级取首值作为素数，创建一个子进程处理下一级，只把不能被当前素数整除的值写入。关闭 pipe 传播 EOF 后，各级由后向前退出并被 `wait` 回收。

`find` 对当前路径执行 `fstat`：普通文件只比较名称；目录则逐个读取有效 `dirent`，拼接完整路径后递归。

`xargs` 保留初始命令参数，从 stdin 逐字符收集一行，在原缓冲区中以 `\0` 切分字段，并将指针追加到 `args` 后执行。

## 9. 测试方法

单项测试在每个实现后执行：

```bash
make clean
make
./grade-lab-util sleep
./grade-lab-util pingpong
./grade-lab-util primes
./grade-lab-util find
./grade-lab-util xargs
```

完整测试执行：

```bash
./scripts/run-current-lab.sh
```

另在 `make qemu` 的 xv6 shell 中手动运行五个程序，输出保存在 `results/util/manual.txt`。

## 10. 测试结果

2026-08-18 的完整官方 grader 结果：

```text
sleep, no arguments: OK
sleep, returns: OK
sleep, makes syscall: OK
pingpong: OK
primes: OK
find, in current directory: OK
find, recursive: OK
xargs: OK
time: OK
Score: 100/100
```

完整原始输出：`results/util/grade.txt`。该次测试对应提交 `1cd9b3fdf47636238b3112d6ef972845c718cd6d`。

## 11. 遇到的问题

1. Python 3.14 缺少官方 grader 导入的 `pipes` 模块。
2. GCC 15 默认 ISA 生成 `c.mul`，QEMU 默认 CPU 启动时非法指令。
3. pipe 的未使用写端若不关闭，会让下游进程一直等待 EOF。
4. 目录项名称不保证以 `\0` 结尾，且 `.`/`..` 会形成递归环。

## 12. 问题原因

前两项是 2021 教学代码与 2026 工具链的版本差异；后两项来自 Unix 文件描述符生命周期和 xv6 磁盘目录格式。

## 13. 解决方法

- 在环境中安装 `standard-pipes==3.13.0`，不修改官方 grader。
- 将编译和汇编目标明确固定为 `rv64gc`。
- 每次 `fork` 后立即关闭本进程不使用的 pipe 端点，写完后关闭写端传播 EOF。
- 对目录名补终止符，跳过 `.`/`..`，并检查拼接后长度。

## 14. 实验结果分析

单项测试证明每个功能满足对应行为；完整 grader 从干净构建开始，验证五个程序可共同写入同一 `fs.img` 并在真实 xv6/QEMU 环境中工作。手动测试进一步验证了可用于答辩的命令与输出。

## 15. 实验总结

本实验完成了 xv6 用户态程序、进程创建、程序替换、父子同步、管道通信和目录遍历的基础训练。最关键的工程认识是：并发程序是否能结束，往往取决于每个进程是否正确关闭了所有文件描述符，而不仅是核心计算是否正确。
