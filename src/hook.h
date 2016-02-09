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
#ifndef __TSFIX__HOOK_H__
#define __TSFIX__HOOK_H__

#pragma comment (lib, "MinHook/lib/libMinHook.x86.lib")
#include "MinHook/include/MinHook.h"

void
TSFix_DrawCommandConsole (void);

MH_STATUS
WINAPI
TSFix_CreateFuncHook ( LPCWSTR pwszFuncName,
                       LPVOID  pTarget,
                       LPVOID  pDetour,
                       LPVOID *ppOriginal );

MH_STATUS
WINAPI
TSFix_CreateDLLHook ( LPCWSTR pwszModule, LPCSTR  pszProcName,
                      LPVOID  pDetour,    LPVOID *ppOriginal,
                      LPVOID* ppFuncAddr = nullptr );

MH_STATUS
WINAPI
TSFix_EnableHook (LPVOID pTarget);

MH_STATUS
WINAPI
TSFix_DisableHook (LPVOID pTarget);

MH_STATUS
WINAPI
TSFix_RemoveHook (LPVOID pTarget);

MH_STATUS
WINAPI
TSFix_Init_MinHook (void);

MH_STATUS
WINAPI
TSFix_UnInit_MinHook (void);

#endif /* __TSFIX__HOOK_H__ */