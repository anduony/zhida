#include "Analytics.h"
#include <shlobj.h>
#include <wininet.h>
#include <sstream>
#include <ctime>

#pragma comment(lib, "wininet.lib")

static const char* SUPABASE_URL = "kqerdmxhwtgidzlvolxe.supabase.co";
static const char* SUPABASE_KEY = "sb_publishable_sPwwm524TXgwJLvl7rC45A_qwMZJDbz";
static const char* SUPABASE_TABLE = "user_stats";

std::string Analytics::WideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, NULL, NULL);
    if (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

std::wstring Analytics::GetFirstUseFlagPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        return std::wstring(path) + L"\\ZhidaLauncher\\first_use_sent";
    }
    return L"";
}

bool Analytics::IsFirstUse()
{
    std::wstring path = GetFirstUseFlagPath();
    if (path.empty()) return true;
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs == INVALID_FILE_ATTRIBUTES);
}

void Analytics::MarkFirstUseDone()
{
    std::wstring path = GetFirstUseFlagPath();
    if (path.empty()) return;

    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        std::wstring dir = path.substr(0, pos);
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        const char* data = "1";
        WriteFile(hFile, data, 1, &written, NULL);
        CloseHandle(hFile);
    }
}

static std::string HttpGetRequest(const char* url, DWORD timeoutMs = 8000)
{
    HINTERNET hInternet = InternetOpenA("ZhidaLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE
        | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url, NULL, 0, flags, 0);

    std::string result;
    if (hConnect)
    {
        char buffer[512];
        DWORD bytesRead = 0;
        while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            result += buffer;
        }
        InternetCloseHandle(hConnect);
    }
    InternetCloseHandle(hInternet);
    return result;
}

static bool IsValidIPAddress(const std::string& s)
{
    if (s.empty() || s.length() > 15) return false;
    int dots = 0;
    int digits = 0;
    for (char c : s)
    {
        if (c == '.') { dots++; digits = 0; }
        else if (c >= '0' && c <= '9') { digits++; if (digits > 3) return false; }
        else return false;
    }
    return dots == 3;
}

static std::string CleanIPString(const std::string& s)
{
    std::string result;
    for (char c : s)
    {
        if ((c >= '0' && c <= '9') || c == '.')
            result += c;
        else if (!result.empty())
            break;
    }
    return result;
}

std::string Analytics::GetPublicIP()
{
    const char* urls[] = {
        "https://api.ipify.org?format=text",
        "https://api.ipify.org",
        "https://checkip.amazonaws.com",
        "https://ifconfig.me",
        "http://api.ipify.org",
        "http://checkip.amazonaws.com"
    };

    for (int i = 0; i < _countof(urls); i++)
    {
        std::string raw = HttpGetRequest(urls[i]);
        if (raw.empty()) continue;

        std::string ip = CleanIPString(raw);
        if (IsValidIPAddress(ip))
            return ip;
    }

    return "";
}

std::string Analytics::GetOSVersion()
{
    OSVERSIONINFOEXW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    typedef NTSTATUS(NTAPI* RtlGetVersionPtr)(OSVERSIONINFOEXW*);
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll)
    {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtDll, "RtlGetVersion");
        if (RtlGetVersion)
        {
            RtlGetVersion(&osvi);
        }
    }

    std::ostringstream oss;
    oss << "Windows " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << "." << osvi.dwBuildNumber;
    return oss.str();
}

static std::string EscapeJsonString(const std::string& s)
{
    std::string result;
    for (char c : s)
    {
        switch (c)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        default: result += c; break;
        }
    }
    return result;
}

bool Analytics::SendToSupabase(const std::string& jsonBody)
{
    HINTERNET hInternet = InternetOpenA("ZhidaLauncher/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    DWORD timeoutMs = 10000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    HINTERNET hConnect = InternetConnectA(hInternet, SUPABASE_URL, INTERNET_DEFAULT_HTTPS_PORT,
        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string path = std::string("/rest/v1/") + SUPABASE_TABLE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE
        | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_IGNORE_CERT_CN_INVALID, 0);
    if (!hRequest)
    {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string apiKeyHeader = std::string("apikey: ") + SUPABASE_KEY;
    std::string authHeader = std::string("Authorization: Bearer ") + SUPABASE_KEY;
    std::string contentHeader = "Content-Type: application/json";
    std::string preferHeader = "Prefer: return=minimal";

    HttpAddRequestHeadersA(hRequest, apiKeyHeader.c_str(), (DWORD)apiKeyHeader.length(), HTTP_ADDREQ_FLAG_ADD);
    HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.length(), HTTP_ADDREQ_FLAG_ADD);
    HttpAddRequestHeadersA(hRequest, contentHeader.c_str(), (DWORD)contentHeader.length(), HTTP_ADDREQ_FLAG_ADD);
    HttpAddRequestHeadersA(hRequest, preferHeader.c_str(), (DWORD)preferHeader.length(), HTTP_ADDREQ_FLAG_ADD);

    BOOL result = HttpSendRequestA(hRequest, NULL, 0, (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length());

    if (result)
    {
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);
        result = (statusCode >= 200 && statusCode < 300);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return (result == TRUE);
}

void Analytics::CheckAndSendFirstUse()
{
    if (!IsFirstUse()) return;

    std::string ip = GetPublicIP();
    std::string osVersion = GetOSVersion();

    std::ostringstream json;
    json << "{";
    json << "\"ip_address\":\"" << EscapeJsonString(ip) << "\",";
    json << "\"os_version\":\"" << EscapeJsonString(osVersion) << "\",";
    json << "\"app_version\":\"1.0.0\"";
    json << "}";

    if (SendToSupabase(json.str()))
        MarkFirstUseDone();
}
