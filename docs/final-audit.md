# xv6 Final Submission Audit

- 审计日期：2026-08-18
- 审计对象：MIT 6.S081 Fall 2021 / xv6-riscv 课程项目
- 审计基准：`mmap` 分支，提交 `b8f4dd7b866b0216c4765d572259da2410680a46`

## 1. Audit Summary

结论：**PASS**。

十个实验分支、最终评分记录、实验报告、总报告、环境说明、架构图和演示脚本均已完成审计。保存的官方 grader 最终分数为 846/846，未发现核心实现与文档描述不一致的问题，也未发现需要修改 `kernel/`、`user/` 或 `Makefile` 的确定错误。

本轮只整理提交文档，文档封版已提交到 `mmap` 分支。最终提交前仍需由项目所有者完成个人远程仓库创建与推送、写入真实仓库 URL、导出提交文件和现场演练。

## 2. Git Status

- 当前分支：`mmap`
- 核心实现与审计基线：`b8f4dd7b866b0216c4765d572259da2410680a46`
- 审计开始时工作树：clean
- 文档封版后工作树：clean；README、`docs/` 与必要图件已提交到当前 `mmap` 分支
- 已配置远程：只有 MIT 官方 `origin`，地址为 `git://g.csail.mit.edu/xv6-labs-2021`
- 个人远程仓库：尚未配置
- 分支结构：十个实验均保留独立分支及实现提交，未在本轮进行 merge、rebase 或 squash
- Git 对象检查：未发现完整性错误

## 3. Lab Completion

| Lab | Branch | Final score | Status |
| --- | --- | ---: | --- |
| Utilities | `util` | 100/100 | Completed |
| System calls | `syscall` | 35/35 | Completed |
| Page tables | `pgtbl` | 46/46 | Completed |
| Traps | `traps` | 85/85 | Completed |
| Copy-on-write | `cow` | 110/110 | Completed |
| Multithreading | `thread` | 60/60 | Completed |
| Network driver | `net` | 100/100 | Completed |
| Lock | `lock` | 70/70 | Completed |
| File system | `fs` | 100/100 | Completed |
| mmap | `mmap` | 140/140 | Completed |
| **Total** | — | **846/846** | **Completed** |

所有 `results/<lab>/grade.txt` 均存在且非空，分数与 README、进度表、总报告和实验报告一致。

## 4. Grade Verification

本轮逐个读取并核对十份最终 `grade.txt`，确认总分算术结果为 846/846。最终日志中没有实质测试失败：常见搜索命中的 `-Werror` 是编译参数，不是错误；File System 日志中的 `Old xv6.out.bigfile failure log removed` 是评分器清理旧日志的提示，同一最终评分记录中 `running bigfile: OK` 且总分为 100/100。

本轮没有重复运行十个完整 grader，避免无必要地改变已完成项目状态。提交目录同时保留了若干 `baseline-*.txt`，其中的 FAIL、timeout 或低分属于实现前的历史基线证据，不是最终结果，不能与 `grade.txt` 混为一谈或删除。

## 5. Documentation Audit

- `README.md`：实验表、分支、逐项分数、总分、环境入口、演示入口和仓库状态完整。
- `docs/progress.md`：十个实验均标为完成，仅保留个人远程仓库这一项人工任务。
- `docs/final-report.md`：项目范围、实现方法、基线/最终成绩、环境、验证证据和限制与源码一致。
- `docs/architecture.md`：描述的是 xv6 用户态、系统调用/trap、内核子系统和 RISC-V/QEMU 的关系，不是 Linux 内核架构。Mermaid 图已通过官方 CLI 实际渲染检查。
- `docs/environment.md`：明确区分 Windows 宿主、WSL Ubuntu、RISC-V 工具链、QEMU 和 xv6，版本信息与环境检查输出一致。
- `docs/defense.md`：包含环境准备、演示命令、核心概念、26 个高频追问、mmap 完整源码讲解路线和 5/10 分钟讲稿。
- `docs/labs/*.md`：十份实验报告均存在，每份有完整的问题、实现、测试和结果结构；其中 Utilities 覆盖 sleep、pingpong、primes、find、xargs，System Calls 同时覆盖 trace 和 sysinfo。
- Markdown 本地链接：README 中被引用的文档路径均存在。
- 占位符与路径：未发现示例域名、虚假仓库 URL 或残留的本机 D 盘绝对路径；脚本中的随机临时目录模板和答辩中的泛化函数名都不是提交占位符。
- 表达质量：未发现空泛的“提高了能力”“加深了理解”等模板化结论替代技术说明的情况。

