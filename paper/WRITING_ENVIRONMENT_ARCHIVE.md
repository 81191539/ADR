# Writing Environment Archive

归档日期：2026-05-22

## 结论

论文写作环境已经落实完成，可以进入论文内容清理、改写、补文献和作图阶段。

已验证事项：

```text
MiKTeX: available
Strawberry Perl: available
latexmk: available
pdflatex: available
xelatex: available
lualatex: available
bibtex: available
biber: available
VS Code: available
LaTeX Workshop: installed
```

## 当前项目状态

Overleaf 项目已导入到 `paper/`，主文件为：

```text
paper/Manuscript.tex
```

当前 JFM 项目使用：

```tex
\documentclass[lineno]{JFM-FLM_Au}
\bibliographystyle{jfm}
\bibliography{Ref}
```

因此编译路线为 `pdfLaTeX + BibTeX`，不使用 `biblatex + biber`。

## 已验证编译命令

```powershell
cd E:\Codex\ADR\paper
latexmk -pdf -interaction=nonstopmode -outdir=build Manuscript.tex
```

已生成：

```text
paper/build/Manuscript.pdf
```

`build/` 已由 `paper/.gitignore` 忽略，编译产物不进入版本管理。

## VS Code 状态

已检测到 VS Code 命令行和以下 LaTeX 相关扩展：

```text
james-yu.latex-workshop
tecosaur.latex-utilities
vomout.latex-workshop-sanity
zhishengye.latexbuildbar
```

推荐日常写作入口：

```text
E:\Codex\ADR\paper\Manuscript.tex
```

## 后续不再归入环境问题的事项

以下问题属于论文内容和 LaTeX 源码清理，不属于写作环境搭建：

```text
缺失 BibTeX 条目
未定义 section / appendix / figure labels
正文彩色协作标记
占位 DOI
重复 equation PDF anchors
```

详细列表见：

```text
paper/OVERLEAF_IMPORT.md
```

