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
#include "hook.h"

#include <mmsystem.h>
#pragma comment (lib, "winmm.lib")

#include "input.h"


///////////////////////////////////////////////////////////////////////////////
//
// General Utility Functions
//
///////////////////////////////////////////////////////////////////////////////
void
tsf::InputManager::FixAltTab (void)
{
  static bool init = false;
  //
  // Make sure the game does not think that Alt or Tab are stuck
  //
  static INPUT kbd [6];

  if (! init) {
    kbd [0].type       = INPUT_KEYBOARD;
    kbd [0].ki.wVk     = VK_MENU;
    kbd [0].ki.dwFlags = 0;
    kbd [0].ki.time    = 0;
    kbd [1].type       = INPUT_KEYBOARD;
    kbd [1].ki.wVk     = VK_MENU;
    kbd [1].ki.dwFlags = KEYEVENTF_KEYUP;
    kbd [1].ki.time    = 0;

    kbd [2].type       = INPUT_KEYBOARD;
    kbd [2].ki.wVk     = VK_TAB;
    kbd [2].ki.dwFlags = 0;
    kbd [2].ki.time    = 0;
    kbd [3].type       = INPUT_KEYBOARD;
    kbd [3].ki.wVk     = VK_TAB;
    kbd [3].ki.dwFlags = KEYEVENTF_KEYUP;
    kbd [3].ki.time    = 0;

    init = true;
  }

  SendInput (4, kbd, sizeof INPUT);
}

#if 0
void
TSF_ComputeAspectCoeffsEx (float& x, float& y, float& xoff, float& yoff, bool force=false)
{
  yoff = 0.0f;
  xoff = 0.0f;

  x = 1.0f;
  y = 1.0f;

  if (! (config.render.aspect_correction || force))
    return;

  config.render.aspect_ratio = tsf::RenderFix::width / tsf::RenderFix::height;
  float rescale              = ((16.0f / 9.0f) / config.render.aspect_ratio);

  // Wider
  if (config.render.aspect_ratio > (16.0f / 9.0f)) {
    int width = (16.0f / 9.0f) * tsf::RenderFix::height;
    int x_off = (tsf::RenderFix::width - width) / 2;

    x    = (float)tsf::RenderFix::width / (float)width;
    xoff = x_off;

#if 0
    // Calculated height will be greater than we started with, so work
    //  in the wrong direction here...
    int height = (9.0f / 16.0f) * tsf::RenderFix::width;
    y          = (float)tsf::RenderFix::height / (float)height;
#endif

    yoff = config.scaling.mouse_y_offset;
  } else {
// No fix is needed in this direction
#if 0
    int height = (9.0f / 16.0f) * tsf::RenderFix::width;
    int y_off  = (tsf::RenderFix::height - height) / 2;

    y    = (float)tsf::RenderFix::height / (float)height;
    yoff = y_off;
#endif
  }
}
#endif

#if 0
// Returns the original cursor position and stores the new one in pPoint
POINT
tsf::InputManager::CalcCursorPos (LPPOINT pPoint, bool reverse)
{
  // Bail-out early if aspect ratio correction is disabled, or if the
  //   aspect ratio is less than or equal to 16:9.
  if  (! ( config.render.aspect_correction &&
           config.render.aspect_ratio > (16.0f / 9.0f) ) )
    return *pPoint;

  float xscale, yscale;
  float xoff,   yoff;

  TSF_ComputeAspectCoeffsEx (xscale, yscale, xoff, yoff);

  yscale = xscale;

  if (! config.render.center_ui) {
    xscale = 1.0f;
    xoff   = 0.0f;
  }

  // Adjust system coordinates to game's (broken aspect ratio) coordinates
  if (! reverse) {
    pPoint->x = ((float)pPoint->x - xoff) * xscale;
    pPoint->y = ((float)pPoint->y - yoff) * yscale;
  }

  // Adjust game's (broken aspect ratio) coordinates to system coordinates
  else {
    pPoint->x = ((float)pPoint->x / xscale) + xoff;
    pPoint->y = ((float)pPoint->y / yscale) + yoff;
  }

  return *pPoint;
}
#endif

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

