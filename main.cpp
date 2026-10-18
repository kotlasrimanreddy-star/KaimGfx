#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>
#include <cwctype>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

static const int WIN_W = 1540, WIN_H = 900;
static const Color BG(255, 7, 10, 16);
static const Color PANEL(255, 13, 18, 28);
static const Color TEXT_COLOR(255, 237, 242, 255);
static const Color MUTED(255, 143, 159, 185);
static const Color RED(255, 255, 51, 79);

struct Item {
    std::wstring name, desc, glyph;
    Color accent;
    bool plugin;
    RectF bounds;
};
std::vector<Item> items;
std::vector<std::wstring> tabs = {L"Downloads", L"Tutorials", L"Store", L"History", L"Location", L"About"};
int activeTab = 0;
HWND hwndMain, hwndSearch;
ULONG_PTR gdiplusToken;
float anim = 0.0f;
float pulse = 0.0f;
POINT mousePt = {-1,-1};
int hovered = -1;
std::wstring searchText;
std::wstring toastText;
DWORD toastAt = 0;
bool searchFocused = false;
WNDPROC oldSearchProc = nullptr;
HBRUSH searchBrush = nullptr;

Color C(BYTE r, BYTE g, BYTE b, BYTE a=255) { return Color(a,r,g,b); }

void AddItem(const wchar_t* n, const wchar_t* d, const wchar_t* g, Color c, bool p) {
    items.push_back({n,d,g,c,p,RectF()});
}

void InitItems() {
    AddItem(L"After Effects",L"Motion Graphics",L"Ae",C(104,96,255),false);
    AddItem(L"Premiere Pro",L"Video Editing",L"Pr",C(100,103,255),false);
    AddItem(L"Photoshop",L"Image Editing",L"Ps",C(20,185,255),false);
    AddItem(L"InDesign",L"Layout & Design",L"Id",C(255,40,125),false);
    AddItem(L"Media Encoder",L"Encoding & Render",L"Me",C(142,111,255),false);
    AddItem(L"Illustrator",L"Vector Design",L"Ai",C(255,151,0),false);
    AddItem(L"DaVinci Resolve",L"Video Editing",L"◉",C(37,226,208),false);
    AddItem(L"Topaz Video",L"AI Upscaling",L"▽",C(117,107,255),false);
    AddItem(L"Motion Blur",L"Motion Effects",L"MB",C(220,231,255),false);
    AddItem(L"CapCut PC",L"Video Editing",L"↔",C(245,247,255),false);
    AddItem(L"Magic Bullet",L"Color Grading",L"RED",C(255,52,71),true);
    AddItem(L"Maxon",L"3D & VFX",L"M",C(255,56,77),true);
    AddItem(L"Twixtor",L"Time Remapping",L"T",C(179,106,255),true);
    AddItem(L"RSMB",L"Motion Blur",L"RSMB",C(255,78,228),true);
    AddItem(L"Deep Glow",L"Glow Effects",L"D",C(25,185,255),true);
    AddItem(L"Boris FX Sapphire",L"Visual Effects",L"B",C(31,168,255),true);
    AddItem(L"Boris FX Continuum",L"Effects Suite",L"B",C(31,168,255),true);
}

