#!/usr/bin/env python3
# Applies custom skin to window.h:
# - Borderless WS_POPUP window
# - Custom blue title bar with app icon, title, min/close buttons
# - CS_DROPSHADOW for shadow
# - WM_NCHITTEST for drag
# - WM_MOUSEMOVE/LBUTTONDOWN for button hover/click
import re, sys

path = '/Users/leeson/Desktop/ProcessGuardCpp/src/window.h'
with open(path, 'r', encoding='utf-8') as f:
    src = f.read()

# ── 1. Add TITLEBAR_H constant after COLS ────────────────────────
src = src.replace(
    'static const int COLS     =   3;\n',
    'static const int COLS     =   3;\nstatic const int TITLEBAR_H=  32;   // custom title bar\n'
)

# ── 2. Add title bar globals after animation state block ─────────
old2 = ('static int g_hoverAlpha  = 0;    // 0-255: hover border blend progress\n'
        'static int g_cbFlash     = -1;   // real proc index being flashed (-1=none)\n'
        'static int g_cbFlashTick = 0;    // countdown frames for flash (0=done)\n')
new2 = (old2 +
        '\n// -- Title bar state -----------------------------------------------\n'
        'static RECT g_btnMin     = {};\n'
        'static RECT g_btnClose   = {};\n'
        'static int  g_titleHover = 0;  // 0=none, 1=min, 2=close\n')
src = src.replace(old2, new2)

# ── 3. Add WM_NCHITTEST + title button handlers before WM_CREATE ─
old3 = ('LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {\n'
        '    switch(msg) {\n'
        '    case WM_CREATE: {\n')
new3 = ('LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {\n'
        '    switch(msg) {\n'
        '    case WM_NCHITTEST: {\n'
        '        LRESULT hit=DefWindowProcW(hw,msg,wp,lp);\n'
        '        if(hit==HTCLIENT){\n'
        '            POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};\n'
        '            ScreenToClient(hw,&pt);\n'
        '            if(pt.y<TITLEBAR_H){\n'
        '                if(PtInRect(&g_btnClose,pt)||PtInRect(&g_btnMin,pt)) return HTCLIENT;\n'
        '                return HTCAPTION;\n'
        '            }\n'
        '        }\n'
        '        return hit;\n'
        '    }\n'
        '    case WM_MOUSEMOVE: {\n'
        '        POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};\n'
        '        int prev=g_titleHover;\n'
        '        if(pt.y<TITLEBAR_H){\n'
        '            g_titleHover=PtInRect(&g_btnClose,pt)?2:PtInRect(&g_btnMin,pt)?1:0;\n'
        '            TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hw,0};\n'
        '            TrackMouseEvent(&tme);\n'
        '        } else { g_titleHover=0; }\n'
        '        if(g_titleHover!=prev) InvalidateRect(hw,nullptr,FALSE);\n'
        '        return DefWindowProcW(hw,msg,wp,lp);\n'
        '    }\n'
        '    case WM_MOUSELEAVE:\n'
        '        if(g_titleHover){g_titleHover=0;InvalidateRect(hw,nullptr,FALSE);}\n'
        '        return 0;\n'
        '    case WM_LBUTTONDOWN: {\n'
        '        POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};\n'
        '        if(PtInRect(&g_btnClose,pt)){SendMessageW(hw,WM_CLOSE,0,0);return 0;}\n'
        '        if(PtInRect(&g_btnMin,  pt)){ShowWindow(hw,SW_MINIMIZE);   return 0;}\n'
        '        return DefWindowProcW(hw,msg,wp,lp);\n'
        '    }\n'
        '    case WM_CREATE: {\n')
src = src.replace(old3, new3)

# ── 4. Update WM_SIZE: controls Y and card area top ──────────────
src = src.replace(
    '        int pad=12,bh=30,bY=(TOOLBAR_H-bh)/2,bw=110,sbW=180;\n',
    '        int pad=12,bh=30,bY=TITLEBAR_H+(TOOLBAR_H-bh)/2,bw=110,sbW=180;\n'
)
src = src.replace(
    '        SetWindowPos(g_cardArea,nullptr,0,TOOLBAR_H,W,H-TOOLBAR_H,SWP_NOZORDER);\n',
    '        SetWindowPos(g_cardArea,nullptr,0,TITLEBAR_H+TOOLBAR_H,W,H-TITLEBAR_H-TOOLBAR_H,SWP_NOZORDER);\n'
)

# ── 5. Rewrite WM_PAINT ──────────────────────────────────────────
# Find WM_PAINT block and replace it entirely
paint_start = src.find('    case WM_PAINT: {\n        PAINTSTRUCT ps; HDC hdc=BeginPaint(hw,&ps);\n        RECT rc; GetClientRect(hw,&rc);\n        RECT tb=')
paint_end   = src.find('\n    case WM_ERASEBKGND:', paint_start)
assert paint_start != -1 and paint_end != -1, 'WM_PAINT block not found'

