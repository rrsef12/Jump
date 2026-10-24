// Included inside main.cpp. Opt-in API for windows without accessible editors.
// A window property advertises availability; the pipe's process identity is the
// authority. The bridge changes the pending destination; submission is an
// explicit capability and still invokes the host's original save action.
void applySaveBridge(const Job &job, Result &result) {
    auto registered = [&]() {
        return sameTarget(job) &&
               reinterpret_cast<ULONG_PTR>(GetPropW(job.hwnd, L"Jump.SaveBridge.v1")) == job.pid;
    };
    auto request = [&](const wchar_t *verb, const std::wstring &directory = L"",
                       const std::wstring &name = L"", bool submit = false,
                       const std::vector<std::wstring> &expected = {}) {
        return blender_bridge::request(job.hwnd, job.pid, g->panel, verb, directory, name, L"", submit,
                                       L"Save", L"JUMP_SAVE_1", expected);
    };
    auto valid = [&](const blender_bridge::Reply &reply) {
        return reply.ok && reply.save && !reply.revision.empty() && reply.revision.size() <= 128 &&
               std::filesystem::path(reply.directory).is_absolute() &&
               (reply.name.empty() || validName(reply.name)) &&
               (reply.extension.empty() ||
                filename_controls::extension(L"file" + reply.extension) == reply.extension);
    };
    auto adopt = [&](const blender_bridge::Reply &reply) {
        auto &data = result.data;
        data.hwnd = job.hwnd;
        data.pid = job.pid;
        data.save = data.generic = data.bridge = true;
        data.genericNavigate = true;
        data.genericDestination = 3;
        data.process = processName(job.hwnd);
        data.name = data.rawName = reply.name;
        data.directory = reply.directory;
        data.extension = reply.extension;
        data.format = reply.format;
        data.source = reply.source;
        data.bridgeRevision = reply.revision;
        data.genericSubmit = reply.canSubmit;
    };
    if (!registered()) {
        result.error = L"程序接入已断开，未改变输出位置。";
        return;
    }
    auto before = request(L"INSPECT");
    if (!valid(before)) {
        result.error = before.error.empty() ? L"程序未提供有效的保存目标。" : before.error;
        return;
    }
    adopt(before);
    if (job.type == JobType::Apply) {
        if ((job.submit && !before.canSubmit) || !registered() || !focusAllowsAction(job) ||
            before.name != job.expectedRawName ||
            normalize(before.directory) != normalize(job.expectedDirectory) ||
            before.extension != job.expectedExtension || before.revision != job.bridgeRevision) {
            result.error = L"原窗口状态已改变，请检查最新预览；程序接入不自动提交保存。";
            return;
        }
        auto directory = job.directory.empty() ? before.directory : job.directory;
        auto attrs = GetFileAttributesW(directory.c_str());
        if (!std::filesystem::path(directory).is_absolute() ||
            directory.find_first_of(L"\r\n\t\"<>|*?") != directory.npos || attrs == INVALID_FILE_ATTRIBUTES ||
            !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            result.error = L"目标目录不存在或无法访问，未更新输出位置。";
            return;
        }
        if (!job.name.empty() && !validName(job.name)) {
            result.error = L"请输入有效的文件名。";
            return;
        }
        auto name = job.name.empty() ? L"" : job.name + before.extension;
        auto after = request(L"APPLY", directory, name, job.submit,
                             {before.name, before.directory, before.extension, before.revision});
        if (!valid(after)) {
            result.error = after.error.empty() ? L"程序未确认接受操作，请检查原窗口。" : after.error;
            return;
        }
        if (job.submit && after.submitted) {
            if (after.name != name || normalize(after.directory) != normalize(directory) ||
                after.extension != before.extension || after.format != before.format) {
                result.error = L"程序提交前回读的输出位置或名称不一致；未提交保存，请检查原窗口。";
                return;
            }
            adopt(after);
            result.navigated = normalize(before.directory) != normalize(after.directory);
            result.submitted = true;
            result.probe = saveProbeFor(result.data);
            return;
        }
        auto verified = request(L"INSPECT");
        if (!valid(verified) || !registered() || verified.name != name ||
            normalize(verified.directory) != normalize(directory) || verified.extension != before.extension ||
            verified.format != before.format || verified.revision != after.revision) {
            result.error = L"程序回读的输出位置或名称不一致；未提交保存，请检查原窗口。";
            return;
        }
        adopt(verified);
        result.navigated = normalize(before.directory) != normalize(verified.directory);
    }
    result.probe = saveProbeFor(result.data);
}
