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
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <cstdint>

#include <comdef.h>

#include <dinput.h>
#pragma comment (lib, "dxguid.lib")

#include "log.h"
#include "config.h"
#include "window.h"
#include "render.h"
#include "render/textures.h"
#include "hook.h"

#include <mmsystem.h>
#pragma comment (lib, "winmm.lib")

#include "input.h"

///////////////////////////////////////////////////////////////////////////////
//
// DirectInput 8
//
///////////////////////////////////////////////////////////////////////////////
typedef HRESULT (WINAPI *IDirectInput8_CreateDevice_pfn)(
  IDirectInput8       *This,
  REFGUID              rguid,
  LPDIRECTINPUTDEVICE *lplpDirectInputDevice,
  LPUNKNOWN            pUnkOuter
);

typedef HRESULT (WINAPI *IDirectInputDevice8_GetDeviceState_pfn)(
  LPDIRECTINPUTDEVICE  This,
  DWORD                cbData,
  LPVOID               lpvData
);

typedef HRESULT (WINAPI *IDirectInputDevice8_SetCooperativeLevel_pfn)(
  LPDIRECTINPUTDEVICE  This,
  HWND                 hwnd,
  DWORD                dwFlags
);

LPVOID
        DirectInput8Create_Hook                          = nullptr;
IDirectInput8_CreateDevice_pfn
        IDirectInput8_CreateDevice_Original              = nullptr;
IDirectInputDevice8_GetDeviceState_pfn
        IDirectInputDevice8_GetDeviceState_Original      = nullptr;
IDirectInputDevice8_SetCooperativeLevel_pfn
        IDirectInputDevice8_SetCooperativeLevel_Original = nullptr;



#define DINPUT8_CALL(_Ret, _Call) {                                     \
  dll_log.LogEx (true, L"[   Input  ]  Calling original function: ");   \
  (_Ret) = (_Call);                                                     \
  _com_error err ((_Ret));                                              \
  if ((_Ret) != S_OK)                                                   \
    dll_log.LogEx (false, L"(ret=0x%04x - %s)\n", err.WCode (),         \
                                                  err.ErrorMessage ()); \
  else                                                                  \
    dll_log.LogEx (false, L"(ret=S_OK)\n");                             \
}

#define __PTR_SIZE   sizeof LPCVOID 
#define __PAGE_PRIVS PAGE_EXECUTE_READWRITE 
 
#define DI8_VIRTUAL_OVERRIDE(_Base,_Index,_Name,_Override,_Original,_Type) {   \
   void** vftable = *(void***)*_Base;                                          \
                                                                               \
   if (vftable [_Index] != _Override) {                                        \
     DWORD dwProtect;                                                          \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, __PAGE_PRIVS, &dwProtect); \
                                                                               \
     /*if (_Original == NULL)                                                */\
       _Original = (##_Type)vftable [_Index];                                  \
                                                                               \
     vftable [_Index] = _Override;                                             \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, dwProtect, &dwProtect);    \
                                                                               \
  }                                                                            \
 }



struct di8_keyboard_s {
  LPDIRECTINPUTDEVICE pDev = nullptr;
  uint8_t             state [512]; // Handle overrun just in case
} _dik;

// I don't care about joysticks, let them continue working while
//   the window does not have focus...

