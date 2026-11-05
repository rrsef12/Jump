# Jump 保存助手

Windows x64 原生便携小工具。运行后驻留通知区域，当“打开 / 另存为”窗口出现时自动显示在旁边，帮你快速选位置、按命名方案生成文件名，再交回原软件完成保存。关闭文件窗口后自动收起，无需快捷键。

## 功能概览

- **保存位置**：主界面提供推荐、收藏、最近、历史四类位置，单击即可切换；放大镜或 `Ctrl+F` 打开搜索，星标收藏。
- **快速命名**：按命名方案显示常用字段，可自定义顺序、候选、分隔符、大小写、补零和出现条件；支持撤销 / 重做和实时回填。
- **前后缀与替换**：常用前后缀预设、可视化文字 / 正则替换，实时预览最终文件名。
- **可核验保存**：确认目标文件真实写入后才记录“最近保存”，不扫描目录或读取文件内容。
- **软件接入**：通用窗口经 Windows UI Automation 自动识别；Blender、Houdini 提供软件内接入脚本。

详细的交互说明、设置中心和命名方案编辑器见 [`docs/dev/CHANGELOG.md`](docs/dev/CHANGELOG.md) 与应用内帮助。

## 使用

1. 将发布包解压到固定文件夹，运行 `Jump.exe`（仅显示通知区域图标）。
2. 在软件中打开“另存为”，助手会出现在旁边，不抢输入焦点。
3. 单击位置或名称即可填入，确认后用原窗口的“保存”完成写入。
4. 点击方案名切换命名方案，更多字段从 **命名设置 ▾** 进入；位置管理从位置菜单进入。
5. 退出请用通知区域菜单的“退出 Jump”。

**开机运行默认关闭**，可在“设置中心 → 常规”中开启。配置默认存放在 `Jump.exe` 同目录的 `data/settings.ini`，目录不可写时改用 `%LOCALAPPDATA%\Jump\settings.ini`。

## 构建

需要 Visual Studio C++ 生成工具和 Windows SDK。

```powershell
./build.ps1          # Release 构建，输出 build\Jump.exe
./build.ps1 -Debug   # Debug 构建
```

实现细节见源码仓库中的 `docs/dev/DEVELOPMENT.md`（不随发布包提供）。

## 兼容范围

- 原生 C++ / Win32 单进程，无网络请求、遥测、服务或驱动。
- 支持 Windows 现代 `IFileSaveDialog / IFileOpenDialog` 与经典 `GetSaveFileName / GetOpenFileName` 文件窗口（中文 / 英文“保存”）。
- 在 Windows 11 x64 测试；Blender 5.2 LTS、Houdini 22.0.368 验证过软件内接入。此版本未进行代码签名。
