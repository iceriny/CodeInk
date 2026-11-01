# CodeInk 项目设计审查

审查状态：已完成。审查日期：2026 年 11 月 1 日。

本次检查覆盖项目整体，判断是否存在过度防御、try/catch 滥用、过度抽象、死代码和过度工程化。审查基于提交 `95bbaf8b857f349e9524fbba133001ce77475b11` 之上的当前工作区，包含尚未提交的分栏与字体改动。审查阶段只更新此文档；下文发现及证据位置保留审查时的状态，后续优化另记于文末。

## 整体结论

没有发现项目整体采用了明显过重的架构，也没有发现 try/catch 滥用。可以确认的简化机会集中在历史入口和模式残留、重复表示、重复读取及构建测试耦合，共 7 项。当前应做局部清理，不需要重写 COM 层、二进制桥接协议或代码整理器。

| 检查项 | 判断 | 主要证据 |
| --- | --- | --- |
| 过度防御 | 有局部重复核验；未发现应大面积删除的防御逻辑 | F5；原文、协议、安装回滚和资源检查应保留 |
| try/catch 滥用 | 未发现 | JS 的请求边界、资源清理，Python 的退出码转交，PowerShell 的下载边界各有用途 |
| 过度抽象 | 未发现多层框架式抽象；有局部冗余表示和闲置通用性 | F4、F7 |
| 死代码 | 有当前生产流程不可达的历史分支和未用声明 | F1、F3、F7 |
| 过度工程化 | 有局部测试与初始化机制过重，未发现整体架构过度工程化 | F2、F6；主题命名另有一个低收益简化项 |

建议优先处理 F2 的校验误报，再清理 F1/F3 的遗留路径；F5/F6 适合随后局部收敛，F4/F7 可在相关文件维护时顺手清理。这是建议顺序，本轮没有实施这些修改。

## 判断标准

- 过度防御：检查或回退没有对应的可达失败场景，重复验证已有保证，或者为了继续运行而掩盖真实错误。
- try/catch 滥用：异常范围过宽、吞掉异常、以异常代替常规分支，或重复包装同一错误而丢失上下文。
- 过度抽象：引入的层次、参数或通用接口没有实际复用价值，反而使调用关系难以理解。
- 死代码：区分完全未引用项、已退出当前产品流程的历史路径，以及仍有用途的测试或手动验证工具。仅凭全文搜索不到普通函数调用，不足以判定 COM/Ribbon 回调为死代码。
- 过度工程化：为尚不存在的需求长期保留机制，或者以明显高于实际需求的维护成本实现现有功能。

各项结论列出证据位置、实际影响、保留理由或简化方向。内存、COM、文件边界及避免破坏用户课件的检查，结合宿主约束判断。

## 覆盖范围

| 范围 | 状态 |
| --- | --- |
| C++ 插件入口、COM 与 Win32 基础层 | 已检查源码、动态入口和 ABI 引用 |
| 高亮桥接、代码整理、捕获和输出 | 已检查源码与关键回归断言 |
| 字体、设置、Ribbon 与目录加载 | 已检查源码、绑定和设置读写路径 |
| JavaScript 引擎、协议、主题和日志 | 已检查全部自有生产模块及调用方 |
| 安装器、运行时下载、构建和打包 | 已检查源码、脚本、回滚和卸载路径 |
| 测试、CI 与开发配置 | 已核对测试入口、辅助脚本、关键断言、CI 及 VS Code 配置 |

生成的依赖锁文件、第三方依赖内容和历史报告不作为自有生产代码逐行审阅；测试以入口、关键行为与疑点对应关系为检查重点，不将静态审查等同于全量测试执行。

## 已确认发现

### F1 中等优先级 24 个历史回调已无当前 Ribbon 入口

分类：已确认退出当前产品流程的历史入口，保留外部 IDispatch 调用的技术可能性。