HRESULT
WINAPI
IDirectInputDevice8_GetDeviceState_Detour ( LPDIRECTINPUTDEVICE        This,
                                            DWORD                      cbData,
                                            LPVOID                     lpvData )
{
  // This seems to be the source of some instability in the game.
  if (/*tsf::RenderFix::tracer.log ||*/ This == _dik.pDev && cbData > 256 || lpvData == nullptr)
    dll_log.Log ( L"[   Input  ] Suspicious GetDeviceState - cbData: "
                  L"%lu, lpvData: %p",
                    cbData,
                      lpvData );

  HRESULT hr;
  hr = IDirectInputDevice8_GetDeviceState_Original ( This,
                                                       cbData,
                                                         lpvData );

  if (SUCCEEDED (hr)) {
    // For input faking (keyboard)
    if (config.render.allow_background && This == _dik.pDev) {
      if (tsf::window.active) {
        memcpy (_dik.state, lpvData, cbData);
      } else {
        ((uint8_t *)lpvData) [DIK_LALT]   = 0x0;
        ((uint8_t *)lpvData) [DIK_RALT]   = 0x0;
        ((uint8_t *)lpvData) [DIK_TAB]    = 0x0;
        ((uint8_t *)lpvData) [DIK_ESCAPE] = 0x0;
        ((uint8_t *)lpvData) [DIK_UP]     = 0x0;
        ((uint8_t *)lpvData) [DIK_DOWN]   = 0x0;
        ((uint8_t *)lpvData) [DIK_LEFT]   = 0x0;
        ((uint8_t *)lpvData) [DIK_RIGHT]  = 0x0;
        ((uint8_t *)lpvData) [DIK_RETURN] = 0x0;
        ((uint8_t *)lpvData) [DIK_LMENU]  = 0x0;
        ((uint8_t *)lpvData) [DIK_RMENU]  = 0x0;

        memcpy (lpvData, _dik.state, cbData);
      }
    }
  }

  return hr;
}

HRESULT
WINAPI
IDirectInputDevice8_SetCooperativeLevel_Detour ( LPDIRECTINPUTDEVICE  This,
                                                 HWND                 hwnd,
                                                 DWORD                dwFlags )
{
  if (config.input.block_windows)
    dwFlags |= DISCL_NOWINKEY;

  if (config.render.allow_background) {
    dwFlags &= ~DISCL_EXCLUSIVE;
    dwFlags &= ~DISCL_BACKGROUND;

    dwFlags |= DISCL_NONEXCLUSIVE;
    dwFlags |= DISCL_FOREGROUND;

    return IDirectInputDevice8_SetCooperativeLevel_Original (This, hwnd, dwFlags);
  }

  return IDirectInputDevice8_SetCooperativeLevel_Original (This, hwnd, dwFlags);
}

HRESULT
WINAPI
IDirectInput8_CreateDevice_Detour ( IDirectInput8       *This,
                                    REFGUID              rguid,
                                    LPDIRECTINPUTDEVICE *lplpDirectInputDevice,
                                    LPUNKNOWN            pUnkOuter )
{
  const wchar_t* wszDevice = (rguid == GUID_SysKeyboard) ? L"Default System Keyboard" :
                                (rguid == GUID_SysMouse) ? L"Default System Mouse" :
                                                           L"Other Device";

  dll_log.Log ( L"[   Input  ][!] IDirectInput8::CreateDevice (%08Xh, %s, %08Xh, %08Xh)",
                  This,
                    wszDevice,
                      lplpDirectInputDevice,
                        pUnkOuter );

  HRESULT hr;
  DINPUT8_CALL ( hr,
                  IDirectInput8_CreateDevice_Original ( This,
                                                         rguid,
                                                          lplpDirectInputDevice,
                                                           pUnkOuter ) );

  if (SUCCEEDED (hr)) {
    void** vftable = *(void***)*lplpDirectInputDevice;

    TSFix_CreateFuncHook ( L"IDirectInputDevice8::GetDeviceState",
                           vftable [9],
                           IDirectInputDevice8_GetDeviceState_Detour,
                 (LPVOID*)&IDirectInputDevice8_GetDeviceState_Original );

    TSFix_EnableHook (vftable [9]);

    TSFix_CreateFuncHook ( L"IDirectInputDevice8::SetCooperativeLevel",
                           vftable [13],
                           IDirectInputDevice8_SetCooperativeLevel_Detour,
                 (LPVOID*)&IDirectInputDevice8_SetCooperativeLevel_Original );

    TSFix_EnableHook (vftable [13]);

    if (rguid == GUID_SysKeyboard)
      _dik.pDev = *lplpDirectInputDevice;
  }

  return hr;
}


///////////////////////////////////////////////////////////////////////////////
//
// General Utility Functions
//
///////////////////////////////////////////////////////////////////////////////

