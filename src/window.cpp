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
SetWindowPos_pfn        SetWindowPos_Original        = nullptr;
SetWindowLongA_pfn      SetWindowLongA_Original      = nullptr;

BOOL
WINAPI
SetWindowPos_Detour(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags)
{
  // Let the game manage its window position...
  if (! config.render.allow_background)
    return SetWindowPos_Original (hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
  else
    return TRUE;
}

LRESULT
CALLBACK
DetourWindowProc ( _In_  HWND   hWnd,
                   _In_  UINT   uMsg,
                   _In_  WPARAM wParam,
                   _In_  LPARAM lParam );

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
    dll_log.Log ( L"SetWindowLongA (0x%06X, %s, 0x%06X)",
                    hWnd,
              nIndex == GWL_EXSTYLE ? L"GWL_EXSTYLE" :
                                      L"GWL_STYLE",
                      dwNewLong );
  }

  tsf::RenderFix::hWndDevice = hWnd;
  tsf::window.hwnd           = hWnd;

  // Setup window message detouring as soon as a window is created..
  if (tsf::window.WndProc_Original == nullptr) {
    tsf::window.WndProc_Original =
      (WNDPROC)GetWindowLong (tsf::RenderFix::hWndDevice, GWL_WNDPROC);

    SetWindowLongA_Original ( tsf::RenderFix::hWndDevice,
                                GWL_WNDPROC,
                                  (LONG)DetourWindowProc );
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

  if (config.render.allow_background) {
    if (nIndex == GWL_STYLE) {
      tsf::window.borderless = true;
      tsf::window.style      = dwNewLong;
      dwNewLong &= ~( WS_BORDER           | WS_CAPTION  | WS_THICKFRAME |
                      WS_OVERLAPPEDWINDOW | WS_MINIMIZE | WS_MAXIMIZE   |
                      WS_SYSMENU          | WS_GROUP );
    }

    if (nIndex == GWL_EXSTYLE) {
      tsf::window.borderless = true;
      tsf::window.style_ex   = dwNewLong;
      dwNewLong &= ~( WS_EX_DLGMODALFRAME    | WS_EX_CLIENTEDGE    |
                      WS_EX_STATICEDGE       | WS_EX_WINDOWEDGE    |
                      WS_EX_OVERLAPPEDWINDOW | WS_EX_PALETTEWINDOW |
                      WS_EX_MDICHILD );
    }

    LONG ret =
      SetWindowLongA_Original (hWnd, nIndex, dwNewLong);

    HMONITOR hMonitor =
      MonitorFromWindow ( tsf::RenderFix::hWndDevice,
                            MONITOR_DEFAULTTOPRIMARY );

    MONITORINFO mi;
    mi.cbSize = sizeof (mi);

    GetMonitorInfo (hMonitor, &mi);

    if (tsf::RenderFix::fullscreen) {
      SetWindowPos_Original ( tsf::RenderFix::hWndDevice,
                                HWND_NOTOPMOST,
                                  mi.rcMonitor.left,
                                  mi.rcMonitor.top,
                                    mi.rcMonitor.right  - mi.rcMonitor.left,
                                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                                      SWP_FRAMECHANGED );
    } else {
      // Don't support windowed borderless windows -- kinda funny notion, right? :P
    }

    return ret;
  } else {
    tsf::window.borderless = false;
    return SetWindowLongA_Original (hWnd, nIndex, dwNewLong);
  }
}

void
tsf::WindowManager::BorderManager::Enable (void)
{
  if (! tsf::window.borderless)
    Toggle ();
}

void
tsf::WindowManager::BorderManager::Disable (void)
{
  if (! tsf::window.borderless)
    Toggle ();
}