当前 `METHODS` 注册 90 个名字，其中 Ribbon XML 使用 60 个，COM 宿主生命周期和 `GetCustomUI` 使用 6 个，剩下 24 个没有当前 Ribbon 绑定：

| 组别 | 回调 |
| --- | --- |
| 已迁入设置窗口的 10 个写入入口 | `OnBackgroundColor`、`OnTableRead`、`OnMerge`、`OnThemeStyles`、`OnWindowButtons`、`OnWindowShadow`、`OnWindowPadding`、`OnWindowRadius`、`OnWindowFill`、`OnWindowColor` |
| 对应的 10 个读取入口 | 上述名字对应的 `GetBackgroundColor` 至 `GetWindowColor` |
| 其他 4 个入口 | `GetWindowEnabled`、`GetBorderEnabled`、`GetWindowLabel`、`GetStatus` |

这些分支仍占据枚举、字符串注册表、`Invoke()` 或 `change()` 分支及部分校验提示。设置窗口直接读写 `Options`，并不通过这些回调；其中一部分仅由模拟测试直接调用，测试因此继续维护了已经退出真实界面的路径。

证据：[addin.cpp:13](D:/Code/CodeInk/src/addin.cpp:13)、[addin.cpp:21](D:/Code/CodeInk/src/addin.cpp:21)、[ribbon.xml](D:/Code/CodeInk/src/ribbon.xml)、[settings.cpp:147](D:/Code/CodeInk/src/settings.cpp:147)、[addin_mock_tests.cpp:122](D:/Code/CodeInk/tests/addin_mock_tests.cpp:122)。检查方法是对 XML 全部 `onAction/onChange/onLoad/get*` 属性与注册表做差集，再搜索生产调用方。

简化方向：清理失去产品入口的回调分支，测试转向设置窗口的现有入口。相关 `Options` 字段和注册表偏好仍在使用，应保留。严格说这些名字依然可以被外部 IDispatch 自动化手动调用，因此结论是“当前产品流程不可达”，不是声称 COM 技术上无法调用；仓库未声明它们为兼容 API。

### F2 中等优先级 包校验把回调命名约定当成功能约束

分类：测试过度约束、重复维护。

`verify_package.py` 已检查 XML 回调存在、枚举和字符串表长度一致，还根据 `C/G` 前缀推导回调名字，并维护一份例外表。后一个检查原本要防止枚举与名字错位，目的有价值；问题在于它用命名习惯推断真实映射，实际 Ribbon 分派只使用注册表与 DISPID。

当前工作区已经能复现误报：

| 枚举 | 实际注册且被 XML 使用的名字 | 校验规则要求 |
| --- | --- | --- |
| `GFontCount` | `FontCount` | `GetFontCount` |
| `GFontLabel` | `FontLabel` | `GetFontLabel` |

这与现有 `LanguageCount/ThemeCount` 的工作方式相同，但它们需要另写例外。单独执行现有命名映射逻辑得到上述两处不一致；没有运行完整打包流程。因此不能将此记录写成“整个安装包验证已执行失败”。

证据：[verify_package.py:80](D:/Code/CodeInk/tests/verify_package.py:80)、[addin.cpp:26](D:/Code/CodeInk/src/addin.cpp:26)、[ribbon.xml:34](D:/Code/CodeInk/src/ribbon.xml:34)。

简化方向：先修正当前合法映射的遗漏，再考虑减少注册数据与测试之间的重复维护。应保留错误 DISPID 对照的检测能力，不能只删除这条断言、剩下数量检查；也没有必要为此建立新的通用代码生成框架。

### F3 中等优先级 Tab 展开模式已经没有生产调用方

分类：不可达分支、遗留参数。

`normalized(..., expandTabs)` 仍保留将 Tab 转成四个空格的分支，`clipboardCapture()` 和 `flattenForTable()` 甚至默认启用它；但唯一生产调用方在两个入口均显式传入 `false`，其他 `normalized()` 调用也全部使用 `false` 或其默认值 `false`。

