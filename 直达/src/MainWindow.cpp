#include "MainWindow.h"
#include "Resource.h"
#include "Analytics.h"
#include <shlwapi.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "shlwapi.lib")

void DrawRoundedRect(Gdiplus::Graphics& graphics, Gdiplus::RectF rect, float radius, Gdiplus::Color fillColor, Gdiplus::Color borderColor, bool hasShadow, bool isHovered)
{
    if (hasShadow)
    {
        for (int i = 0; i < 6; i++)
        {
            float alpha = isHovered ? (50.0f - i * 7.0f) : (30.0f - i * 4.0f);
            Gdiplus::Color shadowColor((BYTE)alpha, 0, 0, 0);
            Gdiplus::SolidBrush shadowBrush(shadowColor);
            Gdiplus::RectF shadowRect = rect;
            shadowRect.X += i * 0.8f;
            shadowRect.Y += i * 0.8f + 2.0f;
            
            Gdiplus::GraphicsPath shadowPath;
            shadowPath.AddArc(shadowRect.X, shadowRect.Y, radius, radius, 180, 90);
            shadowPath.AddArc(shadowRect.X + shadowRect.Width - radius, shadowRect.Y, radius, radius, 270, 90);
            shadowPath.AddArc(shadowRect.X + shadowRect.Width - radius, shadowRect.Y + shadowRect.Height - radius, radius, radius, 0, 90);
            shadowPath.AddArc(shadowRect.X, shadowRect.Y + shadowRect.Height - radius, radius, radius, 90, 90);
            shadowPath.CloseFigure();
            graphics.FillPath(&shadowBrush, &shadowPath);
        }
    }

    Gdiplus::GraphicsPath path;
    path.AddArc(rect.X, rect.Y, radius, radius, 180, 90);
    path.AddArc(rect.X + rect.Width - radius, rect.Y, radius, radius, 270, 90);
    path.AddArc(rect.X + rect.Width - radius, rect.Y + rect.Height - radius, radius, radius, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - radius, radius, radius, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fillBrush(fillColor);
    graphics.FillPath(&fillBrush, &path);

    Gdiplus::Pen borderPen(borderColor, isHovered ? 2.5f : 1.5f);
    graphics.DrawPath(&borderPen, &path);
}

static Gdiplus::Color ColorRefToGdiplus(COLORREF cr)
{
    return Gdiplus::Color(255, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

static LRESULT HandleCtlColor(WPARAM wParam)
{
    HDC hdc = (HDC)wParam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
}

static const wchar_t* GetSystemUIFont()
{
    static wchar_t fontName[LF_FACESIZE] = { 0 };
    if (fontName[0] == 0)
    {
        NONCLIENTMETRICSW ncm = { 0 };
        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
        wcscpy_s(fontName, ncm.lfMessageFont.lfFaceName);
    }
    return fontName;
}

static void SetDialogFont(HWND hwnd)
{
    NONCLIENTMETRICSW ncm = { 0 };
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    
    HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);
    if (hFont)
    {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, lParam, TRUE);
            return TRUE;
        }, (LPARAM)hFont);
    }
}

MainWindow::MainWindow()
    : m_hwnd(NULL)
    , m_settingsHwnd(NULL)
    , m_helpHwnd(NULL)
    , m_addShortcutHwnd(NULL)
    , m_categoryManagerHwnd(NULL)
    , m_addCategoryHwnd(NULL)
    , m_groupManagerHwnd(NULL)
    , m_addGroupHwnd(NULL)
    , m_hInstance(NULL)
    , m_trayIconAdded(false)
    , m_hoveredCard((size_t)-1)
    , m_hoveredGroup((size_t)-1)
    , m_rightClickedCard((size_t)-1)
    , m_mouseDown(false)
    , m_gdiplusToken(0)
    , m_scrollPos(0)
    , m_maxScrollPos(0)
    , m_selectedCategoryColorIndex(0)
    , m_editCategoryIndex(-1)
    , m_editGroupIndex(-1)
{
    ZeroMemory(&m_nid, sizeof(m_nid));
}

MainWindow::~MainWindow()
{
}

BOOL MainWindow::Create(HINSTANCE hInstance)
{
    m_hInstance = hInstance;
    m_config.Load();

    Gdiplus::GdiplusStartup(&m_gdiplusToken, &m_gdiplusInput, NULL);

    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaMainWindow";
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wcex))
    {
        return FALSE;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - WINDOW_WIDTH) / 2;
    int y = (screenHeight - WINDOW_HEIGHT) / 2;

    m_hwnd = CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_APPWINDOW, L"ZhidaMainWindow", L"直达",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL, x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, this);

    if (!m_hwnd)
    {
        return FALSE;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return TRUE;
}

void MainWindow::Show()
{
    if (m_hwnd)
    {
        CenterWindow();
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        UpdateWindow(m_hwnd);
    }
}

void MainWindow::Hide()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void MainWindow::ToggleVisibility()
{
    if (!m_hwnd) return;
    if (IsWindowVisible(m_hwnd))
    {
        Hide();
    }
    else
    {
        Show();
    }
}

void MainWindow::CenterWindow()
{
    if (!m_hwnd) return;
    UpdateCardLayout();
}

HWND MainWindow::GetHwnd() const
{
    return m_hwnd;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* pThis = NULL;
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    }
    else
    {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (pThis)
    {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        OnCreate();
        InvalidateRect(m_hwnd, NULL, TRUE);
        return 0;
    case WM_PAINT:
        OnPaint();
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(lParam);
        return 0;
    case WM_LBUTTONDOWN:
        OnLButtonDown(lParam);
        return 0;
    case WM_RBUTTONDOWN:
        OnRButtonDown(lParam);
        return 0;
    case WM_MOUSEWHEEL:
        OnMouseWheel(wParam, lParam);
        return 0;
    case WM_VSCROLL:
        OnVScroll(wParam, lParam);
        return 0;
    case WM_KILLFOCUS:
        OnKillFocus();
        return 0;
    case WM_TRAYICON:
        OnTrayIcon(wParam, lParam);
        return 0;
    case WM_HOTKEY:
        OnHotKey();
        return 0;
    case WM_TIMER:
        OnTimer(wParam);
        return 0;
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == ID_TRAY_SHOW || id == ID_TRAY_SETTINGS || id == ID_TRAY_EXIT || id == ID_TRAY_HELP)
        {
            OnTrayCommand(id);
        }
        else if (id == ID_CARD_DELETE)
        {
            DeleteShortcut(m_rightClickedCard);
            m_rightClickedCard = (size_t)-1;
        }
        return 0;
    }
    case WM_DESTROY:
        OnDestroy();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

void MainWindow::OnCreate()
{
    InitTrayIcon();
    RegisterGlobalHotKey();
    UpdateCardLayout();
    Analytics::CheckAndSendFirstUse();
}

void MainWindow::OnDestroy()
{
    RemoveTrayIcon();
    UnregisterGlobalHotKey();
    if (m_settingsHwnd && IsWindow(m_settingsHwnd)) { DestroyWindow(m_settingsHwnd); m_settingsHwnd = NULL; }
    if (m_helpHwnd && IsWindow(m_helpHwnd)) { DestroyWindow(m_helpHwnd); m_helpHwnd = NULL; }
    if (m_addShortcutHwnd && IsWindow(m_addShortcutHwnd)) { DestroyWindow(m_addShortcutHwnd); m_addShortcutHwnd = NULL; }
    if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd)) { DestroyWindow(m_categoryManagerHwnd); m_categoryManagerHwnd = NULL; }
    if (m_addCategoryHwnd && IsWindow(m_addCategoryHwnd)) { DestroyWindow(m_addCategoryHwnd); m_addCategoryHwnd = NULL; }
    if (m_groupManagerHwnd && IsWindow(m_groupManagerHwnd)) { DestroyWindow(m_groupManagerHwnd); m_groupManagerHwnd = NULL; }
    if (m_addGroupHwnd && IsWindow(m_addGroupHwnd)) { DestroyWindow(m_addGroupHwnd); m_addGroupHwnd = NULL; }
    m_config.Save();
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

void MainWindow::InitTrayIcon()
{
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = ID_TRAY_ICON;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!m_nid.hIcon)
    {
        m_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }
    wcscpy_s(m_nid.szTip, L"直达");
    if (Shell_NotifyIconW(NIM_ADD, &m_nid))
    {
        m_trayIconAdded = true;
    }
}

