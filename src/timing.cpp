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
#include "timing.h"
#include "config.h"
#include "render.h"
#include "hook.h"

QueryPerformanceCounter_pfn QueryPerformanceCounter_Original = nullptr;
Sleep_pfn                   Sleep_Original                   = nullptr;

int           last_sleep     =   1;
LARGE_INTEGER last_perfCount = { 0 };

void
WINAPI
Sleep_Detour (DWORD dwMilliseconds)
{
  if (GetCurrentThreadId () == tsf::RenderFix::dwRenderThreadID) {
    last_sleep = dwMilliseconds;
    return;
  }

  //if (config.framerate.yield_processor && dwMilliseconds == 0)
    //YieldProcessor ();

  if (dwMilliseconds >= config.stutter.shortest_sleep || (! config.stutter.fix)) {
    Sleep_Original (dwMilliseconds);
  }
}

BOOL
WINAPI
QueryPerformanceCounter_Detour (
  _Out_ LARGE_INTEGER *lpPerformanceCount
){
  DWORD dwThreadId = GetCurrentThreadId               ();
  BOOL  ret        = QueryPerformanceCounter_Original (lpPerformanceCount);

  if (last_sleep != 0 || (! config.stutter.fix) ||
     (dwThreadId != tsf::RenderFix::dwRenderThreadID)) {

    if (dwThreadId == tsf::RenderFix::dwRenderThreadID)
      memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

    return ret;

  } else if (dwThreadId == tsf::RenderFix::dwRenderThreadID) {
    const float fudge_factor = config.stutter.fudge_factor;

    last_sleep = -1;

    // Mess with the numbers slightly to prevent hiccups
    lpPerformanceCount->QuadPart +=
      (lpPerformanceCount->QuadPart - last_perfCount.QuadPart) * 
                        fudge_factor/* * freq.QuadPart*/;
    memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

    return ret;
  }

  if (dwThreadId == tsf::RenderFix::dwRenderThreadID)
    memcpy (&last_perfCount, lpPerformanceCount, sizeof (LARGE_INTEGER));

  return ret;
}

void
tsf::TimingFix::Init (void)
{
  TSFix_CreateDLLHook ( L"d3d9.dll", "QueryPerformanceCounter_Detour",
                        QueryPerformanceCounter_Detour, 
             (LPVOID *)&QueryPerformanceCounter_Original );

  TSFix_CreateDLLHook ( L"kernel32.dll", "Sleep",
                        Sleep_Detour, 
             (LPVOID *)&Sleep_Original );
}

void
tsf::TimingFix::Shutdown (void)
{
}