typedef HRESULT (WINAPI *DirectInput8Create_pfn)(
  HINSTANCE  hinst,
  DWORD      dwVersion,
  REFIID     riidltf,
  LPVOID    *ppvOut,
  LPUNKNOWN  punkOuter
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

DirectInput8Create_pfn
        DirectInput8Create_Original                      = nullptr;
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

struct di8_mouse_s {
  LPDIRECTINPUTDEVICE pDev = nullptr;
  DIMOUSESTATE2       state;
} _dim;

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

  // For input faking (keyboard)
  if ((! tsf::window.active) && config.render.allow_background && This == _dik.pDev) {
    memcpy (lpvData, _dik.state, cbData);
    return S_OK;
  }

  // For input faking (mouse)
  if ((! tsf::window.active) && config.render.allow_background && This == _dim.pDev) {
    memcpy (lpvData, &_dim.state, cbData);
    return S_OK;
  }

  HRESULT hr;
  hr = IDirectInputDevice8_GetDeviceState_Original ( This,
                                                       cbData,
                                                         lpvData );

  if (SUCCEEDED (hr)) {
    if (tsf::window.active && This == _dim.pDev) {
//
// This is only for mouselook, etc. That stuff works fine without aspect ratio correction.
//
//#define FIX_DINPUT8_MOUSE
#ifdef FIX_DINPUT8_MOUSE
      if (cbData == sizeof (DIMOUSESTATE) || cbData == sizeof (DIMOUSESTATE2)) {
        POINT mouse_pos { ((DIMOUSESTATE *)lpvData)->lX,
                          ((DIMOUSESTATE *)lpvData)->lY };

        tsf::InputManager::CalcCursorPos (&mouse_pos);

        ((DIMOUSESTATE *)lpvData)->lX = mouse_pos.x;
        ((DIMOUSESTATE *)lpvData)->lY = mouse_pos.y;
      }
#endif
      memcpy (&_dim.state, lpvData, cbData);
    }

    else if (tsf::window.active && This == _dik.pDev) {
      memcpy (&_dik.state, lpvData, cbData);
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
#if 1
      void** vftable = *(void***)*lplpDirectInputDevice;

// We do not need to hook this for ToS
#if 0
      TSFix_CreateFuncHook ( L"IDirectInputDevice8::GetDeviceState",
                             vftable [9],
                             IDirectInputDevice8_GetDeviceState_Detour,
                   (LPVOID*)&IDirectInputDevice8_GetDeviceState_Original );

      TSFix_EnableHook (vftable [9]);
#endif

      TSFix_CreateFuncHook ( L"IDirectInputDevice8::SetCooperativeLevel",
                             vftable [13],
                             IDirectInputDevice8_SetCooperativeLevel_Detour,
                   (LPVOID*)&IDirectInputDevice8_SetCooperativeLevel_Original );

      TSFix_EnableHook (vftable [13]);
#else
     DI8_VIRTUAL_OVERRIDE ( lplpDirectInputDevice, 9,
                            L"IDirectInputDevice8::GetDeviceState",
                            IDirectInputDevice8_GetDeviceState_Detour,
                            IDirectInputDevice8_GetDeviceState_Original,
                            IDirectInputDevice8_GetDeviceState_pfn );

     DI8_VIRTUAL_OVERRIDE ( lplpDirectInputDevice, 13,
                            L"IDirectInputDevice8::SetCooperativeLevel",
                            IDirectInputDevice8_SetCooperativeLevel_Detour,
                            IDirectInputDevice8_SetCooperativeLevel_Original,
                            IDirectInputDevice8_SetCooperativeLevel_pfn );
#endif

    if (rguid == GUID_SysMouse)
      _dim.pDev = *lplpDirectInputDevice;
    else if (rguid == GUID_SysKeyboard)
      _dik.pDev = *lplpDirectInputDevice;
  }

  return hr;
}

HRESULT
WINAPI
DirectInput8Create_Detour ( HINSTANCE  hinst,
                            DWORD      dwVersion,
                            REFIID     riidltf,
                            LPVOID    *ppvOut,
                            LPUNKNOWN  punkOuter )
{
  dll_log.Log ( L"[   Input  ][!] DirectInput8Create (0x%X, %lu, ..., %08Xh, %08Xh)",
                  hinst, dwVersion, /*riidltf,*/ ppvOut, punkOuter );

  HRESULT hr;
  DINPUT8_CALL (hr,
    DirectInput8Create_Original ( hinst,
                                    dwVersion,
                                      riidltf,
                                        ppvOut,
                                          punkOuter ));

  extern void HookRawInput (void);
  HookRawInput ();

  if (hinst != GetModuleHandle (nullptr)) {
    dll_log.Log (L"[   Input  ] >> A third-party DLL is manipulating DirectInput 8; will not hook.");
    return hr;
  }

  // Avoid multiple hooks for third-party compatibility
  static bool hooked = false;

  if (SUCCEEDED (hr) && (! hooked)) {
#if 1
    void** vftable = *(void***)*ppvOut;

    TSFix_CreateFuncHook ( L"IDirectInput8::CreateDevice",
                           vftable [3],
                           IDirectInput8_CreateDevice_Detour,
                 (LPVOID*)&IDirectInput8_CreateDevice_Original );

    TSFix_EnableHook (vftable [3]);
#else
     DI8_VIRTUAL_OVERRIDE ( ppvOut, 3,
                            L"IDirectInput8::CreateDevice",
                            IDirectInput8_CreateDevice_Detour,
                            IDirectInput8_CreateDevice_Original,
                            IDirectInput8_CreateDevice_pfn );
#endif
    hooked = true;
  }

  return hr;
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

typedef BOOL (WINAPI *GetCursorInfo_pfn)(
  _Inout_ PCURSORINFO pci
);

typedef BOOL (WINAPI *GetCursorPos_pfn)(
  _Out_ LPPOINT lpPoint
);
GetAsyncKeyState_pfn GetAsyncKeyState_Original = nullptr;
GetRawInputData_pfn  GetRawInputData_Original  = nullptr;

GetCursorInfo_pfn    GetCursorInfo_Original    = nullptr;
GetCursorPos_pfn     GetCursorPos_Original     = nullptr;
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
  if (config.input.block_all_keys)
    return 0;

  if (GetFocus () != tsf::RenderFix::hWndDevice || tsf::RenderFix::hWndDevice == 0)
    return 0;

  int size = GetRawInputData_Original (hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

  // Block keyboard input to the game while the console is active
  if (tsf::InputManager::Hooker::getInstance ()->isVisible () && uiCommand == RID_INPUT)
    return 0;

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
  if (lpRect != nullptr)
    tsf::window.cursor_clip = *lpRect;

  if (tsf::window.active) {
    return ClipCursor_Original (lpRect);
  } else {
    return ClipCursor_Original (nullptr);
  }
}

#if 0
BOOL
WINAPI
GetCursorInfo_Detour (PCURSORINFO pci)
{
  BOOL ret = GetCursorInfo_Original (pci);

  // Correct the cursor position for Aspect Ratio
  if (config.render.aspect_correction && config.render.aspect_ratio > (16.0f / 9.0f)) {
    POINT pt;

    pt.x = pci->ptScreenPos.x;
    pt.y = pci->ptScreenPos.y;

    tsf::InputManager::CalcCursorPos (&pt);

    pci->ptScreenPos.x = pt.x;
    pci->ptScreenPos.y = pt.y;
  }

  return ret;
}
#endif

BOOL
WINAPI
SetCursorPos_Detour (_In_ int X, _In_ int Y)
{
  //POINT pt { X, Y };
  //tsf::InputManager::CalcCursorPos (&pt);

  // DO NOT let this stupid game capture the cursor while
  //   it is not active. UGH!
  if (tsf::window.active)
    return SetCursorPos_Original (X, Y);
  else
    return TRUE;
}

#if 0
BOOL
WINAPI
GetCursorPos_Detour (LPPOINT lpPoint)
{
  BOOL ret = GetCursorPos_Original (lpPoint);

  // Correct the cursor position for Aspect Ratio
  if (config.render.aspect_correction && config.render.aspect_ratio > (16.0f / 9.0f))
    tsf::InputManager::CalcCursorPos (lpPoint);

  return ret;
}
#endif

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

  if (config.input.disable_touch || config.input.pause_touch)
    ShutdownTouchServices ();

  //
  // For this game, the only reason we hook this is to block the Windows key.
  //
  if (config.input.block_windows) {
    //
    // We only hook one DLL export from DInput8, all other DInput stuff is
    //   handled through virtual function table overrides
    //
    TSFix_CreateDLLHook ( L"dinput8.dll", "DirectInput8Create",
                          DirectInput8Create_Detour,
                (LPVOID*)&DirectInput8Create_Original,
                (LPVOID*)&DirectInput8Create_Hook );
    TSFix_EnableHook    (DirectInput8Create_Hook);
  }

  //
  // Win32 API Input Hooks
  //

  // If DirectInput isn't going to hook this, we'll do it ourself
  if (! config.input.block_windows) {
    HookRawInput ();
  }

  TSFix_CreateDLLHook ( L"user32.dll", "GetAsyncKeyState",
                        GetAsyncKeyState_Detour,
              (LPVOID*)&GetAsyncKeyState_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "ClipCursor",
                        ClipCursor_Detour,
              (LPVOID*)&ClipCursor_Original );

#if 0
  TSFix_CreateDLLHook ( L"user32.dll", "GetCursorInfo",
                        GetCursorInfo_Detour,
              (LPVOID*)&GetCursorInfo_Original );

  TSFix_CreateDLLHook ( L"user32.dll", "GetCursorPos",
                        GetCursorPos_Detour,
              (LPVOID*)&GetCursorPos_Original );
#endif

  TSFix_CreateDLLHook ( L"user32.dll", "SetCursorPos",
                        SetCursorPos_Detour,
              (LPVOID*)&SetCursorPos_Original );

  tsf::InputManager::Hooker* pHook =
    tsf::InputManager::Hooker::getInstance ();

  pHook->Start ();
}

void
tsf::InputManager::Shutdown (void)
{
  if (config.input.pause_touch)
    RestoreTouchServices ();

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

void
tsf::InputManager::Hooker::Draw (void)
{
  typedef BOOL (__stdcall *BMF_DrawExternalOSD_t)(std::string app_name, std::string text);

  static HMODULE               hMod =
    GetModuleHandle (config.system.injector.c_str ());
  static BMF_DrawExternalOSD_t BMF_DrawExternalOSD
    =
    (BMF_DrawExternalOSD_t)GetProcAddress (hMod, "BMF_DrawExternalOSD");

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

  BMF_DrawExternalOSD ("ToZ Fix", output.c_str ());
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
  ////tsf::RenderFix::hWndDevice = GetForegroundWindow ();

  // Defer initialization of the Window Message redirection stuff until
  //   we have an actual window!
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor ();
  pCommandProc->ProcessCommandFormatted ("TargetFPS %f", config.render.foreground_fps);


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

// No need to hook the mousey right now..
#if 0
  while (! (pHooks->mouse = SetWindowsHookEx ( WH_MOUSE,
                                                  MouseProc,
                                                    hDLLMod,
                                                      dwThreadId ))) {
    _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

    dll_log.Log ( L"  @ SetWindowsHookEx failed: 0x%04X (%s)",
                  err.WCode (), err.ErrorMessage () );

    ++hits;

    if (hits >= 5) {
      dll_log.Log ( L"  * Failed to install mouse hook after %lu tries... "
        L"bailing out!",
        hits );
      return 0;
    }

    Sleep (1);
  }
#endif

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
  static uint32_t freq = 0;

  if (freq == 0)
    freq = *(uint32_t *)0x00B24A88;

  if (nCode >= 0) {
    BYTE    vkCode   = LOWORD (wParam) & 0xFF;
    BYTE    scanCode = HIWORD (lParam) & 0x7F;
    bool    repeated = LOWORD (lParam);
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

      if (keys_ [VK_CONTROL] && keys_ [VK_SHIFT]) {
        if (keys_ [VK_TAB] && new_press) {
          visible = ! visible;

          // Avoid duplicating a BMF feature
          static HMODULE hD3D9 = GetModuleHandle (config.system.injector.c_str ());

          typedef void (__stdcall *BMF_SteamAPI_SetOverlayState_t)(bool);
          static BMF_SteamAPI_SetOverlayState_t BMF_SteamAPI_SetOverlayState =
              (BMF_SteamAPI_SetOverlayState_t)
                GetProcAddress ( hD3D9,
                                    "BMF_SteamAPI_SetOverlayState" );

          BMF_SteamAPI_SetOverlayState (visible);

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
                              GetKeyboardLayout (0) )) {
          strncat (text, key_str, 1);
          command_issued = false;
        }
      }
    }

    else if ((! keyDown)) {
      bool new_release = keys_ [vkCode] != 0x00;

      keys_ [vkCode] = 0x00;
    }

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


bool tsf::InputManager::has_touch_services = false;

bool
tsf::InputManager::ShutdownTouchServices (void)
{
  //
  // Shutdown the Tablet Input Services
  //
  dll_log.LogEx ( true,
                  L"[   Input  ] Stopping TabletInputService..." );

  SC_HANDLE svc_ctl =
    OpenSCManagerW ( nullptr,
                       nullptr,
                         SC_MANAGER_ALL_ACCESS );

  _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

  if (err.WCode () != 0) {
    dll_log.LogEx ( false,
                      L" failed! (%s [%s:%d])\n",
                        (wchar_t *)err.ErrorMessage (),
                          __FILEW__, __LINE__ );
  }

  if (svc_ctl) {
    SC_HANDLE tablet_svc =
      OpenServiceW ( svc_ctl,
                       L"TabletInputService",
                         SERVICE_STOP         |
                         SERVICE_QUERY_STATUS |
                         SERVICE_CHANGE_CONFIG );

    err = _com_error (HRESULT_FROM_WIN32 (GetLastError ()));

    if (err.WCode () != 0) {
      dll_log.LogEx ( false,
                        L" failed! (%s [%s:%d])\n",
                          (wchar_t *)err.ErrorMessage (),
                            __FILEW__, __LINE__ );
    }

    if (tablet_svc) {
      SERVICE_STATUS status;
      QueryServiceStatus (tablet_svc, &status);

      if (status.dwCurrentState != SERVICE_STOPPED) {
        has_touch_services = true;

        ControlService (tablet_svc, SERVICE_CONTROL_STOP, &status);

        err = _com_error (HRESULT_FROM_WIN32 (GetLastError ()));

        if (err.WCode () != 0) {
          dll_log.LogEx ( false,
                            L" failed! (%s [%s:%d])\n",
                              (wchar_t *)err.ErrorMessage (),
                                __FILEW__, __LINE__ );
        }
        else {
          if (config.input.disable_touch) {
            ChangeServiceConfig ( tablet_svc,
                                  SERVICE_NO_CHANGE,
                                    SERVICE_DISABLED,
                                      SERVICE_NO_CHANGE,
                                        nullptr, nullptr,
                                          nullptr, nullptr,
                                            nullptr, nullptr, nullptr );
            dll_log.LogEx ( false,
                              L" success! (Temporarily: Disabled)\n");
          } else {
            dll_log.LogEx ( false,
                              L" success!\n");
          }
        }
      } else {
        dll_log.LogEx ( false,
                          L" service not running.\n" );
      }

      CloseServiceHandle (tablet_svc);
    }

    CloseServiceHandle (svc_ctl);
  }

  return has_touch_services;
}

bool
tsf::InputManager::RestoreTouchServices (void)
{
  if (! has_touch_services)
    return false;

  //
  // Restart the Tablet Input Services
  //
  dll_log.LogEx ( true,
                  L"[   Input  ] Resuming TabletInputService... " );

  SC_HANDLE svc_ctl =
    OpenSCManagerW ( nullptr,
                       nullptr,
                         SC_MANAGER_ALL_ACCESS );

  _com_error err (HRESULT_FROM_WIN32 (GetLastError ()));

  if (err.WCode () != 0) {
    dll_log.LogEx ( false,
                      L" failed! (%s [%s:%d])\n",
                        (wchar_t *)err.ErrorMessage (),
                          __FILEW__, __LINE__ );
  }

  if (svc_ctl) {
    SC_HANDLE tablet_svc =
      OpenServiceW ( svc_ctl,
                       L"TabletInputService",
                         SERVICE_START        |
                         SERVICE_QUERY_STATUS |
                         SERVICE_CHANGE_CONFIG );

    err = _com_error (HRESULT_FROM_WIN32 (GetLastError ()));

    if (err.WCode () != 0) {
      dll_log.LogEx ( false,
                        L" failed! (%s [%s:%d])\n",
                          (wchar_t *)err.ErrorMessage (),
                            __FILEW__, __LINE__ );
    }

    if (tablet_svc) {
       if (config.input.disable_touch && config.input.pause_touch)
          ChangeServiceConfig ( tablet_svc,
                                  SERVICE_NO_CHANGE,
                                    SERVICE_AUTO_START,
                                      SERVICE_NO_CHANGE,
                                        nullptr, nullptr,
                                          nullptr, nullptr,
                                            nullptr, nullptr, nullptr );

      StartServiceW (tablet_svc, 0, nullptr);

      err = _com_error (HRESULT_FROM_WIN32 (GetLastError ()));

      if (err.WCode () != 0) {
        dll_log.LogEx ( false,
                          L" failed! (%s [%s:%d])\n",
                            (wchar_t *)err.ErrorMessage (),
                              __FILEW__, __LINE__ );
      }
      else {
        if (config.input.disable_touch) {
          dll_log.LogEx ( false,
                          L" success! (Restored: Auto-Start)\n" );
        } else {
          dll_log.LogEx ( false,
                          L" success!\n" );
        }
      }

      CloseServiceHandle (tablet_svc);
    }

    CloseServiceHandle (svc_ctl);
  }

  return true;
}


tsf::InputManager::Hooker* tsf::InputManager::Hooker::pInputHook;

char                      tsf::InputManager::Hooker::text [4096];

BYTE                      tsf::InputManager::Hooker::keys_ [256]    = { 0 };
bool                      tsf::InputManager::Hooker::visible        = false;

bool                      tsf::InputManager::Hooker::command_issued = false;
std::string               tsf::InputManager::Hooker::result_str;

tsf::InputManager::Hooker::command_history_s
                          tsf::InputManager::Hooker::commands;