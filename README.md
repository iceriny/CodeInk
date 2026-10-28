# CodeInk

让代码在 PowerPoint 中清晰呈现，保持可编辑。

CodeInk 使用本地 Shiki 引擎为代码着色，可直接更新文本框和表格，也可生成带行号的逐行表格或组合文本框。支持主题、字体、行距、背景、表格边框和模拟窗口。

## 安装

在 [Releases](https://github.com/iceriny/CodeInk/releases/latest) 下载 `CodeInk-Setup-1.0.1-online.exe`。保存课件，彻底退出 PowerPoint 后运行安装程序，再重新打开 PowerPoint。

- 支持 Intel/AMD 64 位 Windows 上的 32/64 位桌面版 PowerPoint。
- 安装到当前用户的 `%LOCALAPPDATA%\CodeInk`，无需管理员权限，不修改系统 PATH。
- 首次安装自动下载私有 Node.js 运行时和高亮依赖；日常使用在本机完成，不上传或执行课件代码。
- 可覆盖升级，保留语言、主题和排版设置。卸载可使用 Windows“已安装的应用”。
- 安装包未数字签名。Mac、网页版 PowerPoint、WPS 和 ARM64 原生 Office 不在支持范围内。

## 使用

1. 选中包含代码的文本框或表格，在“代码高亮”选项卡选择语言和主题。
2. 点击“高亮排版”更新所选对象，或点击“生成代码块”创建表格/组合文本框。
3. 剪贴板中的纯文本可直接“粘贴为代码”。

选中部分文字时会处理整个所属文本框或表格。更新已有代码块时，勾选“替换原对象”后生成；新对象全部生成成功后才删除原对象。逐行表格带有窗口底板时，请将表格与底板一并选中。

| 入口 | 用途 |
|---|---|
| 自动整理代码 | 排版、生成和粘贴时整理空格与缩进；默认开启 |
| 替换原对象 | 生成成功后替换选中的文字对象；默认关闭 |
| 类型 / 行号 | 选择逐行表格或组合文本框，设置行号 |
| 外观设置 | 背景、主题字形、窗口底色、内边距等 |
| 输出 → 选项 | 表格边框、表格读取方式、多选合并 |
| 更多操作 | 仅改配色、统一字色、导入 VS Code 主题 |
| 帮助 | 使用说明、运行状态、问题排查和关于 |

设置变化在下一次对应操作时生效。“仅改配色”“统一字色”和“应用到选中表格”的边框操作不整理代码文字。更多用法见 [使用说明](docs/使用说明.html)。

## 代码整理

基础整理不重排语句，也不增删换行。支持常见运算符、调用参数、下标、控制语句、模板参数、指针声明和多行续行缩进，例如：

```cpp
cout << i << "  " << n1 << "   " << n2 << endl;
```

字符串和注释的内容保留。C++ 整理后会再次比较预处理词法单元；如果空格调整导致词法单元变化，保留原文。带物理续行符的 C++ 片段保守保留，代码内定义的宏与大写宏调用保留参数空白。外部小写宏、自定义语法及复杂不完整片段仍有识别边界，需要保留原有排版时可关闭“自动整理代码”。

共用规则覆盖 C/C++、Arduino、Java、C#、JavaScript/TypeScript、Go、Python 和 JSON/JSONC。Python 保留原有缩进层级；Shell、Markdown、YAML、HTML、JSX/TSX、Rust 等未覆盖语法保持原样。详见 [整理规则](docs/basic-formatting.md)。

## 排版与限制

- 生成和粘贴使用选项卡设置的字号，代码块从所选目标的左上角放置；合并时使用阅读顺序中第一个目标的位置。
- 粘贴时未选中目标，默认从幻灯片左上角向右、向下各 48 pt 处插入。
- 表格的行号分隔线、行间横线、外框独立设置；组合文本框不使用表格边框。
- 逐行表格与窗口底板不能组合；需要整体移动时可选择组合文本框。
- 每对象最多 50,000 个 UTF-16 字符单位、200 行，每批最多 32 个对象。复杂嵌套组合、合并单元格和旋转布局可能无法处理。
- 操作可用 PowerPoint 撤销；取消原地批量修改时，部分对象可能已更新。

## 开发与验证

开发环境、VS Code 任务和调试入口见 [开发指南](docs/development.md)。

```sh
python -X utf8 dev.py deps
python -X utf8 dev.py test
python -X utf8 dev.py self-test
python -X utf8 dev.py build
python -X utf8 dev.py verify
```

C++ 原生模拟和 ASan/UBSan 检查在 Linux/WSL 下执行：`python3 dev.py sanitize`。测试覆盖整理结果、C++ 整理前后的编译运行对比、渲染、Ribbon 回调与设置保存。构建和模拟测试不等同于 PowerPoint 实测，手动验收项见 [实机检查清单](tests/windows-smoke-test.md)。

推送 `v1.0.1` 形式的标签会触发 GitHub Actions：测试、本地引擎自检、构建 x86/x64 Windows 组件、校验安装包，最后发布安装程序和 SHA-256 校验文件。标签必须与 `VERSION` 一致。

## 许可

作者：Iceriny。项目许可见 [LICENSE.txt](LICENSE.txt)，依赖说明见 [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)。高亮依赖为 Shiki 4.4.3、jsonc-parser 3.3.1，私有运行时为 Node.js v24.21.0。
