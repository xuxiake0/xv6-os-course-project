# Submission & Defense Readiness

审查基准：`mmap` 分支，核心实现基线 `b8f4dd7b866b0216c4765d572259da2410680a46` 及其后的文档封版提交。核心 Lab 代码未改变。

## SUBMISSION READINESS

```text
Code:
PASS

Labs:
10/10

Grade:
846/846

Documentation:
PASS

Word/PDF readiness:
NEEDS FIX

PPT readiness:
PASS

Repository:
WAITING FOR USER REMOTE

Defense:
READY
```

## 1. Code and Labs

- `kernel/`、`user/`、`Makefile` 在本阶段没有修改。
- 10 个实验均有独立 branch、实现提交、详细报告和最终结果文件。
- 源码符号已按实验分支交叉核对，final report 和 defense 中引用的关键文件、函数与数据结构均能在对应分支定位。
- 10 个最终 `results/<lab>/grade.txt` 均存在且非空，分数合计 846/846。

## 2. Documentation

`docs/final-report.md` 已整理为 Word/PDF 母稿，包含封面字段、摘要、目录说明、项目概述、工作量、环境、架构、10 个实验、统一验证表、关键问题、总结、仓库状态和附录。

十份 `docs/labs/*.md` 均具备以下真实内容：

1. 实验目的/要求；
2. 实验内容、步骤或执行流程；
3. 关键实现、源码文件、函数和数据结构；
4. 实际遇到的问题、原因与解决方法；
5. 来自现有 results 的测试结果；
6. 对定向测试与完整 grader 的结果分析；
7. 与该实验技术不变量相关的实验总结/心得。

未发现需要用模板化心得替换的段落，也未为追求篇幅重复改写十份已经具体、完整的 Lab 报告。

## 3. Word/PDF Readiness

母稿内容和 Heading 层级已具备自动目录条件，但最终 Word/PDF 尚未生成，因此状态为 `NEEDS FIX`。完成以下用户输入与版式步骤后可转为 PASS：

- 填写学校、学院、姓名、学号、班级、教师和提交日期；
- 按学校模板映射 Heading 1/2/3、插入自动目录和页码；
- 需要时插入本人实际运行获得的 grader、mmap demo 与 xv6 shell 截图；
- 检查表格跨页、中文字体、代码换行和 PDF 链接；
- 在个人远程仓库建立后写入真实 URL。

## 4. PPT Readiness

`docs/ppt-outline.md` 已给出 14 页内容规划，避免按十个 Lab 逐页流水账。COW 与 mmap 共占 4 页，System Call/Trap/Page Table、Multithreading/Lock、File System、测试证据和现场 Demo 形成完整主线。所有数字均来自现有结果，PPT 内容规划状态为 `PASS`。

实际 PPT 文件、学校模板、个人信息和真实终端截图仍需由提交者制作或提供；不得用合成图片替代运行证据。

## 5. Repository

当前 `git remote -v` 只有 MIT 官方：

```text
origin  git://g.csail.mit.edu/xv6-labs-2021 (fetch)
origin  git://g.csail.mit.edu/xv6-labs-2021 (push)
```

没有个人远程，也没有可写入文档的真实 URL。仓库相关只保留一个待办：

```text
创建个人远程仓库并推送全部需要的 Lab branches
```

收到真实 URL 后再同步写入 README 与 final report；当前不虚构地址，也不改变 MIT 官方 `origin`。

## 6. Defense

`docs/defense.md` 已包含：

- 26 个高频问题及 30～90 秒口头答案；
- 26/26 对应的 branch、源码文件、关键函数、现场位置与适用演示命令；
- mmap 从用户 stub 到 VMA、page fault、装页、回写、fork/exit/exec 的 12 步讲解路线；
- 5 分钟和 10 分钟口头讲稿；
- 环境检查、隔离式 grader 和 QEMU demo 备用命令。

答辩材料状态为 `READY`。最终机器仍应做一次真实彩排，以确认投影字号、命令输入和 QEMU 启动时间。

## 7. Remaining User Actions

1. 填写封面个人/课程信息，并按学校格式导出带自动目录的 Word/PDF。
2. 创建个人远程仓库并推送全部需要的 Lab branches。
3. 按 `docs/ppt-outline.md` 制作最终 PPT，并插入本人实际运行截图。
4. 在答辩机器完成一次 5/10 分钟讲稿与 mmap demo 彩排。

## 8. FINAL EXPORT CHECK

实际导出 Word/PDF 前，必须对最终报告正文执行全文检查，确认以下母稿标记或内部说明已经不存在：

```text
由提交者填写
按实际课程名称核对
删除本提示
个人远程仓库尚未配置
唯一仓库待办
不得使用虚构 URL
内部审计说明
```

同时确认：

1. 学校、学院、课程、姓名、学号、班级、教师和日期均已填写；
2. Word 自动目录已更新，标题层级、页码和表格跨页正常；
3. 真实个人 Git URL 已同时写入 `README.md` 与 `docs/final-report.md`，两处完全一致；
4. 正文不包含提交过程、审计过程或 Codex 工作过程说明；
5. 插图与终端截图均来自本人实际运行，图题和引用位置完整；
6. 导出的 PDF 已逐页检查中文字体、代码换行、链接和图片清晰度。

## 9. Conclusion

代码、实验完成度、成绩证据、Markdown 文档和答辩内容已达到提交准备要求。当前阻止“完全可交付”的事项仅涉及用户身份信息、个人远程仓库和最终 Word/PDF/PPT 文件制作，不涉及继续开发或修改已获 846/846 的 xv6 核心实现。