//
// Make sure the game does not think that Alt or Tab are stuck
//
void
tsf::InputManager::FixAltTab (void)
{
  // Make sure no reference counters used to determine keyboard state
  //   are mismatched when alt-tabbing.
  for (int i = 0; i < 255; i++) {

    // Queuing this many window messages seems like a terrible idea, but
    //   it gets the job done.

    CallWindowProc (
      tsf::window.WndProc_Original,
        tsf::window.hwnd,
          WM_KEYUP,
            LOWORD (i),
              0 );
    CallWindowProc (
      tsf::window.WndProc_Original,
        tsf::window.hwnd,
          WM_SYSKEYUP,
            LOWORD (i),
              0 );
  }

  // The game uses DirectInput keyboard state in addition to
   //  Win32 API messages, so set this stuff for consistency.
  ((uint8_t *)_dik.state) [DIK_LALT]   = 0x0;
  ((uint8_t *)_dik.state) [DIK_RALT]   = 0x0;
  ((uint8_t *)_dik.state) [DIK_TAB]    = 0x0;
  ((uint8_t *)_dik.state) [DIK_ESCAPE] = 0x0;
  ((uint8_t *)_dik.state) [DIK_UP]     = 0x0;
  ((uint8_t *)_dik.state) [DIK_DOWN]   = 0x0;
  ((uint8_t *)_dik.state) [DIK_LEFT]   = 0x0;
  ((uint8_t *)_dik.state) [DIK_RIGHT]  = 0x0;
  ((uint8_t *)_dik.state) [DIK_RETURN] = 0x0;
  ((uint8_t *)_dik.state) [DIK_LMENU]  = 0x0;
  ((uint8_t *)_dik.state) [DIK_RMENU]  = 0x0;
}

///////////////////////////////////////////////////////////////////////////////
//
// User32 Input APIs
//
///////////////////////////////////////////////////////////////////////////////
typedef UINT (WINAPI *GetRawInputData_pfn)(
  _In_      HRAWINPUT hRawInput,
  _In_      UINT      uiCommand,
  _Out_opt_ LPVOID    pData,
  _Inout_   PUINT     pcbSize,
  _In_      UINT      cbSizeHeader
);

typedef SHORT (WINAPI *GetAsyncKeyState_pfn)(
  _In_ int vKey
);

GetAsyncKeyState_pfn GetAsyncKeyState_Original = nullptr;
GetRawInputData_pfn  GetRawInputData_Original  = nullptr;

SetCursorPos_pfn     SetCursorPos_Original     = nullptr;
ClipCursor_pfn       ClipCursor_Original       = nullptr;

