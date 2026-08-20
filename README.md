# xv6 Operating System Labs

## 项目简介

本项目基于 MIT 6.S081 Fall 2021 的 `xv6-labs-2021`，按官方分支完成十个 RISC-V 操作系统实验。代码、测试记录与项目文档均以实际实现和运行结果为依据。

## 实验列表

| Lab | Topic | Branch | Score | Status |
| --- | --- | --- | ---: | --- |
| util | Utilities | `util` | 100/100 | Completed |
| syscall | System calls | `syscall` | 35/35 | Completed |
| pgtbl | Page tables | `pgtbl` | 46/46 | Completed |
| traps | Traps | `traps` | 85/85 | Completed |
| cow | Copy-on-write | `cow` | 110/110 | Completed |
| thread | Multithreading | `thread` | 60/60 | Completed |
| net | Network driver | `net` | 100/100 | Completed |
| lock | Lock | `lock` | 70/70 | Completed |
| fs | File system | `fs` | 100/100 | Completed |
| mmap | mmap | `mmap` | 140/140 | Completed |

**Total: 846/846**

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
- `docs/`：环境说明、项目总报告、实验报告和答辩材料
- `results/`：实际测试输出
- `scripts/`：环境检查、测试和答辩脚本

## 项目文档

项目总报告见 [docs/final-report.md](docs/final-report.md)，各 Lab 详细报告位于 `docs/labs/`，答辩手册见 [docs/defense.md](docs/defense.md)，PPT 内容规划见 [docs/ppt-outline.md](docs/ppt-outline.md)。十个官方评分合计 `846/846`。

内部提交检查记录保留在 [docs/submission-readiness.md](docs/submission-readiness.md) 和 [docs/final-audit.md](docs/final-audit.md)，不作为老师阅读项目的主入口。

## Repository

- 项目仓库：https://github.com/xuxiake0/xv6-os-course-project
- MIT 官方上游：`git://g.csail.mit.edu/xv6-labs-2021`