void RoundedRect(Graphics& g, RectF r, float radius, Brush* b, Pen* p=nullptr) {
    GraphicsPath path;
    float d=radius*2;
    path.AddArc(r.X,r.Y,d,d,180,90); path.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
    path.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90); path.AddArc(r.X,r.GetBottom()-d,d,d,90,90);
    path.CloseFigure();
    if(b) g.FillPath(b,&path);
    if(p) g.DrawPath(p,&path);
}
void DrawText(Graphics& g, const std::wstring& s, float x, float y, float size, Color color, bool bold=false, float maxWidth=0) {
    FontFamily family(L"Segoe UI");
    Font font(&family,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
    SolidBrush brush(color);
    StringFormat fmt; fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    if(maxWidth>0) g.DrawString(s.c_str(),-1,&font,RectF(x,y,maxWidth,35),&fmt,&brush);
    else g.DrawString(s.c_str(),-1,&font,PointF(x,y),&brush);
}
void Line(Graphics& g, float x1,float y1,float x2,float y2, Color c,float w=1) {
    Pen p(c,w); g.DrawLine(&p,x1,y1,x2,y2);
}
void Circle(Graphics& g,float x,float y,float d,Color fill,Color edge=Color(0,0,0,0),float ew=1) {
    SolidBrush b(fill); g.FillEllipse(&b,x,y,d,d);
    if(edge.GetA()>0){Pen p(edge,ew);g.DrawEllipse(&p,x,y,d,d);}
}
bool Matches(const Item& it) {
    if(searchText.empty()) return true;
    std::wstring all=it.name+L" "+it.desc;
    std::transform(all.begin(),all.end(),all.begin(),towlower);
    std::wstring q=searchText; std::transform(q.begin(),q.end(),q.begin(),towlower);
    return all.find(q)!=std::wstring::npos;
}

void DrawLogo(Graphics& g,float x,float y,float size) {
    Pen ring(C(255,67,83),2.2f); Pen outer(C(255,51,79,75),1.0f);
    SolidBrush inner(C(27,13,21));
    g.FillEllipse(&inner,x,y,size,size);
    g.DrawEllipse(&outer,x-4,y-4,size+8,size+8);
    g.DrawEllipse(&ring,x,y,size,size);
    DrawText(g,L"KG",x+size*.19f,y+size*.29f,size*.36f,C(255,70,88),true);
}
void DrawRobot(Graphics& g, float x,float y,float scale) {
    GraphicsState state=g.Save();
    g.TranslateTransform(x,y); g.ScaleTransform(scale,scale);
    // stylized futuristic head silhouette
    SolidBrush head(C(255,83,109,148));
    SolidBrush shade(C(255,24,39,62));
    SolidBrush shine(C(150,225,237,255));
    Pen edge(C(255,122,157,204),2);
    g.FillEllipse(&shade,25,8,158,190);
    g.FillEllipse(&head,15,0,156,188);
    g.FillEllipse(&shine,31,8,70,40);
    // face plate
    RoundedRect(g,RectF(37,58,112,75),24,&shade,&edge);
    // eyes
    Pen eye(C(255,255,61,93),4);
    g.DrawLine(&eye,52,86,80,86); g.DrawLine(&eye,105,86,133,86);
    Pen eyeGlow(C(130,255,56,88),10);
    g.DrawLine(&eyeGlow,52,86,80,86); g.DrawLine(&eyeGlow,105,86,133,86);
    // ear ring
    Circle(g,142,55,55,C(255,13,24,40),C(255,117,148,196),7);
    Circle(g,154,67,31,C(255,255,50,80),C(255,42,61,89),4);
    Circle(g,163,76,13,C(255,11,17,28));
    // jaw
    Pen jaw(C(255,154,183,220),2);
    g.DrawArc(&jaw,47,111,90,60,10,160);
    g.Restore(state);
}
void DrawCard(Graphics& g, Item& it, int idx, float x,float y,float w,float h) {
    it.bounds=RectF(x,y,w,h);
    bool hot=(idx==hovered);
    Color fill=hot?C(255,24,29,43):C(255,14,20,31);
    SolidBrush b(fill);
    Pen border(hot?C(200,255,69,91):C(255,37,47,63),hot?1.5f:1.0f);
    RoundedRect(g,it.bounds,13,&b,&border);
    if(hot) {
        Pen glow(C(35,255,51,79),5);
        RoundedRect(g,RectF(x-1,y-1,w+2,h+2),14,nullptr,&glow);
    }
    float iconSize=it.plugin?52:60;
    float ix=x+18, iy=y+(h-iconSize)/2;
    SolidBrush ib(C(255,10,15,25));
    Pen ip(it.accent,1.4f);
    RoundedRect(g,RectF(ix,iy,iconSize,iconSize),12,&ib,&ip);
    SolidBrush tint(Color(22,it.accent.GetR(),it.accent.GetG(),it.accent.GetB()));
    RoundedRect(g,RectF(ix+2,iy+2,iconSize-4,iconSize-4),10,&tint,nullptr);
    DrawText(g,it.glyph,ix+8,iy+iconSize*.28f,it.plugin?17:25,it.accent,true,iconSize-12);
    float tx=ix+iconSize+15;
    float textW=w-(tx-x)-48;
    DrawText(g,it.name,tx,y+h*.31f,it.plugin?12.5f:14.2f,TEXT_COLOR,true,textW);
    DrawText(g,it.desc,tx,y+h*.56f,it.plugin?11.2f:12.2f,MUTED,false,textW);
    Circle(g,x+w-31,y+h/2-15,30,C(255,23,31,45));
    DrawText(g,L"›",x+w-23,y+h/2-14,24,C(255,221,231,247),false);
}
void DrawSectionHeader(Graphics& g,float x,float y,const wchar_t* title,const wchar_t* count) {
    RoundedRect(g,RectF(x,y,4,25),2.0f,new SolidBrush(RED));
    DrawText(g,title,x+20,y+1,19,TEXT_COLOR,true);
    SolidBrush cb(C(255,16,23,35)); Pen cp(C(255,34,43,60));
    RoundedRect(g,RectF(0,0,1,1),1,nullptr,nullptr); // keeps GDI+ path state deterministic
    // count pill is positioned by caller via right aligned canvas
    (void)count;
}
void Paint(HWND hwnd) {
    PAINTSTRUCT ps; HDC hdc=BeginPaint(hwnd,&ps);
    RECT rc; GetClientRect(hwnd,&rc);
    int W=rc.right, H=rc.bottom;
    Bitmap bmp(W,H,PixelFormat32bppARGB);
    Graphics g(&bmp); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    SolidBrush bg(BG); g.FillRectangle(&bg,0,0,W,H);
    // ambient upper glow
    SolidBrush glow(Color(24,44,78,135)); g.FillEllipse(&glow, W * 0.20f, -250.0f, W * 0.65f, 420.0f);
    // topbar
    SolidBrush top(C(255,9,13,21)); g.FillRectangle(&top,0,0,W,105);
    Line(g,0,104,W,104,C(35,140,160,195));
    DrawLogo(g,28,22,62);
    DrawText(g,L"KaimGfx",108,24,27,TEXT_COLOR,true);
    DrawText(g,L"♛  VIP Member  ⌄",109,59,12.5f,C(255,255,81,99),true);
    // nav bar
    float navX=300, navY=23, navW=std::min(730.0f,(float)W*.51f);
    SolidBrush navbg(C(255,16,21,32)); RoundedRect(g,RectF(navX,navY,navW,49),25,&navbg,nullptr);
    const wchar_t* navLabels[]={L"↓  Downloads",L"▷  Tutorials",L"▣  Store",L"◷  History",L"⌖  Location",L"ⓘ  About"};
    float each=navW/6;
    for(int i=0;i<6;i++) {
        RectF r(navX+i*each+3,navY+3,each-6,43);
        if(activeTab==i) { SolidBrush ab(C(255,255,51,79)); RoundedRect(g,r,22,&ab,nullptr); }
        DrawText(g,navLabels[i],r.X+8,r.Y+13,12.5f,activeTab==i?C(255,255,255,255):C(255,177,190,212),activeTab==i,each-15);
    }
    // search field
    float sw=std::min(255.0f,(float)W*.19f), sx=W-sw-27;
    SolidBrush sb(C(255,11,16,26)); Pen sp(C(255,43,54,74));
    RoundedRect(g,RectF(sx,28,sw,43),22,&sb,&sp);
    DrawText(g,L"⌕",sx+14,36,22,C(255,161,179,207),false);
    std::wstring display=searchText.empty()?L"Search software, plugins...":searchText;
    DrawText(g,display,sx+43,42,12.5f,searchText.empty()?C(255,117,133,158):TEXT_COLOR,false,sw-53);
    // main hero
    float pad=24, heroY=124, heroH=235, heroW=W-48;
    SolidBrush hero(C(255,8,13,23)); Pen hp(C(255,34,45,65));
    RoundedRect(g,RectF(pad,heroY,heroW,heroH),17,&hero,&hp);
    // diagonal ambient bands
    Pen band(C(255,14,22,37),22);
    g.DrawLine(&band,W*.53f,heroY+heroH,W*.68f,heroY);
    Pen band2(C(255,12,20,34),14); g.DrawLine(&band2,W*.59f,heroY+heroH,W*.74f,heroY);
    DrawText(g,L"C R E A T I V E   T O O L S",68,heroY+34,11.5f,RED,true);
    DrawText(g,L"Your creative toolkit,",68,heroY+64,40,TEXT_COLOR,true,560);
    DrawText(g,L"one click away.",68,heroY+110,40,C(255,255,78,94),true,500);
    DrawText(g,L"Discover tools for every creative workflow.",68,heroY+174,17,C(255,143,164,201),false,560);
    RoundedRect(g,RectF(68,heroY+216,55,4),2,new SolidBrush(RED));
    RoundedRect(g,RectF(135,heroY+216,55,4),2,new SolidBrush(C(255,31,41,58)));
    RoundedRect(g,RectF(202,heroY+216,55,4),2,new SolidBrush(C(255,31,41,58)));
    DrawRobot(g,W-430,heroY+13,1.08f);
    // sections
    float contentX=26, contentW=W-52;
    float softwareY=heroY+heroH+28;
    DrawSectionHeader(g,contentX,softwareY,L"SOFTWARE",L"10 Items");
    SolidBrush pill(C(255,16,23,35)); Pen pillp(C(255,35,44,61));
    RoundedRect(g,RectF(W-105,softwareY-1,79,29),15,&pill,&pillp);
    DrawText(g,L"10 Items",W-91,softwareY+7,11.5f,C(255,174,188,211),false);
    float gap=14, cardY=softwareY+43, cardH=92;
    float cardW=(contentW-gap*4)/5;
    int shown=0;
    for(int i=0;i<(int)items.size();i++) if(!items[i].plugin && Matches(items[i])) {
        int col=shown%5, row=shown/5;
        DrawCard(g,items[i],i,contentX+col*(cardW+gap),cardY+row*(cardH+13),cardW,cardH);
        shown++;
    }
    float pluginsY=cardY+2*(cardH+13)+15;
    DrawSectionHeader(g,contentX,pluginsY,L"PLUGINS",L"7 Items");
    RoundedRect(g,RectF(W-105,pluginsY-1,79,29),15,&pill,&pillp);
    DrawText(g,L"7 Items",W-91,pluginsY+7,11.5f,C(255,174,188,211),false);
    float pY=pluginsY+42, pGap=13, pW=(contentW-pGap*6)/7, pH=88;
    shown=0;
    for(int i=0;i<(int)items.size();i++) if(items[i].plugin && Matches(items[i])) {
        DrawCard(g,items[i],i,contentX+shown*(pW+pGap),pY,pW,pH);
        shown++;
    }
    // toast overlay
    if(GetTickCount()-toastAt<2300 && !toastText.empty()) {
        FontFamily ff(L"Segoe UI"); Font f(&ff,13,FontStyleRegular,UnitPixel);
        RectF layout(0,0,500,30); GraphicsPath path; path.AddString(toastText.c_str(),-1,&ff,FontStyleRegular,13,PointF(0,0),nullptr);
        float tw=std::max(260.0f,(float)toastText.size()*7.5f+40);
        SolidBrush tb(C(245,20,29,44)); Pen tp(C(255,255,65,87));
        RoundedRect(g,RectF((W-tw)/2,H-62,tw,40),12,&tb,&tp);
        DrawText(g,toastText,(W-tw)/2+18,H-50,13,TEXT_COLOR,false,tw-36);
    }
    HDC mem=CreateCompatibleDC(hdc); HBITMAP hb=CreateCompatibleBitmap(hdc,W,H);
    HGDIOBJ old=SelectObject(mem,hb);
    Bitmap* tmp=&bmp; Graphics out(mem); out.DrawImage(tmp,0,0,W,H);
    BitBlt(hdc,0,0,W,H,mem,0,0,SRCCOPY);
    SelectObject(mem,old); DeleteObject(hb); DeleteDC(mem);
    EndPaint(hwnd,&ps);
}

void Toast(const std::wstring& s) { toastText=s; toastAt=GetTickCount(); InvalidateRect(hwndMain,nullptr,FALSE); }

LRESULT CALLBACK SearchProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if(m==WM_KEYUP) {
        int len=GetWindowTextLengthW(h); std::wstring s(len+1,L'\0');
        GetWindowTextW(h,s.data(),len+1); s.resize(len); searchText=s;
        InvalidateRect(hwndMain,nullptr,FALSE);
    }
    return DefWindowProcW(h,m,w,l);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_CREATE: SetTimer(hwnd,1,16,nullptr); return 0;
    case WM_TIMER:
        anim+=0.025f; pulse+=0.06f;
        if(anim>6.283f) anim=0;
        InvalidateRect(hwnd,nullptr,FALSE); return 0;
    case WM_MOUSEMOVE: {
        mousePt={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
        int old=hovered; hovered=-1;
        for(int i=0;i<(int)items.size();i++) {
            RectF r=items[i].bounds;
            if(mousePt.x>=r.X && mousePt.x<=r.GetRight() && mousePt.y>=r.Y && mousePt.y<=r.GetBottom()) { hovered=i; break; }
        }
        if(old!=hovered) InvalidateRect(hwnd,nullptr,FALSE);
        TRACKMOUSEEVENT tme{sizeof(tme),TME_LEAVE,hwnd,0}; TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: hovered=-1; InvalidateRect(hwnd,nullptr,FALSE); return 0;
    case WM_LBUTTONUP: {
        int x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
        RECT r; GetClientRect(hwnd,&r); int W=r.right;
        float navX=300,navY=23,navW=std::min(730.0f,(float)W*.51f), each=navW/6;
        if(y>=23 && y<=72 && x>=navX && x<navX+navW) {
            activeTab=std::clamp((int)((x-navX)/each),0,5);
            Toast(tabs[activeTab]+L" selected — connect this section to your app logic.");
        } else if(hovered>=0) Toast(items[hovered].name+L" selected — connect its download action.");
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, RGB(237,242,255));
        SetBkColor(dc, RGB(11,16,26));
        if(!searchBrush) searchBrush = CreateSolidBrush(RGB(11,16,26));
        return (LRESULT)searchBrush;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: Paint(hwnd); return 0;
    case WM_SIZE: {
        if(hwndSearch) {
            int W = LOWORD(lParam);
            int sw = (int)std::min(255.0f,(float)W*.19f);
            MoveWindow(hwndSearch, W-sw-27+43, 40, sw-58, 25, TRUE);
        }
        InvalidateRect(hwnd,nullptr,FALSE); return 0;
    }
    case WM_DESTROY: KillTimer(hwnd,1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE, PWSTR,int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    if(GdiplusStartup(&gdiplusToken,&gdiplusStartupInput,nullptr)!=Ok) return 1;
    InitItems();
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName=L"KaimGfxWindow";
    RegisterClassExW(&wc);
    hwndMain=CreateWindowExW(0,wc.lpszClassName,L"KaimGfx | Creative Downloads",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,WIN_W,WIN_H,
        nullptr,nullptr,hInst,nullptr);
    if(!hwndMain) { GdiplusShutdown(gdiplusToken); return 1; }
    ShowWindow(hwndMain,nCmdShow); UpdateWindow(hwndMain);
    // Create a native edit control layered over the drawn search field.
    RECT r; GetClientRect(hwndMain,&r); int W=r.right;
    hwndSearch=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
        W-(int)std::min(255.0f,(float)W*.19f)-27+43,40, (int)std::min(255.0f,(float)W*.19f)-58,25,
        hwndMain,(HMENU)100,hInst,nullptr);
    SendMessageW(hwndSearch,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    oldSearchProc = (WNDPROC)SetWindowLongPtrW(hwndSearch,GWLP_WNDPROC,(LONG_PTR)SearchProc);
    SendMessageW(hwndSearch, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(0,0));
    SetTimer(hwndMain,2,200,nullptr);
    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if(searchBrush) DeleteObject(searchBrush);
    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