new_paint = r'''    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc=BeginPaint(hw,&ps);
        RECT rc; GetClientRect(hw,&rc);
        SetBkMode(hdc,TRANSPARENT);

        // ── Title bar (accent color) ──────────────────────────────────
        RECT tbr={0,0,rc.right,TITLEBAR_H};
        HBRUSH tbb=CreateSolidBrush(CLR_ACCENT);
        FillRect(hdc,&tbr,tbb); DeleteObject(tbb);

        // App icon 16px
        HICON appIco=(HICON)LoadImageW(GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(101),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
        if(appIco) DrawIconEx(hdc,8,(TITLEBAR_H-16)/2,appIco,16,16,0,nullptr,DI_NORMAL);

        // Title text (white)
        SelectObject(hdc,g_fntTitle);
        SetTextColor(hdc,RGB(255,255,255));
        RECT titleR={32,0,rc.right-68,TITLEBAR_H};
        DrawTextW(hdc,L"ProcessGuard \u00B7 \u8FDB\u7A0B\u5B88\u62A4",-1,&titleR,
                  DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);

        // Minimize button
        g_btnMin={rc.right-64,0,rc.right-32,TITLEBAR_H};
        if(g_titleHover==1){
            HBRUSH hb=CreateSolidBrush(RGB(80,140,255));
            FillRect(hdc,&g_btnMin,hb); DeleteObject(hb);
        }
        { HPEN pen=CreatePen(PS_SOLID,2,RGB(255,255,255));
          HPEN op=(HPEN)SelectObject(hdc,pen);
          int my=TITLEBAR_H/2;
          MoveToEx(hdc,g_btnMin.left+10,my,nullptr); LineTo(hdc,g_btnMin.right-10,my);
          SelectObject(hdc,op); DeleteObject(pen); }

        // Close button
        g_btnClose={rc.right-32,0,rc.right,TITLEBAR_H};
        { HBRUSH hb=CreateSolidBrush(g_titleHover==2?RGB(220,38,38):CLR_ACCENT);
          FillRect(hdc,&g_btnClose,hb); DeleteObject(hb); }
        { HPEN pen=CreatePen(PS_SOLID,2,RGB(255,255,255));
          HPEN op=(HPEN)SelectObject(hdc,pen);
          MoveToEx(hdc,g_btnClose.left+9, 7,           nullptr);
          LineTo  (hdc,g_btnClose.right-9,TITLEBAR_H-7);
          MoveToEx(hdc,g_btnClose.right-9,7,           nullptr);
          LineTo  (hdc,g_btnClose.left+9, TITLEBAR_H-7);
          SelectObject(hdc,op); DeleteObject(pen); }

        // ── Controls bar (white) ──────────────────────────────────────
        RECT ctrl={0,TITLEBAR_H,rc.right,TITLEBAR_H+TOOLBAR_H};
        HBRUSH ctb=CreateSolidBrush(CLR_HEADER_BG);
        FillRect(hdc,&ctrl,ctb); DeleteObject(ctb);
        HPEN sep=CreatePen(PS_SOLID,1,CLR_BORDER);
        HPEN osep=(HPEN)SelectObject(hdc,sep);
        MoveToEx(hdc,0,TITLEBAR_H+TOOLBAR_H-1,nullptr);
        LineTo  (hdc,rc.right,TITLEBAR_H+TOOLBAR_H-1);
        SelectObject(hdc,osep); DeleteObject(sep);

        // Status text centred in controls bar
        int n=g_engine?(int)g_engine->procs.size():0;
        wchar_t info[64];
        swprintf_s(info,L"\u5B88\u62A4\u4E2D \u00B7 %d \u4E2A\u8FDB\u7A0B",n);
        SelectObject(hdc,g_fntMeta);
        SetTextColor(hdc,n>0?CLR_GREEN:CLR_GRAY);
        RECT ir={130,TITLEBAR_H,rc.right-200,TITLEBAR_H+TOOLBAR_H};
        DrawTextW(hdc,info,-1,&ir,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);

        EndPaint(hw,&ps); return 0;
    }'''

src = src[:paint_start] + new_paint + src[paint_end:]

# ── 6. Remove WM_GETMINMAXINFO (not needed for WS_POPUP) ─────────
minmax_start = src.find('    case WM_GETMINMAXINFO: {\n')
minmax_end   = src.find('\n    case WM_CLOSE:', minmax_start)
if minmax_start != -1 and minmax_end != -1:
    src = src[:minmax_start] + src[minmax_end+1:]  # keep the newline

# ── 7. Update window class style + creation ───────────────────────
src = src.replace(
    '    wc.style=CS_HREDRAW|CS_VREDRAW;\n',
    '    wc.style=CS_HREDRAW|CS_VREDRAW|CS_DROPSHADOW;\n'
)
src = src.replace(
    '    HWND hw=CreateWindowExW(WS_EX_ACCEPTFILES,L"ProcessGuardWnd",\n'
    '        L"ProcessGuard \\u00B7 \\u8FDB\\u7A0B\\u5B88\\u62A4",\n'
    '        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,\n'
    '        CW_USEDEFAULT,CW_USEDEFAULT,660,460,\n',
    '    HWND hw=CreateWindowExW(WS_EX_ACCEPTFILES|WS_EX_APPWINDOW,L"ProcessGuardWnd",\n'
    '        L"ProcessGuard \\u00B7 \\u8FDB\\u7A0B\\u5B88\\u62A4",\n'
    '        WS_POPUP|WS_SYSMENU,\n'
    '        CW_USEDEFAULT,CW_USEDEFAULT,660,496,\n'  # 460+TITLEBAR_H(32)+4
)

with open(path, 'w', encoding='utf-8') as f:
    f.write(src)
print('Done')