void MainWindow::RemoveTrayIcon()
{
    if (m_trayIconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayIconAdded = false;
    }
}

void MainWindow::ShowTrayMenu()
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"显示窗口");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_HELP, L"帮助");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}

void MainWindow::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (lParam)
    {
    case WM_LBUTTONDBLCLK:
        ToggleVisibility();
        break;
    case WM_RBUTTONUP:
        ShowTrayMenu();
        break;
    }
}

void MainWindow::OnTrayCommand(int id)
{
    switch (id)
    {
    case ID_TRAY_SHOW:
        Show();
        break;
    case ID_TRAY_SETTINGS:
        OpenSettings();
        break;
    case ID_TRAY_HELP:
        OpenHelp();
        break;
    case ID_TRAY_EXIT:
        DestroyWindow(m_hwnd);
        break;
    }
}

void MainWindow::RegisterGlobalHotKey()
{
    RegisterHotKey(m_hwnd, 1, m_config.hotkeyModifiers, m_config.hotkeyVk);
}

void MainWindow::UnregisterGlobalHotKey()
{
    UnregisterHotKey(m_hwnd, 1);
}

void MainWindow::OnHotKey()
{
    ToggleVisibility();
}

void MainWindow::UpdateCardLayout()
{
    m_cardRects.clear();
    m_categoryHeaders.clear();
    m_groupRects.clear();
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientWidth = rc.right - rc.left;
    int availableWidth = clientWidth - 2 * CARD_PADDING;
    int cardWidth = (availableWidth - (COLUMNS - 1) * CARD_GAP) / COLUMNS;

    int currentY = TITLE_HEIGHT + CARD_PADDING;

    for (size_t catIdx = 0; catIdx < m_config.categories.size(); ++catIdx)
    {
        CategoryHeaderRect header;
        header.rc.left = CARD_PADDING;
        header.rc.top = currentY;
        header.rc.right = clientWidth - CARD_PADDING;
        header.rc.bottom = currentY + CATEGORY_HEADER_HEIGHT;
        header.categoryIndex = (int)catIdx;
        m_categoryHeaders.push_back(header);
        currentY += CATEGORY_HEADER_HEIGHT;

        int col = 0;
        for (size_t i = 0; i < m_config.shortcuts.size(); ++i)
        {
            if (m_config.shortcuts[i].categoryIndex != (int)catIdx)
                continue;

            int x = CARD_PADDING + col * (cardWidth + CARD_GAP);
            int y = currentY;

            CardRect card;
            card.rc.left = x;
            card.rc.top = y;
            card.rc.right = x + cardWidth;
            card.rc.bottom = y + CARD_HEIGHT;
            card.index = i;
            card.isAddButton = false;
            card.categoryIndex = (int)catIdx;
            m_cardRects.push_back(card);

            col++;
            if (col >= COLUMNS)
            {
                col = 0;
                currentY += CARD_HEIGHT + CARD_GAP;
            }
        }

        CardRect addCard;
        addCard.rc.left = CARD_PADDING + col * (cardWidth + CARD_GAP);
        addCard.rc.top = currentY;
        addCard.rc.right = addCard.rc.left + cardWidth;
        addCard.rc.bottom = currentY + CARD_HEIGHT;
        addCard.index = m_config.shortcuts.size();
        addCard.isAddButton = true;
        addCard.categoryIndex = (int)catIdx;
        m_cardRects.push_back(addCard);

        currentY += CARD_HEIGHT + CARD_GAP;

        currentY += CARD_PADDING / 2;
    }

    if (!m_config.groups.empty())
    {
        currentY += CARD_PADDING;

        CategoryHeaderRect groupSectionHeader;
        groupSectionHeader.rc.left = CARD_PADDING;
        groupSectionHeader.rc.top = currentY;
        groupSectionHeader.rc.right = clientWidth - CARD_PADDING;
        groupSectionHeader.rc.bottom = currentY + CATEGORY_HEADER_HEIGHT;
        groupSectionHeader.categoryIndex = -1;
        m_categoryHeaders.push_back(groupSectionHeader);
        currentY += CATEGORY_HEADER_HEIGHT;

        for (size_t groupIdx = 0; groupIdx < m_config.groups.size(); ++groupIdx)
        {
            CardGroupRect groupRect;
            groupRect.rc.left = CARD_PADDING;
            groupRect.rc.top = currentY;
            groupRect.rc.right = clientWidth - CARD_PADDING;
            groupRect.rc.bottom = currentY + CARD_HEIGHT;
            groupRect.groupIndex = groupIdx;
            m_groupRects.push_back(groupRect);
            currentY += CARD_HEIGHT + CARD_GAP;
        }
    }

    int totalHeight = currentY + CARD_PADDING;
    int clientHeight = WINDOW_HEIGHT - 40;
    m_maxScrollPos = max(0, totalHeight - clientHeight);
    if (m_scrollPos > m_maxScrollPos) m_scrollPos = m_maxScrollPos;
    UpdateScrollInfo();
}

void MainWindow::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int scrollLines = SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, NULL, 0);
    int scrollAmount = (delta > 0 ? -40 : 40) * (scrollLines > 0 ? scrollLines : 3);
    
    m_scrollPos = max(0, min(m_scrollPos + scrollAmount, m_maxScrollPos));
    UpdateScrollInfo();
    InvalidateRect(m_hwnd, NULL, TRUE);
}

void MainWindow::OnVScroll(WPARAM wParam, LPARAM lParam)
{
    int nScrollCode = LOWORD(wParam);
    int nPos = HIWORD(wParam);
    
    switch (nScrollCode)
    {
    case SB_LINEUP: m_scrollPos -= 30; break;
    case SB_LINEDOWN: m_scrollPos += 30; break;
    case SB_PAGEUP: m_scrollPos -= 150; break;
    case SB_PAGEDOWN: m_scrollPos += 150; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: m_scrollPos = nPos; break;
    }
    
    m_scrollPos = max(0, min(m_scrollPos, m_maxScrollPos));
    UpdateScrollInfo();
    InvalidateRect(m_hwnd, NULL, TRUE);
}

