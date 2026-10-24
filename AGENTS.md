# Development workflow

- During normal development, use `dev.ps1` / `Start-Test.cmd` to update and run the development app. Do not package or overwrite `dist` unless the user asks for a portable release.
- `dev.ps1` builds Debug only when source inputs change and restarts the app with the persistent, separate `build/dev/settings.ini` configuration. Preserve this configuration across iterations.
- Default mode opens our own real save-dialog test host. Use `-Mode Background` for testing with the user's applications, `-Mode Manage` for preset management, and `-Stop` to stop the development session.
- The launcher switches away from this project's portable instance to avoid duplicate helpers. Process operations must remain scoped to this workspace's exact executable paths. Never close the user's application or its file dialog to restart Jump.
- For fixes, run the relevant existing checks; avoid rebuilding, rerunning unrelated suites, or packaging after successful verification without a reason.
- Keep verification proportional to risk. For small visual-only changes, a successful development build and, when useful, one visual check are sufficient. Run targeted automated checks for critical behavior; skip nonessential tests and unrelated suites.


# Repository hygiene

- The workspace is a git repository. Commit after each completed, verified change with a short message; never rewrite history or force-push.
- `tests/output/` is disposable. Each check writes a per-run folder; `tests/output_retention.py` keeps the newest 3 per suite and runs automatically from `run_checks.py` and `native_integration.py`.
- Documentation: `README.md` is the shipped user manual (behavior only, no implementation details). Developer notes go in `docs/dev/DEVELOPMENT.md`. Do not create new dated review/evidence documents, zips, or screenshots under `docs/`; summarize notable changes in `docs/dev/CHANGELOG.md` instead.
- `package.ps1` ships only `Jump.exe`, `README.md`, the quick start, and `integrations/`. Keep it a whitelist.

# Module map (where new code goes)

`main.cpp` is a single translation unit that includes most headers; it is already too large. Put new behavior in the matching header/module below (or a new focused file), and keep `main.cpp` to wiring: window procedures, message dispatch, and startup.

| Area | Files |
| --- | --- |
| Config model, parsing, persistence | `model.h/.cpp`, `config_writer.h`, `structured_naming.h` |
| Shared state / thread transport types | `app_state.h` (UI-thread `State`), `save_session_types.h`, `ui_types.h` |
| Save-dialog detection and UIA adapters (worker thread) | `filename_controls.h`, `generic_dialog.h`, `save_target.h`, `paste_target.h` |
| Host app bridges (named pipes) | `blender_bridge.h`, `save_bridge.h`, `integrations/` |
| Save verification and notices | `save_verification.h`, `save_notice.h` |
| Naming engine and background numbering | `naming_session.h`, `naming_worker.h`, `naming_tools.h`, `naming_choices.h`, `subject_group.h` |
| Main panel UI | `structured_ui.h`, `structured_controls.h`, `experience_ui.h`, `save_assist_ui.h`, `paste_target_ui.h`, `transient_edit.h`, `naming_feedback.h`, `popup_menu.h`, `panel_appearance.h`, `panel_material.h` |
| Settings center | `settings_center.h`, `settings_host.h`, `settings_pages.h`, `settings_draft.h`, `form_dialog.h`, `workflow_ui.h`, `naming_defaults_ui.h` |
| Scheme editor | `naming_designer.h`, `naming_designer_layout.h`, `naming_designer_preview.h` |
| Built-in self tests (`--self-test`) | `workflow_checks.h`, tests at the end of `model.cpp` |

When adding a new `WM_APP` message, timer, or control ID, check the existing constants at the top of `main.cpp` for collisions.

# Product glossary (user-facing Chinese text)

Use these terms in all new or edited UI text and docs. Existing strings may be aligned gradually; update any tests that match changed strings in the same change.

| Use | Meaning | Avoid in new text |
| --- | --- | --- |
| 命名方案 | Field structure, format, numbering, folder suggestions, app association | 规则集、模板 |
| 命名部分 | One part of a scheme in the scheme editor (main panel may say the part's own label) | 字段 (OK in code), 标签 |
| 常用选项 | Predefined values of a 命名部分 | 候选、泡泡 |
| 主要名称 | The `subject` part | 主体、资产名 |
| 前后缀 / 替换规则 | Session modifiers; together 修饰规则 | — |
| 自动版本 / 序号 | Trailing automatic number (v001 / 001) | 编号 when the kind matters |
| 保存流程 | Saved scheme + folder + modifiers bundle | 流程方案 |
| 收藏 / 最近 / 历史 / 推荐 | Location tabs; 最近 = verified saves only, 历史 = browsed folders | — |

Keep UI copy short: prefer a tooltip or placeholder over a permanent explanatory line.
