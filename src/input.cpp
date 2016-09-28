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

#include <process.h>

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

IDirectInput8_CreateDevice_pfn
        IDirectInput8_CreateDevice_Original              = nullptr;
IDirectInputDevice8_GetDeviceState_pfn
        IDirectInputDevice8_GetDeviceState_Original      = nullptr;
IDirectInputDevice8_SetCooperativeLevel_pfn
        IDirectInputDevice8_SetCooperativeLevel_Original = nullptr;



#define DINPUT8_CALL(_Ret, _Call) {                                     \
  dll_log->LogEx (true, L"[   Input  ]  Calling original function: ");  \
  (_Ret) = (_Call);                                                     \
  _com_error err ((_Ret));                                              \
  if ((_Ret) != S_OK)                                                   \
    dll_log->LogEx (false, L"(ret=0x%04x - %s)\n", err.WCode (),        \
                                                  err.ErrorMessage ()); \
  else                                                                  \
    dll_log->LogEx (false, L"(ret=S_OK)\n");                            \
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
  HRESULT hr
    = IDirectInputDevice8_GetDeviceState_Original ( This,
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

  dll_log->Log ( L"[   Input  ][!] IDirectInput8::CreateDevice (%08Xh, %s, %08Xh, %08Xh)",
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

    if (rguid == GUID_SysKeyboard) {
      if (IDirectInputDevice8_GetDeviceState_Original == nullptr) {
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
      }

      _dik.pDev = *lplpDirectInputDevice;
    }
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
#if 0
  // Make sure no reference counters used to determine keyboard state
  //   are mismatched when alt-tabbing.
  for (int i = 0; i < 255; i++) {

    // Queuing this many window messages seems like a terrible idea, but
    //   it gets the job done.

    CallWindowProc (
      tsf::window.WndProc_Original,
        tsf::window.hwnd,
          WM_KEYUP,
            MAKEWPARAM (i, 0),
              0xFFFFFFFF );

    CallWindowProc (
      tsf::window.WndProc_Original,
        tsf::window.hwnd,
          WM_SYSKEYUP,
            MAKEWPARAM (i, 0),
              0xFFFFFFFF );
  }
#endif

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

  // Fixing Alt-Tab by Hooking This is Not Possible
  //
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
    dll_log->LogEx (true, L"[   Input  ] Installing Deferred Hook: \"GetRawInputData (...)\"...\n");
    MH_STATUS status =
      TSFix_CreateDLLHook2 ( config.system.injector.c_str (),
                            "GetRawInputData",
                             GetRawInputData_Detour,
                   (LPVOID*)&GetRawInputData_Original );
      TSFix_ApplyQueuedHooks ();
   //dll_log->LogEx (false, L"%hs\n", MH_StatusToString (status));
  }
}


typedef void (CALLBACK *SK_PluginKeyPress_pfn)( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode );
SK_PluginKeyPress_pfn SK_PluginKeyPress_Original = nullptr;