void MainWindow::UpdateScrollInfo()
{
    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = m_maxScrollPos;
    si.nPage = 100;
    si.nPos = m_scrollPos;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void MainWindow::OnMouseMove(LPARAM lParam)
{
    int x = LOWORD(lParam);
    int y = HIWORD(lParam) + m_scrollPos;
    size_t prevHoveredCard = m_hoveredCard;
    size_t prevHoveredGroup = m_hoveredGroup;

    m_hoveredCard = (size_t)-1;
    m_hoveredGroup = (size_t)-1;

    for (size_t i = 0; i < m_cardRects.size(); ++i)
    {
        const auto& card = m_cardRects[i];
        if (x >= card.rc.left && x < card.rc.right && y >= card.rc.top && y < card.rc.bottom)
        {
            m_hoveredCard = i;
            break;
        }
    }

    if (m_hoveredCard == (size_t)-1)
    {
        for (size_t i = 0; i < m_groupRects.size(); ++i)
        {
            const auto& group = m_groupRects[i];
            if (x >= group.rc.left && x < group.rc.right && y >= group.rc.top && y < group.rc.bottom)
            {
                m_hoveredGroup = i;
                break;
            }
        }
    }

    if (m_hoveredCard != prevHoveredCard || m_hoveredGroup != prevHoveredGroup)
    {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonDown(LPARAM lParam)
{
    int x = LOWORD(lParam);
    int y = HIWORD(lParam) + m_scrollPos;
    bool handled = false;

    for (size_t i = 0; i < m_cardRects.size(); ++i)
    {
        const auto& card = m_cardRects[i];
        if (x >= card.rc.left && x < card.rc.right && y >= card.rc.top && y < card.rc.bottom)
        {
            if (card.isAddButton)
            {
                m_selectedCategoryColorIndex = card.categoryIndex;
                OpenAddShortcutDialog();
            }
            else
            {
                LaunchShortcut(card.index);
            }
            handled = true;
            break;
        }
    }

    if (!handled)
    {
        for (size_t i = 0; i < m_groupRects.size(); ++i)
        {
            const auto& group = m_groupRects[i];
            if (x >= group.rc.left && x < group.rc.right && y >= group.rc.top && y < group.rc.bottom)
            {
                LaunchGroupShortcuts(group.groupIndex);
                break;
            }
        }
    }
}

void MainWindow::OnRButtonDown(LPARAM lParam)
{
    int x = LOWORD(lParam);
    int y = HIWORD(lParam) + m_scrollPos;

    for (size_t i = 0; i < m_cardRects.size(); ++i)
    {
        const auto& card = m_cardRects[i];
        if (!card.isAddButton && x >= card.rc.left && x < card.rc.right && y >= card.rc.top && y < card.rc.bottom)
        {
            m_rightClickedCard = card.index;

            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            ClientToScreen(m_hwnd, &pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_CARD_DELETE, L"删除快捷方式");

            SetForegroundWindow(m_hwnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
            DestroyMenu(hMenu);
            break;
        }
    }
}

void MainWindow::OnKillFocus()
{
    HWND hFocus = GetFocus();
    HWND hFore = GetForegroundWindow();

    if (hFocus && (IsChild(m_hwnd, hFocus) || hFocus == m_hwnd)) return;
    if (hFore == m_hwnd) return;
    if (m_settingsHwnd && IsWindow(m_settingsHwnd) && (hFore == m_settingsHwnd || (hFocus && IsChild(m_settingsHwnd, hFocus)))) return;
    if (m_helpHwnd && IsWindow(m_helpHwnd) && (hFore == m_helpHwnd || (hFocus && IsChild(m_helpHwnd, hFocus)))) return;
    if (m_addShortcutHwnd && IsWindow(m_addShortcutHwnd) && (hFore == m_addShortcutHwnd || (hFocus && IsChild(m_addShortcutHwnd, hFocus)))) return;
    if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd) && (hFore == m_categoryManagerHwnd || (hFocus && IsChild(m_categoryManagerHwnd, hFocus)))) return;
    if (m_addCategoryHwnd && IsWindow(m_addCategoryHwnd) && (hFore == m_addCategoryHwnd || (hFocus && IsChild(m_addCategoryHwnd, hFocus)))) return;

    Hide();
}

void MainWindow::OnTimer(WPARAM timerId)
{
    UNREFERENCED_PARAMETER(timerId);
}

void MainWindow::LaunchShortcut(size_t index)
{
    if (index >= m_config.shortcuts.size()) return;
    const auto& item = m_config.shortcuts[index];
    ShellExecuteW(NULL, L"open", item.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    Hide();
}

void MainWindow::LaunchGroupShortcuts(size_t groupIndex)
{
    if (groupIndex >= m_config.groups.size()) return;

    const auto& group = m_config.groups[groupIndex];
    std::vector<std::wstring> successNames;
    std::vector<std::pair<std::wstring, std::wstring>> failures;

    for (size_t idx : group.shortcutIndices)
    {
        if (idx >= m_config.shortcuts.size())
            continue;

        const auto& item = m_config.shortcuts[idx];
        HINSTANCE result = ShellExecuteW(NULL, L"open", item.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        
        if ((int)result > 32)
        {
            successNames.push_back(item.name);
        }
        else
        {
            DWORD error = GetLastError();
            std::wstring errorMsg;
            switch (error)
            {
            case ERROR_FILE_NOT_FOUND:
                errorMsg = L"文件未找到";
                break;
            case ERROR_PATH_NOT_FOUND:
                errorMsg = L"路径未找到";
                break;
            case ERROR_ACCESS_DENIED:
                errorMsg = L"访问被拒绝";
                break;
            default:
                errorMsg = L"启动失败 (错误码: " + std::to_wstring(error) + L")";
            }
            failures.push_back({ item.name, errorMsg });
        }
    }

    Hide();

    if (!successNames.empty() || !failures.empty())
    {
        ShowGroupLaunchResult(successNames, failures);
    }
}

void MainWindow::ShowGroupLaunchResult(const std::vector<std::wstring>& successNames, const std::vector<std::pair<std::wstring, std::wstring>>& failures)
{
    std::wstring message;

    if (!successNames.empty())
    {
        message += L"已成功启动:\n";
        for (const auto& name : successNames)
        {
            message += L"  • " + name + L"\n";
        }
    }

    if (!failures.empty())
    {
        if (!message.empty())
            message += L"\n";
        message += L"启动失败:\n";
        for (const auto& failure : failures)
        {
            message += L"  • " + failure.first + L": " + failure.second + L"\n";
        }
    }

    MessageBoxW(NULL, message.c_str(), L"快捷组启动结果", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::DeleteShortcut(size_t index)
{
    if (index >= m_config.shortcuts.size()) return;
    int result = MessageBoxW(m_hwnd, L"确定要删除该快捷方式吗？", L"删除确认", MB_YESNO | MB_ICONQUESTION);
    if (result == IDYES)
    {
        m_config.shortcuts.erase(m_config.shortcuts.begin() + index);
        m_config.Save();
        RefreshUI();
    }
}

void MainWindow::OpenAddShortcutDialog()
{
    CreateAddShortcutWindow();
}

void MainWindow::RefreshUI()
{
    UpdateCardLayout();
    InvalidateRect(m_hwnd, NULL, TRUE);
}

void MainWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    FillRect(hdcMem, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    Gdiplus::Graphics graphics(hdcMem);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

    Gdiplus::RectF clipRect(0.0f, 0.0f, (Gdiplus::REAL)rc.right - rc.left, (Gdiplus::REAL)(rc.bottom - rc.top));

    Gdiplus::Region clipRegion(clipRect);
    graphics.SetClip(&clipRegion);

    for (size_t h = 0; h < m_categoryHeaders.size(); ++h)
    {
        const CategoryHeaderRect& header = m_categoryHeaders[h];
        Gdiplus::RectF headerRect(
            (Gdiplus::REAL)header.rc.left,
            (Gdiplus::REAL)header.rc.top - (Gdiplus::REAL)m_scrollPos,
            (Gdiplus::REAL)(header.rc.right - header.rc.left),
            (Gdiplus::REAL)CATEGORY_HEADER_HEIGHT
        );

        if (headerRect.Y + headerRect.Height < 0 || headerRect.Y > rc.bottom - rc.top)
            continue;

        if (header.categoryIndex >= 0 && header.categoryIndex < (int)m_config.categories.size())
        {
            const Category& cat = m_config.categories[header.categoryIndex];
            Gdiplus::Color catColor = ColorRefToGdiplus(cat.color);

            Gdiplus::SolidBrush barBrush(catColor);
            graphics.FillRectangle(&barBrush, Gdiplus::RectF(headerRect.X, headerRect.Y + 8, 4.0f, headerRect.Height - 16));

            Gdiplus::Font catFont(GetSystemUIFont(), 15, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush catTextBrush(catColor);
            Gdiplus::StringFormat catFormat;
            catFormat.SetAlignment(Gdiplus::StringAlignmentNear);
            catFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF catTextRect(headerRect.X + 12, headerRect.Y, headerRect.Width - 12, headerRect.Height);
            graphics.DrawString(cat.name.c_str(), -1, &catFont, catTextRect, &catFormat, &catTextBrush);
        }
        else if (header.categoryIndex == -1)
        {
            Gdiplus::Color groupColor = Gdiplus::Color(126, 87, 194);
            Gdiplus::SolidBrush barBrush(groupColor);
            graphics.FillRectangle(&barBrush, Gdiplus::RectF(headerRect.X, headerRect.Y + 8, 4.0f, headerRect.Height - 16));

            Gdiplus::Font catFont(GetSystemUIFont(), 15, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush catTextBrush(groupColor);
            Gdiplus::StringFormat catFormat;
            catFormat.SetAlignment(Gdiplus::StringAlignmentNear);
            catFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF catTextRect(headerRect.X + 12, headerRect.Y, headerRect.Width - 12, headerRect.Height);
            graphics.DrawString(L"快捷组", -1, &catFont, catTextRect, &catFormat, &catTextBrush);
        }
    }

    Gdiplus::Font cardFont(GetSystemUIFont(), 14, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(50, 60, 70));
    Gdiplus::SolidBrush addTextBrush(Gdiplus::Color(80, 120, 255));
    Gdiplus::StringFormat cardFormat;
    cardFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
    cardFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    for (size_t i = 0; i < m_cardRects.size(); ++i)
    {
        const CardRect& card = m_cardRects[i];
        bool isHovered = (m_hoveredCard == i);

        Gdiplus::RectF cardRect(
            (Gdiplus::REAL)card.rc.left,
            (Gdiplus::REAL)card.rc.top - (Gdiplus::REAL)m_scrollPos,
            (Gdiplus::REAL)(card.rc.right - card.rc.left),
            (Gdiplus::REAL)(card.rc.bottom - card.rc.top)
        );

        if (cardRect.Y + cardRect.Height < 0 || cardRect.Y > rc.bottom - rc.top)
            continue;

        Gdiplus::Color fillColor = Gdiplus::Color(252, 254, 255);
        Gdiplus::Color borderColor = Gdiplus::Color(210, 220, 235);

        if (card.categoryIndex >= 0 && card.categoryIndex < (int)m_config.categories.size())
        {
            COLORREF catColorRef = m_config.categories[card.categoryIndex].color;
            BYTE r = GetRValue(catColorRef), g = GetGValue(catColorRef), b = GetBValue(catColorRef);
            fillColor = Gdiplus::Color(255, min(252, r + 190), min(254, g + 190), min(255, b + 190));
            borderColor = Gdiplus::Color(255, min(210, r + 140), min(220, g + 140), min(235, b + 140));
        }

        if (card.isAddButton)
        {
            fillColor = Gdiplus::Color(245, 250, 255);
            borderColor = Gdiplus::Color(180, 200, 240);
            if (card.categoryIndex >= 0 && card.categoryIndex < (int)m_config.categories.size())
            {
                COLORREF catColorRef = m_config.categories[card.categoryIndex].color;
                BYTE r = GetRValue(catColorRef), g = GetGValue(catColorRef), b = GetBValue(catColorRef);
                fillColor = Gdiplus::Color(255, min(245, r + 185), min(250, g + 185), min(255, b + 185));
                borderColor = Gdiplus::Color(255, min(180, r + 110), min(200, g + 110), min(240, b + 110));
            }
        }

        if (isHovered)
        {
            fillColor = Gdiplus::Color(255, 255, 255);
            if (card.categoryIndex >= 0 && card.categoryIndex < (int)m_config.categories.size())
            {
                COLORREF catColorRef = m_config.categories[card.categoryIndex].color;
                borderColor = ColorRefToGdiplus(catColorRef);
            }
            else
            {
                borderColor = Gdiplus::Color(100, 140, 255);
            }
        }

        DrawRoundedRect(graphics, cardRect, 14.0f, fillColor, borderColor, true, isHovered);

        if (card.categoryIndex >= 0 && card.categoryIndex < (int)m_config.categories.size() && !card.isAddButton)
        {
            COLORREF catColorRef = m_config.categories[card.categoryIndex].color;
            Gdiplus::Color barColor = ColorRefToGdiplus(catColorRef);
            Gdiplus::SolidBrush leftBarBrush(barColor);
            graphics.FillRectangle(&leftBarBrush, Gdiplus::RectF(cardRect.X + 8, cardRect.Y + 8, 4, cardRect.Height - 16));
        }

        if (card.isAddButton)
        {
            Gdiplus::Font addFont(GetSystemUIFont(), 15, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::Color addColor = Gdiplus::Color(80, 120, 255);
            if (card.categoryIndex >= 0 && card.categoryIndex < (int)m_config.categories.size())
            {
                addColor = ColorRefToGdiplus(m_config.categories[card.categoryIndex].color);
            }
            Gdiplus::SolidBrush addBrush(addColor);
            graphics.DrawString(L"+ 添加", -1, &addFont, cardRect, &cardFormat, &addBrush);
        }
        else
        {
            const auto& item = m_config.shortcuts[card.index];
            Gdiplus::RectF textRect = cardRect;
            textRect.X += 16;
            textRect.Width -= 16;
            graphics.DrawString(item.name.c_str(), -1, &cardFont, textRect, &cardFormat, &textBrush);
        }
    }

    for (size_t i = 0; i < m_groupRects.size(); ++i)
    {
        const CardGroupRect& group = m_groupRects[i];
        bool isHovered = (m_hoveredGroup == i);

        Gdiplus::RectF groupRect(
            (Gdiplus::REAL)group.rc.left,
            (Gdiplus::REAL)group.rc.top - (Gdiplus::REAL)m_scrollPos,
            (Gdiplus::REAL)(group.rc.right - group.rc.left),
            (Gdiplus::REAL)(group.rc.bottom - group.rc.top)
        );

        if (groupRect.Y + groupRect.Height < 0 || groupRect.Y > rc.bottom - rc.top)
            continue;

        Gdiplus::Color fillColor = Gdiplus::Color(248, 245, 252);
        Gdiplus::Color borderColor = Gdiplus::Color(180, 160, 220);

        if (isHovered)
        {
            fillColor = Gdiplus::Color(255, 253, 255);
            borderColor = Gdiplus::Color(126, 87, 194);
        }

        DrawRoundedRect(graphics, groupRect, 14.0f, fillColor, borderColor, true, isHovered);

        Gdiplus::Color groupBarColor = Gdiplus::Color(126, 87, 194);
        Gdiplus::SolidBrush leftBarBrush(groupBarColor);
        graphics.FillRectangle(&leftBarBrush, Gdiplus::RectF(groupRect.X + 8, groupRect.Y + 8, 4, groupRect.Height - 16));

        const auto& groupData = m_config.groups[group.groupIndex];
        Gdiplus::RectF groupTextRect = groupRect;
        groupTextRect.X += 16;

        Gdiplus::Font groupNameFont(GetSystemUIFont(), 14, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush groupNameBrush(Gdiplus::Color(60, 50, 80));
        graphics.DrawString(groupData.name.c_str(), -1, &groupNameFont, groupTextRect, &cardFormat, &groupNameBrush);

        if (!groupData.shortcutIndices.empty())
        {
            std::wstring membersText = L"(";
            for (size_t j = 0; j < groupData.shortcutIndices.size(); ++j)
            {
                if (j > 0) membersText += L", ";
                size_t idx = groupData.shortcutIndices[j];
                if (idx < m_config.shortcuts.size())
                {
                    membersText += m_config.shortcuts[idx].name;
                }
            }
            membersText += L")";

            Gdiplus::Font membersFont(GetSystemUIFont(), 11, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush membersBrush(Gdiplus::Color(120, 100, 160));
            Gdiplus::StringFormat membersFormat;
            membersFormat.SetAlignment(Gdiplus::StringAlignmentFar);
            membersFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF membersRect(groupRect.X + 16, groupRect.Y, groupRect.Width - 32, groupRect.Height);
            graphics.DrawString(membersText.c_str(), -1, &membersFont, membersRect, &membersFormat, &membersBrush);
        }
    }

    graphics.ResetClip();

    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);

    EndPaint(m_hwnd, &ps);
}

void MainWindow::OpenSettings()
{
    if (m_settingsHwnd && IsWindow(m_settingsHwnd))
    {
        SetForegroundWindow(m_settingsHwnd);
        return;
    }
    CreateSettingsWindow();
}

void MainWindow::CloseSettings()
{
    if (m_settingsHwnd && IsWindow(m_settingsHwnd))
    {
        DestroyWindow(m_settingsHwnd);
        m_settingsHwnd = NULL;
        Show();
    }
}

void MainWindow::CreateSettingsWindow()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = SettingsWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaSettingsWindow";
    RegisterClassExW(&wcex);

    int settingsWidth = 480;
    int settingsHeight = 460;
    RECT rcMain;
    GetWindowRect(m_hwnd, &rcMain);
    int x = rcMain.left + (rcMain.right - rcMain.left - settingsWidth) / 2;
    int y = rcMain.top + 50;

    m_settingsHwnd = CreateWindowExW(0, L"ZhidaSettingsWindow", L"设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, settingsWidth, settingsHeight,
        m_hwnd, NULL, m_hInstance, this);

    if (m_settingsHwnd)
    {
        CreateWindowW(L"STATIC", L"快捷键设置", WS_CHILD | WS_VISIBLE, 40, 40, 150, 24, m_settingsHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"STATIC", L"修饰键 (Ctrl/Alt/Shift):", WS_CHILD | WS_VISIBLE, 40, 80, 200, 20, m_settingsHwnd, NULL, m_hInstance, NULL);
        
        HWND hCtrlCheck = CreateWindowW(L"BUTTON", L"Ctrl", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 110, 80, 24, m_settingsHwnd, (HMENU)100, m_hInstance, NULL);
        SendMessageW(hCtrlCheck, BM_SETCHECK, (m_config.hotkeyModifiers & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND hAltCheck = CreateWindowW(L"BUTTON", L"Alt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 130, 110, 80, 24, m_settingsHwnd, (HMENU)101, m_hInstance, NULL);
        SendMessageW(hAltCheck, BM_SETCHECK, (m_config.hotkeyModifiers & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
        HWND hShiftCheck = CreateWindowW(L"BUTTON", L"Shift", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 220, 110, 80, 24, m_settingsHwnd, (HMENU)102, m_hInstance, NULL);
        SendMessageW(hShiftCheck, BM_SETCHECK, (m_config.hotkeyModifiers & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
        
        CreateWindowW(L"STATIC", L"主按键 (点击输入框后按下按键):", WS_CHILD | WS_VISIBLE, 40, 155, 260, 20, m_settingsHwnd, NULL, m_hInstance, NULL);
        wchar_t vkStr[4] = { (wchar_t)m_config.hotkeyVk, 0 };
        if (m_config.hotkeyVk >= 0x70 && m_config.hotkeyVk <= 0x87)
        {
            swprintf_s(vkStr, L"F%d", m_config.hotkeyVk - 0x70 + 1);
        }
        CreateWindowW(L"EDIT", vkStr, WS_CHILD | WS_VISIBLE | WS_BORDER, 310, 150, 80, 28, m_settingsHwnd, (HMENU)105, m_hInstance, NULL);
        {
            HWND hKeyEdit = GetDlgItem(m_settingsHwnd, 105);
            SetWindowLongPtrW(hKeyEdit, GWLP_USERDATA, (LONG_PTR)m_config.hotkeyVk);
        }
        
        CreateWindowW(L"STATIC", L"启动设置", WS_CHILD | WS_VISIBLE, 40, 210, 150, 24, m_settingsHwnd, NULL, m_hInstance, NULL);
        HWND hAutoStartCheck = CreateWindowW(L"BUTTON", L"开机自动启动", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 40, 245, 180, 24, m_settingsHwnd, (HMENU)110, m_hInstance, NULL);
        SendMessageW(hAutoStartCheck, BM_SETCHECK, m_config.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
        
        CreateWindowW(L"BUTTON", L"管理分类", WS_CHILD | WS_VISIBLE, 40, 300, 130, 36, m_settingsHwnd, (HMENU)120, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"管理快捷组", WS_CHILD | WS_VISIBLE, 180, 300, 130, 36, m_settingsHwnd, (HMENU)121, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE, 220, 360, 100, 36, m_settingsHwnd, (HMENU)IDOK, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 340, 360, 100, 36, m_settingsHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);
        
        SetDialogFont(m_settingsHwnd);
        
        ShowWindow(m_settingsHwnd, SW_SHOW);
        SetForegroundWindow(m_settingsHwnd);
        UpdateWindow(m_settingsHwnd);
    }
}

LRESULT CALLBACK MainWindow::SettingsWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleSettingsMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleSettingsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_KEYDOWN:
    {
        HWND hFocus = GetFocus();
        HWND hKeyEdit = GetDlgItem(hwnd, 105);
        if (hFocus == hKeyEdit)
        {
            BYTE vk = (BYTE)wParam;
            wchar_t vkStr[8] = { 0 };
            if (vk >= 0x70 && vk <= 0x87)
                swprintf_s(vkStr, L"F%d", vk - 0x70 + 1);
            else if (vk >= 'A' && vk <= 'Z')
                vkStr[0] = (wchar_t)vk;
            else if (vk >= '0' && vk <= '9')
                vkStr[0] = (wchar_t)vk;
            else
            {
                const wchar_t* names[] = { L"Space", L"PageUp", L"PageDown", L"End", L"Home", L"Left", L"Up", L"Right", L"Down" };
                BYTE codes[] = { VK_SPACE, VK_PRIOR, VK_NEXT, VK_END, VK_HOME, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN };
                bool found = false;
                for (int i = 0; i < 9; i++)
                {
                    if (vk == codes[i]) { wcscpy_s(vkStr, names[i]); found = true; break; }
                }
                if (!found) vkStr[0] = (wchar_t)vk;
            }
            SetDlgItemTextW(hwnd, 105, vkStr);
            SetWindowLongPtrW(hKeyEdit, GWLP_USERDATA, (LONG_PTR)vk);
        }
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDOK)
        {
            UINT newModifiers = 0;
            if (SendMessageW(GetDlgItem(hwnd, 100), BM_GETCHECK, 0, 0) == BST_CHECKED) newModifiers |= MOD_CONTROL;
            if (SendMessageW(GetDlgItem(hwnd, 101), BM_GETCHECK, 0, 0) == BST_CHECKED) newModifiers |= MOD_ALT;
            if (SendMessageW(GetDlgItem(hwnd, 102), BM_GETCHECK, 0, 0) == BST_CHECKED) newModifiers |= MOD_SHIFT;
            m_config.hotkeyModifiers = newModifiers;

            HWND hKeyEdit = GetDlgItem(hwnd, 105);
            LONG_PTR newVk = GetWindowLongPtrW(hKeyEdit, GWLP_USERDATA);
            if (newVk != 0)
                m_config.hotkeyVk = (UINT)newVk;

            m_config.autoStart = (SendMessageW(GetDlgItem(hwnd, 110), BM_GETCHECK, 0, 0) == BST_CHECKED);
            ToggleAutoStart(m_config.autoStart);
            UnregisterGlobalHotKey();
            RegisterGlobalHotKey();
            m_config.Save();
            CloseSettings();
            return 0;
        }
        else if (id == IDCANCEL)
        {
            CloseSettings();
            return 0;
        }
        else if (id == 120)
        {
            OpenCategoryManager();
            return 0;
        }
        else if (id == 121)
        {
            OpenGroupManager();
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        CloseSettings();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::ApplySettings()
{
    m_config.Save();
}

void MainWindow::ToggleAutoStart(bool enable)
{
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE | KEY_READ, &hKey);
    if (result == ERROR_SUCCESS)
    {
        if (enable)
        {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, L"直达", 0, REG_SZ, (const BYTE*)path, (DWORD)(wcslen(path) + 1) * sizeof(wchar_t));
        }
        else
        {
            RegDeleteValueW(hKey, L"直达");
        }
        RegCloseKey(hKey);
    }
}

void MainWindow::OpenHelp()
{
    if (m_helpHwnd && IsWindow(m_helpHwnd)) { SetForegroundWindow(m_helpHwnd); return; }
    CreateHelpWindow();
}

void MainWindow::CloseHelp()
{
    if (m_helpHwnd && IsWindow(m_helpHwnd)) { DestroyWindow(m_helpHwnd); m_helpHwnd = NULL; Show(); }
}

void MainWindow::CreateHelpWindow()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = HelpWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaHelpWindow";
    RegisterClassExW(&wcex);

    int helpWidth = 520;
    int helpHeight = 600;
    RECT rcMain;
    GetWindowRect(m_hwnd, &rcMain);
    int x = rcMain.left + (rcMain.right - rcMain.left - helpWidth) / 2;
    int y = rcMain.top + 40;

    m_helpHwnd = CreateWindowExW(0, L"ZhidaHelpWindow", L"帮助",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, helpWidth, helpHeight,
        m_hwnd, NULL, m_hInstance, this);

    if (m_helpHwnd)
    {
        ShowWindow(m_helpHwnd, SW_SHOW);
        SetForegroundWindow(m_helpHwnd);
        UpdateWindow(m_helpHwnd);
    }
}

LRESULT CALLBACK MainWindow::HelpWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleHelpMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleHelpMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

        Gdiplus::Graphics graphics(hdc);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

        Gdiplus::Font titleFont(GetSystemUIFont(), 22, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(50, 60, 80));
        Gdiplus::StringFormat titleFormat;
        titleFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF titleRect(0, 30, (Gdiplus::REAL)rc.right - rc.left, 40);
        graphics.DrawString(L"直达 - 使用帮助", -1, &titleFont, titleRect, &titleFormat, &titleBrush);

        Gdiplus::Font contentFont(GetSystemUIFont(), 13, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(60, 70, 80));
        Gdiplus::StringFormat textFormat;
        textFormat.SetAlignment(Gdiplus::StringAlignmentNear);
        textFormat.SetLineAlignment(Gdiplus::StringAlignmentNear);

        const wchar_t* versionText =
            L"软件版本：v1.0.0\n"
            L"更新日期：2026年5月\n"
            L"主要特性：\n"
            L"  • 支持快捷方式分类管理\n"
            L"  • 卡片组一键启动功能\n"
            L"  • 自定义全局快捷键\n"
            L"  • 开机自动启动\n\n";

        const wchar_t* helpText =
            L"快捷键使用：\n"
            L"  • 使用快捷键（默认 Alt+X）可快速显示/隐藏窗口\n\n"
            L"添加快捷方式：\n"
            L"  • 点击每个分类下的 \"+ 添加\" 按钮\n"
            L"  • 选择分类、浏览程序或文件夹\n\n"
            L"删除快捷方式：\n"
            L"  • 右键点击卡片，选择\"删除快捷方式\"\n\n"
            L"分类管理：\n"
            L"  • 在设置中点击\"管理分类\"\n"
            L"  • 可添加、删除分类，选择分类颜色\n\n"
            L"卡片组功能：\n"
            L"  • 在设置中点击\"管理快捷组\"\n"
            L"  • 创建包含多个快捷方式的组\n"
            L"  • 点击组名一键启动所有成员\n\n"
            L"滚动查看：\n"
            L"  • 使用鼠标滚轮或右侧滚动条查看更多\n\n";

        const wchar_t* privacyText =
            L"隐私说明：\n"
            L"  • 本软件仅在首次启动时收集一次用户IP地址信息\n"
            L"  • 该信息仅用于统计软件安装量及地域分布分析\n"
            L"  • 不会与用户个人身份信息关联\n"
            L"  • 不会用于其他任何目的";

        Gdiplus::Font versionFont(GetSystemUIFont(), 12, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush versionBrush(Gdiplus::Color(126, 87, 194));
        Gdiplus::RectF versionRect(50, 90, (Gdiplus::REAL)rc.right - rc.left - 100, 120);
        graphics.DrawString(versionText, -1, &versionFont, versionRect, &textFormat, &versionBrush);

        int privacyTop = rc.bottom - rc.top - 110;
        Gdiplus::RectF contentRect(50, 210, (Gdiplus::REAL)rc.right - rc.left - 100, (Gdiplus::REAL)(privacyTop - 220));
        graphics.DrawString(helpText, -1, &contentFont, contentRect, &textFormat, &textBrush);

        Gdiplus::Font privacyFont(GetSystemUIFont(), 12, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush privacyBrush(Gdiplus::Color(60, 140, 80));
        Gdiplus::RectF privacyRect(50, privacyTop, (Gdiplus::REAL)rc.right - rc.left - 100, 100);
        graphics.DrawString(privacyText, -1, &privacyFont, privacyRect, &textFormat, &privacyBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        CloseHelp();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::CreateAddShortcutWindow()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = AddShortcutWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaAddShortcutWindow";
    RegisterClassExW(&wcex);

    int winWidth = 500;
    int winHeight = 460;
    RECT rcMain;
    GetWindowRect(m_hwnd, &rcMain);
    int x = rcMain.left + (rcMain.right - rcMain.left - winWidth) / 2;
    int y = rcMain.top + (rcMain.bottom - rcMain.top - winHeight) / 2;

    m_addShortcutHwnd = CreateWindowExW(0, L"ZhidaAddShortcutWindow", L"添加快捷方式",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        m_hwnd, NULL, m_hInstance, this);

    if (m_addShortcutHwnd)
    {
        CreateWindowW(L"STATIC", L"快捷方式名称:", WS_CHILD | WS_VISIBLE, 40, 30, 150, 24, m_addShortcutHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 40, 58, 420, 32, m_addShortcutHwnd, (HMENU)101, m_hInstance, NULL);
        CreateWindowW(L"STATIC", L"路径:", WS_CHILD | WS_VISIBLE, 40, 105, 150, 24, m_addShortcutHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 40, 133, 280, 32, m_addShortcutHwnd, (HMENU)102, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"浏览程序", WS_CHILD | WS_VISIBLE, 330, 133, 130, 32, m_addShortcutHwnd, (HMENU)100, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"浏览文件夹", WS_CHILD | WS_VISIBLE, 330, 173, 130, 32, m_addShortcutHwnd, (HMENU)103, m_hInstance, NULL);

        CreateWindowW(L"STATIC", L"所属分类:", WS_CHILD | WS_VISIBLE, 40, 185, 100, 24, m_addShortcutHwnd, NULL, m_hInstance, NULL);
        HWND hCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 40, 213, 420, 300, m_addShortcutHwnd, (HMENU)104, m_hInstance, NULL);
        for (size_t i = 0; i < m_config.categories.size(); ++i)
        {
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)m_config.categories[i].name.c_str());
        }
        int selCat = m_selectedCategoryColorIndex;
        if (selCat >= 0 && selCat < (int)m_config.categories.size())
        {
            SendMessageW(hCombo, CB_SETCURSEL, selCat, 0);
        }
        else if (m_config.categories.size() > 0)
        {
            SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        }

        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE, 210, 360, 110, 40, m_addShortcutHwnd, (HMENU)IDOK, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 330, 360, 110, 40, m_addShortcutHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);

        SetDialogFont(m_addShortcutHwnd);

        ShowWindow(m_addShortcutHwnd, SW_SHOW);
        SetForegroundWindow(m_addShortcutHwnd);
        UpdateWindow(m_addShortcutHwnd);
    }
}

LRESULT CALLBACK MainWindow::AddShortcutWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleAddShortcutMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleAddShortcutMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t nameBuf[256];
            wchar_t pathBuf[512];
            GetDlgItemTextW(hwnd, 101, nameBuf, 256);
            GetDlgItemTextW(hwnd, 102, pathBuf, 512);
            int catIdx = (int)SendMessageW(GetDlgItem(hwnd, 104), CB_GETCURSEL, 0, 0);
            if (catIdx < 0) catIdx = 0;

            if (wcslen(nameBuf) > 0 && wcslen(pathBuf) > 0)
            {
                ShortcutItem newItem;
                newItem.name = nameBuf;
                newItem.path = pathBuf;
                newItem.type = L"app";
                newItem.colorIndex = catIdx;
                newItem.categoryIndex = catIdx;
                m_config.shortcuts.push_back(newItem);
                m_config.Save();
                RefreshUI();
            }
            DestroyWindow(hwnd);
            m_addShortcutHwnd = NULL;
            Show();
            return 0;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            m_addShortcutHwnd = NULL;
            Show();
            return 0;
        }
        else if (LOWORD(wParam) == 100)
        {
            OPENFILENAMEW ofn;
            wchar_t szFile[512];
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.lpstrFile[0] = L'\0';
            ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
            ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
            {
                SetDlgItemTextW(hwnd, 102, szFile);
                wchar_t* name = PathFindFileNameW(szFile);
                if (name)
                {
                    wchar_t temp[256];
                    wcscpy_s(temp, name);
                    wchar_t* dot = wcsrchr(temp, L'.');
                    if (dot) *dot = L'\0';
                    SetDlgItemTextW(hwnd, 101, temp);
                }
            }
        }
        else if (LOWORD(wParam) == 103)
        {
            BROWSEINFOW bi;
            wchar_t path[MAX_PATH] = { 0 };
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = hwnd;
            bi.pszDisplayName = path;
            bi.lpszTitle = L"请选择文件夹";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl != NULL)
            {
                if (SHGetPathFromIDListW(pidl, path))
                {
                    SetDlgItemTextW(hwnd, 102, path);
                    wchar_t* name = PathFindFileNameW(path);
                    if (name) SetDlgItemTextW(hwnd, 101, name);
                }
                CoTaskMemFree(pidl);
            }
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        m_addShortcutHwnd = NULL;
        Show();
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::OpenCategoryManager()
{
    if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd)) { SetForegroundWindow(m_categoryManagerHwnd); return; }
    CreateCategoryManagerWindow();
}

void MainWindow::CloseCategoryManager()
{
    if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd)) { DestroyWindow(m_categoryManagerHwnd); m_categoryManagerHwnd = NULL; }
}

void MainWindow::CreateCategoryManagerWindow()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = CategoryManagerWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaCategoryManagerWindow";
    RegisterClassExW(&wcex);

    int winWidth = 480;
    int winHeight = 420;
    RECT rcMain;
    GetWindowRect(m_hwnd, &rcMain);
    int x = rcMain.left + (rcMain.right - rcMain.left - winWidth) / 2;
    int y = rcMain.top + 50;

    m_categoryManagerHwnd = CreateWindowExW(0, L"ZhidaCategoryManagerWindow", L"管理分类",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        m_hwnd, NULL, m_hInstance, this);

    if (m_categoryManagerHwnd)
    {
        CreateWindowW(L"STATIC", L"已有分类:", WS_CHILD | WS_VISIBLE, 30, 20, 150, 24, m_categoryManagerHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 30, 50, 420, 220, m_categoryManagerHwnd, (HMENU)200, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"添加分类", WS_CHILD | WS_VISIBLE, 30, 290, 130, 36, m_categoryManagerHwnd, (HMENU)201, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"编辑分类", WS_CHILD | WS_VISIBLE, 170, 290, 130, 36, m_categoryManagerHwnd, (HMENU)203, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"删除分类", WS_CHILD | WS_VISIBLE, 310, 290, 130, 36, m_categoryManagerHwnd, (HMENU)202, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, 170, 340, 130, 36, m_categoryManagerHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);

        RefreshCategoryList(m_categoryManagerHwnd);

        SetDialogFont(m_categoryManagerHwnd);

        ShowWindow(m_categoryManagerHwnd, SW_SHOW);
        SetForegroundWindow(m_categoryManagerHwnd);
        UpdateWindow(m_categoryManagerHwnd);
    }
}

void MainWindow::RefreshCategoryList(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, 200);
    if (!hList) return;
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.categories.size(); ++i)
    {
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)m_config.categories[i].name.c_str());
    }
}

LRESULT CALLBACK MainWindow::CategoryManagerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleCategoryManagerMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleCategoryManagerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notifyCode = HIWORD(wParam);
        if (id == 200 && notifyCode == LBN_DBLCLK)
        {
            HWND hList = GetDlgItem(hwnd, 200);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.categories.size())
            {
                OpenEditCategoryDialog(sel);
            }
            return 0;
        }
        if (id == 201)
        {
            OpenAddCategoryDialog();
            return 0;
        }
        else if (id == 202)
        {
            HWND hList = GetDlgItem(hwnd, 200);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.categories.size())
            {
                int result = MessageBoxW(hwnd, L"确定要删除该分类吗？该分类下的快捷方式将移至第一个分类。", L"删除确认", MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES)
                {
                    for (size_t i = 0; i < m_config.shortcuts.size(); ++i)
                    {
                        if (m_config.shortcuts[i].categoryIndex == sel)
                            m_config.shortcuts[i].categoryIndex = 0;
                        else if (m_config.shortcuts[i].categoryIndex > sel)
                            m_config.shortcuts[i].categoryIndex--;
                    }
                    m_config.categories.erase(m_config.categories.begin() + sel);
                    m_config.Save();
                    RefreshCategoryList(hwnd);
                    RefreshUI();
                }
            }
            return 0;
        }
        else if (id == 203)
        {
            HWND hList = GetDlgItem(hwnd, 200);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.categories.size())
            {
                OpenEditCategoryDialog(sel);
            }
            return 0;
        }
        else if (id == IDCANCEL)
        {
            CloseCategoryManager();
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        CloseCategoryManager();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::OpenAddCategoryDialog()
{
    m_editCategoryIndex = -1;
    OpenAddCategoryDialogInternal();
}

void MainWindow::OpenEditCategoryDialog(int editIndex)
{
    m_editCategoryIndex = editIndex;
    OpenAddCategoryDialogInternal();
}

void MainWindow::OpenAddCategoryDialogInternal()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = AddCategoryWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaAddCategoryWindow";
    RegisterClassExW(&wcex);

    int winWidth = 400;
    int winHeight = 320;
    RECT rcMgr;
    if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd))
        GetWindowRect(m_categoryManagerHwnd, &rcMgr);
    else
        GetWindowRect(m_hwnd, &rcMgr);
    int x = rcMgr.left + (rcMgr.right - rcMgr.left - winWidth) / 2;
    int y = rcMgr.top + (rcMgr.bottom - rcMgr.top - winHeight) / 2;

    m_selectedCategoryColorIndex = 0;
    const wchar_t* dialogTitle = L"添加分类";
    if (m_editCategoryIndex >= 0 && m_editCategoryIndex < (int)m_config.categories.size())
    {
        dialogTitle = L"编辑分类";
        for (int i = 0; i < 12; i++)
        {
            if (Config::PresetColors[i] == m_config.categories[m_editCategoryIndex].color)
            {
                m_selectedCategoryColorIndex = i;
                break;
            }
        }
    }

    m_addCategoryHwnd = CreateWindowExW(0, L"ZhidaAddCategoryWindow", dialogTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        m_categoryManagerHwnd, NULL, m_hInstance, this);

    if (m_addCategoryHwnd)
    {
        CreateWindowW(L"STATIC", L"分类名称:", WS_CHILD | WS_VISIBLE, 30, 30, 120, 24, m_addCategoryHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 30, 58, 340, 32, m_addCategoryHwnd, (HMENU)300, m_hInstance, NULL);
        CreateWindowW(L"STATIC", L"选择颜色:", WS_CHILD | WS_VISIBLE, 30, 110, 120, 24, m_addCategoryHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE, 120, 240, 110, 40, m_addCategoryHwnd, (HMENU)IDOK, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 240, 240, 110, 40, m_addCategoryHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);

        if (m_editCategoryIndex >= 0 && m_editCategoryIndex < (int)m_config.categories.size())
        {
            SetDlgItemTextW(m_addCategoryHwnd, 300, m_config.categories[m_editCategoryIndex].name.c_str());
        }

        SetDialogFont(m_addCategoryHwnd);

        ShowWindow(m_addCategoryHwnd, SW_SHOW);
        SetForegroundWindow(m_addCategoryHwnd);
        UpdateWindow(m_addCategoryHwnd);
    }
}

