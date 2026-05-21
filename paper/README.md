# Paper Writing Workspace

本目录用于把 Overleaf 上的 LaTeX 论文迁移到本地仓库，目标是同时支持论文写作、稳定编译、科研作图和结果复现。

当前状态：写作环境已落实并归档。MiKTeX、Strawberry Perl、`latexmk`、BibTeX/Biber、VS Code 与 LaTeX Workshop 均已验证可用；`Manuscript.tex` 已能编译生成 `paper/build/Manuscript.pdf`。归档记录见 `paper/WRITING_ENVIRONMENT_ARCHIVE.md`。

推荐目录结构：

```text
paper/
  README.md              # 本说明文档
  WRITING_ENVIRONMENT_ARCHIVE.md  # 写作环境归档记录
  Manuscript.tex         # 从 Overleaf 迁移后的 JFM 主 tex 文件
  Ref.bib                # Overleaf 项目的参考文献数据库
  JFM-FLM_Au.cls         # JFM LaTeX class
  jfm.bst                # JFM BibTeX style
  figure/                # Overleaf 项目原始图片目录，保留原相对路径
  sections/              # 章节 tex 文件
  figures/               # 论文最终引用的图片
  data/                  # 作图所需的轻量数据或结果摘要
  scripts/               # 生成论文图片的脚本
  reference_projects/    # 外部科研写作参考项目和本地使用说明
  build/                 # LaTeX 临时文件和编译输出，不提交
```

## 1. 本地 LaTeX 环境（已落实）

本机已使用 MiKTeX 作为本地 LaTeX 编译链。以下内容作为后续重装或迁移机器时的参考。

离开 Overleaf 后，本地需要一个完整 LaTeX 编译链。二选一即可：

- MiKTeX：Windows 上体积较小，缺包时可自动安装。
- TeX Live：更接近服务器和 CI 环境，稳定但安装体积较大。

建议优先安装以下能力：

```text
latexmk
xelatex
pdflatex
biber
bibtex
```

如果论文包含中文、`ctex`、系统字体或复杂字体设置，优先使用 `xelatex` 或 `lualatex`。如果 Overleaf 当前项目用的是默认英文模板，则多数情况下 `pdflatex` 也可工作。

安装后在 PowerShell 中检查：

```powershell
latexmk -v
xelatex --version
biber --version
```

如果命令不可用，通常是安装目录没有加入 `PATH`，或终端需要重启。

当前机器已验证可用的工具链：

```text
MiKTeX: C:\Users\huang\AppData\Local\Programs\MiKTeX\miktex\bin\x64
latexmk: available
pdflatex: available
xelatex: available
lualatex: available
bibtex: available
biber: available
Perl: C:\Strawberry\perl\bin\perl.exe
```

说明：MiKTeX 版 `latexmk` 依赖 Perl；当前已检测到 Strawberry Perl，因此可以直接使用 `latexmk`。

## 2. 编辑器支持（已落实）

推荐使用 VS Code：

1. 安装 VS Code。
2. 安装扩展 `LaTeX Workshop`。
3. 打开仓库根目录 `E:\Codex\ADR`。
4. 在 VS Code 中打开 `paper/Manuscript.tex`。
5. 使用 LaTeX Workshop 的 build 命令编译并预览 PDF。

如果不使用 VS Code，也可以直接用命令行编译。

当前机器已验证 VS Code 命令行和 LaTeX 扩展可用：

```text
code
james-yu.latex-workshop
tecosaur.latex-utilities
vomout.latex-workshop-sanity
zhishengye.latexbuildbar
```

## 3. 从 Overleaf 迁移文件

当前 Overleaf 压缩包 `Oscillating_adsorption__JFM_.zip` 已导入到 `paper/` 根目录。为了保持原项目可以直接编译，暂时保留 Overleaf 原始文件名和图片路径：

```text
paper/Manuscript.tex
paper/Ref.bib
paper/JFM-FLM_Au.cls
paper/jfm.bst
paper/lineno-FLM.sty
paper/figure/
```

导入记录和待清理点见：

```text
paper/OVERLEAF_IMPORT.md
```

