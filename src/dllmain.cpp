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

#include "config.h"
#include "log.h"
#include "command.h"

#include "hook.h"

#include "timing.h"
#include "render.h"
#include "input.h"
#include "window.h"

#include <process.h>
#include <winsvc.h>
#include <comdef.h>

#pragma comment (lib, "kernel32.lib")

HMODULE hDLLMod      = { 0 }; // Handle to SELF
HMODULE hInjectorDLL = { 0 }; // Handle to Special K

typedef HRESULT (__stdcall *SK_UpdateSoftware_pfn)(const wchar_t* wszProduct);
typedef bool    (__stdcall *SK_FetchVersionInfo_pfn)(const wchar_t* wszProduct);

typedef void (__stdcall *SK_SetPluginName_pfn)(std::wstring name);
SK_SetPluginName_pfn SK_SetPluginName = nullptr;

std::wstring injector_dll;

DWORD
WINAPI
DllThread (LPVOID user)
{
  std::wstring plugin_name = L"Tales of Symphonia \"Fix\" v " + TSFIX_VER_STR;

  //
  // Until we modify the logger to use Kernel32 File I/O, we have to
  //   do this stuff from here... dependency DLLs may not be loaded
  //     until initial DLL startup finishes and threads are allowed
  //       to start.
  //
  dll_log = TSF_CreateLog (L"logs/tsfix.log");

  dll_log->LogEx ( false, L"------- [Tales of Symphonia \"Fix\"] "
                          L"-------\n" ); // <--- I was bored ;)
  dll_log->Log   (        L"tsfix.dll Plug-In\n"
                          L"=========== (Version: v %s) "
                          L"===========",
                            TSFIX_VER_STR.c_str () );

  if (! TSFix_LoadConfig ()) {
    config.system.injector = injector_dll;

    // Save a new config if none exists
    TSFix_SaveConfig ();
  }

  config.system.injector = injector_dll;

  SK_SetPluginName = 
    (SK_SetPluginName_pfn)
      GetProcAddress (hInjectorDLL, "SK_SetPluginName");
  SK_GetCommandProcessor =
    (SK_GetCommandProcessor_pfn)
      GetProcAddress (hInjectorDLL, "SK_GetCommandProcessor");

  //
  // If this is NULL, the injector system isn't working right!!!
  //
  if (SK_SetPluginName != nullptr)
    SK_SetPluginName (plugin_name);

  // Plugin State
  if (TSFix_Init_MinHook () == MH_OK) {
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    tsf::TimingFix::Init     ();
    tsf::WindowManager::Init ();
    tsf::InputManager::Init  ();
    tsf::RenderFix::Init     ();

    extern void TSFix_PatchZelosAchievement (void);
    TSFix_PatchZelosAchievement ();

    CoUninitialize ();
  }


#if 0
  BOOL WINAPI QueryPerformanceFrequency_Detour (LARGE_INTEGER *pLI);

  TSFix_CreateDLLHook ( L"Kernel32.dll", "QueryPerformanceFrequency",
                          QueryPerformanceFrequency_Detour, 
               (LPVOID *)&QueryPerformanceFrequency_Original );
#endif

  SK_UpdateSoftware_pfn SK_UpdateSoftware =
    (SK_UpdateSoftware_pfn)
      GetProcAddress ( hInjectorDLL,
                         "SK_UpdateSoftware" );

  SK_FetchVersionInfo_pfn SK_FetchVersionInfo =
    (SK_FetchVersionInfo_pfn)
      GetProcAddress ( hInjectorDLL,
                         "SK_FetchVersionInfo" );

  if (! wcsstr (injector_dll.c_str (), L"SpecialK")) {
    if ( SK_FetchVersionInfo != nullptr &&
         SK_UpdateSoftware   != nullptr ) {
      if (SK_FetchVersionInfo (L"TSF")) {
        SK_UpdateSoftware (L"TSF");
      }
    }
  }

  return 0;
}

__declspec (dllexport)
BOOL
WINAPI
SKPlugIn_Init (HMODULE hModSpecialK)
{
  wchar_t wszSKFileName [MAX_PATH] = { L'\0' };
          wszSKFileName [MAX_PATH - 1] = L'\0';

  GetModuleFileName (hModSpecialK, wszSKFileName, MAX_PATH - 1);

  injector_dll = wszSKFileName;

  hInjectorDLL = hModSpecialK;

#if 1
  CreateThread ( nullptr,
                   0,
                     DllThread,
                       nullptr,
                         0x00,
                           nullptr );
#else

  // Not really a thread now is it? :P
  DllThread (nullptr);
#endif

  return TRUE;
}

extern "C" BOOL WINAPI _CRT_INIT (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

BOOL
APIENTRY
DllMain (HMODULE hModule,
         DWORD   ul_reason_for_call,
         LPVOID  /* lpReserved */)
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
    {
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
      hDLLMod = hModule;
    } break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
      break;

    case DLL_PROCESS_DETACH:
    {
      if (dll_log != nullptr) {
        tsf::WindowManager::Shutdown ();
        tsf::RenderFix::Shutdown     ();
        tsf::InputManager::Shutdown  ();
        tsf::TimingFix::Shutdown     ();

        TSFix_UnInit_MinHook ();
        TSFix_SaveConfig     ();

        dll_log->LogEx ( false, L"=========== (Version: v %s) "
                                L"===========\n",
                                TSFIX_VER_STR.c_str () );
        dll_log->LogEx ( true,  L"End TSFix Plug-In\n" );
        dll_log->LogEx ( false, L"------- [Tales of Symphonia \"Fix\"] "
                                L"-------\n" );

        dll_log->close ();
      }
      _CRT_INIT ((HINSTANCE)hModule, ul_reason_for_call, nullptr);
    } break;
  }

  return TRUE;
}