# Changelog

新的改动在此追加简短条目（日期 · 改了什么 · 影响），不要再新建按日期命名的评审文档。历史评审稿保留在本目录作为设计背景。

## 2026-10-24 · 仓库整理与代码拆分
- 建立 git 仓库，提交基线。
- `tests/output` 增加按套件保留最近 3 次运行的自动清理（`tests/output_retention.py`）。
- 内部评审文档、截图与验收证据移入 `docs/dev/`，删除重复的评审 zip；发布包只含用户文档。
- README 拆分为用户手册与 `docs/dev/DEVELOPMENT.md`。
- `AGENTS.md` 增加模块地图、术语表与文档规范。
- 清理 `main.cpp` 未使用的控件 ID 与 `JobType::FocusFormat`；修正 `showPanelMenu` 缺少控件时的弹出锚点回退；统一非测试敏感的“泡泡 / 候选”文案为“预设 / 常用选项”。
- 将 `main.cpp` 中的窗口检测与后台线程、GDI+ 绘制、弹出搜索/预设浮层、设置中心及内建界面自测拆分为 `dialog_detect.h`、`panel_graphics.h`、`flyout_ui.h`、`manager_ui.h`、`ui_checks.h`（`main.cpp` 从 6,630 行降至约 3,530 行）；构建启用 `/MP`。
- 使用 `.clang-format` 统一 `src/` 代码格式。

## 历史记录（2026-10-17 至 2026-10-21）

| 日期 | 主题 | 文档 |
| --- | --- | --- |
| 10-17 | 本轮升级完成记录 | [UPGRADE-2026-10-17.md](UPGRADE-2026-10-17.md) |
| 10-17 | 保存窗口兼容性与通用接入 | [save-window-compatibility.md](save-window-compatibility.md) |
| 10-19 | 主面板体验改进 | [experience-2026-10-19.md](experience-2026-10-19.md) |
| 10-19 | 使用体验与可靠性改进 | [quality-2026-10-19.md](quality-2026-10-19.md) |
| 10-20 | 性能检查与优化 | [performance-audit-2026-10-20.md](performance-audit-2026-10-20.md)、[performance-optimization-2026-10-20.md](performance-optimization-2026-10-20.md) |
| 10-20 | 命名方案设置评审 | [naming-scheme-review-2026-10-20.md](naming-scheme-review-2026-10-20.md) |
| 10-20 | 命名方案编辑器：实现评审、视觉层级、表格布局、默认内容与固定保存区 | [ux-refactor](naming-editor-ux-refactor-2026-10-20.md)、[visual-hierarchy](naming-editor-visual-hierarchy-2026-10-20.md)、[table-layout](naming-editor-table-layout-2026-10-20.md)、[defaults-footer](naming-editor-defaults-footer-2026-10-20.md) |
| 10-21 | 命名方案编辑器：精细化 UX、状态修正与场景验证 | [ux-refinement](naming-editor-ux-refinement-2026-10-21.md)、[usage-validation](naming-editor-usage-validation-2026-10-21.md) |
| — | 快速命名：交付说明、深化方案、下一步、共用系统 | [delivery](quick-naming-delivery.md)、[experience](quick-naming-experience.md)、[next](quick-naming-next.md)、[shared-naming-system](shared-naming-system.md) |
