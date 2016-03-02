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

#include <winsvc.h>
#include <comdef.h>

#pragma comment (lib, "kernel32.lib")

HMODULE hDLLMod      = { 0 }; // Handle to SELF
HMODULE hInjectorDLL = { 0 }; // Handle to Special K

typedef void (__stdcall *SK_SetPluginName_pfn)(std::wstring name);
SK_SetPluginName_pfn SK_SetPluginName = nullptr;

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
  dll_log.init  ( "logs/tsfix.log",
                    "w" );
  dll_log.LogEx ( false, L"------- [Tales of Symphonia \"Fix\"] "
                         L"-------\n" ); // <--- I was bored ;)
  dll_log.Log   (        L"tsfix.dll Plug-In\n"
                         L"=========== (Version: v %s) "
                         L"===========",
                           TSFIX_VER_STR.c_str () );

  if (! TSFix_LoadConfig ()) {
    // Save a new config if none exists
    TSFix_SaveConfig ();
  }

  hInjectorDLL =
    GetModuleHandle (config.system.injector.c_str ());

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

  //
  // Kill Raptr instead of it killing us!
  //
  extern void TSF_InitCompatBlacklist (void);
  TSF_InitCompatBlacklist ();

  // Plugin State
  if (TSFix_Init_MinHook () == MH_OK) {
    tsf::TimingFix::Init     ();
    tsf::WindowManager::Init ();
    tsf::InputManager::Init  ();
    tsf::RenderFix::Init     ();
  }

  return 0;
}

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
      DisableThreadLibraryCalls ((hDLLMod = hModule));

      // This is safe because this DLL is never loaded at launch, it is always
      //   loaded from d3d9.dll
      DllThread (nullptr);

      //CreateThread              (nullptr, 0, DllThread, nullptr, 0, nullptr);
    } break;

    case DLL_PROCESS_DETACH:
    {
      tsf::WindowManager::Shutdown ();
      tsf::RenderFix::Shutdown     ();
      tsf::InputManager::Shutdown  ();
      tsf::TimingFix::Shutdown     ();

      TSFix_UnInit_MinHook ();
      TSFix_SaveConfig     ();

      dll_log.LogEx ( false, L"=========== (Version: v %s) "
                             L"===========\n",
                               TSFIX_VER_STR.c_str () );
      dll_log.LogEx ( true,  L"End TSFix Plug-In\n" );
      dll_log.LogEx ( false, L"------- [Tales of Symphonia \"Fix\"] "
                             L"-------\n" );

      dll_log.close ();
    } break;
  }

  return TRUE;
}