因此 `expandTabs=true` 的分支在当前生产流程不可达，`textutil.cpp` 的 `normalizedLength()/normalize()` 只通过该分支和直接单测保留。真实操作测试明确要求保留字符串等原文数据里的 Tab；整理器按语言规则调整缩进是另一条路径，不能与全量 Tab 展开混同。测试一个历史辅助函数本身，不能说明产品仍需要这个模式。

证据：[render.cpp:35](D:/Code/CodeInk/src/render.cpp:35)、[render.cpp:550](D:/Code/CodeInk/src/render.cpp:550)、[render.cpp:563](D:/Code/CodeInk/src/render.cpp:563)、[render.cpp:580](D:/Code/CodeInk/src/render.cpp:580)、[textutil.cpp:4](D:/Code/CodeInk/src/textutil.cpp:4)、[textutil_tests.cpp:7](D:/Code/CodeInk/tests/textutil_tests.cpp:7)。核对范围包括 `src/`、`engine/` 和全部测试中的这些符号。

简化方向：删除归一化阶段全量展开 Tab 的模式参数和专门实现，保留换行、UTF-16 和真实操作的回归测试，不改变整理器现有的缩进规则。不建议为了让遗留测试继续通过而恢复旧模式到界面。

### F4 低优先级 Catalog 的 name 和 label 永远相同

分类：冗余数据表示、轻度过度抽象。

目录文件读取与内置回退两条路径，都把 `name` 原样复制到 `label`；没有生产或测试代码修改它们形成不同内容。`resolve()` 随后同时比较 `name` 和 `label`，做了两次相同比较。`label()` 可以直接返回 `name`，不需要独立字段。

证据：[catalog.h:3](D:/Code/CodeInk/src/catalog.h:3)、[catalog.cpp:7](D:/Code/CodeInk/src/catalog.cpp:7)、[catalog.cpp:14](D:/Code/CodeInk/src/catalog.cpp:14)、[catalog.cpp:16](D:/Code/CodeInk/src/catalog.cpp:16)。

每项额外存储 256 个 Windows WCHAR，即 512 字节。按单个目录最多分配 600 项计算，冗余字段上界约 300 KiB；这是容量上界，不是实测常驻内存。更主要的成本是让读者以为显示标签与名称具有不同语义。

简化方向：保留 ID 和名称两种实际语义，去掉没有差异的第三份表示。

### F5 中等优先级 分栏捕获反复向 COM 读取相同元数据

分类：重复验证、局部不必要复杂度。

`captureGroup()` 对每个预期的“栏号＋代码/行号角色”都遍历全部文字叶节点，每次重新调用两次 `readTag()`。此函数没有主动修改这些元数据，也没有显式泵送消息；重复读取同一组标签没有对应的额外一致性检查。

若有 C 栏且开启行号，文字叶节点数为 2C，匹配循环执行约 `(2C)²` 次。当前设置允许 200 栏，因此该路径可执行 160,000 次匹配、320,000 次 `readTag()`；每次还会获取 `Tags` 并通过 IDispatch 读取值。这里给出的是由循环计算的调用量，没有声称已测出实际耗时。

证据：[render.cpp:130](D:/Code/CodeInk/src/render.cpp:130)、[render.cpp:9](D:/Code/CodeInk/src/render.cpp:9)、[textutil.cpp:53](D:/Code/CodeInk/src/textutil.cpp:53)。

简化方向：一次读取各叶节点的栏号与角色，在内存中完成唯一性、缺失项和排序检查。应保留结构校验和元数据本身；它们用于区分代码、行号和表格补齐空位，删除它们会破坏正确读取。

### F6 中等优先级 导入构建模块给单测带来无关工具依赖

分类：构建与测试耦合、初始化职责过重。

