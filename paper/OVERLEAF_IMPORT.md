# Overleaf Import Notes

本文件记录 `Oscillating_adsorption__JFM_.zip` 的导入结果和下一步处理建议。

当前状态：Overleaf 项目已导入，本地 LaTeX 写作环境已验证并归档。环境归档见 `paper/WRITING_ENVIRONMENT_ARCHIVE.md`。

## 导入结果

压缩包已解压到 `paper/` 根目录，并保留 Overleaf 原始文件名和相对路径。

主文件：

```text
paper/Manuscript.tex
```

参考文献与 JFM 模板文件：

```text
paper/Ref.bib
paper/jfm.bst
paper/JFM-FLM_Au.cls
paper/lineno-FLM.sty
```

图片目录：

```text
paper/figure/
```

压缩包本身：

```text
paper/Oscillating_adsorption__JFM_.zip
```

根目录 `.gitignore` 已忽略 `*.zip`，所以压缩包不会被 Git 跟踪。

## 编译方式

当前机器已验证可用 MiKTeX、Strawberry Perl、`latexmk`、`pdflatex`、`bibtex` 和 VS Code LaTeX Workshop。

推荐使用 `latexmk`：

```powershell
cd E:\Codex\ADR\paper
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

当前项目使用的是：

```tex
\documentclass[lineno]{JFM-FLM_Au}
\bibliographystyle{jfm}
\bibliography{Ref}
```

因此参考文献走传统 BibTeX 路线，不是 `biblatex + biber`。

如果没有 `latexmk`，可以先用手动流程：

```powershell
cd E:\Codex\ADR\paper
pdflatex -interaction=nonstopmode Manuscript.tex
bibtex Manuscript
pdflatex -interaction=nonstopmode Manuscript.tex
pdflatex -interaction=nonstopmode Manuscript.tex
```

本地编译验证已完成：

```text
paper/build/Manuscript.pdf
```

输出 PDF 为 24 页。编译命令成功退出，但日志中仍有未定义引用、缺失参考文献条目和重复 equation anchor 警告。

当前缺失的 BibTeX 条目：

```text
Zhang2017
Gijsen1999
Federspiel1986
Bothe2006
Lake1989
```

当前未定义的交叉引用包括：

```text
sec:methods
sec:discussion
sec:conclusion
sec:regime_identification
appdix-C
fig:regime_I_collapse
fig:regime_III_collapse
sec:regime_II_mechanism
```

日志还报告了重复 equation PDF anchor，例如 `equation.1`、`equation.2` 等。这通常来自附录或多个编号环境重复重置，PDF 可生成，但超链接目标可能不唯一。

## 当前图片引用

`Manuscript.tex` 当前引用的图片主要来自 `figure/`：

```text
figure/Schematic8.pdf
figure/f_a_v5.pdf
figure/composite_case1_Pe1_10_Pe2_10.pdf
figure/composite_case2_Pe1_10_Pe2_100000_v3.pdf
figure/composite_case3_Pe1_10_Pe2_0.01_v3.pdf
figure/composite_case4_Pe1_10_Pe2_1000_v3.pdf
figure/eta_ave_ABCD_v6.pdf
figure/contour_plot_v6.pdf
```

因此不要立即把 `figure/` 改名为 `figures/`，否则需要同步修改 LaTeX 路径。

## 已发现的待清理点

这些不是导入错误，而是 manuscript 中原本存在的写作或编译风险。当前已能生成 PDF，建议按以下顺序逐项清理。

1. `Manuscript.tex` 中存在未完成的命令定义：

```tex
\newcommand{\RomanNumeralCaps}[1]
```

它缺少命令体。当前 `latexmk` 可生成 PDF，但这仍属于不完整定义；若不再使用，可以删除，若需要，应补全定义。

2. 正文中仍有彩色协作标记，例如：

```tex
{\color{blue} Zuo}
{\color{red} Hua}
{\color{green} Huang}
```

以及若干 `{\color{magenta}(...)} / {\color{red}(...)} / {\color{green}...}` 修改提示。

3. Introduction 中有若干非 LaTeX 标准引用文本，例如：

```text
[cite{...}]
cite{...}
cite{}
```

这些需要改为 `\cite{...}`、`\citep{...}` 或删除占位。

4. 图注中仍有占位 DOI：

```text
https://doi.org/xxx.xxx
```

提交前需要替换为正式补充材料链接或删除。

5. `hyperref` 被加载了两次。通常不致命，但建议在模板稳定后只保留一次。

## 建议下一步

写作环境搭建已经结束。后续工作集中在 manuscript 内容清理：

1. 补齐 `Ref.bib` 中缺失的 5 个条目。
2. 修复未定义的 section、appendix 和 figure labels。
3. 清理彩色协作标记、占位引用和占位 DOI。
4. 检查重复 equation anchor，必要时调整附录编号或 `hyperref` anchor。
5. 再次运行 `latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex`，直到未定义引用和 citation 警告清零。
6. 编译稳定后，再考虑把章节拆入 `sections/`，或把最终论文图统一管理到 `figures/`。