void
CALLBACK
SK_TSF_PluginKeyPress ( BOOL Control,
                        BOOL Shift,
                        BOOL Alt,
                        BYTE vkCode )
{
  SK_ICommandProcessor* pCommandProc =
    SK_GetCommandProcessor ();

  if (Alt && vkCode == 'L') {
    pCommandProc->ProcessCommandLine ("Trace.NumFrames 1");
    pCommandProc->ProcessCommandLine ("Trace.Enable true");
  }

// Not really that useful, and we want the 'B' key for something else
#if 0
  else if (keys_ [VK_MENU] && vkCode == 'B' && new_press) {
    pCommandProc->ProcessCommandLine ("Render.AllowBG toggle");
  }
#endif

  else if (Alt && vkCode == 'H') {
    pCommandProc->ProcessCommandLine ("Timing.HyperSpeed toggle");
  }


  if (Control && Shift) {
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

    if (vkCode == VK_OEM_PERIOD) {
      pCommandProc->ProcessCommandLine ("Render.OutlineTechnique inc");

      if (config.render.outline_technique > 2)
        config.render.outline_technique = 0;
    }

    else if (vkCode == VK_OEM_COMMA) {
      pCommandProc->ProcessCommandLine ("Render.MSAA toggle");
    }

    else if (vkCode == '1') {
      pCommandProc->ProcessCommandLine ("Timing.DefaultFPS 60.0");
    }

    else if (vkCode == '2') {
      pCommandProc->ProcessCommandLine ("Timing.DefaultFPS 30.0");
    }

    else if (vkCode == 'U') {
      pCommandProc->ProcessCommandLine  ("Textures.Remap toggle");
      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == 'Z') {
      pCommandProc->ProcessCommandLine  ("Textures.Purge true");
      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == 'X') {
      pCommandProc->ProcessCommandLine  ("Textures.Trace true");
      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == 'V') {
      pCommandProc->ProcessCommandLine  ("Textures.ShowCache toggle");
      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == VK_OEM_6) {
      extern std::vector <uint32_t> textures_used_last_dump;
      extern int                    tex_dbg_idx;
      ++tex_dbg_idx;

      if (tex_dbg_idx > textures_used_last_dump.size ())
        tex_dbg_idx = textures_used_last_dump.size ();

      extern int debug_tex_id;
      debug_tex_id = (int)textures_used_last_dump [tex_dbg_idx];

      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == VK_OEM_4) {
      extern std::vector <uint32_t> textures_used_last_dump;
      extern int                    tex_dbg_idx;
      extern int                    debug_tex_id;

      --tex_dbg_idx;

      if (tex_dbg_idx < 0) {
        tex_dbg_idx = -1;
        debug_tex_id = 0;
      } else {
        if (tex_dbg_idx > textures_used_last_dump.size ())
          tex_dbg_idx = textures_used_last_dump.size ();

        debug_tex_id = (int)textures_used_last_dump [tex_dbg_idx];
      }

      tsf::RenderFix::tex_mgr.updateOSD ();
    }

    else if (vkCode == 'N') {
      SK_ICommandResult result =
        pCommandProc->ProcessCommandLine ("Render.AnimSpeed");

      float val =
        *(float *)result.getVariable ()->getValuePointer ();

      val -= 10000.0f;

      pCommandProc->ProcessCommandFormatted ("Render.AnimSpeed %f", val);
    }

    else if (vkCode == 'M') {
      SK_ICommandResult result =
        pCommandProc->ProcessCommandLine ("Render.AnimSpeed");

      float val =
        *(float *)result.getVariable ()->getValuePointer ();

      val += 10000.0f;

      pCommandProc->ProcessCommandFormatted ("Render.AnimSpeed %f", val);
    }
  }

  SK_PluginKeyPress_Original (Control, Shift, Alt, vkCode);
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

  TSFix_CreateDLLHook2 ( config.system.injector.c_str (),
                         "GetAsyncKeyState_Detour",
                          GetAsyncKeyState_Detour,
                (LPVOID*)&GetAsyncKeyState_Original );

  TSFix_CreateDLLHook2 ( L"user32.dll",
                         "ClipCursor",
                          ClipCursor_Detour,
                (LPVOID*)&ClipCursor_Original );

  TSFix_CreateDLLHook2 ( L"user32.dll",
                         "SetCursorPos",
                          SetCursorPos_Detour,
                (LPVOID*)&SetCursorPos_Original );

  TSFix_CreateDLLHook2 ( L"user32.dll",
                         "SetPhysicalCursorPos",
                          SetPhysicalCursorPos_Detour,
                (LPVOID*)&SetPhysicalCursorPos_Original );

  TSFix_CreateDLLHook2 ( config.system.injector.c_str (),
                         "D3D9SetCursorPosition_Override",
                          D3D9SetCursorPosition_Detour,
                (LPVOID*)&D3D9SetCursorPosition_Original );

  TSFix_CreateDLLHook2 ( config.system.injector.c_str (),
                         "SK_PluginKeyPress",
                          SK_TSF_PluginKeyPress,
               (LPVOID *)&SK_PluginKeyPress_Original );

    TSFix_ApplyQueuedHooks ();
}

void
tsf::InputManager::Shutdown (void)
{
}

std::string output;
std::string mod_text;

void
TSFix_DrawCommandConsole (void)
{
  static int draws = 0;

  // Skip the first several frames, so that the console appears below the
  //  other OSD.
  if (draws++ > 20) {
    typedef BOOL (__stdcall *SK_DrawExternalOSD_pfn)(std::string app_name, std::string text);

    static HMODULE               hMod =
      GetModuleHandle (config.system.injector.c_str ());
    static SK_DrawExternalOSD_pfn SK_DrawExternalOSD
      =
      (SK_DrawExternalOSD_pfn)GetProcAddress (hMod, "SK_DrawExternalOSD");

    extern bool __show_cache;
    if (__show_cache) {
      output += "Texture Cache\n";
      output += "-------------\n";
      output += tsf::RenderFix::tex_mgr.osdStats ();
      output += "\n\n";
    }

    // The "mod_text" includes the little spinny wheel while texture streaming
    //  is going on. That should be displayed unconditionally...
    output += mod_text;

    SK_DrawExternalOSD ("ToSFix", output);

    mod_text = "";
    output   = "";
  }
}