`tests/run_addin_mock.py` 只想调用 `ribbon()` 生成嵌入 XML，却通过导入 `build.py` 同时触发 PATH 配置、创建 `build/` 与 `dist/`、检查 clang++ 和 Windows 链接器。这个 Linux 宿主模拟测试本身使用 clang++ 链接可执行文件，不调用 lld-link。

只读复现中屏蔽了实际目录创建，并模拟“clang++ 存在、lld-link 不存在”。仅执行模块导入，就记录到两次目录创建请求，并得到 `Install LLVM clang++ and lld-link, and add them to PATH.`。因此这不是猜测中的耦合：Ribbon 生成器没有用到的链接器，确实能阻断测试入口。

证据：[build.py:6](D:/Code/CodeInk/build.py:6)、[build.py:12](D:/Code/CodeInk/build.py:12)、[tests/run_addin_mock.py:6](D:/Code/CodeInk/tests/run_addin_mock.py:6)。

简化方向：让工具链检查、输出目录创建在真正构建时发生，让 Ribbon 生成函数可独立使用。无需为这个问题引入新的构建框架。

### F7 低优先级 少量能力开关和 ABI 声明没有使用者

分类：闲置通用性、声明残留。

- `runChild(..., hidden=true)` 的全部生产调用都使用默认值，没有调用方需要 `hidden=false`。这是小范围的未用通用性，可收敛为始终隐藏的子进程启动方式。
- `IsWow64Process`、`GetCurrentProcess`、`URLDownloadToFileW` 在生产与测试中均没有引用；最后一个还让构建脚本维持 URLMON 导入库生成配置。它们可以作为顺手清理项，但声明没有调用，不等于最终 DLL 一定加载对应系统库。

证据：[progress.h:9](D:/Code/CodeInk/src/progress.h:9)、[progress.cpp:29](D:/Code/CodeInk/src/progress.cpp:29)、[winapi.h:90](D:/Code/CodeInk/src/winapi.h:90)、[winapi.h:177](D:/Code/CodeInk/src/winapi.h:177)、[build.py:28](D:/Code/CodeInk/build.py:28)。

ABI 引用检查同时检查头文件，不能只扫描 `.cpp`：例如 `VariantCopy` 虽然没有直接出现在生产 `.cpp` 中，但被 `com.h` 内联 `Value` 操作使用，不应删除。

## 已核实应保留的机制

### JavaScript 的异常处理目前没有发现滥用

`engine/bridge.mjs` 有两处 `try/finally`，分别释放 Shiki 实例和删除自检临时目录；它们是资源清理，不应计为吞异常。请求入口的内层 `catch` 将读取、解析或高亮错误转换成协议错误响应；外层 `catch` 处理初始化、命令或响应文件写入失败，并设置非零退出码。两个边界处理不同的失败，不能仅因嵌套而删除其中一层。

证据：[bridge.mjs:39](D:/Code/CodeInk/engine/bridge.mjs:39)、[bridge.mjs:81](D:/Code/CodeInk/engine/bridge.mjs:81)、[bridge.mjs:92](D:/Code/CodeInk/engine/bridge.mjs:92)。

`engine/diagnostics.mjs` 的空参数 `catch` 仅包围诊断日志追加，写入失败返回 `false`，不会将实际高亮错误当成成功。这是辅助日志的失败隔离，目前没有删除理由。证据：[diagnostics.mjs:10](D:/Code/CodeInk/engine/diagnostics.mjs:10)。

### 协议与主题读取中的边界检查有实际对象

`protocol.mjs` 检查跨进程二进制输入的长度、对象数、总量和版本，并将 Shiki 的字符偏移映射回 PowerPoint 的 UTF-16 原文。`theme-loader.mjs` 读取用户提供的 JSON/JSONC 和相对 include，限制循环、深度与文件总量。这些都处在外部数据边界，不能因为两端都由本项目开发，就直接判为重复防御。

