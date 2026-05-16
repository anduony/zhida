#pragma once

#include <windows.h>
#include <string>

class Analytics
{
public:
    static void CheckAndSendFirstUse();
    static bool IsFirstUse();
    static void MarkFirstUseDone();

private:
    static std::wstring GetFirstUseFlagPath();
    static std::string GetPublicIP();
    static std::string GetOSVersion();
    static bool SendToSupabase(const std::string& jsonBody);
    static std::string WideToUtf8(const std::wstring& wstr);
};
