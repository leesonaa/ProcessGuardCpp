#pragma once
#include "common.h"
#include "engine.h"
#include <map>

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CardAreaProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

// ── Globals ───────────────────────────────────────────────────────
static GuardEngine*   g_engine   = nullptr;
static HWND           g_hwnd     = nullptr;
static HWND           g_cardArea = nullptr;
static HFONT          g_fntTitle = nullptr;
static HFONT          g_fntName  = nullptr;
static HFONT          g_fntMeta  = nullptr;
static HFONT          g_fntStat  = nullptr;
static HFONT          g_fntIcon  = nullptr;
static NOTIFYICONDATA g_nid      = {};
static HWND           g_searchBox = nullptr;
static std::map<std::wstring, HICON> g_iconCache;
static wchar_t g_filter[256] = {};

static const int CARD_H   = 110;
static const int CARD_PAD =  12;
static const int GPADX    =  12;   // grid pad X
static const int GPADY    =  12;   // grid pad Y
static const int TOOLBAR_H=  52;
static const int COLS     =   3;
static const int TITLEBAR_H=  32;   // custom title bar

static int g_scrollY  = 0;
static int g_totalH   = 0;
static int g_hoverCard= -1;

// ── Animation state ───────────────────────────────────────────────
static int g_hoverAlpha  = 0;    // 0-255: hover border blend progress
static int g_cbFlash     = -1;   // real proc index being flashed (-1=none)
static int g_cbFlashTick = 0;    // countdown frames for flash (0=done)

// -- Title bar state -----------------------------------------------
static RECT g_btnMin     = {};
static RECT g_btnClose   = {};
static int  g_titleHover = 0;  // 0=none, 1=min, 2=close

// ── Font factory ─────────────────────────────────────────────────
inline HFONT MkFont(int sz, int w, const wchar_t* f) {
    return CreateFontW(-sz,0,0,0,w,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE,f);
}

inline void ApplyDark(HWND h) {
    typedef HRESULT(WINAPI*PF)(HWND,DWORD,LPCVOID,DWORD);
    static PF fn=(PF)GetProcAddress(LoadLibraryW(L"dwmapi.dll"),"DwmSetWindowAttribute");
    if(fn){BOOL d=TRUE;fn(h,20,&d,sizeof(d));}
}

// ── Card geometry ─────────────────────────────────────────────────
inline int CardW(int W) {
    return std::max(160, (W - GPADX*2 - CARD_PAD*(COLS-1)) / COLS);
}
inline int CardCols(int) { return COLS; }
inline RECT CardRect(int i, int W) {
    int col=i%COLS, row=i/COLS;
    int cw=CardW(W);
    int x=GPADX+col*(cw+CARD_PAD);
    int y=GPADY+row*(CARD_H+CARD_PAD)-g_scrollY;
    return {x,y,x+cw,y+CARD_H};
}
inline int ContentH(int n, int W) {
    int rows=(n+COLS-1)/COLS;
    return GPADY*2+rows*(CARD_H+CARD_PAD)-CARD_PAD;
}
inline void RefreshScroll(HWND hw, int n) {
    RECT rc; GetClientRect(hw,&rc);
    g_totalH=ContentH(n,rc.right);
    SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS};
    si.nMin=0; si.nMax=std::max(0,g_totalH-1);
    si.nPage=rc.bottom; si.nPos=g_scrollY;
    SetScrollInfo(hw,SB_VERT,&si,TRUE);
}

// ── Resolve .lnk shortcut to target exe ─────────────────────────
inline std::wstring ResolveLnk(const std::wstring& lnkPath) {
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&psl))) return {};
    std::wstring result;
    IPersistFile* ppf = nullptr;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
        if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
            wchar_t target[MAX_PATH] = {};
            WIN32_FIND_DATAW wfd{};
            if (SUCCEEDED(psl->GetPath(target, MAX_PATH, &wfd, SLGP_UNCPRIORITY)))
                result = target;
        }
        ppf->Release();
    }
    psl->Release();
    return result;
}

// Returns the guarded exe path from a dropped file (.exe or .lnk)
inline std::wstring GetExeFromDrop(const wchar_t* path) {
    std::wstring s(path);
    auto ends = [&](const wchar_t* ext) {
        return s.size() >= 4 && _wcsicmp(s.c_str() + s.size() - 4, ext) == 0;
    };
    if (ends(L".exe")) return s;
    if (ends(L".lnk")) {
        std::wstring t = ResolveLnk(s);
        if (t.size() >= 4 && _wcsicmp(t.c_str() + t.size() - 4, L".exe") == 0)
            return t;
    }
    return {};
}