证据：[protocol.mjs:11](D:/Code/CodeInk/engine/protocol.mjs:11)、[protocol.mjs:34](D:/Code/CodeInk/engine/protocol.mjs:34)、[protocol.mjs:100](D:/Code/CodeInk/engine/protocol.mjs:100)、[theme-loader.mjs:9](D:/Code/CodeInk/engine/theme-loader.mjs:9)。

### 安装回滚和卸载清单不能按重复防御删除

安装开始前及下载完成后分别检查 PowerPoint 是否关闭，覆盖的是用户在下载期间重新打开 PowerPoint 的真实情况。备份、恢复旧 DLL 和引擎文件承担升级失败后的回退；托管文件清单保证卸载只删除安装器拥有的内容。JS 生成清单时验证路径与 C++ 卸载时再次验证路径，位于不同时间和数据入口，并非重复检查同一个不可变对象。

证据：[installer.cpp:67](D:/Code/CodeInk/src/installer.cpp:67)、[installer.cpp:71](D:/Code/CodeInk/src/installer.cpp:71)、[installer.cpp:82](D:/Code/CodeInk/src/installer.cpp:82)、[maintenance.mjs:8](D:/Code/CodeInk/engine/maintenance.mjs:8)。

### Python 与 PowerShell 的 catch 目前没有发现滥用

`dev.py` 在 CLI 最外层捕获 `CalledProcessError` 并原样返回子进程退出码；PowerShell 下载脚本在单一业务边界失败后退出 1。下载脚本内的 `catch {}` 只隔离可选错误日志的二次写入失败，不会吞掉主流程失败。C++ 构建使用 `-fno-exceptions`，源文件中的 `try/catch` 字样是被整理语言的关键字表，不是 C++ 异常处理。

证据：[dev.py:80](D:/Code/CodeInk/dev.py:80)、[download-runtime.ps1:34](D:/Code/CodeInk/setup/download-runtime.ps1:34)、[build.py:46](D:/Code/CodeInk/build.py:46)、[basicformat.cpp:283](D:/Code/CodeInk/src/basicformat.cpp:283)。

### COM 基础层和格式器复杂度有当前用途

`Ref`、`Value`、`Items<T>` 分别管理 COM 引用、VARIANT 和连续缓冲区，没有层层包装的服务或策略体系。手写 Win32/COM ABI 是当前架构维护成本较高的取舍之一；现有 Ubuntu CI 用它完成无 SDK 的 Windows 双架构构建，确有消费方。本次保留判断以继续支持这一构建目标为条件，不能仅因现有实现依赖它就认为这种架构永远必要。COM 必需的生命周期空实现、工厂方法和 DLL 导出，也不能因为没有普通源码调用点就删除。

`basicformat.cpp` 的状态较多，但对应当前支持的模板参数、宏、原始字符串、括号嵌套和多语言间距规则。独立比较 C++ 词法单元用于防止整理空格改变语义，语义测试还编译并比较整理前后的程序输出。当前证据支持保留这一防护，而不是为了缩短函数去掉校验。

证据：[com.h](D:/Code/CodeInk/src/com.h)、[common.h:46](D:/Code/CodeInk/src/common.h:46)、[build.py:46](D:/Code/CodeInk/build.py:46)、[basicformat.cpp:91](D:/Code/CodeInk/src/basicformat.cpp:91)、[basicformat.cpp:371](D:/Code/CodeInk/src/basicformat.cpp:371)、[run_basicformat_semantics.py](D:/Code/CodeInk/tests/run_basicformat_semantics.py)。

### 字体回退与文字写入校验有实际失败场景

字体原生列表为空时使用宿主字体资源，是当前 PowerPoint 版本已经出现的兼容场景；读取字体实际名称并比较字宽，用来排除字体替换及比例字体，不属于无目标的防御。前轮真实宿主检查已验证原生列表为空时仍能得到可用等宽字体，本轮没有为了审查再启动 PowerPoint。

