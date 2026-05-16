#include "Config.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "shell32.lib")

const ThemeColors Config::Themes[5] = {
    { L"Minimal White", RGB(245, 246, 250), RGB(255, 255, 255), RGB(30, 30, 30), RGB(66, 133, 244), RGB(235, 238, 245), RGB(120, 120, 120) },
    { L"Dark Night", RGB(30, 30, 30), RGB(45, 45, 45), RGB(230, 230, 230), RGB(138, 180, 248), RGB(60, 60, 60), RGB(160, 160, 160) },
    { L"Graphite", RGB(50, 50, 55), RGB(65, 65, 72), RGB(220, 220, 225), RGB(160, 170, 180), RGB(80, 80, 88), RGB(150, 150, 155) },
    { L"Deep Ocean", RGB(20, 30, 48), RGB(30, 45, 70), RGB(220, 230, 240), RGB(70, 160, 220), RGB(40, 60, 90), RGB(140, 160, 180) },
    { L"Warm Sand", RGB(245, 240, 230), RGB(255, 252, 245), RGB(60, 50, 40), RGB(210, 150, 90), RGB(240, 235, 220), RGB(130, 120, 110) }
};

const COLORREF Config::PresetColors[12] = {
    RGB(66, 133, 244),
    RGB(234, 67, 53),
    RGB(52, 168, 83),
    RGB(251, 188, 4),
    RGB(171, 71, 188),
    RGB(0, 172, 193),
    RGB(255, 112, 67),
    RGB(126, 87, 194),
    RGB(38, 166, 91),
    RGB(66, 66, 66),
    RGB(236, 64, 122),
    RGB(255, 167, 38)
};

Config::Config()
    : hotkeyModifiers(MOD_ALT)
    , hotkeyVk(0x58)
    , opacity(255)
    , themeId(0)
    , autoStart(false)
{
    LoadDefaults();
}

Config::~Config()
{
}

std::wstring Config::GetConfigPath() const
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        return std::wstring(path) + L"\\ZhidaLauncher\\config.json";
    }
    return L"";
}

void Config::EnsureDirectoryExists(const std::wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        std::wstring dir = path.substr(0, pos);
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }
}

void Config::LoadDefaults()
{
    categories.clear();
    categories.push_back({ L"系统工具", RGB(66, 133, 244) });
    categories.push_back({ L"常用应用", RGB(52, 168, 83) });
    categories.push_back({ L"文件夹", RGB(251, 188, 4) });

    shortcuts.clear();
    shortcuts.push_back({ L"我的电脑", L"shell:MyComputerFolder", L"system", 0, 0 });
    shortcuts.push_back({ L"控制面板", L"control.exe", L"system", 0, 0 });
    shortcuts.push_back({ L"计算器", L"calc.exe", L"app", 1, 1 });
    shortcuts.push_back({ L"记事本", L"notepad.exe", L"app", 1, 1 });
    shortcuts.push_back({ L"画图", L"mspaint.exe", L"app", 1, 1 });
    shortcuts.push_back({ L"我的文档", L"shell:Personal", L"folder", 2, 2 });
}

static std::string EscapeJsonString(const std::string& str)
{
    std::string result;
    for (char c : str)
    {
        switch (c)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += c; break;
        }
    }
    return result;
}