LRESULT CALLBACK MainWindow::AddCategoryWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleAddCategoryMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleAddCategoryMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        int startX = 30;
        int startY = 140;
        int size = 28;
        int gap = 6;

        for (int i = 0; i < 12; i++)
        {
            int cx = startX + (i % 6) * (size + gap);
            int cy = startY + (i / 6) * (size + gap);

            Gdiplus::Color color = ColorRefToGdiplus(Config::PresetColors[i]);
            Gdiplus::SolidBrush brush(color);
            graphics.FillEllipse(&brush, (Gdiplus::REAL)cx, (Gdiplus::REAL)cy, (Gdiplus::REAL)size, (Gdiplus::REAL)size);

            if (i == m_selectedCategoryColorIndex)
            {
                Gdiplus::Pen selPen(Gdiplus::Color(40, 40, 40), 3.0f);
                graphics.DrawEllipse(&selPen, (Gdiplus::REAL)cx - 2, (Gdiplus::REAL)cy - 2, (Gdiplus::REAL)(size + 4), (Gdiplus::REAL)(size + 4));
            }
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);
        int startX = 30;
        int startY = 140;
        int size = 28;
        int gap = 6;

        for (int i = 0; i < 12; i++)
        {
            int cx = startX + (i % 6) * (size + gap);
            int cy = startY + (i / 6) * (size + gap);
            if (mx >= cx && mx < cx + size && my >= cy && my < cy + size)
            {
                m_selectedCategoryColorIndex = i;
                InvalidateRect(hwnd, NULL, TRUE);
                break;
            }
        }
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDOK)
        {
            wchar_t nameBuf[256];
            GetDlgItemTextW(hwnd, 300, nameBuf, 256);
            if (wcslen(nameBuf) > 0)
            {
                if (m_editCategoryIndex >= 0 && m_editCategoryIndex < (int)m_config.categories.size())
                {
                    m_config.categories[m_editCategoryIndex].name = nameBuf;
                    m_config.categories[m_editCategoryIndex].color = Config::PresetColors[m_selectedCategoryColorIndex];
                }
                else
                {
                    Category newCat;
                    newCat.name = nameBuf;
                    newCat.color = Config::PresetColors[m_selectedCategoryColorIndex];
                    m_config.categories.push_back(newCat);
                }
                m_config.Save();
                if (m_categoryManagerHwnd && IsWindow(m_categoryManagerHwnd))
                    RefreshCategoryList(m_categoryManagerHwnd);
                RefreshUI();
            }
            DestroyWindow(hwnd);
            m_addCategoryHwnd = NULL;
            return 0;
        }
        else if (id == IDCANCEL)
        {
            DestroyWindow(hwnd);
            m_addCategoryHwnd = NULL;
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        m_addCategoryHwnd = NULL;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::OpenGroupManager()
{
    if (m_groupManagerHwnd && IsWindow(m_groupManagerHwnd)) { SetForegroundWindow(m_groupManagerHwnd); return; }
    CreateGroupManagerWindow();
}

void MainWindow::CloseGroupManager()
{
    if (m_groupManagerHwnd && IsWindow(m_groupManagerHwnd)) { DestroyWindow(m_groupManagerHwnd); m_groupManagerHwnd = NULL; }
}

void MainWindow::CreateGroupManagerWindow()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = GroupManagerWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaGroupManagerWindow";
    RegisterClassExW(&wcex);

    int winWidth = 520;
    int winHeight = 460;
    RECT rcMain;
    GetWindowRect(m_hwnd, &rcMain);
    int x = rcMain.left + (rcMain.right - rcMain.left - winWidth) / 2;
    int y = rcMain.top + 50;

    m_groupManagerHwnd = CreateWindowExW(0, L"ZhidaGroupManagerWindow", L"管理快捷组",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        m_hwnd, NULL, m_hInstance, this);

    if (m_groupManagerHwnd)
    {
        CreateWindowW(L"STATIC", L"已有快捷组:", WS_CHILD | WS_VISIBLE, 30, 20, 150, 24, m_groupManagerHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 30, 50, 460, 220, m_groupManagerHwnd, (HMENU)300, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"添加快捷组", WS_CHILD | WS_VISIBLE, 30, 290, 130, 36, m_groupManagerHwnd, (HMENU)301, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"编辑快捷组", WS_CHILD | WS_VISIBLE, 170, 290, 130, 36, m_groupManagerHwnd, (HMENU)303, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"删除快捷组", WS_CHILD | WS_VISIBLE, 310, 290, 130, 36, m_groupManagerHwnd, (HMENU)302, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, 190, 340, 130, 36, m_groupManagerHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);

        RefreshGroupList(m_groupManagerHwnd);

        SetDialogFont(m_groupManagerHwnd);

        ShowWindow(m_groupManagerHwnd, SW_SHOW);
        SetForegroundWindow(m_groupManagerHwnd);
        UpdateWindow(m_groupManagerHwnd);
    }
}