`verifyAndWrite()` 在高亮子进程返回后再次核对捕获文本，`writeLine()` 在写入后再次确认原文与偏移。进程等待期间会泵送消息，且格式将写回用户可编辑对象；这些校验保护的是原文及字符位置。表格边框同时设置 `Visible` 和 `Transparency` 也有宿主渲染差异的明确注释和对应回归测试，不应仅凭看似重复就删除。

证据：[fonts.cpp:36](D:/Code/CodeInk/src/fonts.cpp:36)、[progress.cpp:35](D:/Code/CodeInk/src/progress.cpp:35)、[render.cpp:326](D:/Code/CodeInk/src/render.cpp:326)、[render.cpp:463](D:/Code/CodeInk/src/render.cpp:463)、[render.cpp:291](D:/Code/CodeInk/src/render.cpp:291)。

### 现有测试层次并不构成整体过度工程化

JS 单测检查协议和主题，真实桥接错误路径检查 CLI 边界，C++ 语义测试比较整理前后可观察输出，COM 模拟验证对象创建与清理，Windows 手工检查覆盖宿主界面。这些层次承担不同职责。发现 F1/F2/F3 中的遗留断言，应局部更新对应测试，不应因此删除整组测试。

PE 架构、COM 导出和内嵌载荷一致性检查对应真实打包错误，值得保留。VS Code 配置主要复用 `dev.py`，没有再引入一套独立构建流程。`settings_preview.cpp` 和两个 `windows_*_check.cpp` 是手动验证入口，不因未加入 Linux CI 就判为死代码。

## 低收益可选简化

自定义主题名字通过 SHA-256 内容哈希产生稳定标识，但当前每次 `highlight()` 都创建并释放独立 highlighter，只加载一个自定义主题。仓库没有用这个名字做缓存、持久化或跨实例比较。固定的内部保留名字即可满足当前隔离需求；现有测试只验证相同主题名稳定，并未证明需要不同内容拥有不同名字。

证据：[theme-loader.mjs:40](D:/Code/CodeInk/engine/theme-loader.mjs:40)、[bridge.mjs:38](D:/Code/CodeInk/engine/bridge.mjs:38)、[bridge.mjs:51](D:/Code/CodeInk/engine/bridge.mjs:51)、[engine_tests.mjs:30](D:/Code/CodeInk/tests/engine_tests.mjs:30)。

这是可以收敛的局部设计，收益仅是省去一次主题 JSON 序列化、哈希及对应导入，不是值得单独推动的重构项目，也不计入上面的 7 项主要发现。

## 本轮验证记录

| 检查 | 结果 |
| --- | --- |
| Ribbon XML 属性与注册回调差集 | 90 个注册名，60 个 Ribbon 回调，6 个宿主入口，24 个无当前 Ribbon 绑定 |
| 包校验中的命名映射逻辑 | 单独复现 `GFontCount/GFontLabel` 两条命名误报 |
| 构建模块导入模拟 | 模拟只有 clang++ 时，仅导入就要求 lld-link；目录创建已用 mock 屏蔽，未实际创建 |
| C++ 只读编译检查 | 12 个插件/检查器翻译单元通过 `-fsyntax-only -Wunused-function -Wunused-private-field -Wunreachable-code`；安装器做源码审查 |
| 文档证据链接 | 逐项核对文件与行号 |

编译器没有报告未使用静态函数，不代表不存在字符串分派中的遗留入口或恒定参数下的不可达分支；F1/F3 是通过调用路径核对得出。没有运行完整安装包校验、全量行为测试、安装或 PowerPoint 操作，也没有把前轮的宿主测试结果冒充本轮新执行的检查。

## 优化实施记录

实施状态：已完成。实施日期：2026 年 11 月 1 日。用户已授权按本文优化代码。范围为 F1～F7 及主题命名简化；保留上文明确应保留的协议、原文、宿主兼容和安装回滚机制。

