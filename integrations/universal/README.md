# 通用保存目标接口 v1

供有脚本或插件能力、但没有可靠辅助功能控件的软件使用。接入后 Jump 自动伴随该软件的保存/导出窗口，快捷位置、路径输入、命名组合和自动编号直接作用于原软件的待保存目标，无需复制粘贴。

这是**应用开发接入接口**，不会自动给所有第三方软件安装插件。普通系统窗口、标准 Qt、自定义面包屑地址栏、可识别输出表单由 Jump 的现有控件适配器处理，不需要这个接口。

## Python 接入

将本目录的 `jump_save_bridge.py` 和 `jump_save_pipe.py` 放在应用的插件 Python 路径。仅依赖 Python 标准库，Windows 专用。应用负责将真实状态映射为两个回调：

```python
from jump_save_bridge import Bridge, Snapshot

def submit_original_save():
    # 可选：调用宿主原有的 Save/Export 动作。不要自行写文件或绕过覆盖确认。
    export_dialog.invoke_original_save()

bridge = Bridge([r'D:\Code\Jump\build\Jump.exe'], submit=submit_original_save)

def read_destination():
    # 必须读取真实待保存状态，不返回上次收到的请求缓存。
    if not export_dialog.has_pending_save():
        return None
    return Snapshot(
        directory=export_dialog.output_directory(),  # 绝对目录
        name=export_dialog.complete_filename(),      # 完整文件名，含扩展名
        extension='.png',
        format='PNG',
    )

def set_destination(directory, complete_filename):
    # 实现为应用自身的设置接口；不要调用保存、导出、覆盖确认。
    export_dialog.set_output_directory(directory)
    export_dialog.set_complete_filename(complete_filename)

# 仅在实际保存/导出操作已打开时绑定其顶层 HWND。
bridge.attach(export_dialog.hwnd(), read_destination, set_destination)
# 在应用 UI 定时器中每 15–50 ms 调用 bridge.tick()。
# 保存窗口结束、取消、插件卸载时调用 bridge.detach()。
```

上例的 `export_dialog` 是宿主需要实现的映射，不是一个现成通用软件对象。每个进程一个 Bridge；切换保存窗口时重新 attach，会产生新的会话版本。创建、attach、tick、detach 和所有回调必须在同一个 UI 线程执行；回调应及时返回。输出目录需要已存在。格式改变由原软件负责。首次可传空名称。

Jump 只更新待保存位置及名称，保存仍在原窗口确认，保留原软件覆盖提示。文件记录继续要求现有文件核验成功；接口返回成功不等于文件已经写入。原软件自行点击保存不享有 Jump 发起提交时的最后一次编号确认。

## 可运行示例

```powershell
python integrations/universal/demo.py --jump D:\Code\Jump\build\Jump.exe --directory D:\Code\Jump
```

先用 `dev.ps1 -Mode Background` 启动开发版。示例只有 Canvas 绘制的内容，没有可写的文件名或目录控件；选择 Jump 快捷位置、点击命名标签即可看到宿主状态改变。示例不保存任何文档。关闭示例即撤销接入。

## 非 Python 应用的协议

C++、C#、Java 等可实现相同协议，无需嵌入 Python。

- 当前保存窗口通过 `SetPropW(hwnd, L"Jump.SaveBridge.v1", (HANDLE)(UINT_PTR)GetCurrentProcessId())` 声明接入；结束时移除属性。属性仅供发现，不能替代身份核验。
- 添加或移除属性后，向通过允许列表核验的 `Jump.Control` 窗口发送 `PostMessageW(controller, WM_APP + 11, (WPARAM)hwnd, (LPARAM)pid)`。这样即使在已显示的主窗口内开始/结束保存操作，也能及时出现/收起面板。Python SDK 已实现通知。
- 宿主创建本地消息模式命名管道 `\\.\pipe\Jump.Save.<PID>`，拒绝远程连接。Jump 检查管道服务端 PID 等于窗口 PID；宿主通过 `GetNamedPipeClientProcessId` 检查调用者的完整可执行文件路径在显式允许列表内。
- 每个连接一问一答，UTF-16LE，无 BOM、无末尾换行；字段以 LF 分隔，字段不允许 CR/LF/NUL。单条消息不超过 32000 字节。连接和读写必须有界；Python 服务端用非阻塞 UI 轮询，并清理超时连接。
- 窗口必须属于宿主、可见且启用；应用还必须确认真实保存操作仍存在。执行 APPLY 时前台必须是该窗口，或属于调用方 PID 的 `Jump.Panel`。
- 没有 `submit` 回调时只填入；提供回调后，Jump 菜单才会显示显式提交入口。提交仍由原软件执行，保留覆盖确认。

INSPECT 请求按顺序 8 个字段：

| 序号 | 字段 |
|---|---|
| 0 | `JUMP_SAVE_1` |
| 1 | `INSPECT` |
| 2 | 目标 HWND 的十进制值 |
| 3、4、5 | 空字符串 |
| 6 | `0` |
| 7 | Jump 面板 HWND，尚未创建时为 `0` |

APPLY 为 12 个字段：前 8 个字段中，1 改为 `APPLY`，3 为目标绝对目录，4 为完整文件名，5 仍为空，6 必须为 `0`。追加字段 8–11 分别为预期旧文件名、旧目录、旧扩展名和旧会话版本。**在 UI 线程修改任何状态前**，必须将它们与当前快照比较。版本必须包含会话身份并随状态变化；关闭重开同一个窗口也不能复用旧版本。

成功响应 12 个字段依次为：`JUMP_SAVE_1`、`OK`、`1`、实际完整文件名、实际绝对目录、扩展名、格式描述、源文档路径（可空）、是否已提交（`0`/`1`）、目录是否改变（`0`/`1`）、新会话版本、是否支持提交（`0`/`1`）。会话版本非空且最多 128 字符。错误响应：`JUMP_SAVE_1`、`ERROR`、单行原因。

APPLY 后先重新读取应用状态，确认目录、文件名、格式一致再回答。Jump 收到成功响应后还会再发 INSPECT 独立核验；不一致时提示失败且不会自动重试。不能用请求文本直接组成“实际状态”冒充核验结果。

## 验证

- `tests/universal_save_integration.py`：真实命名管道 + 自绘宿主 + Jump 全流程，及两类输出表单。
- `tests/universal_protocol_checks.py`：不可信客户端、错误窗口、过期快照、重开会话、用户同时修改、无效目录、禁用保存提交和回读失败。
- 与其他前台集成测试串行运行，使用隔离配置并限定宿主 PID。
