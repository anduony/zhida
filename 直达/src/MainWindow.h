#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "Config.h"

#pragma comment(lib, "gdiplus.lib")

struct CardRect
{
    RECT rc;
    size_t index;
    bool isAddButton;
    int categoryIndex;
};

struct CategoryHeaderRect
{
    RECT rc;
    int categoryIndex;
};

struct CardGroupRect
{
    RECT rc;
    size_t groupIndex;
};

class MainWindow
{
public:
    MainWindow();
    ~MainWindow();

    BOOL Create(HINSTANCE hInstance);
    void Show();
    void Hide();
    void ToggleVisibility();
    void CenterWindow();
    HWND GetHwnd() const;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnCreate();
    void OnDestroy();
    void OnTrayIcon(WPARAM wParam, LPARAM lParam);
    void OnTrayCommand(int id);
    void OnHotKey();
    void OnMouseMove(LPARAM lParam);
    void OnLButtonDown(LPARAM lParam);
    void OnRButtonDown(LPARAM lParam);
    void OnKillFocus();
    void OnTimer(WPARAM timerId);
    void OnMouseWheel(WPARAM wParam, LPARAM lParam);
    void OnVScroll(WPARAM wParam, LPARAM lParam);
    void UpdateScrollInfo();

    void DeleteShortcut(size_t index);
    void OpenAddShortcutDialog();
    void RefreshUI();

    void InitTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();
    void RegisterGlobalHotKey();
    void UnregisterGlobalHotKey();
    void UpdateCardLayout();
    void LaunchShortcut(size_t index);

    void OpenSettings();
    void CloseSettings();
    void CreateSettingsWindow();
    static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleSettingsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnSettingsPaint(HWND hwnd);
    void ApplySettings();
    void ToggleAutoStart(bool enable);

    void OpenHelp();
    void CloseHelp();
    void CreateHelpWindow();
    static LRESULT CALLBACK HelpWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleHelpMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnHelpPaint(HWND hwnd);

    void CreateAddShortcutWindow();
    static LRESULT CALLBACK AddShortcutWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleAddShortcutMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OpenCategoryManager();
    void CloseCategoryManager();
    void CreateCategoryManagerWindow();
    static LRESULT CALLBACK CategoryManagerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleCategoryManagerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RefreshCategoryList(HWND hwnd);
    void OpenAddCategoryDialog();
    void OpenEditCategoryDialog(int editIndex);
    void OpenAddCategoryDialogInternal();
    static LRESULT CALLBACK AddCategoryWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleAddCategoryMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void LaunchGroupShortcuts(size_t groupIndex);
    void ShowGroupLaunchResult(const std::vector<std::wstring>& successNames, const std::vector<std::pair<std::wstring, std::wstring>>& failures);

    void OpenGroupManager();
    void CloseGroupManager();
    void CreateGroupManagerWindow();
    static LRESULT CALLBACK GroupManagerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleGroupManagerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RefreshGroupList(HWND hwnd);
    void OpenAddGroupDialog();
    void OpenEditGroupDialog(int editIndex);
    void OpenAddGroupDialogInternal();
    static LRESULT CALLBACK AddGroupWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleAddGroupMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    HWND m_settingsHwnd;
    HWND m_helpHwnd;
    HWND m_addShortcutHwnd;
    HWND m_categoryManagerHwnd;
    HWND m_addCategoryHwnd;
    HWND m_groupManagerHwnd;
    HWND m_addGroupHwnd;
    HINSTANCE m_hInstance;
    ULONG_PTR m_gdiplusToken;
    Gdiplus::GdiplusStartupInput m_gdiplusInput;

    int m_editCategoryIndex;
    int m_editGroupIndex;

    NOTIFYICONDATAW m_nid;
    bool m_trayIconAdded;

    Config m_config;
    std::vector<CardRect> m_cardRects;
    std::vector<CategoryHeaderRect> m_categoryHeaders;
    std::vector<CardGroupRect> m_groupRects;
    size_t m_hoveredCard;
    size_t m_hoveredGroup;
    size_t m_rightClickedCard;
    bool m_mouseDown;

    int m_scrollPos;
    int m_maxScrollPos;
    int m_selectedCategoryColorIndex;

    static const int WINDOW_WIDTH = 700;
    static const int WINDOW_HEIGHT = 550;
    static const int CARD_PADDING = 24;
    static const int CARD_GAP = 18;
    static const int CARD_HEIGHT = 54;
    static const int COLUMNS = 4;
    static const int TITLE_HEIGHT = 0;
    static const int CATEGORY_HEADER_HEIGHT = 36;
};
