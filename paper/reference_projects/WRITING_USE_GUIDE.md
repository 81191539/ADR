# Research Writing Reference Guide

本目录保存两个外部科研写作参考项目的本地浅克隆，用于辅助 `paper/` 下的论文写作、文献整理、作图和自审。

已拉取的项目：

```text
paper/reference_projects/academic-research-skills/
paper/reference_projects/awesome-ai-research-writing/
```

当前本地版本：

```text
academic-research-skills: 2236bed
awesome-ai-research-writing: c07628b
```

说明：两个外部项目作为本地参考资料使用，不作为 ADR 项目源码的一部分提交。`paper/reference_projects/.gitignore` 已忽略这两个克隆目录，避免把第三方仓库嵌套进本项目版本历史。

## 1. 两个项目各自适合做什么

### academic-research-skills

这个项目更像一套完整的科研写作流程框架，适合用于：

- 论文选题、研究问题和章节结构规划。
- 文献综述的证据矩阵和研究空白整理。
- IMRaD、综述、理论分析、案例研究等论文类型的结构选择。
- 摘要、引言、方法、结果、讨论等章节的质量检查。
- 图表规范、统计可视化规范和图注规范。
- 模拟审稿、自我审查、修改路线图和引用核查。

本项目最值得参考的文件：

```text
academic-research-skills/QUICKSTART.md
academic-research-skills/academic-paper/templates/imrad_template.md
academic-research-skills/academic-paper/templates/latex_article_template.tex
academic-research-skills/academic-paper/references/paper_structure_patterns.md
academic-research-skills/academic-paper/references/abstract_writing_guide.md
academic-research-skills/academic-paper/references/writing_quality_check.md
academic-research-skills/academic-paper/references/statistical_visualization_standards.md
academic-research-skills/academic-paper-reviewer/references/review_criteria_framework.md
academic-research-skills/deep-research/templates/literature_matrix_template.md
```

注意：该项目使用 CC BY-NC 4.0 许可证，适合作为非商业写作参考；如果后续要直接复制模板到论文仓库或公开材料中，应保留来源说明并注意非商业限制。

### awesome-ai-research-writing

这个项目更像一套日常可复制的 prompt 模板库，适合用于：

- 中文草稿转英文 LaTeX 段落。
- 英文 LaTeX 段落转中文理解稿。
- 缩写、扩写、表达润色和逻辑检查。
- 生成论文架构图提示词。
- 根据实验数据选择合适图表类型。
- 撰写图标题、表标题和实验分析段落。
- 从 reviewer 视角审视整篇论文。

本项目最值得参考的 README 章节：

```text
awesome-ai-research-writing/README.md
  - 中转英-latex
  - 英转中-latex
  - 缩写
  - 扩写
  - 表达润色（英文论文）
  - 逻辑检查
  - 论文架构图
  - 实验绘图推荐
  - 生成图的标题
  - 生成表的标题
  - 实验分析
  - 论文整体以 Reviewer 视角进行审视
```

注意：该仓库当前未看到独立 LICENSE 文件。引用、复制或改编其中内容前，建议先确认许可证或仅作为私人写作参考。

## 2. 推荐用于 ADR 论文的写作流程

### Step 1: 先确定论文类型和主线

使用 `academic-research-skills` 的结构模板，不要一开始就进入逐句润色。

建议操作：

1. 阅读 `paper_structure_patterns.md`。
2. 判断论文更接近哪一类：
   - 原创数值方法或实验研究：优先 IMRaD。
   - 对 ADR 方法体系做系统梳理：优先 thematic literature review。
   - 对数值模型、边界条件或算法框架做理论论证：优先 theoretical analysis。
   - 以 demo/case 说明求解器能力：优先 case study。
3. 如果本文围绕 ADR 求解器的模型、实现、验证和案例结果展开，建议先按 IMRaD 起草：
   - Introduction：问题背景、现有方法不足、本文贡献。
   - Methodology：方程、离散格式、边界条件、参数设置、实现细节。
   - Results：收敛性、稳定性、案例结果、对比实验。
   - Discussion：适用范围、限制、误差来源和未来扩展。

### Step 2: 建立文献矩阵，而不是堆引用

使用 `literature_matrix_template.md` 做一个本地文献矩阵，建议放到：

```text
paper/data/literature_matrix.md
```

建议列：

```text
Source | Year | Method | Problem | Numerical scheme | Boundary condition | Evidence | Relevance | Gap
```

这样写引言和相关工作时，可以避免“逐篇流水账”，而是按主题综合：

- 反应-扩散模型的建模假设。
- ADR 方程数值方法的常见离散策略。
- 稳定性、守恒性、误差控制和边界处理。
- 现有工具或论文中未覆盖的问题。

### Step 3: 用 Overleaf/LaTeX 源码做主写作，不用 Word 中转

由于本论文已经是 LaTeX 项目，优先使用 `awesome-ai-research-writing` 中的 LaTeX 场景 prompt：

