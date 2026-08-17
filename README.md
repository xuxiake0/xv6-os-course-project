# xv6 Operating System Labs

## 项目简介

本项目基于 MIT 6.S081 Fall 2021 的 `xv6-labs-2021`，按官方分支完成十个 RISC-V 操作系统实验。代码、测试记录与实验报告素材均以实际运行结果为准。

## 实验列表

| Lab | Topic | Branch | Status |
| --- | --- | --- | --- |
| 1 | Utilities | `util` | 已完成（100/100） |
| 2 | System calls | `syscall` | 已完成（35/35） |
| 3 | Page tables | `pgtbl` | 已完成（46/46） |
| 4 | Traps | `traps` | 已完成（85/85） |
| 5 | Copy-on-write | `cow` | 已完成（110/110） |
| 6 | Multithreading | `thread` | 已完成（60/60） |
| 7 | Network driver | `net` | 已完成（100/100） |
| 8 | Lock | `lock` | 已完成（70/70） |
| 9 | File system | `fs` | 已完成（100/100） |
| 10 | mmap | `mmap` | 已完成（140/140） |

## 环境

- Windows 11 + WSL2
- Ubuntu 26.04 LTS（发行版存储位于 `E:\WSL\Ubuntu`）
- RISC-V GNU toolchain
- QEMU RISC-V system emulator
- GNU Make、Git、GDB multiarch

精确版本和启动验收见 [docs/environment.md](docs/environment.md)。

## 构建与运行

在 WSL Ubuntu 中进入仓库：

```bash
make clean
make
make qemu
```

进入 xv6 shell 后，使用 `Ctrl-a x` 退出 QEMU。

## 测试

当前分支完整测试：

```bash
make grade
```

带可追溯信息并保存结果：

```bash
./scripts/run-current-lab.sh
```

在不改动主工作树的隔离克隆中复测全部分支：

```bash
./scripts/run-all-tests.sh
```

答辩冷构建与交互演示：

```bash
./scripts/defense-demo.sh mmap demo
```

## 项目结构

- `kernel/`：xv6 内核
- `user/`：用户程序与用户态库
- `mkfs/`：文件系统镜像生成工具
- `docs/`：环境、进度和实验报告素材
- `results/`：实际测试输出
- `scripts/`：环境检查、测试和答辩脚本

## 实验报告

各 Lab 报告位于 `docs/labs/`；总报告素材见 [docs/final-report.md](docs/final-report.md)，答辩提纲见 [docs/defense.md](docs/defense.md)。十个官方评分合计 `846/846`。

## Repository

上游仓库：`git://g.csail.mit.edu/xv6-labs-2021`。课程项目远程仓库链接待用户配置后补充。
