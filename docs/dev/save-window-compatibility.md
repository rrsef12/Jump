# 保存窗口兼容性与通用接入（2026-10-17）

依据 GitHub 实现梳理窗口类型，并将 Jump 的接入按实际能力选择。软件名称不是可靠分类：同一软件的“保存”“导出图片”“批量输出”可能使用三套不同窗口。

## 当前处理方式

| 类型 | 识别及操作 | 现状与边界 |
|---|---|---|
| Windows Common Item Dialog（IFileSaveDialog） | 系统控件与原有路径导航 | 原生宿主实测；Electron、Chromium、WPF、wxWidgets、NFD 等源码确认采用此类接口，但没有逐个认证所有产品版本 |
| 旧式 GetSaveFileName / OPENFILENAME | 传统系统文件窗口 | 原生宿主实测；保留原窗口保存及覆盖提示 |
| 标准 Qt QFileDialog 的非原生模式 | Qt 文件名、目录、格式控件组合；存在目录触发导航后从独立目录控件核验 | 使用本机 SD 携带的 Qt 运行库实测；之前已验证真实 SD Save Image 自动出现及回填 |
| 自定义面包屑地址栏 | 找编辑入口、核验地址框焦点、导航后重新打开地址栏读取实际目录 | WPS 真实窗口已在前轮验证；本轮重新通过自有结构宿主回归。并非所有外观相似的地址栏都自动匹配 |
| 完整输出文件路径 | 一次写入目标目录＋完整文件名，再读取控件及格式 | 本轮新增并实测；直接改变输出目标，不宣称浏览器文件列表已导航 |
| 文件名＋明确的输出目录字段 | 唯一可读写输出目录、唯一文件名，分步填写并重新检查两者 | 本轮新增并实测；普通 Folder / Search / 地址框不会被当作输出字段盲写。只读或歧义时保留命名能力 |
| Avalonia 托管选择器、Swing、GTK 等工具包自有窗口 | 先检查实际辅助功能结构；符合上面的控件协议才启用对应能力 | 源码研究完成；没有声称每个框架已实际适配。读不到可核验位置时需应用接入 |
| ImGui、Canvas、游戏引擎等自行绘制窗口 | 应用主动接入统一保存目标协议；在宿主内部设置并回读真实状态 | 本轮新增通用协议、Python SDK、自绘示例及真实管道集成验证。第三方软件仍需要实现对应回调 |
| 没有窗口的直接下载、自动导出、后台批处理 | 由宿主在保存动作前暴露待保存目标，才能让 Jump 介入 | 没有统一 Windows“保存前窗口”可供发现。浏览器下载需浏览器扩展/应用接口，目前没有安装或实现浏览器下载拦截 |

这些通路共用 Jump 的快捷位置、手动路径和命名 UI。自动发现不了但存在可写文件名控件时，可用 Ctrl+Alt+J 主动检测、Ctrl+Alt+N 首次关联文件名；它们不会替未知目录赋予未经核验的导航能力。菜单的“本地接入诊断”显示位置究竟来自哪种能力。

## 从源码得到的关键约束

Qt 的 `QFileDialog::accept()` 在 AnyFile 模式下遇到真实目录，会 `setDirectory()` 并返回，正常文件则继续保存流程；因此只有严格确认是标准 Qt 控件、目录确实存在并回读独立目录状态后，才能使用这条导航通路。

ImGuiFileDialog 的文件名 `InputText` 使用 `EnterReturnsTrue`，按 Enter 会设置 `isOk`。这说明不能把“往任意输入框填路径再按回车”当作通用跳转方案。自绘控件也不会因为名字像文件名就自动拥有 Windows UIA 编辑接口。

Swing、GTK、Avalonia 的自定义文件选择器维护各自的目录、选择项、格式和确认状态；应用也可以替换界面。对这些框架，源码有导航函数不等于外部进程能直接调用该函数。统一接口解决的是如何接入，不能免除宿主提供访问能力。

本轮没有把复制名称作为新增兼容方案，也没有监控任意文件写入后偷偷移动或改名。新增通路都是直接改变保存前的目标。

## Blender / Houdini 迁移

Blender 和 Houdini 的宿主插件现在都使用 `JUMP_SAVE_1`、`\\.\pipe\Jump.Save.<PID>`、`Jump.SaveBridge.v1` 窗口属性和 `WM_APP + 11` 通知。Jump 先处理统一协议，不再根据这两个软件选择不同的 Jump 管道。插件内保留的 Blender `bpy` 文件浏览器读取、Houdini 原生窗口 DLL 读写，是宿主 API 映射，不能由一个跨软件协议消除。

统一协议增加了提交能力位。没有原软件提交回调时只填入；提供回调时才显示显式提交入口。提交前仍检查会话版本、名称、目录和格式，宿主执行原始保存动作，覆盖确认仍由宿主显示。旧协议分支保留在源码中用于尚未升级的已安装插件，升级后的插件不再使用它们。

## 本轮验证范围