void
tsf::WindowManager::BorderManager::Toggle (void)
{
  tsf::window.borderless = (! window.borderless);

  if (! window.borderless) {
    SetWindowLongW (window.hwnd, GWL_STYLE,   window.style);
    SetWindowLongW (window.hwnd, GWL_EXSTYLE, window.style_ex);
  } else {
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
  }

    HMONITOR hMonitor =
      MonitorFromWindow ( tsf::RenderFix::hWndDevice,
                            MONITOR_DEFAULTTONEAREST );

    MONITORINFO mi;
    mi.cbSize = sizeof (mi);

    GetMonitorInfo (hMonitor, &mi);

    SetWindowPos  ( tsf::RenderFix::hWndDevice,
                      HWND_NOTOPMOST,
                        mi.rcMonitor.left, mi.rcMonitor.top,
                          tsf::RenderFix::width, tsf::RenderFix::height,
                            SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE );
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
  if (config.render.allow_background) {
    return tsf::RenderFix::hWndDevice;
  }

  return GetForegroundWindow_Original ();
}

HWND
WINAPI
GetFocus_Detour (void)
{
  //dll_log.Log (L" [!] GetFocus (...)");

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
  bool console_visible   =
    tsf::InputManager::Hooker::getInstance ()->isVisible ();

  bool background_render =
    config.render.allow_background && (! tsf::window.active);

  // Block keyboard input to the game while the console is visible
  if (console_visible || background_render) {
    // Only prevent the mouse from working while the window is in the bg
    if (background_render && uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      return DefWindowProc (hWnd, uMsg, wParam, lParam);

    if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
      return 0;
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);

    // Block RAW Input
    if (uMsg == WM_INPUT)
      return 0;
      //return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  const int32_t  WM_TABLET_QUERY_SYSTEM_GESTURE_STATUS  = 0x02D2;
  const uint32_t SYSTEM_GESTURE_STATUS_NOHOLD           = 0x0001;
  const uint32_t SYSTEM_GESTURE_STATUS_TOUCHUI_FORCEOFF = 0x0200;

  if (uMsg == WM_TABLET_QUERY_SYSTEM_GESTURE_STATUS) {
    dll_log.Log (L"Disabling Tablet Gesture Support...");
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

    dll_log.Log ( L" >> Filtering Out Tablet Input "
                  L"(uMsg=0x%04X, wParam=%p, lParam=%p)",
                    uMsg, wParam, lParam );
    //return 0;
    return DefWindowProc (hWnd, uMsg, wParam, lParam);
  }

  // Ignore this event
  if (uMsg == WM_MOUSEACTIVATE && config.render.allow_background)
    return DefWindowProc (hWnd, uMsg, wParam, lParam);

  // Allow the game to run in the background
  if (uMsg == WM_ACTIVATEAPP || uMsg == WM_ACTIVATE || uMsg == WM_NCACTIVATE) {
    // Consume the Alt key
    tsf::InputManager::Hooker::getInstance ()->consumeKey (VK_LMENU);
    tsf::InputManager::Hooker::getInstance ()->consumeKey (VK_RMENU);
    tsf::InputManager::Hooker::getInstance ()->consumeKey (VK_MENU);

    bool last_active = tsf::window.active;

    tsf::window.active = wParam;

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
      else
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

    if (config.render.allow_background) {
      // We must fully consume one of these messages or audio will stop playing
      //   when the game loses focus, so do not simply pass this through to the
      //     default window procedure.
      return 0;// DefWindowProc (hWnd, uMsg, wParam, lParam);
    }
  }


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
  // Stupid game is using the old ANSI API
  TSFix_CreateDLLHook ( L"user32.dll", "SetWindowLongA",
                        SetWindowLongA_Detour,
              (LPVOID*)&SetWindowLongA_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "SetWindowPos",
                        SetWindowPos_Detour,
              (LPVOID*)&SetWindowPos_Original );

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
    dll_log.Log ( L" [Window Manager]: UNKNOWN Variable Changed (%p --> %p)",
                    var,
                      val );
  }

  return false;
}

tsf::WindowManager::CommandProcessor*
   tsf::WindowManager::CommandProcessor::pCommProc;