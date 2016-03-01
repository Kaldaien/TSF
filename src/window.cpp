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
#if 0
  dll_log.Log ( L"[Window Mgr][!] SetWindowPos (...)");
#endif

  // Ignore this, because it's invalid.
  if (cy == 0 || cx == 0 && (! (uFlags & SWP_NOSIZE))) {
    tsf::window.window_rect.right  = tsf::window.window_rect.left + tsf::RenderFix::width;
    tsf::window.window_rect.bottom = tsf::window.window_rect.top  + tsf::RenderFix::height;

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
  if (config.render.borderless) {
    if (! tsf::RenderFix::fullscreen)
      return SetWindowPos_Original (hWnd, hWndInsertAfter, 0, 0, tsf::RenderFix::width, tsf::RenderFix::height, uFlags);
    else
      return TRUE;
  } else {
    return SetWindowPos_Original (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
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
  if (nIndex == GWL_EXSTYLE || nIndex == GWL_STYLE) {
    dll_log.Log ( L"[Window Mgr] SetWindowLongA (0x%06X, %s, 0x%06X)",
                    hWnd,
              nIndex == GWL_EXSTYLE ? L"GWL_EXSTYLE" :
                                      L" GWL_STYLE ",
                      dwNewLong );
  }

  //
  // Sort of a dirty hack, but this is an opportune place to call this
  //
  RegisterTouchWindow (hWnd, 0);

  const DWORD dwDieTablet =
    TABLET_DISABLE_FLICKFALLBACKKEYS |
    TABLET_DISABLE_FLICKS            |
    TABLET_DISABLE_PENBARRELFEEDBACK |
    TABLET_DISABLE_PENTAPFEEDBACK    |
    TABLET_DISABLE_PRESSANDHOLD      |
    TABLET_DISABLE_SMOOTHSCROLLING   |
    TABLET_DISABLE_TOUCHUIFORCEOFF;

  ATOM atom = GlobalAddAtom (MICROSOFT_TABLETPENSERVICE_PROPERTY);
              SetProp (hWnd, MICROSOFT_TABLETPENSERVICE_PROPERTY, reinterpret_cast<HANDLE>(dwDieTablet));
     GlobalDeleteAtom (atom);

  // Override window styles
  if (nIndex == GWL_STYLE || nIndex == GWL_EXSTYLE) {
    // For proper return behavior
    DWORD dwOldStyle = GetWindowLong (hWnd, nIndex);

    //
    // Ignore in Tales of Symphonia, we know the style that is always used
    //
    //if (nIndex == GWL_STYLE)
      //tsf::window.style = dwNewLong;

    if (nIndex == GWL_EXSTYLE)
      tsf::window.style_ex = dwNewLong;

    tsf::window.activating = true;
    SetActiveWindow     (tsf::RenderFix::hWndDevice);
    tsf::window.activating = false;

    // Allow the game to change its frame
    if (! config.render.borderless)
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

  // Handle window style on creation
  //
  //   This should be figured out at window creation time instead
  //if (window.style == 0)
    //window.style = GetWindowLong (window.hwnd, GWL_STYLE);

  //if (window.style_ex == 0)
    //window.style_ex = GetWindowLong (window.hwnd, GWL_EXSTYLE);

  DWORD dwNewLong = window.style;

  dwNewLong &= ~( WS_BORDER           | WS_CAPTION  | WS_THICKFRAME |
                  WS_OVERLAPPEDWINDOW | WS_MINIMIZE | WS_MAXIMIZE   |
                  WS_SYSMENU          | WS_GROUP );

  SetWindowLongW (window.hwnd, GWL_STYLE,   dwNewLong);

  dwNewLong = window.style_ex;

  dwNewLong &= ~( WS_EX_DLGMODALFRAME    | WS_EX_CLIENTEDGE    |
                  WS_EX_STATICEDGE       | WS_EX_WINDOWEDGE    |
                  WS_EX_OVERLAPPEDWINDOW | WS_EX_PALETTEWINDOW |
                  WS_EX_MDICHILD );

  SetWindowLongW (window.hwnd, GWL_EXSTYLE, dwNewLong);

  AdjustWindow ();
}

void
tsf::WindowManager::BorderManager::Enable (void)
{
  window.borderless = false;

  // Handle window style on creation
  //
  //   This should be figured out at window creation time instead
  //if (window.style == 0)
    //window.style = GetWindowLong (window.hwnd, GWL_STYLE);

  //if (window.style_ex == 0)
    //window.style_ex = GetWindowLong (window.hwnd, GWL_EXSTYLE);

  SetWindowLongW (window.hwnd, GWL_STYLE,   window.style);
  SetWindowLongW (window.hwnd, GWL_EXSTYLE, window.style_ex);

  AdjustWindow ();
}

void
tsf::WindowManager::BorderManager::AdjustWindow (void)
{
  tsf::window.activating = true;

  HMONITOR hMonitor =
    MonitorFromWindow ( tsf::RenderFix::hWndDevice,
                          MONITOR_DEFAULTTONEAREST );

  MONITORINFO mi = { 0 };
  mi.cbSize      = sizeof (mi);

  GetMonitorInfo (hMonitor, &mi);

  if (tsf::RenderFix::fullscreen) {
    //dll_log.Log (L"BorderManager::AdjustWindow - Fullscreen");

    SetWindowPos_Original ( tsf::RenderFix::hWndDevice,
                              HWND_TOP,
                                mi.rcMonitor.left,
                                mi.rcMonitor.top,
                                  mi.rcMonitor.right  - mi.rcMonitor.left,
                                  mi.rcMonitor.bottom - mi.rcMonitor.top,
                                    SWP_FRAMECHANGED );
  } else {
    //dll_log.Log (L"BorderManager::AdjustWindow - Windowed");

    window.window_rect.left = mi.rcMonitor.left;
    window.window_rect.top  = mi.rcMonitor.top;

    window.window_rect.right  = window.window_rect.left + tsf::RenderFix::width;
    window.window_rect.bottom = window.window_rect.top  + tsf::RenderFix::height;

    SetWindowPos_Original ( tsf::RenderFix::hWndDevice,
                              HWND_TOP,
                                window.window_rect.left, window.window_rect.top,
                                  window.window_rect.right  - window.window_rect.left,
                                  window.window_rect.bottom - window.window_rect.top,
                                    SWP_FRAMECHANGED );
  }

  BringWindowToTop (tsf::RenderFix::hWndDevice);
  SetActiveWindow  (tsf::RenderFix::hWndDevice);
  tsf::window.activating = false;
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

  if (config.render.allow_background && (! tsf::window.activating)) {
    return tsf::RenderFix::hWndDevice;
  }

  return GetForegroundWindow_Original ();
}

HWND
WINAPI
GetFocus_Detour (void)
{
  //dll_log.Log (L"[Window Mgr][!] GetFocus (...)");

  if (config.render.allow_background && (! tsf::window.activating)) {
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
  bool last_active = tsf::window.active;
  tsf::window.active = GetForegroundWindow_Original () == tsf::window.hwnd ||
                       GetForegroundWindow_Original () == nullptr;

  bool console_visible   =
    tsf::InputManager::Hooker::getInstance ()->isVisible ();

  bool background_render =
    config.render.allow_background && (! tsf::window.active);

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
    if (! tsf::window.active)
      pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.render.background_fps);

    // Went from inactive to active (restore foreground limit)
    else {
      tsf::window.activating = true;
      SetActiveWindow     (tsf::RenderFix::hWndDevice);
      tsf::window.activating = false;

      tsf::InputManager::FixAltTab ();

      pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.render.foreground_fps);
    }

    // Unrestrict the mouse when the app is deactivated
    if ((! tsf::window.active) && config.render.allow_background) {
      ClipCursor_Original (nullptr);
      SetCursorPos        (tsf::window.cursor_pos.x, tsf::window.cursor_pos.y);
      ShowCursor          (TRUE);
    }

    // Restore it when the app is activated
    else {
      GetCursorPos        (&tsf::window.cursor_pos);
      ShowCursor          (FALSE);
      ClipCursor_Original (&tsf::window.cursor_clip);
    }
  }

  // Ignore this event
  if (uMsg == WM_MOUSEACTIVATE && config.render.allow_background) {
    return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  // Allow the game to run in the background
  if (uMsg == WM_ACTIVATEAPP || uMsg == WM_ACTIVATE || uMsg == WM_NCACTIVATE /*|| uMsg == WM_MOUSEACTIVATE*/) {
    if (config.render.allow_background) {
      // We must fully consume one of these messages or audio will stop playing
      //   when the game loses focus, so do not simply pass this through to the
      //     default window procedure.
      return 0;//DefWindowProc (hWnd, uMsg, wParam, lParam);
    }
  }


  // Block keyboard input to the game while the console is visible
  if (console_visible || background_render) {
    // Only prevent the mouse from working while the window is in the bg
    //if (background_render && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
      return 0;//DefWindowProc (hWnd, uMsg, wParam, lParam);

    // Block RAW Input
    if (uMsg == WM_INPUT)
      return 0;//DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  // Block the menu key from messing with stuff
  if (config.input.block_left_alt &&
      (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)) {
    // Make an exception for Alt+Enter, for fullscreen mode toggle.
    //   F4 as well for exit
    if (wParam != VK_RETURN && wParam != VK_F4)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

#if 0
  const int32_t  WM_TABLET_QUERY_SYSTEM_GESTURE_STATUS  = 0x02D2;
  const uint32_t SYSTEM_GESTURE_STATUS_NOHOLD           = 0x0001;
  const uint32_t SYSTEM_GESTURE_STATUS_TOUCHUI_FORCEOFF = 0x0200;

  if (uMsg == WM_TABLET_QUERY_SYSTEM_GESTURE_STATUS) {
    dll_log.Log (L"[Window Mgr] Disabling Tablet Gesture Support...");
    int result = 0x00;
    result |= (SYSTEM_GESTURE_STATUS_NOHOLD |
               SYSTEM_GESTURE_STATUS_TOUCHUI_FORCEOFF);
    return result;
  }

  // Ignore ALL touch-based windows messages, because some of them crash the game
  bool tablet_msg = false;

  if (uMsg >= WM_TABLET_FIRST        && uMsg <= WM_TABLET_LAST)    tablet_msg = true;
  if (uMsg >= WM_PENWINFIRST         && uMsg <= WM_PENWINLAST)     tablet_msg = true;
  if (uMsg >= WM_POINTERDEVICECHANGE && uMsg <= DM_POINTERHITTEST) tablet_msg = true;
  if (uMsg == WM_GESTURE             || uMsg == WM_GESTURENOTIFY)  tablet_msg = true;

  if (tablet_msg) {
    // Close the handle; we never wanted it in the first place.
    if (uMsg == WM_TOUCH)
      CloseTouchInputHandle ((HTOUCHINPUT)lParam);

    dll_log.Log ( L"[Window Mgr] >> Filtering Out Tablet Input "
                  L"(uMsg=0x%04X, wParam=%p, lParam=%p)",
                    uMsg, wParam, lParam );
    //return 0;
    return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }
#endif

#if 0
  if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) {
    static POINT last_p = { LONG_MIN, LONG_MIN };

    POINT p;

    p.x = MAKEPOINTS (lParam).x;
    p.y = MAKEPOINTS (lParam).y;

    if (/*game_state.needsFixedMouseCoords () &&*/config.render.aspect_correction) {
      // Only do this if cursor actually moved!
      //
      //   Otherwise, it tricks the game into thinking the input device changed
      //     from gamepad to mouse (and changes buessagetton icons).
      if (last_p.x != p.x || last_p.y != p.y) {
        tsf::InputManager::CalcCursorPos (&p);

        last_p = p;
      }

      return CallWindowProc (original_wndproc, hWnd, uMsg, wParam, MAKELPARAM (p.x, p.y));
    }

    last_p = p;
  }
#endif

  return CallWindowProc (tsf::window.WndProc_Original, hWnd, uMsg, wParam, lParam);
}



void
tsf::WindowManager::Init (void)
{
  if (config.render.borderless)
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
  foreground_fps_ = new eTB_VarStub <float> (&config.render.foreground_fps, this);
  background_fps_ = new eTB_VarStub <float> (&config.render.background_fps, this);

  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

  pCommandProc->AddVariable ("Window.BackgroundFPS", background_fps_);
  pCommandProc->AddVariable ("Window.ForegroundFPS", foreground_fps_);

  // If the user has an FPS limit preference, set it up now...
  pCommandProc->ProcessCommandFormatted ("TargetFPS %f",        config.render.foreground_fps);
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
      config.render.background_fps = *(float *)val;

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
      config.render.foreground_fps = *(float *)val;

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