# Git Hooks / Git 钩子配置

This directory contains useful Git hook scripts to maintain code quality and repository hygiene.

本目录包含维护代码规范与仓库卫生的 Git 钩子脚本。

## Install Pre-commit Hook / 安装提交前钩子

```bash
# Linux / macOS
cp tools/git_hooks/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Windows (Git Bash)
cp tools/git_hooks/pre-commit .git/hooks/pre-commit
```

The hook automatically prevents accidental staging of:
1. Compiled binary artifacts (`.exe`, `.pyc`, `.o`, `.obj`).
2. Large files exceeding 50 MB.
