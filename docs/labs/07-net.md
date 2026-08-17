# Lab 07：Networking

## 1. 实验目的

实现 E1000 网卡驱动的数据收发路径，理解 MMIO、DMA、描述符环、设备中断和驱动缓冲区所有权。

## 2. 实验环境

- 分支：`net`
- 官方原始提交：`1bd9c80b1ef5eb70b91bbd1dbea9c1d953cdc555`
- WSL2 Ubuntu 与工具链位于 E 盘；QEMU E1000 用户网络
- xv6 地址 `10.0.2.15`，QEMU host 地址 `10.0.2.2`

## 3. 相关 xv6 原理

E1000 通过 DMA 直接读写内存中的 packet buffer。驱动和设备以 TX/RX descriptor ring 交换缓冲区所有权，并通过 MMIO 的 `TDT/RDT` 寄存器推进环尾。TX 的 DD 位表示设备已经发完并允许驱动复用描述符；RX 的 DD 位表示设备已经写入一个新包。

## 4. 实验任务

1. 实现 `e1000_transmit()`，提交 mbuf、处理环满并延迟释放旧缓冲区。
2. 实现 `e1000_recv()`，批量取出完成描述符、补充新缓冲区并交给 `net_rx()`。
3. 支持 ring wrap-around、多进程并发、ARP/UDP/DNS 流量。

## 5. 原始代码分析

初始化代码已配置 PCI/MMIO、16 项 TX/RX ring、MAC、接收中断和每个 RX descriptor 的 mbuf，但两个数据路径为空。基线 `nettests` 停在首次 ping，45 秒超时，得分 `0/100`。记录见 `results/net/baseline-grade.txt`。

## 6. 设计思路

- 使用 `e1000_lock` 串行化描述符与 MMIO tail 的更新。
- TX 读取 `TDT`，只有 DD 已置位才复用；先释放该槽上一次发送的 mbuf，再填地址、长度、EOP/RS 并推进 tail。
- RX 从 `(RDT+1)%16` 开始循环处理所有 DD 描述符。
- RX 在交出旧 mbuf 前先分配 replacement，更新 descriptor 并推进 RDT。
- 调用 `net_rx()` 前释放设备锁，因为 ARP 请求会从接收栈同步调用 transmit。

## 7. 具体实现

唯一核心文件为 `kernel/e1000.c`，提交 `790c48b`。

TX 描述符使用：

```c
desc->addr = (uint64)m->head;
desc->length = m->len;
desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
desc->status = 0;
regs[E1000_TDT] = (i + 1) % TX_RING_SIZE;
```

RX 完成后把 `m->len` 设置为硬件报告长度，换入 fresh mbuf，清零 status 并更新 `RDT`。寄存器写入前用 `__sync_synchronize()` 保证 DMA 可见顺序。

## 8. 执行流程分析

用户 `nettests` 通过 socket 系统调用形成 UDP mbuf，网络栈加入 UDP/IP/Ethernet header 后调用 transmit。E1000 DMA 读包并由 QEMU 交给 host server。host 首先发 ARP 请求，E1000 把帧 DMA 到 RX mbuf 并中断；驱动补充 descriptor 后把旧 mbuf交给网络栈，网络栈生成 ARP reply，再通过同一 TX 环发出，随后 UDP reply 才能被接收。

## 9. 测试方法

```bash
make clean
./grade-lab-net -v
./scripts/run-current-lab.sh
```

grader 自动启动 `make server`，QEMU 运行 `nettests`。每次运行的帧记录在 `packets.pcap`。

## 10. 测试结果

2026-08-18 正式评分：ping、single-process、multi-process、DNS 与 time 全部 OK，`Score: 100/100`。输出在 `results/net/grade.txt`，测试前提交 `e620fb05f1dcd56928ff20a1cb38234d5db5c725`；数据包捕获为 `packets.pcap`。

## 11. 遇到的问题

1. TX mbuf 不能在提交 descriptor 后立即释放。
2. RX descriptor 交给上层后必须立即补充可 DMA 的新 buffer。
3. 接收网络栈可能因 ARP reply 再次进入发送函数。
4. CPU 写 descriptor 与通知设备之间存在内存顺序要求。

## 12. 问题原因

DMA 与 CPU 异步执行，DD 位才是所有权归还信号。若持有 E1000 锁调用 `net_rx()`，ARP 路径再次获取同一锁会死锁。若先推进 tail 再写完 descriptor，设备可能看到半初始化请求。

## 13. 解决方法

- TX 每槽保存 mbuf 指针，只在该槽 DD 后释放。
- RX 先换入 replacement 并把 descriptor 归还设备，再把完成包交给上层。
- descriptor 操作在锁内，`net_rx()` 在锁外。
- 用 memory barrier 后再更新 `TDT/RDT`，索引均按 ring size 取模。

## 14. 实验结果分析

首次 ping 同时覆盖 TX、RX、ARP 与 UDP；单进程连续 ping 验证描述符复用和回绕；多进程测试验证锁；DNS 进一步验证面向真实外网的收发和长路径协议处理。四项全部通过说明环所有权与中断补充逻辑正确。

## 15. 实验总结

设备驱动的核心不是简单复制数据，而是维护 CPU、DMA 设备和协议栈之间的所有权协议。DD、tail、锁和内存屏障共同定义了安全的交接边界。