如果以后要把长文拆成章节文件，可以在确认能本地编译后，再逐步迁移到 `sections/`，不要一次性大改路径。

在 Overleaf 中执行：

1. 打开论文项目。
2. 使用下载源码或同步 Git 的方式导出项目。
3. 将主文件放到 `paper/Manuscript.tex`。
4. 将章节文件放到 `paper/sections/`。
5. 将参考文献文件放到 `paper/Ref.bib`。
6. 将论文中直接引用的图片放到 `paper/figures/`。
7. 将原始数据、结果摘要或 CSV 放到 `paper/data/`。
8. 将生成图片的 Python、MATLAB 或其他脚本放到 `paper/scripts/`。

迁移后检查 `Manuscript.tex` 中的路径。例如：

```tex
\input{sections/introduction}
\bibliography{references}
\includegraphics[width=0.8\linewidth]{figures/example.pdf}
```

避免继续引用 Overleaf 专属路径、绝对路径或本机临时目录。

## 4. 论文编译方式（已验证）

先查看 Overleaf 项目的编译器设置：

- 如果是 `pdfLaTeX`，本地可以先用 `pdflatex`。
- 如果是 `XeLaTeX`，本地使用 `xelatex`。
- 如果使用 `biblatex`，参考文献通常需要 `biber`。
- 如果使用传统 `.bst`，参考文献通常需要 `bibtex`。

推荐优先使用 `latexmk`，因为它会自动处理多轮编译。

常见命令：

```powershell
cd E:\Codex\ADR\paper
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

中文或 XeLaTeX 项目使用：

```powershell
cd E:\Codex\ADR\paper
latexmk -xelatex -interaction=nonstopmode -outdir=build Manuscript.tex
```

编译成功后，PDF 通常位于：

```text
paper/build/main.pdf
```

对当前 JFM 项目，预计输出文件名为：

```text
paper/build/Manuscript.pdf
```

当前已验证命令：

```powershell
cd E:\Codex\ADR\paper
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

验证结果：

```text
paper/build/Manuscript.pdf
```

PDF 已成功生成。环境配置阶段到此结束；当前仍有若干 manuscript 内容层面的未定义引用和参考文献条目，详见 `paper/OVERLEAF_IMPORT.md`。

## 5. 准备科研作图环境

建议把论文图改成可复现脚本生成，而不是手工截图。

推荐 Python 环境：

```powershell
cd E:\Codex\ADR\paper
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install numpy pandas matplotlib scipy seaborn
```

作图脚本建议放在：

```text
paper/scripts/
```

数据建议放在：

```text
paper/data/
```

图片输出建议放在：

```text
paper/figures/
```

论文优先引用矢量图：

```text
PDF > SVG > EPS > PNG/JPG
```

Matplotlib 保存论文图的常用方式：

```python
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(3.4, 2.4))
ax.plot([0, 1, 2], [0, 1, 4])
ax.set_xlabel("x")
ax.set_ylabel("value")
fig.tight_layout()
fig.savefig("../figures/example.pdf")
```

然后在 LaTeX 中引用：

```tex
\includegraphics[width=0.75\linewidth]{figures/example.pdf}
```

## 6. 使用科研写作参考项目

外部科研写作参考项目放在：

```text
paper/reference_projects/
```

本地使用说明见：

```text
paper/reference_projects/WRITING_USE_GUIDE.md
```

这份说明把两个参考项目整理为适合本论文的工作流：论文结构规划、文献矩阵、LaTeX 写作、科研作图、实验分析、写作质量检查和 reviewer 视角自审。

## 7. 可选图形工具

这些工具不是必须，但很适合论文整理：

- Inkscape：编辑 SVG/PDF 矢量图、检查线宽和字体。
- Ghostscript：处理 PDF/EPS 转换和压缩。
- ImageMagick：批量转换图片格式。
- diagrams.net：画流程图、结构图。
- TikZ/PGFPlots：适合数学图、示意图、小规模数据图。

原则：

1. 数值结果图优先由脚本生成。
2. 示意图可以使用 Inkscape、diagrams.net 或 TikZ。
3. 最终论文引用图放在 `figures/`。
4. 能复现图片的数据和脚本放在 `data/` 与 `scripts/`。

