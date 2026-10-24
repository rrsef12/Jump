#pragma once

#include "model.h"
#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

enum class Action { Location, Recipe, Prefix, Suffix, Folder, Variables };

struct PanelLabel {
    RECT rect;
    std::wstring text;
    bool heading = false;
};

struct ButtonAction {
    Action kind;
    size_t index = 0;
};

struct BubbleColor {
    const wchar_t *name;
    COLORREF value;
};

inline constexpr std::array<BubbleColor, jump::PRESET_COLOR_COUNT + 1> bubbleColors{
    {{L"无标记", 0},
     {L"红色", RGB(224, 92, 103)},
     {L"橙色", RGB(225, 144, 63)},
     {L"黄色", RGB(202, 171, 51)},
     {L"绿色", RGB(64, 168, 116)},
     {L"蓝色", RGB(80, 144, 224)},
     {L"紫色", RGB(164, 111, 213)}}};

struct TextHighlights {
    std::wstring text;
    std::vector<std::pair<size_t, size_t>> matches;
};

struct HoverMotion {
    int from = 0, to = 0;
    ULONGLONG started = 0;
};
