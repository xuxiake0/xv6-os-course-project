# 实验环境

## 1. 宿主与 WSL

- 宿主系统：Windows 11（build 22631.6199）
- WSL：2.7.11.0，WSL2
- Linux 内核：6.18.33.2-microsoft-standard-WSL2
- 发行版：Ubuntu 26.04 LTS（Resolute Raccoon）
- Ubuntu 存储位置：`E:\WSL\Ubuntu\ext4.vhdx`

Ubuntu、APT 安装的软件包与 Linux 用户文件均保存在上述 E 盘虚拟磁盘中。课程源码位于用户指定的 D 盘课程工作区。

## 2. 工具版本

| 工具 | 已验证版本 |
| --- | --- |
| Git | 2.53.0 |
| GNU Make | 4.4.1 |
| QEMU | 10.2.1 (`qemu-system-riscv64`) |
| RISC-V GCC | 15.2.0 (`riscv64-linux-gnu-gcc`) |
| RISC-V Binutils | 2.46 |
| GDB multiarch | 17.1 |

## 3. xv6 来源

- 官方仓库：`git://g.csail.mit.edu/xv6-labs-2021`
- Utilities 原始提交：`f654383cdec479c9d53a02bffa1ab5526f6c3ca4`
- 官方分支：`util`、`syscall`、`pgtbl`、`traps`、`cow`、`thread`、`net`、`lock`、`fs`、`mmap`

## 4. 新工具链兼容处理

Ubuntu 26.04 的 GCC 15 默认目标包含比 2021 课程环境更晚的 RISC-V 扩展。未固定 ISA 时，`kernel/entry.S` 的 `mul` 被汇编为 `c.mul`，QEMU 默认 CPU 在 `0x80000010` 处触发非法指令。构建现已显式固定为 `rv64gc`。

GCC 15 还新增/加强了递归和旧式函数指针诊断，因此只针对上游 2021 源码抑制 `-Winfinite-recursion` 与 `-Wincompatible-pointer-types`，其他警告仍由 `-Wall -Werror` 管理。

Binutils 2.46 会对 xv6 教学用 RWX 段发出警告；链接标志使用 `--no-warn-rwx-segments` 抑制这一已知且预期的教学内核布局提示。

## 5. 启动验收

执行：

```bash
make clean
make qemu
```

2026-08-18 实际终端出现：

```text
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

随后使用 `Ctrl-a x` 正常退出，QEMU 输出 `QEMU: Terminated`。原始验收摘要见 `results/environment/bootstrap.txt`。