static std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    if (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

static std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    if (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

void Config::Load()
{
    std::wstring path = GetConfigPath();
    if (path.empty())
    {
        LoadDefaults();
        return;
    }

    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        LoadDefaults();
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    shortcuts.clear();
    categories.clear();

    auto findInt = [&](const std::string& key, int& out)
    {
        size_t pos = content.find("\"" + key + "\"");
        if (pos != std::string::npos)
        {
            pos = content.find(":", pos);
            if (pos != std::string::npos)
            {
                out = std::atoi(content.c_str() + pos + 1);
            }
        }
    };

    auto findBool = [&](const std::string& key, bool& out)
    {
        size_t pos = content.find("\"" + key + "\"");
        if (pos != std::string::npos)
        {
            pos = content.find(":", pos);
            if (pos != std::string::npos)
            {
                while (content[pos + 1] == ' ' || content[pos + 1] == '\t') pos++;
                out = (content.substr(pos + 1, 4) == "true");
            }
        }
    };

    auto unescapeJsonString = [](const std::string& s) -> std::string
    {
        std::string result;
        for (size_t i = 0; i < s.length(); i++)
        {
            if (s[i] == '\\' && i + 1 < s.length())
            {
                switch (s[i + 1])
                {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += s[i + 1]; break;
                }
                i++;
            }
            else
            {
                result += s[i];
            }
        }
        return result;
    };

    auto parseStringField = [&](const std::string& objContent, const std::string& fieldName) -> std::wstring
    {
        size_t fieldPos = objContent.find("\"" + fieldName + "\"");
        if (fieldPos != std::string::npos)
        {
            size_t valStart = objContent.find('"', fieldPos + fieldName.length() + 2);
            size_t valEnd = objContent.find('"', valStart + 1);
            if (valStart != std::string::npos && valEnd != std::string::npos)
            {
                return StringToWString(unescapeJsonString(objContent.substr(valStart + 1, valEnd - valStart - 1)));
            }
        }
        return L"";
    };

    auto parseIntField = [&](const std::string& objContent, const std::string& fieldName) -> int
    {
        size_t fieldPos = objContent.find("\"" + fieldName + "\"");
        if (fieldPos != std::string::npos)
        {
            size_t colonPos = objContent.find(':', fieldPos);
            if (colonPos != std::string::npos)
            {
                return std::atoi(objContent.c_str() + colonPos + 1);
            }
        }
        return 0;
    };

    auto parseUIntField = [&](const std::string& objContent, const std::string& fieldName) -> unsigned int
    {
        size_t fieldPos = objContent.find("\"" + fieldName + "\"");
        if (fieldPos != std::string::npos)
        {
            size_t colonPos = objContent.find(':', fieldPos);
            if (colonPos != std::string::npos)
            {
                return (unsigned int)strtoul(objContent.c_str() + colonPos + 1, nullptr, 10);
            }
        }
        return 0;
    };

    auto trimWString = [](std::wstring& s)
    {
        while (!s.empty() && (s.back() == L'\0' || s.back() == L' ' || s.back() == L'\t' || s.back() == L'\n' || s.back() == L'\r'))
            s.pop_back();
        size_t start = 0;
        while (start < s.size() && (s[start] == L'\0' || s[start] == L' ' || s[start] == L'\t'))
            start++;
        if (start > 0) s = s.substr(start);
    };

    auto removeNullBytes = [](std::string& s)
    {
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    };

    size_t categoriesPos = content.find("\"categories\"");
    size_t categoriesArrayEnd = std::string::npos;
    if (categoriesPos != std::string::npos)
    {
        size_t arrayStart = content.find('[', categoriesPos);
        if (arrayStart != std::string::npos)
        {
            int depth = 0;
            for (size_t i = arrayStart; i < content.size(); i++)
            {
                if (content[i] == '[') depth++;
                else if (content[i] == ']') { depth--; if (depth == 0) { categoriesArrayEnd = i; break; } }
            }
            if (categoriesArrayEnd != std::string::npos)
            {
                size_t pos = arrayStart + 1;
                while (pos < categoriesArrayEnd)
                {
                    size_t objStart = content.find('{', pos);
                    size_t objEnd = content.find('}', objStart);
                    if (objStart == std::string::npos || objEnd == std::string::npos || objEnd > categoriesArrayEnd)
                        break;

                    std::string objContent = content.substr(objStart + 1, objEnd - objStart - 1);
                    removeNullBytes(objContent);

                    Category cat;
                    cat.name = parseStringField(objContent, "name");
                    trimWString(cat.name);
                    cat.color = (COLORREF)parseUIntField(objContent, "color");
                    if (cat.color == 0)
                        cat.color = RGB(120, 120, 120);
                    categories.push_back(cat);
                    pos = objEnd + 1;
                }
            }
        }
    }

    size_t shortcutsSearchStart = (categoriesArrayEnd != std::string::npos) ? categoriesArrayEnd + 1 : 0;
    size_t shortcutsPos = content.find("\"shortcuts\"", shortcutsSearchStart);
    if (shortcutsPos != std::string::npos)
    {
        size_t arrayStart = content.find('[', shortcutsPos);
        if (arrayStart != std::string::npos)
        {
            int depth = 0;
            size_t shortcutsArrayEnd = std::string::npos;
            for (size_t i = arrayStart; i < content.size(); i++)
            {
                if (content[i] == '[') depth++;
                else if (content[i] == ']') { depth--; if (depth == 0) { shortcutsArrayEnd = i; break; } }
            }
            if (shortcutsArrayEnd != std::string::npos)
            {
                size_t pos = arrayStart + 1;
                while (pos < shortcutsArrayEnd)
                {
                    size_t objStart = content.find('{', pos);
                    size_t objEnd = content.find('}', objStart);
                    if (objStart == std::string::npos || objEnd == std::string::npos || objEnd > shortcutsArrayEnd)
                        break;

                    std::string objContent = content.substr(objStart + 1, objEnd - objStart - 1);
                    removeNullBytes(objContent);

                    ShortcutItem item;
                    item.name = parseStringField(objContent, "name");
                    trimWString(item.name);
                    item.path = parseStringField(objContent, "path");
                    trimWString(item.path);
                    item.type = parseStringField(objContent, "type");
                    trimWString(item.type);
                    item.colorIndex = parseIntField(objContent, "colorIndex");
                    item.categoryIndex = parseIntField(objContent, "categoryIndex");
                    if (item.categoryIndex < 0)
                        item.categoryIndex = 0;
                    shortcuts.push_back(item);
                    pos = objEnd + 1;
                }
            }
        }
    }

    {
        std::vector<Category> uniqueCats;
        for (const auto& cat : categories)
        {
            bool found = false;
            for (const auto& uc : uniqueCats)
            {
                if (uc.name == cat.name) { found = true; break; }
            }
            if (!found) uniqueCats.push_back(cat);
        }
        categories = uniqueCats;
    }

    for (auto& item : shortcuts)
    {
        if (item.categoryIndex >= (int)categories.size())
            item.categoryIndex = 0;
    }

    if (categories.empty())
    {
        categories.push_back({ L"系统工具", RGB(66, 133, 244) });
        categories.push_back({ L"常用应用", RGB(52, 168, 83) });
        categories.push_back({ L"文件夹", RGB(251, 188, 4) });
    }

    if (shortcuts.empty())
    {
        LoadDefaults();
    }

    int modifiers = MOD_ALT;
    int vkKey = 0x58;
    findInt("hotkeyModifiers", modifiers);
    findInt("hotkeyVk", vkKey);
    hotkeyModifiers = (UINT)modifiers;
    hotkeyVk = (UINT)vkKey;
    findInt("opacity", opacity);
    findInt("themeId", themeId);
    findBool("autoStart", autoStart);
}

void Config::Save()
{
    std::wstring path = GetConfigPath();
    if (path.empty()) return;

    EnsureDirectoryExists(path);

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"hotkeyModifiers\": " << hotkeyModifiers << ",\n";
    oss << "  \"hotkeyVk\": " << hotkeyVk << ",\n";
    oss << "  \"opacity\": " << opacity << ",\n";
    oss << "  \"themeId\": " << themeId << ",\n";
    oss << "  \"autoStart\": " << (autoStart ? "true" : "false") << ",\n";

    oss << "  \"categories\": [\n";
    for (size_t i = 0; i < categories.size(); i++)
    {
        const auto& cat = categories[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << EscapeJsonString(WStringToString(cat.name)) << "\",\n";
        oss << "      \"color\": " << (unsigned int)cat.color << "\n";
        oss << "    }";
        if (i != categories.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    oss << "  \"shortcuts\": [\n";
    for (size_t i = 0; i < shortcuts.size(); i++)
    {
        const auto& item = shortcuts[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << EscapeJsonString(WStringToString(item.name)) << "\",\n";
        oss << "      \"path\": \"" << EscapeJsonString(WStringToString(item.path)) << "\",\n";
        oss << "      \"type\": \"" << EscapeJsonString(WStringToString(item.type)) << "\",\n";
        oss << "      \"colorIndex\": " << item.colorIndex << ",\n";
        oss << "      \"categoryIndex\": " << item.categoryIndex << "\n";
        oss << "    }";
        if (i != shortcuts.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";

    std::ofstream file(path.c_str(), std::ios::binary);
    if (file.is_open())
    {
        std::string jsonStr = oss.str();
        file.write(jsonStr.c_str(), jsonStr.size());
        file.close();
    }
}