void MainWindow::RefreshGroupList(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, 300);
    if (!hList) return;
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.groups.size(); ++i)
    {
        const auto& group = m_config.groups[i];
        std::wstring displayName = group.name + L" (" + std::to_wstring(group.shortcutIndices.size()) + L" 个快捷方式)";
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)displayName.c_str());
    }
}

LRESULT CALLBACK MainWindow::GroupManagerWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleGroupManagerMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleGroupManagerMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notifyCode = HIWORD(wParam);
        if (id == 300 && notifyCode == LBN_DBLCLK)
        {
            HWND hList = GetDlgItem(hwnd, 300);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.groups.size())
            {
                OpenEditGroupDialog(sel);
            }
            return 0;
        }
        if (id == 301)
        {
            OpenAddGroupDialog();
            return 0;
        }
        else if (id == 302)
        {
            HWND hList = GetDlgItem(hwnd, 300);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.groups.size())
            {
                int result = MessageBoxW(hwnd, L"确定要删除该快捷组吗？", L"删除确认", MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES)
                {
                    m_config.groups.erase(m_config.groups.begin() + sel);
                    m_config.Save();
                    RefreshGroupList(hwnd);
                    RefreshUI();
                }
            }
            return 0;
        }
        else if (id == 303)
        {
            HWND hList = GetDlgItem(hwnd, 300);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)m_config.groups.size())
            {
                OpenEditGroupDialog(sel);
            }
            return 0;
        }
        else if (id == IDCANCEL)
        {
            CloseGroupManager();
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        CloseGroupManager();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void MainWindow::OpenAddGroupDialog()
{
    m_editGroupIndex = -1;
    OpenAddGroupDialogInternal();
}

void MainWindow::OpenEditGroupDialog(int editIndex)
{
    m_editGroupIndex = editIndex;
    OpenAddGroupDialogInternal();
}

void MainWindow::OpenAddGroupDialogInternal()
{
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = AddGroupWindowProc;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wcex.lpszClassName = L"ZhidaAddGroupWindow";
    RegisterClassExW(&wcex);

    int winWidth = 460;
    int winHeight = 440;
    RECT rcMgr;
    if (m_groupManagerHwnd && IsWindow(m_groupManagerHwnd))
    {
        GetWindowRect(m_groupManagerHwnd, &rcMgr);
    }
    else
    {
        GetWindowRect(m_hwnd, &rcMgr);
    }
    int x = rcMgr.left + (rcMgr.right - rcMgr.left - winWidth) / 2;
    int y = rcMgr.top + (rcMgr.bottom - rcMgr.top - winHeight) / 2;

    const wchar_t* dialogTitle = L"添加快捷组";
    if (m_editGroupIndex >= 0 && m_editGroupIndex < (int)m_config.groups.size())
    {
        dialogTitle = L"编辑快捷组";
    }

    m_addGroupHwnd = CreateWindowExW(0, L"ZhidaAddGroupWindow", dialogTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, winWidth, winHeight,
        m_groupManagerHwnd, NULL, m_hInstance, this);

    if (m_addGroupHwnd)
    {
        CreateWindowW(L"STATIC", L"组名称:", WS_CHILD | WS_VISIBLE, 30, 30, 120, 24, m_addGroupHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 30, 58, 400, 32, m_addGroupHwnd, (HMENU)400, m_hInstance, NULL);
        CreateWindowW(L"STATIC", L"选择成员（可多选）:", WS_CHILD | WS_VISIBLE, 30, 110, 200, 24, m_addGroupHwnd, NULL, m_hInstance, NULL);
        CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_MULTIPLESEL | LBS_NOTIFY, 30, 140, 400, 200, m_addGroupHwnd, (HMENU)401, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE, 140, 360, 110, 40, m_addGroupHwnd, (HMENU)IDOK, m_hInstance, NULL);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 270, 360, 110, 40, m_addGroupHwnd, (HMENU)IDCANCEL, m_hInstance, NULL);

        HWND hList = GetDlgItem(m_addGroupHwnd, 401);
        for (size_t i = 0; i < m_config.shortcuts.size(); ++i)
        {
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)m_config.shortcuts[i].name.c_str());
        }

        if (m_editGroupIndex >= 0 && m_editGroupIndex < (int)m_config.groups.size())
        {
            SetDlgItemTextW(m_addGroupHwnd, 400, m_config.groups[m_editGroupIndex].name.c_str());
            const auto& group = m_config.groups[m_editGroupIndex];
            for (size_t idx : group.shortcutIndices)
            {
                SendMessageW(hList, LB_SETSEL, TRUE, (LPARAM)idx);
            }
        }

        SetDialogFont(m_addGroupHwnd);

        ShowWindow(m_addGroupHwnd, SW_SHOW);
        SetForegroundWindow(m_addGroupHwnd);
        UpdateWindow(m_addGroupHwnd);
    }
}