- 完整路径和输出目录：自动出现、中文及空格目录、快捷位置、手动路径、名称与后缀保留、无效目录拒绝；只读、目录歧义、普通 Folder 标签、应用忽略修改均不误报。
- 自绘宿主：无文件名/目录编辑控件且标题不含 Save/Export；通过窗口注册和真实本地管道自动出现、更新位置及名称。忽略修改时报告失败。
- 协议：调用者完整可执行路径、窗口归属、前台身份、会话版本和旧状态核验；拒绝保存提交、路径穿越、无效目标和过期请求。
- 回归：系统现代/传统窗口、通用控件、编号及保存工作流、Qt 导航、WPS 类面包屑导航、隐藏布局检查。

所有本轮前台测试均绑定自建宿主 PID 或本工作区测试宿主路径，使用独立配置。没有保存、关闭或操作用户的 WPS/SD 文档窗口。所有修改只在开发版本，未更新 dist。

结果索引：`tests/output/universal-save/report.json`、`tests/output/check-runs/foreground.json`、`tests/output/check-runs/hidden.json`、`tests/output/qt-save/report.json`、`tests/output/breadcrumb/report.json`。最终构建的汇总见 `tests/output/universal-save/verification-summary.json`。

程序接口说明与示例：[通用保存目标 SDK](../../integrations/universal/README.md)。

## GitHub 源码依据

以下链接固定到本轮实际读取的提交，而非会持续变化的分支。它们用于解释窗口实现，不能作为“所有软件已实测”的证据。

<!-- GitHub source links generated from the recorded fetch metadata below. -->

- [electron/electron — file_dialog_win.cc](https://github.com/electron/electron/blob/baccc3ffd0c27266af887e2384363327bd3820ad/shell/browser/ui/file_dialog_win.cc)

- [dotnet/winforms — FileDialog.cs](https://github.com/dotnet/winforms/blob/c4891de0851184dcbe95d16ca543969fe2540245/src/System.Windows.Forms/System/Windows/Forms/Dialogs/CommonDialogs/FileDialog.cs)

- [dotnet/wpf — CommonItemDialog.cs](https://github.com/dotnet/wpf/blob/65d15adece1df9a73e3d1abc11f8bbf99e9fc7a9/src/Microsoft.DotNet.Wpf/src/PresentationFramework/Microsoft/Win32/CommonItemDialog.cs)

- [qt/qtbase — qfiledialog.cpp](https://github.com/qt/qtbase/blob/7faa468198d1d631452db7e5cbfab203cecbdee2/src/widgets/dialogs/qfiledialog.cpp)

- [wxWidgets/wxWidgets — filedlg.cpp](https://github.com/wxWidgets/wxWidgets/blob/b96429bf2cacc2c19e728201409dee1441c8aa59/src/msw/filedlg.cpp)

- [mlabbe/nativefiledialog — nfd_win.cpp](https://github.com/mlabbe/nativefiledialog/blob/67345b80ebb429ecc2aeda94c478b3bcc5f7888e/src/nfd_win.cpp)

- [btzy/nativefiledialog-extended — nfd_win.cpp](https://github.com/btzy/nativefiledialog-extended/blob/65b2f070a0fcfee0d453a64e770a217ab0b6ff4f/src/nfd_win.cpp)

- [openjdk/jdk — BasicFileChooserUI.java](https://github.com/openjdk/jdk/blob/cd37d2bb07b7a050b4925ed3312c2a8dd7a801b1/src/java.desktop/share/classes/javax/swing/plaf/basic/BasicFileChooserUI.java)

- [GNOME/gtk — gtkfilechooserwidget.c](https://github.com/GNOME/gtk/blob/e4db180344c8f6d60b3b043ad70980776b297d69/gtk/gtkfilechooserwidget.c)

- [aiekick/ImGuiFileDialog — ImGuiFileDialog.cpp](https://github.com/aiekick/ImGuiFileDialog/blob/d0e97b2adc3d3452d72c750c7305dc0291acd052/ImGuiFileDialog.cpp)

- [chromium/chromium — select_file_dialog_win.cc](https://github.com/chromium/chromium/blob/5926523bcfedb77ba2533ace1dd93b477673f72a/ui/shell_dialogs/select_file_dialog_win.cc)

- [AvaloniaUI/Avalonia — ManagedFileChooserViewModel.cs](https://github.com/AvaloniaUI/Avalonia/blob/66b7273b9a2b5ba8416feb4376b9cdc3078b8573/src/Avalonia.Dialogs/Internal/ManagedFileChooserViewModel.cs)

- [chromium/chromium — execute_select_file_win.cc](https://github.com/chromium/chromium/blob/5926523bcfedb77ba2533ace1dd93b477673f72a/ui/shell_dialogs/execute_select_file_win.cc)

- [dotnet/wpf — SaveFileDialog.cs](https://github.com/dotnet/wpf/blob/65d15adece1df9a73e3d1abc11f8bbf99e9fc7a9/src/Microsoft.DotNet.Wpf/src/PresentationFramework/Microsoft/Win32/SaveFileDialog.cs)
