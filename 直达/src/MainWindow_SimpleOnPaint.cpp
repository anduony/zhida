void MainWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // 填充背景
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));

    // 画文字 - 使用最简单的方式
    HFONT hFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    DrawTextW(hdc, L"Zhida", -1, &rc, DT_CENTER | DT_TOP);

    // 画一些简单的卡片
    RECT cardRc;
    for (int i = 0; i < 6; ++i)
    {
        int col = i % 4;
        int row = i / 4;
        int x = 16 + col * (136 + 12);
        int y = 60 + row * (64 + 12);
        cardRc.left = x;
        cardRc.top = y;
        cardRc.right = x + 136;
        cardRc.bottom = y + 64;

        FillRect(hdc, &cardRc, GetSysColorBrush(COLOR_WINDOW));
        
        // 左边的彩色条
        RECT barRc = cardRc;
        barRc.right = barRc.left + 4;
        FillRect(hdc, &barRc, (HBRUSH)(COLOR_HIGHLIGHT + 1));

        // 文字
        RECT textRc = cardRc;
        textRc.left += 16;
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        const wchar_t* names[] = {L"My Computer", L"My Documents", L"Calculator", L"Notepad", L"Paint", L"Control Panel"};
        DrawTextW(hdc, names[i], -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 清理
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    EndPaint(m_hwnd, &ps);
}