LRESULT CALLBACK MainWindow::AddGroupWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        MainWindow* pThis = static_cast<MainWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (pThis) return pThis->HandleAddGroupMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleAddGroupMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
        return HandleCtlColor(wParam);
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t nameBuf[256];
            GetDlgItemTextW(hwnd, 400, nameBuf, 256);

            if (wcslen(nameBuf) == 0)
            {
                MessageBoxW(hwnd, L"请输入组名称", L"提示", MB_OK | MB_ICONWARNING);
                return 0;
            }

            HWND hList = GetDlgItem(hwnd, 401);
            int count = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);

            CardGroup group;
            group.name = nameBuf;

            for (int i = 0; i < count; ++i)
            {
                if (SendMessageW(hList, LB_GETSEL, i, 0))
                {
                    group.shortcutIndices.push_back((size_t)i);
                }
            }

            if (m_editGroupIndex >= 0 && m_editGroupIndex < (int)m_config.groups.size())
            {
                m_config.groups[m_editGroupIndex] = group;
            }
            else
            {
                m_config.groups.push_back(group);
            }

            m_config.Save();

            if (m_groupManagerHwnd && IsWindow(m_groupManagerHwnd))
            {
                RefreshGroupList(m_groupManagerHwnd);
            }
            RefreshUI();

            DestroyWindow(hwnd);
            m_addGroupHwnd = NULL;
            return 0;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            m_addGroupHwnd = NULL;
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        m_addGroupHwnd = NULL;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}