| 项目 | 处理方向 | 状态 |
| --- | --- | --- |
| F1 | 删除退出 Ribbon 的 24 个历史回调，测试改用设置窗口入口；偏好字段和保存路径保留 | 已完成 |
| F2 | 由一份显式绑定表产生枚举与回调名，去掉命名推断和例外表；包检查核对入口覆盖及名称唯一性 | 已完成 |
| F3 | 删除全量展开 Tab 的不可达模式及其专用辅助函数；保留现有换行转换与整理器缩进 | 已完成 |
| F4 | Catalog 只保存 ID 和名称，显示标签复用名称 | 已完成 |
| F5 | 一次读取各文本框标签，按栏号和角色直接定位；数量、范围和唯一性保证结构完整 | 已完成 |
| F6 | 工具查找及目录初始化移至构建入口；Ribbon 生成可独立调用；补入 3 项回归检查 | 已完成 |
| F7 | 收敛隐藏子进程参数，移除无引用 ABI 及 URLMON 配置 | 已完成 |
| 可选项 | 使用当前独立 highlighter 内的固定主题名，删除哈希导入与计算 | 已完成 |

### 实施验证

| 检查 | 本次实际结果 |
| --- | --- |
| `python -X utf8 dev.py test` | 3 项构建模块检查、全部自有引擎模块语法检查、22 项 JS 单测和 4 项桥接 CLI 错误路径检查通过 |
| `python -X utf8 dev.py self-test` | 按锁文件安装依赖后，真实 Shiki 多语言高亮、JSONC 主题继承与自定义主题通过 |
| 文本和布局辅助测试 | UTF-16 行数/宽度、行号、设置值解析、空行跨度、栏数分配等通过，保留 1～200 行与可用栏数的均分检查 |
| C++ 渲染模拟 | 56 个场景通过；覆盖 Tab 和全部支持的换行、空行、分栏、边框、替换失败及原文保留 |
| Ribbon/偏好模拟 | 15 个场景通过；设置保存、输出模式切换、重连后的实际操作参数、字体及日志检查通过 |
| 设置窗口模拟 | 14 个场景通过；覆盖取消、输入校验、分栏预览及容量冲突的调整 |
| 完整 Windows 构建 | LLVM 生成 x86/x64 插件、两种架构检查器及在线安装器，通过现有编译警告检查 |
| `python -X utf8 tests/verify_package.py` | PE 架构、系统导入、COM 导出、manifest、当前载荷逐字节一致性及 Ribbon 回调映射全部通过；74 个控件 ID、60 个 Ribbon 回调、66 个分派入口 |
| 源码收尾检查 | 无遗留的旧回调引用、Tab 展开参数或被删除的 ABI 引用；`git diff --check` 通过 |

200 栏且含行号的模拟将 400 个文本框顺序反转，仍能还原原文。计数得到 801 次标签读取，即一项容器布局和每个文本框两项角色标记；原嵌套循环在相同规模下需要约 320,000 次叶节点标签读取。此结果验证调用量及正确性，不代表实测 PowerPoint 延迟。重复标记、缺少栏号标记、缺少文本框、越界栏号、非整数栏号、非法角色和不匹配的行号开关共 7 种异常情况均被拒绝。

C++ 模拟本轮使用现有 Windows GCC 和项目 Win32/COM 桩编译运行；不是 Linux ASan/UBSan 执行记录，也不是 Office 界面实测。没有运行安装器或注册检查器，没有替换已安装插件，没有修改现有课件。PowerPoint Ribbon 外观、显示缩放和保存重开未在本轮重新验收。

临时链接器硬链接及其空目录 `build/validation-tools/` 已清理，原始 LLVM 链接器的 SHA256 保持不变。

完整构建和包检查产物位于 `dist/`，包检查详情位于 `build/reports/pe-validation.json`；它们继续遵循仓库的生成文件忽略规则。
