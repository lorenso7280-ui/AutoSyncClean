#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "protocol.h"

namespace {
HWND g_main{}; std::atomic_bool g_stop{}, g_down{};
std::atomic_int g_x{}, g_y{}, g_downButton{-1};
std::array<std::atomic_uint64_t,4> g_hits{};
std::atomic_uint64_t g_seq{}; std::mutex g_mutex;
std::wstring g_event{L"Chua nhan lenh"};

std::wstring PipeName(){return std::wstring(vilab::kPipePrefix)+std::to_wstring(GetCurrentProcessId());}
std::array<RECT,4> Buttons(const RECT& r){
 int w=static_cast<int>(r.right-r.left), h=static_cast<int>(r.bottom-r.top);
 int m=std::max(18,w/18), gap=std::max(12,w/40), bw=std::max(80,(w-m*2-gap)/2);
 int bh=std::clamp(h/7,48,72), top=std::max(95,h*38/100);
 return {RECT{m,top,m+bw,top+bh},RECT{m+bw+gap,top,m+bw*2+gap,top+bh},
 RECT{m,top+bh+gap,m+bw,top+bh*2+gap},
 RECT{m+bw+gap,top+bh+gap,m+bw*2+gap,top+bh*2+gap}};
}
POINT Resolve(const vilab::Command& c,const RECT& r){
 POINT p{c.x,c.y}; if(c.flags&vilab::NormalizedCoordinates){
  p.x=MulDiv(c.x,std::max(1L,r.right),vilab::kCoordinateScale);
  p.y=MulDiv(c.y,std::max(1L,r.bottom),vilab::kCoordinateScale);}
 p.x=std::clamp<LONG>(p.x,0L,std::max(0L,r.right-1));
 p.y=std::clamp<LONG>(p.y,0L,std::max(0L,r.bottom-1)); return p;
}
int Hit(const std::array<RECT,4>& b,POINT p){for(int i=0;i<4;++i)if(PtInRect(&b[i],p))return i;return -1;}
void Event(const wchar_t* kind,const vilab::Command& c,POINT p,int hit){
 SYSTEMTIME t{};GetLocalTime(&t);std::lock_guard lock(g_mutex);
 g_event=std::wstring(kind)+L" X="+std::to_wstring(p.x)+L" Y="+std::to_wstring(p.y)+
 L" | step="+std::to_wstring(c.step)+L" | seq="+std::to_wstring(c.sequence)+L" | "+
 (hit>=0?L"TRUNG NUT "+std::to_wstring(hit+1):L"NGOAI NUT")+L" | "+
 std::to_wstring(t.wHour)+L":"+std::to_wstring(t.wMinute)+L":"+
 std::to_wstring(t.wSecond)+L"."+std::to_wstring(t.wMilliseconds);
}
void Apply(const vilab::Command& c){
 if(c.magic!=vilab::kMagic||!IsWindow(g_main))return;RECT r{};GetClientRect(g_main,&r);
 POINT p=Resolve(c,r);auto b=Buttons(r);int hit=Hit(b,p);g_x=p.x;g_y=p.y;g_seq=c.sequence;
 if(c.type==vilab::CommandType::Move)Event(L"MOVE",c,p,hit);
 else if(c.type==vilab::CommandType::LeftDown){g_down=true;g_downButton=hit;Event(L"LEFT DOWN",c,p,hit);}
 else if(c.type==vilab::CommandType::LeftUp){int old=g_downButton.exchange(-1);g_down=false;
  if(old>=0&&old==hit)++g_hits[hit];Event(L"LEFT UP",c,p,old==hit?hit:-1);}
 else if(c.type==vilab::CommandType::Reset){for(auto&v:g_hits)v=0;g_down=false;g_downButton=-1;Event(L"RESET",c,p,-1);}
 InvalidateRect(g_main,nullptr,FALSE);
}
void PipeLoop(){auto name=PipeName();while(!g_stop){HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_INBOUND,
 PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,1,sizeof(vilab::Command),sizeof(vilab::Command),250,nullptr);
 if(pipe==INVALID_HANDLE_VALUE)return;BOOL connected=ConnectNamedPipe(pipe,nullptr)||(GetLastError()==ERROR_PIPE_CONNECTED);
 if(connected){vilab::Command c{};DWORD n{};if(ReadFile(pipe,&c,sizeof(c),&n,nullptr)&&n==sizeof(c))Apply(c);}
 DisconnectNamedPipe(pipe);CloseHandle(pipe);}}
LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_CREATE){std::thread(PipeLoop).detach();return 0;}if(m==WM_SIZE){InvalidateRect(h,nullptr,TRUE);return 0;}
 if(m==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT r{};GetClientRect(h,&r);
  FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(20,38,60));
  std::wstring title=L"Virtual Input Target V3 | PID "+std::to_wstring(GetCurrentProcessId())+L" | "+
   std::to_wstring(r.right)+L"x"+std::to_wstring(r.bottom);TextOutW(dc,18,15,title.c_str(),static_cast<int>(title.size()));
  std::wstring state=L"Toa do: X="+std::to_wstring(g_x.load())+L", Y="+std::to_wstring(g_y.load())+
   L" | Seq="+std::to_wstring(g_seq.load());TextOutW(dc,18,42,state.c_str(),static_cast<int>(state.size()));
  auto b=Buttons(r);for(int i=0;i<4;++i){bool pressed=g_down&&g_downButton==i;
   HBRUSH br=CreateSolidBrush(pressed?RGB(230,145,30):RGB(255,199,55));FillRect(dc,&b[i],br);DeleteObject(br);
   FrameRect(dc,&b[i],reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));RECT tr=b[i];
   std::wstring label=L"BUOC "+std::to_wstring(i+1)+L" | so lan: "+std::to_wstring(g_hits[i].load());
   DrawTextW(dc,label.c_str(),-1,&tr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
  HBRUSH cur=CreateSolidBrush(g_down?RGB(240,70,60):RGB(25,185,105));HGDIOBJ old=SelectObject(dc,cur);
  int x=g_x.load(),y=g_y.load();Ellipse(dc,x-8,y-8,x+8,y+8);SelectObject(dc,old);DeleteObject(cur);
  std::wstring ev;{std::lock_guard lock(g_mutex);ev=g_event;}TextOutW(dc,18,r.bottom-50,ev.c_str(),static_cast<int>(ev.size()));
  const wchar_t* note=L"Nhan MOVE/DOWN/UP qua IPC; khong di chuyen con tro Windows.";
  TextOutW(dc,18,r.bottom-25,note,lstrlenW(note));EndPaint(h,&ps);return 0;}
 if(m==WM_DESTROY){g_stop=true;PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
}
int WINAPI wWinMain(HINSTANCE i,HINSTANCE,PWSTR,int show){WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=i;wc.lpfnWndProc=WndProc;
 wc.lpszClassName=vilab::kTargetClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
 RegisterClassExW(&wc);g_main=CreateWindowExW(0,wc.lpszClassName,L"Virtual Input Target V3",WS_OVERLAPPEDWINDOW,
 CW_USEDEFAULT,CW_USEDEFAULT,660,480,nullptr,nullptr,i,nullptr);if(!g_main)return 1;ShowWindow(g_main,show);UpdateWindow(g_main);
 MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);}
