/**
 * This file is part of Tales of Symphonia "Fix".
 *
 * Tales of Symphonia "Fix" is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Tales of Symphonia "Fix" is distributed in the hope that it will be
 * useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tales of Symphonia "Fix".
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#include <Windows.h>

#include "window.h"
#include "input.h"
#include "render.h"
#include "config.h"
#include "log.h"
#include "hook.h"

tsf::window_state_s tsf::window;

IsIconic_pfn            IsIconic_Original            = nullptr;
GetForegroundWindow_pfn GetForegroundWindow_Original = nullptr;
GetFocus_pfn            GetFocus_Original            = nullptr;
MoveWindow_pfn          MoveWindow_Original          = nullptr;
SetWindowPos_pfn        SetWindowPos_Original        = nullptr;
SetWindowLongA_pfn      SetWindowLongA_Original      = nullptr;

LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam );

BOOL
WINAPI
MoveWindow_Detour(
    _In_ HWND hWnd,
    _In_ int  X,
    _In_ int  Y,
    _In_ int  nWidth,
    _In_ int  nHeight,
    _In_ BOOL bRedraw )
{
  if (hWnd != tsf::window.hwnd) {
    return MoveWindow_Original ( hWnd,
                                   X, Y,
                                     nWidth, nHeight,
                                       bRedraw );
  }

  dll_log.Log (L"[Window Mgr][!] MoveWindow (...)");

  tsf::window.window_rect.left = X;
  tsf::window.window_rect.top  = Y;

  tsf::window.window_rect.right  = X + nWidth;
  tsf::window.window_rect.bottom = Y + nHeight;

  if (! config.render.allow_background)
    return MoveWindow_Original (hWnd, X, Y, nWidth, nHeight, bRedraw);
  else
    return TRUE;
}

//
// Lord oh mighty, why are all these special case exceptions necessary?
//
//   * Something is seriously borked either on my end or Namco's
//
BOOL
WINAPI
SetWindowPos_Detour(
    _In_     HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_     int  X,
    _In_     int  Y,
    _In_     int  cx,
    _In_     int  cy,
    _In_     UINT uFlags)
{
  if (hWnd != tsf::window.hwnd) {
    return SetWindowPos_Original ( hWnd, hWndInsertAfter,
                                     X, Y,
                                       cx, cy,
                                         uFlags );
  }

#if 0
  dll_log.Log ( L"[Window Mgr][!] SetWindowPos (...)");
#endif

  // Ignore this, because it's invalid.
  if ((cy == 0 || cx == 0) && (! (uFlags & SWP_NOSIZE))) {
    tsf::window.window_rect.right  = tsf::window.window_rect.left + tsf::RenderFix::width;
    tsf::window.window_rect.bottom = tsf::window.window_rect.top  + tsf::RenderFix::height;

    dll_log.Log ( L"[Window Mgr] *** Encountered invalid SetWindowPos (...) - "
                                    L"{cy:%lu, cx:%lu, uFlags:0x%x} - "
                                    L"<left:%lu, top:%lu, WxH=(%lux%lu)>",
                    cy, cx, uFlags,
                      tsf::window.window_rect.left, tsf::window.window_rect.top,
                        tsf::RenderFix::width, tsf::RenderFix::height );

    return TRUE;
  }

#if 0
  dll_log.Log ( L"  >> Before :: Top-Left: [%d/%d], Bottom-Right: [%d/%d]",
                  tsf::window.window_rect.left, tsf::window.window_rect.top,
                    tsf::window.window_rect.right, tsf::window.window_rect.bottom );
#endif

  int original_width  = tsf::window.window_rect.right -
                        tsf::window.window_rect.left;
  int original_height = tsf::window.window_rect.bottom -
                        tsf::window.window_rect.top;

  if (original_width <= 0 || original_height <= 0) {
    original_width  = tsf::RenderFix::width;
    original_height = tsf::RenderFix::height;
  }

  if (! (uFlags & SWP_NOMOVE)) {
    tsf::window.window_rect.left = X;
    tsf::window.window_rect.top  = Y;
  }

  if (! (uFlags & SWP_NOSIZE)) {
    tsf::window.window_rect.right  = tsf::window.window_rect.left + cx;
    tsf::window.window_rect.bottom = tsf::window.window_rect.top  + cy;
  } else {
    tsf::window.window_rect.right  = tsf::window.window_rect.left +
                                       original_width;
    tsf::window.window_rect.bottom = tsf::window.window_rect.top  +
                                       original_height;
  }

#if 0
  dll_log.Log ( L"  >> After :: Top-Left: [%d/%d], Bottom-Right: [%d/%d]",
                  tsf::window.window_rect.left, tsf::window.window_rect.top,
                    tsf::window.window_rect.right, tsf::window.window_rect.bottom );
#endif

  //
  // Fix an invalid scenario that happens for some reason...
  //
  if (tsf::window.window_rect.left == tsf::window.window_rect.right)
    tsf::window.window_rect.left = 0;
  if (tsf::window.window_rect.left == tsf::window.window_rect.right)
    tsf::window.window_rect.right = tsf::window.window_rect.left + tsf::RenderFix::width;

  if (tsf::window.window_rect.top == tsf::window.window_rect.bottom)
    tsf::window.window_rect.top = 0;
  if (tsf::window.window_rect.top == tsf::window.window_rect.bottom)
    tsf::window.window_rect.bottom = tsf::window.window_rect.top + tsf::RenderFix::height;



#if 0
  // Let the game manage its window position...
  if (! config.render.borderless)
    return SetWindowPos_Original (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
  else
    return TRUE;
#else
  if (config.window.borderless) {
    if (! tsf::RenderFix::fullscreen)
      return SetWindowPos_Original (hWnd, hWndInsertAfter, 0, 0, tsf::RenderFix::width, tsf::RenderFix::height, uFlags | SWP_SHOWWINDOW );
    else
      return TRUE;
  } else {
    return SetWindowPos_Original (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags | SWP_SHOWWINDOW );
  }
#endif
}

#include <tchar.h>
#include <tpcshrd.h>

LONG
WINAPI
SetWindowLongA_Detour (
  _In_ HWND hWnd,
  _In_ int nIndex,
  _In_ LONG dwNewLong
)
{
  if (hWnd != tsf::window.hwnd) {
    return SetWindowLongA_Original ( hWnd,
                                       nIndex,
                                         dwNewLong );
  }

  if (nIndex == GWL_EXSTYLE || nIndex == GWL_STYLE) {
    dll_log.Log ( L"[Window Mgr] SetWindowLongA (0x%06X, %s, 0x%06X)",
                    hWnd,
              nIndex == GWL_EXSTYLE ? L"GWL_EXSTYLE" :
                                      L" GWL_STYLE ",
                      dwNewLong );
  }

  // Override window styles
  if (nIndex == GWL_STYLE || nIndex == GWL_EXSTYLE) {
    // For proper return behavior
    DWORD dwOldStyle = GetWindowLong (hWnd, nIndex);

    // Allow the game to change its frame
    if (! config.window.borderless)
      return SetWindowLongA_Original (hWnd, nIndex, dwNewLong);

    return dwOldStyle;
  }

  return SetWindowLongA_Original (hWnd, nIndex, dwNewLong);
}

void
tsf::WindowManager::BorderManager::Disable (void)
{
  //dll_log.Log (L"BorderManager::Disable");

  window.borderless = true;

  //BringWindowToTop (window.hwnd);
  //SetActiveWindow  (window.hwnd);

  DWORD dwNewLong = window.style;

#if 0
  dwNewLong &= ~( WS_BORDER           | WS_CAPTION  | WS_THICKFRAME |
                  WS_OVERLAPPEDWINDOW | WS_MINIMIZE | WS_MAXIMIZE   |
                  WS_SYSMENU          | WS_GROUP );
#endif

  dwNewLong = WS_POPUP | WS_MINIMIZEBOX;

  SetWindowLongW (window.hwnd, GWL_STYLE,   dwNewLong);

  dwNewLong = window.style_ex;

#if 0
  dwNewLong &= ~( WS_EX_DLGMODALFRAME    | WS_EX_CLIENTEDGE    |
                  WS_EX_STATICEDGE       | WS_EX_WINDOWEDGE    |
                  WS_EX_OVERLAPPEDWINDOW | WS_EX_PALETTEWINDOW |
                  WS_EX_MDICHILD );
#endif

  dwNewLong = WS_EX_APPWINDOW;

  SetWindowLongW (window.hwnd, GWL_EXSTYLE, dwNewLong);

  AdjustWindow ();
}

void
tsf::WindowManager::BorderManager::Enable (void)
{
  window.borderless = false;

  SetWindowLongW (window.hwnd, GWL_STYLE,   (WS_OVERLAPPEDWINDOW & (~WS_THICKFRAME)) | WS_DLGFRAME);
  SetWindowLongW (window.hwnd, GWL_EXSTYLE,  WS_EX_APPWINDOW);

  AdjustWindow ();
}

void
tsf::WindowManager::BorderManager::AdjustWindow (void)
{
  BringWindowToTop (window.hwnd);
  SetActiveWindow  (window.hwnd);

  HMONITOR hMonitor =
    MonitorFromWindow ( tsf::window.hwnd,
                          MONITOR_DEFAULTTONEAREST );

  MONITORINFO mi = { 0 };
  mi.cbSize      = sizeof (mi);

  GetMonitorInfo (hMonitor, &mi);

  if (tsf::RenderFix::fullscreen) {
    //dll_log.Log (L"BorderManager::AdjustWindow - Fullscreen");

    SetWindowPos_Original ( tsf::window.hwnd,
                              HWND_TOP,
                                mi.rcMonitor.left,
                                mi.rcMonitor.top,
                                  mi.rcMonitor.right  - mi.rcMonitor.left,
                                  mi.rcMonitor.bottom - mi.rcMonitor.top,
                                    SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOSENDCHANGING );
    dll_log.Log ( L"[Border Mgr] FULLSCREEN => {Left: %li, Top: %li} - (WxH: %lix%li)",
                    mi.rcMonitor.left, mi.rcMonitor.top,
                      mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top );
  } else {
    //dll_log.Log (L"BorderManager::AdjustWindow - Windowed");

    AdjustWindowRect (&mi.rcWork, GetWindowLongW (tsf::window.hwnd, GWL_STYLE), FALSE);

    LONG mon_width  = mi.rcWork.right  - mi.rcWork.left;
    LONG mon_height = mi.rcWork.bottom - mi.rcWork.top;

    LONG win_width  = min (mon_width,  tsf::RenderFix::width);
    LONG win_height = min (mon_height, tsf::RenderFix::height);

    if (config.window.x_offset >= 0)
      window.window_rect.left  = mi.rcWork.left  + config.window.x_offset;
    else
      window.window_rect.right = mi.rcWork.right + config.window.x_offset + 1;


    if (config.window.y_offset >= 0)
      window.window_rect.top    = mi.rcWork.top    + config.window.y_offset;
    else
      window.window_rect.bottom = mi.rcWork.bottom + config.window.y_offset + 1;


    if (config.window.center && config.window.x_offset == 0 && config.window.y_offset == 0) {
      //dll_log.Log ( L"[Window Mgr] Center --> (%li,%li)",
      //                mi.rcWork.right - mi.rcWork.left,
      //                  mi.rcWork.bottom - mi.rcWork.top );

      window.window_rect.left = max (0, (mon_width  - win_width)  / 2);
      window.window_rect.top  = max (0, (mon_height - win_height) / 2);
    }


    if (config.window.x_offset >= 0)
      window.window_rect.right = window.window_rect.left  + win_width;
    else
      window.window_rect.left  = window.window_rect.right - win_width;


    if (config.window.y_offset >= 0)
      window.window_rect.bottom = window.window_rect.top    + win_height;
    else
      window.window_rect.top    = window.window_rect.bottom - win_height;


    SetWindowPos_Original ( tsf::window.hwnd,
                              HWND_TOP,
                                window.window_rect.left, window.window_rect.top,
                                  window.window_rect.right  - window.window_rect.left,
                                  window.window_rect.bottom - window.window_rect.top,
                                    SWP_FRAMECHANGED  | SWP_SHOWWINDOW | SWP_NOSENDCHANGING );

    dll_log.Log ( L"[Border Mgr] WINDOW => {Left: %li, Top: %li} - (WxH: %lix%li)",
                    window.window_rect.left, window.window_rect.top,
                      window.window_rect.right - window.window_rect.left,
                        window.window_rect.bottom - window.window_rect.top );
  }

  BringWindowToTop (window.hwnd);
  SetActiveWindow  (window.hwnd);
}

void
tsf::WindowManager::BorderManager::Toggle (void)
{
  if (window.borderless)
    BorderManager::Enable ();
  else
    BorderManager::Disable ();
}

BOOL
WINAPI
IsIconic_Detour (HWND hWnd)
{
  if (config.render.allow_background)
    return FALSE;
  else
    return IsIconic_Original (hWnd);
}

HWND
WINAPI
GetForegroundWindow_Detour (void)
{
  //dll_log.Log (L"[Window Mgr][!] GetForegroundWindow (...)");

  if (config.render.allow_background) {
    return tsf::RenderFix::hWndDevice;
  }

  return GetForegroundWindow_Original ();
}

HWND
WINAPI
GetFocus_Detour (void)
{
  //dll_log.Log (L"[Window Mgr][!] GetFocus (...)");

  if (config.render.allow_background) {
    return tsf::RenderFix::hWndDevice;
  }

  return GetFocus_Original ();
}


LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam )
{
  if (hWnd != tsf::window.hwnd)
    return CallWindowProc (tsf::window.WndProc_Original, hWnd, uMsg, wParam, lParam);

  static bool last_active = tsf::window.active;

  bool console_visible   =
    tsf::InputManager::Hooker::getInstance ()->isVisible ();

  //
  // The window activation state is changing, among other things we can take
  //   this opportunity to setup a special framerate limit.
  //
  if (tsf::window.active != last_active) {
    eTB_CommandProcessor* pCommandProc =
      SK_GetCommandProcessor           ();

#if 0
    eTB_CommandResult     result       =
      pCommandProc->ProcessCommandLine ("TargetFPS");

    eTB_VarStub <float>* pOriginalLimit = (eTB_VarStub <float>*)result.getVariable ();
#endif
    // Went from active to inactive (enforce background limit)
    if (! tsf::window.active) {
      pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.window.background_fps);

      // Show the cursor when focus is lost
      if (config.render.allow_background || config.window.borderless || (! tsf::RenderFix::fullscreen)) {
        if (GetSystemMetrics (SM_MOUSEPRESENT))
          while (ShowCursor (TRUE) <= 0)
            ;
      }

    // Went from inactive to active (restore foreground limit)
    } else {
      if (GetSystemMetrics (SM_MOUSEPRESENT))
        while (ShowCursor (FALSE) >= 0)
          ;

      pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.window.foreground_fps);
    }
  }

  last_active = tsf::window.active;

  // Ignore this event
  if (uMsg == WM_MOUSEACTIVATE && config.render.allow_background) {
    if ((HWND)wParam == tsf::window.hwnd) {
      dll_log.Log (L"[Window Mgr] WM_MOUSEACTIVATE ==> Activate and Eat");
      tsf::window.active = true;
      return MA_ACTIVATEANDEAT;
    }

    return MA_NOACTIVATE;
  }

  // Allow the game to run in the background
  if (uMsg == WM_ACTIVATEAPP || uMsg == WM_ACTIVATE || uMsg == WM_NCACTIVATE /*|| uMsg == WM_MOUSEACTIVATE*/) {
     {
      if (uMsg == WM_NCACTIVATE) {
        if (wParam == TRUE) {
          //dll_log.Log (L"[Window Mgr] Application Activated");

          tsf::window.active = true;
        } else {
          //dll_log.Log (L"[Window Mgr] Application Deactivated");

          tsf::window.active = false;
        }

        if (config.render.allow_background)
          return 0;
      }

      // We must fully consume one of these messages or audio will stop playing
      //   when the game loses focus, so do not simply pass this through to the
      //     default window procedure.
      if (config.render.allow_background)
        return 0;
    }
  }

  // Block the menu key from messing with stuff
  if ( true /*config.input.block_left_alt*/ &&
       (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP) ) {
    // Make an exception for Alt+Enter, for fullscreen mode toggle.
    //   F4 as well for exit
    if ( wParam != VK_RETURN && wParam != VK_F4 &&
         wParam != VK_TAB    && wParam != VK_PRINT )
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  bool background_render =
    config.render.allow_background && (! tsf::window.active);

  // Block keyboard input to the game while the console is visible
  if (console_visible || (background_render && uMsg != WM_SYSKEYDOWN && uMsg != WM_SYSKEYUP)) {
    // Only prevent the mouse from working while the window is in the bg
    //if (background_render && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
      return 0;
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    // Block RAW Input
    if (uMsg == WM_INPUT)
      return 0;
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  return CallWindowProc (tsf::window.WndProc_Original, hWnd, uMsg, wParam, lParam);
}



void
tsf::WindowManager::Init (void)
{
  if (config.window.borderless)
    window.style = 0x10000000;
  else
    window.style = 0x90CA0000;

  TSFix_CreateDLLHook ( L"user32.dll", "SetWindowLongA",
                        SetWindowLongA_Detour,
              (LPVOID*)&SetWindowLongA_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "SetWindowPos",
                        SetWindowPos_Detour,
              (LPVOID*)&SetWindowPos_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "MoveWindow",
                        MoveWindow_Detour,
              (LPVOID*)&MoveWindow_Original );

// Not used by ToS
  TSFix_CreateDLLHook ( L"user32.dll", "GetForegroundWindow",
                        GetForegroundWindow_Detour,
              (LPVOID*)&GetForegroundWindow_Original );

// Used, but not for anything important...
  TSFix_CreateDLLHook ( L"user32.dll", "GetFocus",
                        GetFocus_Detour,
              (LPVOID*)&GetFocus_Original );

// Not used by ToS
#if 0
  TSFix_CreateDLLHook ( L"user32.dll", "IsIconic",
                        IsIconic_Detour,
              (LPVOID*)&IsIconic_Original );
#endif

  CommandProcessor* comm_proc = CommandProcessor::getInstance ();
}

void
tsf::WindowManager::Shutdown (void)
{
}


tsf::WindowManager::
  CommandProcessor::CommandProcessor (void)
{
  foreground_fps_ = new eTB_VarStub <float> (&config.window.foreground_fps, this);
  background_fps_ = new eTB_VarStub <float> (&config.window.background_fps, this);

  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

  pCommandProc->AddVariable ("Window.BackgroundFPS", background_fps_);
  pCommandProc->AddVariable ("Window.ForegroundFPS", foreground_fps_);

  // If the user has an FPS limit preference, set it up now...
  pCommandProc->ProcessCommandFormatted ("TargetFPS %f",        config.window.foreground_fps);
  pCommandProc->ProcessCommandFormatted ("LimiterTolerance %f", config.stutter.tolerance);
  pCommandProc->ProcessCommandFormatted ("MaxDeltaTime %d",     config.stutter.shortest_sleep);
}

bool
  tsf::WindowManager::
    CommandProcessor::OnVarChange (eTB_Variable* var, void* val)
{
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

  bool known = false;

  if (var == background_fps_) {
    known = true;

    // Range validation
    if (val != nullptr && *(float *)val >= 0.0f) {
      config.window.background_fps = *(float *)val;

      // How this was changed while the window was inactive is a bit of a
      //   mystery, but whatever :P
      if ((! window.active))
        pCommandProc->ProcessCommandFormatted ("TargetFPS %f", *(float *)val);

      return true;
    }
  }

  if (var == foreground_fps_) {
    known = true;

    // Range validation
    if (val != nullptr && *(float *)val >= 0.0f) {
      config.window.foreground_fps = *(float *)val;

      // Immediately apply limiter changes
      if (window.active)
        pCommandProc->ProcessCommandFormatted ("TargetFPS %f", *(float *)val);

      return true;
    }
  }

  if (! known) {
    dll_log.Log ( L"[Window Mgr] UNKNOWN Variable Changed (%p --> %p)",
                    var,
                      val );
  }

  return false;
}

tsf::WindowManager::CommandProcessor*
   tsf::WindowManager::CommandProcessor::pCommProc;