// ── Exe icon cache (32x32 direct from shell, pixel-perfect) ─────────────────
inline HICON GetProcessIcon(const std::wstring& exePath) {
    auto it = g_iconCache.find(exePath);
    if (it != g_iconCache.end()) return it->second;
    HICON hIcon = nullptr;
    // Primary: SHGetFileInfo LARGEICON = 32x32, exact match, no stretching
    SHFILEINFOW sfi{};
    if (SHGetFileInfoW(exePath.c_str(), 0, &sfi, sizeof(sfi),
                       SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon)
        hIcon = sfi.hIcon;
    // Fallback: ExtractIconExW
    if (!hIcon) {
        ExtractIconExW(exePath.c_str(), 0, &hIcon, nullptr, 1);
    }
    g_iconCache[exePath] = hIcon;
    return hIcon;
}

// ── Per-exe fallback color ────────────────────────────────────────
inline COLORREF ExeClr(const std::wstring& nm) {
    static const COLORREF p[]={
        RGB(79,142,247),RGB(167,91,247),RGB(34,197,94),
        RGB(249,115,22),RGB(239,68,68), RGB(20,184,166),
        RGB(234,179,8), RGB(236,72,153),RGB(99,102,241),
    };
    size_t h=0; for(wchar_t c:nm) h=h*31+c;
    return p[h%(sizeof(p)/sizeof(p[0]))];
}

// ── Color lerp helper ─────────────────────────────────────────────
inline COLORREF LerpClr(COLORREF a, COLORREF b, int t255) {
    int r=GetRValue(a)+(GetRValue(b)-GetRValue(a))*t255/255;
    int g=GetGValue(a)+(GetGValue(b)-GetGValue(a))*t255/255;
    int bv=GetBValue(a)+(GetBValue(b)-GetBValue(a))*t255/255;
    return RGB(r,g,bv);
}

// ── Draw rounded rect helper ──────────────────────────────────────
inline void RR(HDC dc,RECT r,int rx,COLORREF fill,COLORREF border,int bw=1) {
    HBRUSH br=CreateSolidBrush(fill);
    HPEN   pn=CreatePen(PS_SOLID,bw,border);
    HBRUSH ob=(HBRUSH)SelectObject(dc,br);
    HPEN   op=(HPEN)SelectObject(dc,pn);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,rx,rx);
    SelectObject(dc,ob); SelectObject(dc,op);
    DeleteObject(br); DeleteObject(pn);
}

