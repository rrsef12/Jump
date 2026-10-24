# Development workflow

- During normal development, use `dev.ps1` / `Start-Test.cmd` to update and run the development app. Do not package or overwrite `dist` unless the user asks for a portable release.
- `dev.ps1` builds Debug only when source inputs change and restarts the app with the persistent, separate `build/dev/settings.ini` configuration. Preserve this configuration across iterations.
- Default mode opens our own real save-dialog test host. Use `-Mode Background` for testing with the user's applications, `-Mode Manage` for preset management, and `-Stop` to stop the development session.
- The launcher switches away from this project's portable instance to avoid duplicate helpers. Process operations must remain scoped to this workspace's exact executable paths. Never close the user's application or its file dialog to restart Jump.
- For fixes, run the relevant existing checks; avoid rebuilding, rerunning unrelated suites, or packaging after successful verification without a reason.
- Keep verification proportional to risk. For small visual-only changes, a successful development build and, when useful, one visual check are sufficient. Run targeted automated checks for critical behavior; skip nonessential tests and unrelated suites.