- `中转英-latex`：把中文草稿转成英文 LaTeX 正文。
- `英转中-latex`：快速理解英文论文段落。
- `缩写`：压缩过长段落。
- `扩写`：把过短的实验观察扩成完整分析段落。
- `表达润色（英文论文）`：最终语言打磨。
- `逻辑检查`：检查段落内部是否跳跃。

建议不要直接使用 Word 场景 prompt，除非后续投稿格式要求 Word。

### Step 4: 作图先定叙事，再定图表类型

两个项目都能帮作图，但分工不同：

- `awesome-ai-research-writing` 的“实验绘图推荐”：适合根据数据表快速判断用柱状图、折线图、热力图、箱线图还是分面图。
- `academic-research-skills` 的 `statistical_visualization_standards.md`：适合检查图是否符合学术规范。

对 ADR 论文，常见图表建议：

```text
误差随网格加密变化          -> log-log 折线图，展示收敛阶
不同时间步长下稳定性        -> 折线图或热力图
不同 Pe / Da 参数下分布      -> 分面等值线图或热力图
多组 case 的误差/耗时对比    -> 横向条形图或分组柱状图
解场 phi/eta 的空间分布      -> heatmap / contour plot
残差或质量守恒误差随时间变化 -> 带标记的折线图
```

每张图生成后用以下清单检查：

```text
1. 图是否比一句话或表格更有信息量。
2. 坐标轴是否有变量名和单位。
3. 图例是否靠近数据，且不会遮挡曲线。
4. 多组实验是否有误差线、置信区间或重复实验说明。
5. 颜色是否色盲友好，灰度打印是否仍可区分。
6. 图题是否说明数据、变量和核心结论。
```

### Step 5: 实验分析必须绑定真实数据

可以借鉴 `awesome-ai-research-writing` 的“实验分析” prompt，但必须遵守一条硬规则：

```text
所有结论只来自 paper/data/ 或 ADR 程序输出，不能让模型补数据、猜趋势或夸大提升。
```

推荐流程：

1. 从求解器输出整理 CSV 或 TOML 摘要到 `paper/data/`。
2. 用 `paper/scripts/` 生成 `paper/figures/` 中的图。
3. 把 CSV 表格和想强调的结论一起交给 AI 生成分析段落。
4. 人工核对每个数值、百分比、趋势和图表引用。
5. 再写进 `paper/sections/results.tex`。

### Step 6: 写完每一节后做质量检查

使用 `writing_quality_check.md` 的思路检查：

- 是否有空泛词，如 robust、comprehensive、pivotal、showcase。
- 是否有过多破折号、分号和模板化转折。
- 是否存在“本节将讨论……”这类元叙述。
- 是否为了避免重复而乱换术语。
- 段落长度是否机械一致。
- 是否每段都服务于一个清晰主张。

对数值论文尤其要注意：术语重复不是问题。`mesh size`、`time step`、`boundary condition`、`residual` 这类术语应该保持一致，不要为了“文采”替换成模糊同义词。

### Step 7: 投稿前用 reviewer 视角反查

使用 `academic-research-skills` 的 `review_criteria_framework.md` 和 `awesome-ai-research-writing` 的“论文整体以 Reviewer 视角进行审视”。

对 ADR 论文建议重点问：

```text
1. 原创性：本文相对已有 ADR 数值方法或工具的新增贡献是什么？
2. 方法严谨性：离散格式、稳定性条件、边界条件是否交代清楚？
3. 证据充分性：每个贡献是否有实验或理论支撑？
4. 论证一致性：Introduction 声称的贡献是否都在 Results 中验证？
5. 文献整合：是否只是列举文献，还是明确定位了研究空白？
6. 可复现性：输入文件、参数、作图脚本和结果图能否对应起来？
```

## 3. 建议落地到本目录的文件

后续可以逐步补充这些文件：

```text
paper/main.tex
paper/references.bib
paper/sections/introduction.tex
paper/sections/methodology.tex
paper/sections/results.tex
paper/sections/discussion.tex
paper/data/literature_matrix.md
paper/data/experiment_index.md
paper/scripts/plot_convergence.py
paper/scripts/plot_case_fields.py
paper/figures/
```

其中 `experiment_index.md` 建议记录：

```text
Case name | Input file | Command | Output file | Figure | Section used
```

这样论文中的每个图都能追溯到输入文件和生成脚本。

## 4. 使用这些参考项目时的边界

建议使用：

- 结构模板。
- 写作检查清单。
- 图表类型判断。
- 图题、表题和摘要的写法规范。
- Reviewer 自审框架。

谨慎使用：

- 直接复制长 prompt 或模板进入正式论文仓库。
- 未确认许可证的内容。
- “去 AI 味”类 prompt。它可以用来提升语言自然度，但不能用于隐藏 AI 协作事实，也不能替代作者对事实、公式和结果的核查。

禁止依赖：

- AI 自动补文献。
- AI 猜测实验结果。
- AI 替你判断没有数据支持的结论。
- AI 生成不可追溯的引用和数值。

