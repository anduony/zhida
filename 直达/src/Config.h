#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct Category
{
    std::wstring name;
    COLORREF color;
};

struct ShortcutItem
{
    std::wstring name;
    std::wstring path;
    std::wstring type;
    int colorIndex;
    int categoryIndex;
};

struct CardGroup
{
    std::wstring name;
    std::vector<size_t> shortcutIndices;
};

struct ThemeColors
{
    const wchar_t* name;
    COLORREF windowBg;
    COLORREF cardBg;
    COLORREF titleText;
    COLORREF cardBar;
    COLORREF hoverBg;
    COLORREF subtitleText;
};

class Config
{
public:
    Config();
    ~Config();

    void Load();
    void Save();
    std::wstring GetConfigPath() const;

    UINT hotkeyModifiers;
    UINT hotkeyVk;
    int opacity;
    int themeId;
    bool autoStart;
    std::vector<ShortcutItem> shortcuts;
    std::vector<Category> categories;
    std::vector<CardGroup> groups;

    static const ThemeColors Themes[5];
    static const COLORREF PresetColors[12];

private:
    void LoadDefaults();
    void EnsureDirectoryExists(const std::wstring& path);
};