## 8. 建议的日常工作流

每次写作或更新实验图时：

1. 修改 `sections/` 或 `Manuscript.tex`。
2. 如果更新结果图，先更新 `data/` 或结果摘要。
3. 运行 `scripts/` 中对应作图脚本。
4. 确认 `figures/` 中输出图片已更新。
5. 运行 LaTeX 编译命令。
6. 打开 `build/main.pdf` 检查版面、公式、引用、图片和参考文献。
7. 用 `git diff` 检查只改动了必要文件。

推荐检查命令：

```powershell
cd E:\Codex\ADR
git diff -- paper
```

## 9. 建议纳入版本管理的内容

建议提交：

- `paper/Manuscript.tex`
- `paper/sections/*.tex`
- `paper/Ref.bib`
- `paper/figures/` 中最终论文引用图
- `paper/scripts/` 中作图脚本
- `paper/data/` 中可公开、轻量、可复现实验图的数据
- `paper/reference_projects/WRITING_USE_GUIDE.md`

不建议提交：

- `paper/build/`
- `paper/reference_projects/academic-research-skills/`
- `paper/reference_projects/awesome-ai-research-writing/`
- LaTeX 临时文件，如 `.aux`、`.log`、`.bbl`、`.blg`、`.fls`、`.fdb_latexmk`
- 本地 Python 虚拟环境 `.venv/`
- 大体积原始仿真输出，除非它们是论文复现所必需并且仓库能承受

## 10. 常见问题排查

### 找不到 LaTeX 包

如果使用 MiKTeX，打开 MiKTeX Console，允许自动安装缺失包。  
如果使用 TeX Live，通常需要通过 TeX Live Manager 安装缺失宏包。

### 中文无法编译

优先检查是否使用 `xelatex`：

```powershell
latexmk -xelatex -interaction=nonstopmode -outdir=build Manuscript.tex
```

并确认模板中使用了合适的中文宏包和字体设置。

### 参考文献不显示

检查项目使用的是 `biblatex + biber` 还是 `BibTeX`。

如果 `.tex` 中有：

```tex
\usepackage{biblatex}
\addbibresource{Ref.bib}
```

通常需要 `biber`。

如果 `.tex` 中有：

```tex
\bibliographystyle{...}
\bibliography{references}
```

通常需要 `bibtex`。

使用 `latexmk` 一般可以自动处理，但前提是 `biber` 或 `bibtex` 已安装。

### 图片路径错误

确认 LaTeX 中引用的是相对于 `paper/Manuscript.tex` 的路径：

```tex
\includegraphics{figures/name.pdf}
```

不要写成本机绝对路径，例如：

```tex
\includegraphics{C:/Users/name/Desktop/name.pdf}
```

### Overleaf 可以编译，本地不可以

按顺序检查：

1. Overleaf 使用的编译器是否和本地一致。
2. 本地是否缺少宏包。
3. 图片文件名大小写是否一致。
4. `.bib` 文件路径是否正确。
5. 是否使用了 Overleaf 特有设置或隐藏文件。
6. 是否需要清理旧的 `build/` 后重新编译。

清理并重编译：

```powershell
cd E:\Codex\ADR\paper
latexmk -C -outdir=build Manuscript.tex
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

如果项目不是 XeLaTeX，把最后一条命令中的 `-xelatex` 换成 `-pdf`。

## 11. 迁移完成的验收标准

迁移完成后，至少应满足：

1. `paper/Manuscript.tex` 可以在本地一条命令编译出 PDF。
2. 参考文献、交叉引用、公式编号和图片编号正常。
3. 论文图片都从 `paper/figures/` 引用。
4. 重要结果图可以从 `paper/scripts/` 和 `paper/data/` 复现。
5. `git diff -- paper` 中没有 LaTeX 临时文件、虚拟环境或无关大文件。

当前环境验收已完成：

```text
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

已生成：

```text
paper/build/Manuscript.pdf
```

剩余工作不再属于写作环境搭建，而是论文内容清理：补齐缺失文献、修复未定义交叉引用、清理彩色协作标记和占位 DOI。
