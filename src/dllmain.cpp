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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "config.h"
#include "log.h"
#include "command.h"

#include "hook.h"

#include "timing.h"
#include "render.h"
#include "input.h"
#include "window.h"
#include "winsock.h"

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


  // Plugin State
  if (TSFix_Init_MinHook () == MH_OK) {
    tsf::WinsockHook::Init   ();
    tsf::InputManager::Init  ();
    tsf::TimingFix::Init     ();
    tsf::RenderFix::Init     ();
    tsf::WindowManager::Init ();
  }

  return 0;
}

BOOL
APIENTRY
DllMain (HMODULE hModule,
         DWORD    ul_reason_for_call,
         LPVOID  /* lpReserved */)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
  {
    DisableThreadLibraryCalls (hModule);

    hDLLMod = hModule;

    dll_log.init ("logs/tsfix.log", "w");
    dll_log.Log  (L"tsfix.log created");

    if (! TSFix_LoadConfig ()) {
      // Save a new config if none exists
      TSFix_SaveConfig ();
    }

    HANDLE hThread =
      CreateThread (NULL, NULL, DllThread, 0, 0, NULL);

    // Initialization delay
    if (hThread != 0)
      WaitForSingleObject (hThread, 250UL);
  } break;

  case DLL_PROCESS_DETACH:
    tsf::WindowManager::Shutdown ();
    tsf::RenderFix::Shutdown     ();
    tsf::TimingFix::Shutdown     ();
    tsf::InputManager::Shutdown  ();
    tsf::WinsockHook::Shutdown   ();

    TSFix_UnInit_MinHook ();
    TSFix_SaveConfig     ();

    dll_log.LogEx      (true,  L"Closing log file... ");
    dll_log.close      ();
    break;
  }

  return TRUE;
}