UINT
WINAPI
GetRawInputData_Detour (_In_      HRAWINPUT hRawInput,
                        _In_      UINT      uiCommand,
                        _Out_opt_ LPVOID    pData,
                        _Inout_   PUINT     pcbSize,
                        _In_      UINT      cbSizeHeader)
{
  if (config.input.block_all_keys) {
    *pcbSize = 0;
    return 0;
  }

  int size = GetRawInputData_Original (hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

  // Block keyboard input to the game while the console is active
  if (tsf::InputManager::Hooker::getInstance ()->isVisible () && uiCommand == RID_INPUT) {
    *pcbSize = 0;
    return 0;
  }

  // Fixing Alt-Tab by Hooking This Is Not Possible
  //  -- Search through the binary to find the boolean that is set when
  //       a key is pressed (it will be somewhere after GetRawInputData (...) is
  //          called. Reset this boolean whenever Alt+Tab happens.

  return size;
}

SHORT
WINAPI
GetAsyncKeyState_Detour (_In_ int vKey)
{
#define TSFix_ConsumeVKey(vKey) { GetAsyncKeyState_Original(vKey); return 0; }

  // Window is not active, but we are faking it...
  if ((! tsf::window.active) && config.render.allow_background)
    TSFix_ConsumeVKey (vKey);

  // Block keyboard input to the game while the console is active
  if (tsf::InputManager::Hooker::getInstance ()->isVisible ()) {
    TSFix_ConsumeVKey (vKey);
  }

  // Block Left Alt
  if (vKey == VK_LMENU)
    if (config.input.block_left_alt)
      TSFix_ConsumeVKey (vKey);

  // Block Left Ctrl
  if (vKey == VK_LCONTROL)
    if (config.input.block_left_ctrl)
      TSFix_ConsumeVKey (vKey);

  return GetAsyncKeyState_Original (vKey);
}

BOOL
WINAPI
ClipCursor_Detour (const RECT *lpRect)
{
  tsf::InputManager::FixAltTab ();

  if (lpRect != nullptr)
    tsf::window.cursor_clip = *lpRect;

  if (tsf::window.active) {
    return ClipCursor_Original (nullptr);//lpRect);
  } else {
    return ClipCursor_Original (nullptr);
  }
}


BOOL
WINAPI
SetCursorPos_Detour (_In_ int X, _In_ int Y)
{
  // DO NOT let this stupid game capture the cursor while
  //   it is not active. UGH!
  return TRUE;
}

// Don't care
LPVOID SetPhysicalCursorPos_Original = nullptr;

BOOL
WINAPI
SetPhysicalCursorPos_Detour
  (
    _In_ int X,
    _In_ int Y
  )
{
  return TRUE;
}

typedef HRESULT (STDMETHODCALLTYPE *D3D9SetCursorPosition_pfn)
(
       IDirect3DDevice9 *This,
  _In_ INT               X,
  _In_ INT               Y,
  _In_ DWORD             Flags
);

D3D9SetCursorPosition_pfn D3D9SetCursorPosition_Original = nullptr;

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetCursorPosition_Detour (      IDirect3DDevice9* This,
                               _In_ INT               X,
                               _In_ INT               Y,
                               _In_ DWORD             Flags )
{
  // Don't let the game do this.
  return S_OK;
}

void
HookRawInput (void)
{
  // Defer installation of this hook until DirectInput8 is setup
  if (GetRawInputData_Original == nullptr) {
    dll_log.LogEx (true, L"[   Input  ] Installing Deferred Hook: \"GetRawInputData (...)\"... ");
    MH_STATUS status =
      TSFix_CreateDLLHook ( L"user32.dll", "GetRawInputData",
                            GetRawInputData_Detour,
                  (LPVOID*)&GetRawInputData_Original );
   dll_log.LogEx (false, L"%hs\n", MH_StatusToString (status));
  }
}



void
tsf::InputManager::Init (void)
{
  FixAltTab ();

  //
  // For this game, the only reason we hook this is to block the Windows key.
  //
  CoInitializeEx (nullptr, COINIT_MULTITHREADED);

  IDirectInput8A* pDInput8 = nullptr;
  HRESULT hr =
    CoCreateInstance ( CLSID_DirectInput8,
                         nullptr,
                           CLSCTX_INPROC_SERVER,
                             IID_IDirectInput8A,
                               (LPVOID *)&pDInput8 );

  if (SUCCEEDED (hr)) {
    void** vftable = *(void***)*&pDInput8;

    TSFix_CreateFuncHook ( L"IDirectInput8::CreateDevice",
                           vftable [3],
                           IDirectInput8_CreateDevice_Detour,
                 (LPVOID*)&IDirectInput8_CreateDevice_Original );

    TSFix_EnableHook (vftable [3]);

    pDInput8->Release ();
  }

  //
  // Win32 API Input Hooks
  //

  TSFix_CreateDLLHook ( L"user32.dll", "GetAsyncKeyState",
                        GetAsyncKeyState_Detour,
              (LPVOID*)&GetAsyncKeyState_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "ClipCursor",
                        ClipCursor_Detour,
              (LPVOID*)&ClipCursor_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "SetCursorPos",
                        SetCursorPos_Detour,
              (LPVOID*)&SetCursorPos_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "SetPhysicalCursorPos",
                        SetPhysicalCursorPos_Detour,
              (LPVOID*)&SetPhysicalCursorPos_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (), "D3D9SetCursorPosition_Override",
                        D3D9SetCursorPosition_Detour,
              (LPVOID*)&D3D9SetCursorPosition_Original );

  tsf::InputManager::Hooker* pHook =
    tsf::InputManager::Hooker::getInstance ();

  pHook->Start ();
}

void
tsf::InputManager::Shutdown (void)
{
  tsf::InputManager::Hooker* pHook = tsf::InputManager::Hooker::getInstance ();

  pHook->End ();
}


void
tsf::InputManager::Hooker::Start (void)
{
  hMsgPump =
    CreateThread ( NULL,
                     NULL,
                       Hooker::MessagePump,
                         &hooks,
                           NULL,
                             NULL );
}

void
tsf::InputManager::Hooker::End (void)
{
  TerminateThread     (hMsgPump, 0);
  UnhookWindowsHookEx (hooks.keyboard);
  UnhookWindowsHookEx (hooks.mouse);
}

std::string console_text;
std::string mod_text;

void
tsf::InputManager::Hooker::Draw (void)
{
  typedef BOOL (__stdcall *SK_DrawExternalOSD_pfn)(std::string app_name, std::string text);

  static HMODULE               hMod =
    GetModuleHandle (config.system.injector.c_str ());
  static SK_DrawExternalOSD_pfn SK_DrawExternalOSD
    =
    (SK_DrawExternalOSD_pfn)GetProcAddress (hMod, "SK_DrawExternalOSD");

  std::string output;

  static DWORD last_time = timeGetTime ();
  static bool  carret    = true;

  if (visible) {
    output += text;

    // Blink the Carret
    if (timeGetTime () - last_time > 333) {
      carret = ! carret;

      last_time = timeGetTime ();
    }

    if (carret)
      output += "-";

    // Show Command Results
    if (command_issued) {
      output += "\n";
      output += result_str;
    }
  }

  console_text = output;

  if (output.length ())
    output += "\n\n";

  output += mod_text;

  SK_DrawExternalOSD ("ToZ Fix", output.c_str ());
}

DWORD
WINAPI
tsf::InputManager::Hooker::MessagePump (LPVOID hook_ptr)
{
  hooks_s* pHooks = (hooks_s *)hook_ptr;

  ZeroMemory (text, 4096);

  text [0] = '>';

  extern    HMODULE hDLLMod;

  DWORD dwThreadId;

  int hits = 0;

  DWORD dwTime = timeGetTime ();

  while (true) {
    // Spin until the game has a render window setup and various
    //   other resources loaded
    if (! tsf::RenderFix::pDevice) {
      Sleep (83);
      continue;
    }

    DWORD dwProc;

    dwThreadId =
      GetWindowThreadProcessId (GetForegroundWindow (), &dwProc);

    // Ugly hack, but a different window might be in the foreground...
    if (dwProc != GetCurrentProcessId ()) {
      //dll_log.Log (L" *** Tried to hook the wrong process!!!");
      Sleep (83);
      continue;
    }

    break;
  }

  //tsf::WindowManager::Init ();

  // Defer initialization of the Window Message redirection stuff until
  //   we have an actual window!
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();
  pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.window.foreground_fps);


  dll_log.Log ( L"[   Input  ] # Found window in %03.01f seconds, "
                    L"installing keyboard hook...",
                  (float)(timeGetTime () - dwTime) / 1000.0f );

  dwTime = timeGetTime ();
  hits   = 1;

  while (! (pHooks->keyboard = SetWindowsHookEx ( WH_KEYBOARD,
                                                    KeyboardProc,
                                                      hDLLMod,
                                                        dwThreadId ))) {
    _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

    dll_log.Log ( L"[   Input  ] @ SetWindowsHookEx failed: 0x%04X (%s)",
                  err.WCode (), err.ErrorMessage () );

    ++hits;

    if (hits >= 5) {
      dll_log.Log ( L"[   Input  ] * Failed to install keyboard hook after %lu tries... "
        L"bailing out!",
        hits );
      return 0;
    }

    Sleep (1);
  }

  dll_log.Log ( L"[   Input  ] * Installed keyboard hook for command console... "
                      L"%lu %s (%lu ms!)",
                hits,
                  hits > 1 ? L"tries" : L"try",
                    timeGetTime () - dwTime );

  while (true) {
    Sleep (10);
  }
  //193 - 199

  return 0;
}

