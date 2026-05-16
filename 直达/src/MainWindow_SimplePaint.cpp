void MainWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // 1. 使用 GDI 先绘制背景
    const ThemeColors& theme = Config::Themes[m_config.themeId];
    HBRUSH bgBrush = CreateSolidBrush(theme.windowBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // 2. 创建内存 DC - 用于 GDI+
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    // 拷贝背景
    BitBlt(memDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

    // 3. 使用 GDI+ 绘制
    Gdiplus::Graphics graphics(memDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 绘制标题
    Gdiplus::SolidBrush titleBrush(ColorRefToGdiplus(theme.titleText));
    Gdiplus::FontFamily fontFamily(L"Microsoft YaHei");
    Gdiplus::Font font(&fontFamily, 20, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    Gdiplus::RectF titleRect(0.0f, 8.0f, (Gdiplus::REAL)width, 40.0f);
    graphics.DrawString(L"Zhida", -1, &font, titleRect, &format, &titleBrush);

    // 绘制卡片
    for (size_t i = 0; i < m_cardRects.size(); ++i)
    {
        bool hovered = (m_cardRects[i].index == m_hoveredCard);
        DrawCard(graphics, m_cardRects[i], hovered);
    }

    // 4. 复制到屏幕
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // 5. 清理
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(m_hwnd, &ps);
}
