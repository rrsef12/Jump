# Jump 保存助手 · 0.1.0 测试版

适用于 Windows x64。本版在 Windows 11 验证，Windows 10 尚未单独实测；程序未进行代码签名。

## 第一次使用

1. 将压缩包完整解压到固定位置，例如 `D:\Apps\Jump`，不要直接在压缩包内运行。
2. 双击 `Jump.exe`。程序在任务栏通知区域后台运行；图标可能在隐藏图标区域中。
3. 在软件中打开“另存为”，Jump 会在支持的文件窗口旁显示。点选位置、修改名称后，在原软件中确认保存。格式、覆盖确认和实际文件写入由原软件处理。
4. 右键通知区域的 Jump 图标，选择“设置中心…”可管理命名方案和选项；选择“退出 Jump”可完全退出。

## 开机自动启动

1. 先把解压后的 Jump 文件夹放到准备长期使用的位置。
2. 右键通知区域的 Jump 图标 → **设置中心… → 常规**。
3. 勾选 **开机后台运行**，点击 **保存选项**。

之后每次登录 Windows，Jump 会自动在后台启动，无需管理员权限。该选项默认关闭。
取消勾选并保存即可关闭。移动 Jump 文件夹后，运行新位置的程序，保持勾选并再次保存，以更新启动位置。
删除程序前，建议先关闭开机后台运行。该功能仅在当前用户启动文件夹中创建 `Jump.lnk`。

## Blender 和 Houdini（可选）

标准 Windows 文件窗口无需安装接入脚本。以下安装命令在 Jump 文件夹中打开 PowerShell 后运行；安装只影响对应软件的当前用户配置，重启对应软件后生效。

Blender（已验证 5.2.2）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\integrations\blender\install.ps1 -Version 5.2
```

Houdini（严格限定 22.0.368 / Python 3.13 / Windows x64）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\integrations\houdini\install.ps1
```

包内已包含该 Houdini 构建的桥接库，不需要用户自行编译；其他 Houdini 构建不启用。Houdini 只接入工程保存，节点与独立导出暂不支持。
移动 Jump 文件夹后需重新运行对应安装命令，更新允许接入的程序路径。

## 配置和退出

- 配置保存在程序旁的 `data\settings.ini`；目录不可写时使用 `%LOCALAPPDATA%\Jump\settings.ini`。发布包不包含开发者个人配置或保存记录。
- 更新时先从托盘退出 Jump，再替换程序文件；保留 `data` 文件夹即可保留已有配置。
- 在不支持的自绘或自定义保存窗口中，助手可能无法自动出现，或只具备部分能力。不要把“已提交”当作实际写入完成。
- 详细功能、兼容范围和本地接入说明见 `README.md`；包内 `release-manifest.json` 记录文件校验值。
