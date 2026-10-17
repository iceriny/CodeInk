# 开发指南

直接用 VS Code 打开仓库根目录。工程使用 Python 构建脚本，不使用 CMake；无需创建解决方案或安装 VSTO。

## 目录

| 路径 | 用途 |
| --- | --- |
| `src/` | C++ COM 插件、Windows ABI 声明、安装器与 Ribbon |
| `engine/` | Node / Shiki 引擎及固定依赖、npm 锁文件 |
| `setup/` | 产品安装时下载私有 Node 的脚本 |
| `tests/` | 自动化测试与真实 PowerPoint 验收清单 |
| `docs/reports/0.3.1-preview/` | 原始预览版的历史验证记录，保留当时结论 |
| `.vscode/` | 工作区设置、任务、调试与扩展推荐 |
| `build/` | 中间文件、生成头文件、编译数据库和本次检查报告（忽略提交） |
| `dist/` | DLL、检查器、安装器与 SHA-256（忽略提交） |

根目录保留 `build.py` 和 `make_payload.py`，兼容既有构建命令。`dev.py` 为终端和 VS Code 提供统一入口。

## Windows 工具

- Python 3：使用 `python --version` 检查；所有脚本只使用标准库，无需 pip 依赖或虚拟环境。
- Node.js 22 或以上及 npm：开发使用系统 Node；产品安装时的私有 Node 版本由 `setup/download-runtime.ps1` 单独管理。
- LLVM（`clang++`、`lld-link`）：可用 `winget install --id LLVM.LLVM --exact` 安装。开发脚本会自动查找 `%ProgramFiles%\LLVM\bin`，也会查找 `%LOCALAPPDATA%\Programs\LLVM\bin` 及 PATH 中的工具。本次工作区的 IntelliSense 默认指向用户目录中的 LLVM；若使用系统安装，请把 `compilerPath` 改为 `${env:ProgramFiles}/LLVM/bin/clang++.exe`。
- VS Code 扩展见 `.vscode/extensions.json`。JavaScript 调试器由 VS Code 内置提供。

```powershell
python -X utf8 dev.py doctor
python -X utf8 dev.py deps
python -X utf8 dev.py test
python -X utf8 dev.py self-test
python -X utf8 dev.py build
python -X utf8 dev.py verify
```

`deps` 使用 `npm ci --ignore-scripts` 按锁文件安装依赖。只有有意更新依赖时才修改 `engine/package.json` 并用 `npm install --prefix engine --ignore-scripts` 更新锁文件。

`test` 执行全部引擎语法检查、JavaScript 单测及真实桥接错误路径测试；`self-test` 运行真实 Shiki、JSONC 主题继承和多语言高亮，需要先安装依赖。自检生成的语言/主题目录文件不提交、不嵌入安装包。

`build` 生成 x86/x64 插件、检查器与安装器；`dll` 仅构建插件和检查器。直接执行 `python -X utf8 build.py` 仍然可用。包检查前必须完整构建，不能只构建 DLL；检查结果写入 `build/reports/pe-validation.json`，不覆盖历史报告。

## VS Code 操作

- `Ctrl+Shift+B`：完整构建。
- “终端 → 运行任务”：选择 `CodeInk: doctor`、`deps`、`test`、`self-test` 或 `verify` 等任务。`verify` 自动先构建。
- “运行和调试”：选择引擎单测、Shiki 自检、Python 构建或包检查，然后按 F5。调试自检前先运行 `deps`。
- C++ 状态栏可切换 `Windows x64` / `Windows x86`。首次构建生成对应的 `build/compile_commands-*.json` 与 Ribbon/安装器头文件，供 IntelliSense 使用。
- 若 LLVM 安装在自定义路径，请调整 `c_cpp_properties.json` 的 `compilerPath`，并把工具目录加入 PATH。
- C++ / Python / JavaScript 已指定格式化工具，可用 `Shift+Alt+F` 手动格式化；关闭这些语言的保存时自动格式化，避免既有紧凑源码被整文件重排。

本工作区没有 PowerPoint 原生调试配置：当前 DLL 是优化发布构建，不生成 PDB。现有 F5 配置用于引擎和构建脚本。安装、COM 加载、Ribbon 及真实排版验收见 `tests/windows-smoke-test.md`，自动化构建通过不代表 Office 实测通过。

## Linux / WSL 原生模拟测试

现有 COM 模拟器采用 Linux 宿主 ABI 与 ELF 链接参数，需要 Linux（或 WSL）内的 Python、Node、clang++ 与 lld-link。Windows 上不把这些测试计为通过。

```sh
python3 -X utf8 dev.py mock
python3 -X utf8 dev.py sanitize
```

`mock` 执行文本/布局、基础代码整理、C++ 整理前后语义对比、两组 COM 模拟和设置窗口模拟测试；`sanitize` 为基础代码整理、C++ 整理前后语义对比、两组 COM 模拟及设置窗口模拟测试开启 ASan/UBSan。Linux 下可通过命令行开发；已提供的 C++ IntelliSense 配置面向 Windows。

## 发布

`VERSION` 定义安装包版本；`src/common.cpp`、安装器/关于文案及 `engine/package*.json` 应与其一致。包检查会验证关键版本和嵌入载荷。发布说明保存在 `docs/releases/v版本号.md`。

推送与 `VERSION` 一致的 `v版本号` 标签会触发 `.github/workflows/release.yml`。工作流安装固定依赖，运行引擎、自检与原生模拟测试，构建并检查 x86/x64 Windows 产物，上传安装器和 SHA-256 校验文件到 GitHub Release。拉取请求或手动运行也可验证构建；只有标签运行会发布。

## 提交内容

提交源码、工作区配置和 npm 锁文件。`build/`、`dist/`、`node_modules/`、Python 缓存和引擎运行产物由 `.gitignore` 排除。历史验证报告对应原始发布产物，重新构建后应以当前 `dist/SHA256SUMS.txt` 和 `build/reports/` 为准。