// ── Draw one card (light flat style) ─────────────────────────────
inline void DrawCard(HDC dc, RECT r, const GuardedProcess* p, bool hover, int idx, int realIdx) {
    // ── Card shadow (offset rect, semi-transparent gray) ──────────
    RECT shadow={r.left+3,r.top+3,r.right+3,r.bottom+3};
    RR(dc,shadow,10,RGB(200,205,220),RGB(200,205,220),1);

    // ── Card background: green=guarding, red=not guarding ────────
    bool guarding = p->enabled;
    bool running  = (p->status == ProcStatus::Running);
    COLORREF baseBd = guarding ? (running ? RGB(22,163,74) : RGB(74,222,128))
                                : RGB(220,38,38);
    // Smooth hover blend: interpolate from baseBd → CLR_ACCENT
    COLORREF bd = hover ? LerpClr(baseBd, CLR_ACCENT, g_hoverAlpha)
                        : LerpClr(CLR_ACCENT, baseBd, 255-g_hoverAlpha);
    // If not this card being hovered, just use base color
    if (!hover && g_hoverCard >= 0) bd = baseBd;
    int bw = (g_hoverAlpha > 64 && hover) ? 2 : 1;
    COLORREF bg = guarding ? RGB(236,253,245) : RGB(254,242,242);
    RR(dc, r, 10, bg, bd, bw);

    SetBkMode(dc,TRANSPARENT);
    int px=r.left+12, py=r.top+12;

    // ── Left accent bar: green=guarding, red=not guarding ──────────
    COLORREF barClr = guarding ? (running ? RGB(22,163,74) : RGB(74,222,128)) : RGB(220,38,38);
    RECT bar={r.left,r.top+12,r.left+4,r.bottom-12};
    // clip to card rounded edge by drawing inside
    HBRUSH barBr=CreateSolidBrush(barClr);
    HPEN   barPn=CreatePen(PS_NULL,0,0);
    HBRUSH ob2=(HBRUSH)SelectObject(dc,barBr);
    HPEN   op2=(HPEN)SelectObject(dc,barPn);
    RoundRect(dc,bar.left+1,bar.top,bar.right,bar.bottom,3,3);
    SelectObject(dc,ob2); SelectObject(dc,op2);
    DeleteObject(barBr); DeleteObject(barPn);

    // ── Icon (48x48 real exe icon, or colored letter fallback) ───────
    int ico_sz=32;
    HICON ico = GetProcessIcon(p->exePath);
    if (ico) {
        DrawIconEx(dc, px, py, ico, ico_sz, ico_sz, 0, nullptr, DI_NORMAL);
    } else {
        RECT ir={px,py,px+ico_sz,py+ico_sz};
        COLORREF ic=p->enabled ? ExeClr(p->name) : CLR_GRAY;
        RR(dc,ir,8,ic,ic,1);
        wchar_t ltr[2]={(wchar_t)(p->name.empty()?L'?':towupper(p->name[0])),0};
        SelectObject(dc,g_fntIcon);
        SetTextColor(dc,RGB(255,255,255));
        DrawTextW(dc,ltr,-1,&ir,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }

    // ── Index badge (top-right: #01 #02 …) ───────────────────────
    wchar_t badge[8]; swprintf_s(badge,L"#%02d",idx+1);
    SelectObject(dc,g_fntMeta);
    SIZE bsz; GetTextExtentPoint32W(dc,badge,(int)wcslen(badge),&bsz);
    RECT badgeR={r.right-bsz.cx-14, r.top+8, r.right-6, r.top+8+bsz.cy+4};
    RR(dc,badgeR,4,RGB(230,234,245),RGB(218,222,234),1);
    SetTextColor(dc,CLR_TEXT_DIM);
    DrawTextW(dc,badge,-1,&badgeR,DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    // ── Process name ──────────────────────────────────────────────
    int nameX=px+ico_sz+10;
    SelectObject(dc,g_fntName);
    SetTextColor(dc,CLR_TEXT);
    RECT nr={nameX,py,r.right-42,py+18};
    DrawTextW(dc,p->name.c_str(),-1,&nr,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);

    // ── Status text ───────────────────────────────────────────────
    const wchar_t* sname=StatusName(p->status);
    COLORREF sclr=p->enabled ? StatusColor(p->status) : CLR_GRAY;
    SelectObject(dc,g_fntStat);
    SetTextColor(dc,sclr);
    RECT sr={nameX,py+20,r.right-12,py+36};
    DrawTextW(dc,sname,-1,&sr,DT_LEFT|DT_SINGLELINE);

    // ── Divider ───────────────────────────────────────────────────
    int dY = py+ico_sz+8;
    HPEN dp=CreatePen(PS_SOLID,1,CLR_BORDER);
    HPEN odp=(HPEN)SelectObject(dc,dp);
    MoveToEx(dc,r.left+10,dY,nullptr); LineTo(dc,r.right-10,dY);
    SelectObject(dc,odp); DeleteObject(dp);

    // ── Stats row: CPU | MEM | restarts ──────────────────────────
    wchar_t buf[64];
    SelectObject(dc,g_fntMeta);
    SetTextColor(dc,CLR_TEXT_DIM);
    int cs=22;
    int cx=r.right-10-cs, cy=r.bottom-10-cs;
    int sy=dY+6;
    int statRight=cx-6;
    int sw=(statRight-(r.left+10))/3;
    // cpu
    swprintf_s(buf,L"CPU %.1f%%",p->cpuPercent);
    RECT sc1={r.left+10,sy,r.left+10+sw,sy+14};
    DrawTextW(dc,buf,-1,&sc1,DT_LEFT|DT_SINGLELINE);
    // mem
    swprintf_s(buf,L"%.0fMB",p->memMB);
    RECT sc2={r.left+10+sw,sy,r.left+10+sw*2,sy+14};
    DrawTextW(dc,buf,-1,&sc2,DT_LEFT|DT_SINGLELINE);
    // restarts
    swprintf_s(buf,L"\u91CD\u542F%d",p->restartCount);
    RECT sc3={r.left+10+sw*2,sy,statRight,sy+14};
    DrawTextW(dc,buf,-1,&sc3,DT_LEFT|DT_SINGLELINE);

    // ── Toggle checkbox (with click flash animation) ──────────────
    RECT cbr={cx,cy,cx+cs,cy+cs};
    // Flash: briefly show bright highlight when clicked
    bool flashing = (g_cbFlash == realIdx && g_cbFlashTick > 0);
    int flashPct  = flashing ? g_cbFlashTick * 255 / 8 : 0;  // 0-255
    if(p->enabled) {
        COLORREF cbBg = flashing ? LerpClr(CLR_ACCENT, RGB(255,255,255), flashPct)
                                 : CLR_ACCENT;
        RR(dc,cbr,5,cbBg,cbBg,1);
        HPEN cp=CreatePen(PS_SOLID,2,RGB(255,255,255));
        HPEN ocp=(HPEN)SelectObject(dc,cp);
        MoveToEx(dc,cx+4,  cy+11, nullptr);
        LineTo  (dc,cx+9,  cy+17);
        LineTo  (dc,cx+18, cy+5);
        SelectObject(dc,ocp); DeleteObject(cp);
    } else {
        COLORREF cbBg = flashing ? LerpClr(RGB(248,249,252), CLR_ACCENT, flashPct)
                                 : RGB(248,249,252);
        RR(dc,cbr,5,cbBg,flashing?CLR_ACCENT:CLR_GRAY,1);
    }
}

// ── Filter helper ─────────────────────────────────────────────────
inline bool CardVisible(const GuardedProcess* p) {
    if (g_filter[0]==0) return true;
    // case-insensitive contains
    std::wstring name=p->name, flt=g_filter;
    std::transform(name.begin(),name.end(),name.begin(),towlower);
    std::transform(flt.begin(), flt.end(), flt.begin(), towlower);
    return name.find(flt) != std::wstring::npos;
}

// ── Filtered indices ──────────────────────────────────────────────
inline std::vector<int> VisibleIndices() {
    std::vector<int> vis;
    if (!g_engine) return vis;
    for (int i=0;i<(int)g_engine->procs.size();i++)
        if (CardVisible(g_engine->procs[i])) vis.push_back(i);
    return vis;
}

// ── Paint card area ───────────────────────────────────────────────
inline void PaintCards(HWND hw) {
    PAINTSTRUCT ps; HDC hdc=BeginPaint(hw,&ps);
    RECT rc; GetClientRect(hw,&rc);
    int W=rc.right, H=rc.bottom;

    HDC     mdc=CreateCompatibleDC(hdc);
    HBITMAP mb =CreateCompatibleBitmap(hdc,W,H);
    HBITMAP ob =(HBITMAP)SelectObject(mdc,mb);

    HBRUSH bg=CreateSolidBrush(CLR_BG);
    FillRect(mdc,&rc,bg); DeleteObject(bg);

    if(g_engine) {
        std::lock_guard<std::mutex> g(g_engine->lock);
        auto vis=VisibleIndices();
        int n=(int)vis.size();
        if(n==0) {
            SelectObject(mdc,g_fntStat);
            SetTextColor(mdc,CLR_GRAY);
            SetBkMode(mdc,TRANSPARENT);
            RECT hr={0,H/2-20,W,H/2+20};
            const wchar_t* hint = g_filter[0]
                ? L"\u6CA1\u6709\u5339\u914D\u7684\u8FDB\u7A0B"
                : L"\u5C06 .exe \u6216\u5FEB\u6377\u65B9\u5F0F\u62D6\u5165\u6B64\u5904\uFF0C\u6216\u70B9\u51FB\u300C\u6DFB\u52A0\u8FDB\u7A0B\u300D";
            DrawTextW(mdc,hint,-1,&hr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        SetBkMode(mdc,TRANSPARENT);
        for(int vi=0;vi<n;vi++) {
            int realIdx=vis[vi];
            RECT r=CardRect(vi,W);
            if(r.bottom<0||r.top>H) continue;
            // hoverCard is in visible-slot index
            DrawCard(mdc,r,g_engine->procs[realIdx],vi==g_hoverCard,realIdx,realIdx);
        }
    }

    BitBlt(hdc,0,0,W,H,mdc,0,0,SRCCOPY);
    SelectObject(mdc,ob); DeleteObject(mb); DeleteDC(mdc);
    EndPaint(hw,&ps);
}

// ── Hit test ─────────────────────────────────────────────────────
inline int HitCard(POINT pt, int W) {
    // Returns visible-slot index
    if(!g_engine) return -1;
    std::lock_guard<std::mutex> g(g_engine->lock);
    auto vis=VisibleIndices();
    for(int vi=0;vi<(int)vis.size();vi++){
        RECT r=CardRect(vi,W); if(PtInRect(&r,pt)) return vi;
    }
    return -1;
}
// Convert visible-slot index → real proc index
inline int RealIdx(int visSlot) {
    auto vis=VisibleIndices();
    if(visSlot<0||visSlot>=(int)vis.size()) return -1;
    return vis[visSlot];
}
inline bool HitCB(POINT pt, RECT r) {
    int cs=22;
    RECT cb={r.right-10-cs,r.bottom-10-cs,r.right-10,r.bottom-10};
    return PtInRect(&cb,pt)!=0;
}

// ── Tray ─────────────────────────────────────────────────────────
inline void SetupTray(HWND hw) {
    g_nid.cbSize=sizeof(g_nid); g_nid.hWnd=hw; g_nid.uID=1;
    g_nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
    g_nid.uCallbackMessage=WM_APP+1;
    g_nid.hIcon=LoadIconW(GetModuleHandleW(nullptr),(LPCWSTR)MAKEINTRESOURCE(101));
    if(!g_nid.hIcon) g_nid.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(g_nid.szTip,L"ProcessGuard \u00B7 \u8FDB\u7A0B\u5B88\u62A4");
    Shell_NotifyIconW(NIM_ADD,&g_nid);
}
inline void TrayMenu(HWND hw) {
    HMENU hm=CreatePopupMenu();
    AppendMenuW(hm,MF_STRING,1001,L"\u663E\u793A\u4E3B\u7A97\u53E3");
    AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
    AppendMenuW(hm,MF_STRING,1002,L"\u9000\u51FA\u5B88\u62A4");
    POINT pt; GetCursorPos(&pt); SetForegroundWindow(hw);
    int cmd=TrackPopupMenu(hm,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,hw,nullptr);
    DestroyMenu(hm);
    if(cmd==1001){ShowWindow(hw,SW_RESTORE);SetForegroundWindow(hw);}
    else if(cmd==1002) DestroyWindow(hw);
}

// ════════════════════════════════════════════════════════════════
// Card area subclass
// ════════════════════════════════════════════════════════════════
LRESULT CALLBACK CardAreaProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR uid,DWORD_PTR d) {
    switch(msg) {
    case WM_NCHITTEST:  return HTCLIENT;  // prevent STATIC from returning HTTRANSPARENT
    case WM_PAINT:      PaintCards(hw); return 0;
    case WM_ERASEBKGND: return 1;

    case WM_MOUSEWHEEL: {
        RECT rc; GetClientRect(hw,&rc);
        int delta=-(int)(short)HIWORD(wp)/WHEEL_DELTA*50;
        g_scrollY=std::max(0,std::min(g_scrollY+delta,std::max(0,(int)(g_totalH-rc.bottom))));
        RefreshScroll(hw,g_engine?(int)g_engine->procs.size():0);
        InvalidateRect(hw,nullptr,FALSE); return 0;
    }
    case WM_VSCROLL: {
        RECT rc; GetClientRect(hw,&rc);
        SCROLLINFO si{sizeof(si),SIF_ALL}; GetScrollInfo(hw,SB_VERT,&si);
        int old=g_scrollY;
        switch(LOWORD(wp)){
            case SB_LINEUP:     g_scrollY-=30; break;
            case SB_LINEDOWN:   g_scrollY+=30; break;
            case SB_PAGEUP:     g_scrollY-=rc.bottom; break;
            case SB_PAGEDOWN:   g_scrollY+=rc.bottom; break;
            case SB_THUMBTRACK: g_scrollY=si.nTrackPos; break;
        }
        g_scrollY=std::max(0,std::min(g_scrollY,std::max(0,(int)(g_totalH-rc.bottom))));
        if(g_scrollY!=old){
            RefreshScroll(hw,g_engine?(int)g_engine->procs.size():0);
            InvalidateRect(hw,nullptr,FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt={LOWORD(lp),HIWORD(lp)};
        RECT rc; GetClientRect(hw,&rc);
        int prev=g_hoverCard; g_hoverCard=HitCard(pt,rc.right);
        if(g_hoverCard!=prev){
            // Reset alpha so new card animates in from start
            if(g_hoverCard>=0) g_hoverAlpha=0;
            SetCursor(LoadCursor(nullptr,g_hoverCard>=0?IDC_HAND:IDC_ARROW));
            InvalidateRect(hw,nullptr,FALSE);
        }
        TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hw,0};
        TrackMouseEvent(&tme); return 0;
    }
    case WM_MOUSELEAVE:
        if(g_hoverCard>=0){g_hoverCard=-1;InvalidateRect(hw,nullptr,FALSE);}
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt={LOWORD(lp),(int)(short)HIWORD(lp)};
        RECT rc; GetClientRect(hw,&rc);
        int visSlot=HitCard(pt,rc.right);
        if(visSlot>=0 && g_engine) {
            RECT r=CardRect(visSlot,rc.right);
            if(HitCB(pt,r)){
                int real=RealIdx(visSlot);
                if(real>=0){
                    g_engine->ToggleEnabled((size_t)real);
                    g_cbFlash=real;
                    g_cbFlashTick=8;  // 8 frames @ 30ms = ~240ms flash
                }
                InvalidateRect(hw,nullptr,FALSE);
            }
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        POINT pt={LOWORD(lp),(int)(short)HIWORD(lp)};
        RECT rc; GetClientRect(hw,&rc);
        int visSlot=HitCard(pt,rc.right);
        if(visSlot<0||!g_engine) return 0;
        int idx=RealIdx(visSlot);
        if(idx<0) return 0;
        POINT sc=pt; ClientToScreen(hw,&sc);
        bool en=false; std::wstring nm;
        {
            std::lock_guard<std::mutex> g(g_engine->lock);
            if(idx<(int)g_engine->procs.size()){
                en=g_engine->procs[idx]->enabled;
                nm=g_engine->procs[idx]->name;
            }
        }
        HMENU hm=CreatePopupMenu();
        AppendMenuW(hm,MF_STRING,2001,en?L"\u6682\u505C\u5B88\u62A4":L"\u542F\u7528\u5B88\u62A4");
        AppendMenuW(hm,MF_SEPARATOR,0,nullptr);
        AppendMenuW(hm,MF_STRING,2002,L"\u79FB\u9664\u6B64\u8FDB\u7A0B");
        SetForegroundWindow(g_hwnd);
        int cmd=TrackPopupMenu(hm,TPM_RETURNCMD|TPM_RIGHTBUTTON,sc.x,sc.y,0,g_hwnd,nullptr);
        DestroyMenu(hm);
        if(cmd==2001){
            g_engine->ToggleEnabled((size_t)idx);
            InvalidateRect(hw,nullptr,FALSE);
        } else if(cmd==2002){
            std::wstring msg=L"\u786E\u5B9A\u79FB\u9664\u5BF9 \""+nm+L"\" \u7684\u5B88\u62A4\uFF1F";
            if(MessageBoxW(g_hwnd,msg.c_str(),L"\u786E\u8BA4\u79FB\u9664",MB_YESNO|MB_ICONQUESTION)==IDYES){
                g_engine->RemoveProcess((size_t)idx);
                RECT crc; GetClientRect(hw,&crc);
                RefreshScroll(hw,g_engine?(int)g_engine->procs.size():0);
                InvalidateRect(hw,nullptr,FALSE);
            }
        }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hd=(HDROP)wp;
        UINT n=DragQueryFileW(hd,0xFFFFFFFF,nullptr,0);
        for(UINT i=0;i<n;i++){
            wchar_t path[MAX_PATH]; DragQueryFileW(hd,i,path,MAX_PATH);
            std::wstring exe=GetExeFromDrop(path);
            if(!exe.empty()) g_engine->AddProcess(exe);
        }
        DragFinish(hd);
        RECT crc; GetClientRect(hw,&crc);
        RefreshScroll(hw,g_engine?(int)g_engine->procs.size():0);
        InvalidateRect(hw,nullptr,FALSE);
        if(g_hwnd) InvalidateRect(g_hwnd,nullptr,FALSE);
        return 0;
    }
    case WM_SIZE: {
        RefreshScroll(hw,g_engine?(int)g_engine->procs.size():0);
        InvalidateRect(hw,nullptr,FALSE); return 0;
    }
    }
    return DefSubclassProc(hw,msg,wp,lp);
}

// ════════════════════════════════════════════════════════════════
// Main window
// ════════════════════════════════════════════════════════════════
LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_NCHITTEST: {
        LRESULT hit=DefWindowProcW(hw,msg,wp,lp);
        if(hit==HTCLIENT){
            POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};
            ScreenToClient(hw,&pt);
            if(pt.y<TITLEBAR_H){
                if(PtInRect(&g_btnClose,pt)||PtInRect(&g_btnMin,pt)) return HTCLIENT;
                return HTCAPTION;
            }
        }
        return hit;
    }
    case WM_MOUSEMOVE: {
        POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};
        int prev=g_titleHover;
        if(pt.y<TITLEBAR_H){
            g_titleHover=PtInRect(&g_btnClose,pt)?2:PtInRect(&g_btnMin,pt)?1:0;
            TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hw,0};
            TrackMouseEvent(&tme);
        } else { g_titleHover=0; }
        if(g_titleHover!=prev) InvalidateRect(hw,nullptr,FALSE);
        return DefWindowProcW(hw,msg,wp,lp);
    }
    case WM_MOUSELEAVE:
        if(g_titleHover){g_titleHover=0;InvalidateRect(hw,nullptr,FALSE);}
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={(int)(short)LOWORD(lp),(int)(short)HIWORD(lp)};
        if(PtInRect(&g_btnClose,pt)){SendMessageW(hw,WM_CLOSE,0,0);return 0;}
        if(PtInRect(&g_btnMin,  pt)){ShowWindow(hw,SW_MINIMIZE);   return 0;}
        return DefWindowProcW(hw,msg,wp,lp);
    }
    case WM_CREATE: {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        g_hwnd=hw;
        g_fntTitle=MkFont(13,FW_BOLD,   L"\u5FAE\u8F6F\u96C5\u9ED1");
        g_fntName =MkFont(11,FW_SEMIBOLD,L"\u5FAE\u8F6F\u96C5\u9ED1");
        g_fntMeta =MkFont(10,FW_NORMAL,  L"\u5FAE\u8F6F\u96C5\u9ED1");
        g_fntStat =MkFont(10,FW_NORMAL,  L"\u5FAE\u8F6F\u96C5\u9ED1");
        g_fntIcon =MkFont(17,FW_BOLD,    L"\u5FAE\u8F6F\u96C5\u9ED1");

        ChangeWindowMessageFilterEx(hw, WM_DROPFILES,    MSGFLT_ALLOW, nullptr);
        ChangeWindowMessageFilterEx(hw, WM_COPYDATA,     MSGFLT_ALLOW, nullptr);
        ChangeWindowMessageFilterEx(hw, 0x0049, MSGFLT_ALLOW, nullptr);

        // Add button (owner-draw)
        HWND ba=CreateWindowW(L"BUTTON",L"\uFF0B  \u6DFB\u52A0\u8FDB\u7A0B",
            WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            0,0,0,0,hw,(HMENU)(UINT_PTR)IDC_BTN_ADD,
            GetModuleHandleW(nullptr),nullptr);
        SendMessageW(ba,WM_SETFONT,(WPARAM)g_fntMeta,TRUE);

        // Search box
        g_searchBox=CreateWindowExW(0,L"EDIT",nullptr,
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            0,0,0,0,hw,(HMENU)(UINT_PTR)IDC_SEARCH,
            GetModuleHandleW(nullptr),nullptr);
        SendMessageW(g_searchBox,WM_SETFONT,(WPARAM)g_fntMeta,TRUE);
        SendMessageW(g_searchBox,EM_SETCUEBANNER,0,(LPARAM)L"\u641C\u7D22\u8FDB\u7A0B\u540D\u79F0");

        // Card area
        g_cardArea=CreateWindowExW(WS_EX_ACCEPTFILES,L"STATIC",nullptr,
            WS_CHILD|WS_VISIBLE|WS_VSCROLL,
            0,0,0,0,hw,nullptr,GetModuleHandleW(nullptr),nullptr);
        SetWindowSubclass(g_cardArea,CardAreaProc,1,0);

        ChangeWindowMessageFilterEx(g_cardArea, WM_DROPFILES,    MSGFLT_ALLOW, nullptr);
        ChangeWindowMessageFilterEx(g_cardArea, WM_COPYDATA,     MSGFLT_ALLOW, nullptr);
        ChangeWindowMessageFilterEx(g_cardArea, 0x0049, MSGFLT_ALLOW, nullptr);

        SetupTray(hw);
        SetTimer(hw,IDC_TIMER_REFRESH,2000,nullptr);
        SetTimer(hw,IDC_TIMER_ANIM,   30,  nullptr);  // ~33fps animation
        g_engine->Start();

        // ── Rounded corners ──────────────────────────────────────
        // Win11: ask DWM for system round corners (no border needed)
        typedef HRESULT(WINAPI*PFN_DWMSWA)(HWND,DWORD,LPCVOID,DWORD);
        static PFN_DWMSWA dwmSet=(PFN_DWMSWA)GetProcAddress(
            GetModuleHandleW(L"dwmapi.dll"),"DwmSetWindowAttribute");
        if (dwmSet) {
            // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2
            DWORD pref = 2;
            dwmSet(hw, 33, &pref, sizeof(pref));
        }
        // Win10 / fallback: SetWindowRgn with rounded rectangle
        {
            RECT rc; GetClientRect(hw, &rc);
            HRGN rgn = CreateRoundRectRgn(0,0,rc.right+1,rc.bottom+1,12,12);
            SetWindowRgn(hw, rgn, TRUE);
        }
        return 0;
    }
    case WM_SIZE: {
        int W=LOWORD(lp),H=HIWORD(lp);
        int pad=12,bh=30,bY=TITLEBAR_H+(TOOLBAR_H-bh)/2,bw=110,sbW=180;
        SetWindowPos(GetDlgItem(hw,IDC_BTN_ADD),nullptr,pad,bY,bw,bh,SWP_NOZORDER);
        if(g_searchBox)
            SetWindowPos(g_searchBox,nullptr,W-pad-sbW,bY,sbW,bh,SWP_NOZORDER);
        SetWindowPos(g_cardArea,nullptr,0,TITLEBAR_H+TOOLBAR_H,W,H-TITLEBAR_H-TOOLBAR_H,SWP_NOZORDER);
        if(g_engine&&g_cardArea){
            auto vis=VisibleIndices();
            RefreshScroll(g_cardArea,(int)vis.size());
        }
        // Keep rounded clip region in sync with window size
        if (W>0 && H>0) {
            HRGN rgn=CreateRoundRectRgn(0,0,W+1,H+1,12,12);
            SetWindowRgn(hw,rgn,TRUE);
        }
        return 0;
    }
    case WM_PAINT: {
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
          MoveToEx(hdc,g_btnMin.left+11,my,nullptr); LineTo(hdc,g_btnMin.right-11,my);
          SelectObject(hdc,op); DeleteObject(pen); }

        // Close button
        g_btnClose={rc.right-32,0,rc.right,TITLEBAR_H};
        { HBRUSH hb=CreateSolidBrush(g_titleHover==2?RGB(220,38,38):CLR_ACCENT);
          FillRect(hdc,&g_btnClose,hb); DeleteObject(hb); }
        { HPEN pen=CreatePen(PS_SOLID,2,RGB(255,255,255));
          HPEN op=(HPEN)SelectObject(hdc,pen);
          int cx2=(g_btnClose.left+g_btnClose.right)/2;
          int cy2=TITLEBAR_H/2;
          MoveToEx(hdc,cx2-5,cy2-5,nullptr); LineTo(hdc,cx2+6,cy2+6);
          MoveToEx(hdc,cx2+5,cy2-5,nullptr); LineTo(hdc,cx2-6,cy2+6);
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
    }
    case WM_ERASEBKGND: {
        HDC hdc=(HDC)wp; RECT rc; GetClientRect(hw,&rc);
        HBRUSH b=CreateSolidBrush(CLR_BG); FillRect(hdc,&rc,b); DeleteObject(b);
        return 1;
    }
    case WM_DRAWITEM: {
        auto* di=(DRAWITEMSTRUCT*)lp;
        if(di->CtlID==IDC_BTN_ADD){
            bool pressed=(di->itemState&ODS_SELECTED)!=0;
            COLORREF bg=pressed?RGB(30,90,210):CLR_ACCENT;
            RR(di->hDC,di->rcItem,6,bg,bg,1);
            SetBkMode(di->hDC,TRANSPARENT);
            SetTextColor(di->hDC,RGB(255,255,255));
            SelectObject(di->hDC,g_fntMeta);
            DrawTextW(di->hDC,L"\uFF0B  \u6DFB\u52A0\u8FDB\u7A0B",-1,&di->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        return TRUE;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc=(HDC)wp;
        SetBkColor(hdc,RGB(255,255,255));
        SetTextColor(hdc,CLR_TEXT);
        static HBRUSH wb=CreateSolidBrush(RGB(255,255,255));
        return (LRESULT)wb;
    }
    case WM_COMMAND: {
        if(LOWORD(wp)==IDC_BTN_ADD) {
            wchar_t buf[MAX_PATH]={};
            OPENFILENAMEW ofn={sizeof(ofn)};
            ofn.hwndOwner=hw;
            ofn.lpstrFilter=L"\u53EF\u6267\u884C\u6587\u4EF6\0*.exe\0\u5FEB\u6377\u65B9\u5F0F\0*.lnk\0\u6240\u6709\u6587\u4EF6\0*.*\0";
            ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            ofn.lpstrTitle=L"\u9009\u62E9\u8981\u5B88\u62A4\u7684\u7A0B\u5E8F";
            if(GetOpenFileNameW(&ofn)&&g_engine){
                std::wstring exe=GetExeFromDrop(buf);
                if(exe.empty()) exe=buf;
                g_engine->AddProcess(exe);
                auto vis=VisibleIndices();
                RefreshScroll(g_cardArea,(int)vis.size());
                InvalidateRect(g_cardArea,nullptr,FALSE);
                InvalidateRect(hw,nullptr,FALSE);
            }
        } else if(LOWORD(wp)==IDC_SEARCH&&HIWORD(wp)==EN_CHANGE&&g_searchBox) {
            GetWindowTextW(g_searchBox,g_filter,256);
            g_scrollY=0;
            auto vis=VisibleIndices();
            RefreshScroll(g_cardArea,(int)vis.size());
            InvalidateRect(g_cardArea,nullptr,FALSE);
        }
        return 0;
    }
    case WM_TIMER:
        if(wp==IDC_TIMER_REFRESH){
            InvalidateRect(g_cardArea,nullptr,FALSE);
            InvalidateRect(hw,nullptr,FALSE);
        }
        if(wp==IDC_TIMER_ANIM){
            bool dirty=false;
            // Hover alpha: fade in when hovering, fade out when not
            if(g_hoverCard>=0 && g_hoverAlpha<255){
                g_hoverAlpha=std::min(255,g_hoverAlpha+51); dirty=true;
            } else if(g_hoverCard<0 && g_hoverAlpha>0){
                g_hoverAlpha=std::max(0,g_hoverAlpha-51);  dirty=true;
            }
            // Checkbox flash: count down
            if(g_cbFlashTick>0){
                g_cbFlashTick--; dirty=true;
                if(g_cbFlashTick==0) g_cbFlash=-1;
            }
            if(dirty) InvalidateRect(g_cardArea,nullptr,FALSE);
        }
        return 0;
    case WM_DROPFILES: {
        HDROP hd=(HDROP)wp;
        UINT n=DragQueryFileW(hd,0xFFFFFFFF,nullptr,0);
        for(UINT i=0;i<n;i++){
            wchar_t path[MAX_PATH]; DragQueryFileW(hd,i,path,MAX_PATH);
            std::wstring exe=GetExeFromDrop(path);
            if(!exe.empty()) g_engine->AddProcess(exe);
        }
        DragFinish(hd);
        if(g_engine&&g_cardArea){
            RefreshScroll(g_cardArea,(int)g_engine->procs.size());
            InvalidateRect(g_cardArea,nullptr,FALSE);
            InvalidateRect(hw,nullptr,FALSE);
        }
        return 0;
    }
    case WM_APP+1:
        if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU) TrayMenu(hw);
        else if(lp==WM_LBUTTONDBLCLK){ShowWindow(hw,SW_RESTORE);SetForegroundWindow(hw);}
        return 0;
    case WM_CLOSE: ShowWindow(hw,SW_HIDE); return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE,&g_nid);
        KillTimer(hw,IDC_TIMER_REFRESH);
        KillTimer(hw,IDC_TIMER_ANIM);
        for(auto& kv:g_iconCache) if(kv.second) DestroyIcon(kv.second);
        g_iconCache.clear();
        DeleteObject(g_fntTitle); DeleteObject(g_fntName);
        DeleteObject(g_fntMeta);  DeleteObject(g_fntStat);
        DeleteObject(g_fntIcon);
        CoUninitialize();
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hw,msg,wp,lp);
}

// ── Window creation ───────────────────────────────────────────────
inline HWND CreateMainWindow(HINSTANCE hInst) {
    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    WNDCLASSEXW wc={sizeof(wc)};
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_DROPSHADOW;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=CreateSolidBrush(CLR_BG);
    wc.lpszClassName=L"ProcessGuardWnd";
    wc.hIcon  = LoadIconW(GetModuleHandleW(nullptr),(LPCWSTR)MAKEINTRESOURCE(101));
    if(!wc.hIcon) wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wc.hIconSm= (HICON)LoadImageW(GetModuleHandleW(nullptr),(LPCWSTR)MAKEINTRESOURCE(101),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
    RegisterClassExW(&wc);
    HWND hw=CreateWindowExW(WS_EX_ACCEPTFILES|WS_EX_APPWINDOW,L"ProcessGuardWnd",
        L"ProcessGuard \u00B7 \u8FDB\u7A0B\u5B88\u62A4",
        WS_POPUP|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,660,496,
        nullptr,nullptr,hInst,nullptr);
    return hw;
}