LRESULT
CALLBACK
tsf::InputManager::Hooker::MouseProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  MOUSEHOOKSTRUCT* pmh = (MOUSEHOOKSTRUCT *)lParam;

  return CallNextHookEx (Hooker::getInstance ()->hooks.mouse, nCode, wParam, lParam);
}

LRESULT
CALLBACK
tsf::InputManager::Hooker::KeyboardProc (int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode == 0) {
    // If DirectInput isn't going to hook this, we'll do it ourself
    HookRawInput ();

    BYTE    vkCode   = LOWORD (wParam) & 0xFF;
    BYTE    scanCode = HIWORD (lParam) & 0x7F;
    SHORT   repeated = LOWORD (lParam);
    bool    keyDown  = ! (lParam & 0x80000000);

    if (visible && vkCode == VK_BACK) {
      if (keyDown) {
        size_t len = strlen (text);
               len--;

        if (len < 1)
          len = 1;

        text [len] = '\0';
      }
    }

    // We don't want to distinguish between left and right on these keys, so alias the stuff
    else if ((vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT)) {
      if (keyDown) keys_ [VK_SHIFT] = 0x81; else keys_ [VK_SHIFT] = 0x00;
    }

    else if ((vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_MENU)) {
      if (keyDown) keys_ [VK_MENU] = 0x81; else keys_ [VK_MENU] = 0x00;
    }

    else if ((!repeated) && vkCode == VK_CAPITAL) {
      if (keyDown) if (keys_ [VK_CAPITAL] == 0x00) keys_ [VK_CAPITAL] = 0x81; else keys_ [VK_CAPITAL] = 0x00;
    }

    else if ((vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL)) {
      if (keyDown) keys_ [VK_CONTROL] = 0x81; else keys_ [VK_CONTROL] = 0x00;
    }

    else if ((vkCode == VK_UP) || (vkCode == VK_DOWN)) {
      if (keyDown && visible) {
        if (vkCode == VK_UP)
          commands.idx--;
        else
          commands.idx++;

        // Clamp the index
        if (commands.idx < 0)
          commands.idx = 0;
        else if (commands.idx >= commands.history.size ())
          commands.idx = commands.history.size () - 1;

        if (commands.history.size ()) {
          strcpy (&text [1], commands.history [commands.idx].c_str ());
          command_issued = false;
        }
      }
    }

    else if (visible && vkCode == VK_RETURN) {
      if (keyDown && LOWORD (lParam) < 2) {
        size_t len = strlen (text+1);
        // Don't process empty or pure whitespace command lines
        if (len > 0 && strspn (text+1, " ") != len) {
          eTB_CommandResult result = SK_GetCommandProcessor ()->ProcessCommandLine (text+1);

          if (result.getStatus ()) {
            // Don't repeat the same command over and over
            if (commands.history.size () == 0 ||
                commands.history.back () != &text [1]) {
              commands.history.push_back (&text [1]);
            }

            commands.idx = commands.history.size ();

            text [1] = '\0';

            command_issued = true;
          }
          else {
            command_issued = false;
          }

          result_str = result.getWord   () + std::string (" ")   +
                       result.getArgs   () + std::string (":  ") +
                       result.getResult ();
        }
      }
    }

    else if (keyDown) {
      eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();

      bool new_press = keys_ [vkCode] != 0x81;

      keys_ [vkCode] = 0x81;

      if (keys_ [VK_CONTROL] && keys_ [VK_SHIFT] ) {
        if (vkCode == VK_TAB && keys_ [VK_TAB] && new_press) {
          visible = ! visible;

          // Avoid duplicating a SK feature
          static HMODULE hD3D9 = GetModuleHandle (config.system.injector.c_str ());

          typedef void (__stdcall *SK_SteamAPI_SetOverlayState_pfn)(bool);
          static SK_SteamAPI_SetOverlayState_pfn SK_SteamAPI_SetOverlayState =
              (SK_SteamAPI_SetOverlayState_pfn)
                GetProcAddress ( hD3D9,
                                    "SK_SteamAPI_SetOverlayState" );

          SK_SteamAPI_SetOverlayState (visible);

          // Prevent the Steam Overlay from being a real pain
          return -1;
        }

        if (keys_ [VK_MENU] && vkCode == 'L' && new_press) {
          pCommandProc->ProcessCommandLine ("Trace.NumFrames 1");
          pCommandProc->ProcessCommandLine ("Trace.Enable true");
        }

// Not really that useful, and we want the 'B' key for something else
#if 0
        else if (keys_ [VK_MENU] && vkCode == 'B' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.AllowBG toggle");
        }
#endif

        else if (keys_ [VK_MENU] && vkCode == 'H' && new_press) {
          pCommandProc->ProcessCommandLine ("Timing.HyperSpeed toggle");
        }

        else if (vkCode == 'B' && new_press) {
          pCommandProc->ProcessCommandLine ("Render.RemoveBlur toggle");
          if (! config.render.remove_blur)
            tsf::RenderFix::draw_state.blur_proxy.first = nullptr;
        }

        //else if (vkCode == 'C' && new_press) {
          //pCommandProc->ProcessCommandLine ("Render.ConservativeMSAA toggle");
        //}

#if 0
        else if (vkCode == 'Z') {
          extern void TSF_Zoom (double incr);
          TSF_Zoom (-0.01);
        }

        else if (vkCode == 'X') {
          extern void TSF_Zoom (double incr);
          TSF_Zoom (0.01);
        }

        else if (vkCode == 'C') {
          extern void TSF_ZoomEx (double incr);
          TSF_ZoomEx (0.5);
        }
#endif

        else if (vkCode == VK_OEM_PERIOD && new_press) {
          pCommandProc->ProcessCommandLine ("Render.OutlineTechnique inc");

          if (config.render.outline_technique > 2)
            config.render.outline_technique = 0;
        }

        else if (vkCode == VK_OEM_COMMA && new_press) {
          pCommandProc->ProcessCommandLine ("Render.MSAA toggle");
        }

        else if (vkCode == '1' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 60.0");
          pCommandProc->ProcessCommandLine ("Timing.RouteSixty false");
        }

        else if (vkCode == '2' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 30.0");
          pCommandProc->ProcessCommandLine ("Timing.RouteSixty false");
        }

        else if (vkCode == '3' && new_press) {
          pCommandProc->ProcessCommandLine ("Window.ForegroundFPS 0.0");
          pCommandProc->ProcessCommandLine ("Timing.RouteSixty true");
        }

        // TODO: Command processor variable for this
        else if (vkCode == 'U' && new_press) {
          extern bool __remap_textures;
          __remap_textures = (! __remap_textures);
        }

        else if (vkCode == 'Z' && new_press) {
          extern bool __need_purge;
          __need_purge = true;
        }
      }

      // Don't print the tab character, it's pretty useless.
      if (visible && vkCode != VK_TAB) {
        char key_str [2];
        key_str [1] = '\0';

        if (1 == ToAsciiEx ( vkCode,
                              scanCode,
                              keys_,
                            (LPWORD)key_str,
                              0,
                              GetKeyboardLayout (0) ) &&
                 isprint ( *key_str )) {
          strncat (text, key_str, 1);
          command_issued = false;
        }
      }
    }

    else if ((! keyDown))
      keys_ [vkCode] = 0x00;

    if (visible) return -1;
  }

  return CallNextHookEx (Hooker::getInstance ()->hooks.keyboard, nCode, wParam, lParam);
};


void
TSFix_DrawCommandConsole (void)
{
  static int draws = 0;

  // Skip the first several frames, so that the console appears below the
  //  other OSD.
  if (draws++ > 20) {
    tsf::InputManager::Hooker* pHook = tsf::InputManager::Hooker::getInstance ();
    pHook->Draw ();
  }
}


tsf::InputManager::Hooker* tsf::InputManager::Hooker::pInputHook;

char                      tsf::InputManager::Hooker::text [4096];

BYTE                      tsf::InputManager::Hooker::keys_ [256]    = { 0 };
bool                      tsf::InputManager::Hooker::visible        = false;

bool                      tsf::InputManager::Hooker::command_issued = false;
std::string               tsf::InputManager::Hooker::result_str;

tsf::InputManager::Hooker::command_history_s
                          tsf::InputManager::Hooker::commands;