## 6. Source / Documentation Consistency

通过按分支读取源码交叉核对，文档中的关键实现与代码一致：

- `util`：Makefile 和用户程序包含 sleep、pingpong、primes、find、xargs。
- `syscall`：trace mask 位于 proc，fork 继承；trace/sysinfo 的编号、分派、参数和用户桩完整。
- `pgtbl`：USYSCALL、vmprint、pgaccess 及 PTE_A 清除路径存在。
- `traps`：backtrace、sigalarm/sigreturn、trapframe 备份、非重入状态和时钟路径一致。
- `cow`：PTE_COW、物理页引用计数、uvmcopy、写故障和 copyout 路径形成闭环。
- `thread`：RISC-V 用户线程切换、哈希桶锁和 barrier round 与报告一致。
- `net`：E1000 TX/RX 环、DD 所有权位和 mbuf 生命周期与报告一致。
- `lock`：per-CPU freelist/stealing、13 个 bcache bucket 和淘汰协调逻辑与报告一致。
- `fs`：11 个直接块、一级/二级间接块、对称释放、symlink、`O_NOFOLLOW` 和深度限制一致。
- `mmap`：16 个 VMA、lazy fault、权限映射、shared 回写、部分 unmap、fork 复制及 exit/exec 清理一致。

未发现夸大为 Linux 通用实现、声称超出 xv6 实验范围，或把尚未实现功能写成已完成的情况。

## 7. Test Result Audit

- 最终结果位置统一为 `results/<lab>/grade.txt`，十份文件均可读、非空且含精确 Score。
- 环境冷启动记录位于 `results/environment/final-cold-boot.txt`。
- 基线日志保留在明确命名的 `baseline-*.txt` 中，用于说明实现前状态。
- Shell 脚本已通过 `bash -n` 语法检查。
- `scripts/defense-demo.sh` 在 `/tmp` 创建隔离克隆，只在副本中 checkout 和构建，不切换主工作树。
- `scripts/run-all-tests.sh` 同样使用隔离克隆，按十个实验分支调用对应评分器，结果写到仓库外的时间戳目录。
- 环境检查脚本已成功报告 WSL、Ubuntu、Git、Make、QEMU、RISC-V GCC/Binutils 和 GDB。

建议最终归档或答辩材料保留三类截图：一张十项最终 PASS/Score 汇总、一张 `mmaptest: all tests succeeded` 现场输出、一张能看到 xv6 shell 与 QEMU 启动信息的运行画面。截图应由项目所有者在最终环境中实际运行后获取，不应用编辑后的图片替代日志。

## 8. Defense Readiness

答辩材料已可支持以下流程：

1. 用 `scripts/check-env.sh` 说明宿主机、WSL、交叉工具链与 QEMU 的边界。
2. 用 `scripts/defense-demo.sh <lab> grade` 在隔离副本复核随机实验。
3. 用 mmap 作为综合演示，从用户桩、系统调用、VMA、page fault、装页、回写讲到 fork/exit/exec 清理。
4. 根据答辩时长直接使用 5 分钟或 10 分钟讲稿。
5. 对系统调用、页表、trap、COW、同步、锁、文件系统、网卡和 mmap 的高频追问，回答可落到具体数据结构和函数路径。

现场演示前应至少完整排练一次，并准备保存的 `grade.txt` 作为 QEMU 启动异常时的只读备用证据。

## 9. Remaining Manual Tasks

以下任务需要项目所有者的账号、提交决定或现场操作，本轮未代为执行。其中仓库相关待办只保留一项：

1. 创建个人远程仓库并推送全部需要的 Lab branches。
2. 按老师最终提交格式导出 PDF/Word 或打包文件，并检查导出内容。
3. 答辩前在最终机器上完成一次现场彩排，并截取真实的成绩、mmap demo 和 xv6 shell 画面。

## 10. Final Conclusion

项目在代码实现、十实验分支、官方评分证据、实验报告、总报告、环境复现、架构说明和答辩准备方面达到提交条件，最终审计结果为 **PASS**。本轮没有修改 xv6 核心代码；文档变更已通过 `git diff` 检查并提交，个人远程仓库创建与推送仍由项目所有者完成。
