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

#define INCL_WINSOCK_API_PROTOTYPES 0
#include <WinSock2.h>

#include "winsock.h"
#include "log.h"

#include "hook.h"

typedef char* (WSAAPI *inet_ntoa_pfn)(_In_ struct in_addr in);

inet_ntoa_pfn inet_ntoa = nullptr;

typedef int (WINAPI *connect_pfn)(
  _In_ SOCKET                s,
  _In_ const struct sockaddr *name,
  _In_ int                   namelen
);

connect_pfn connect_Original = nullptr;

int
WINAPI
connect_Detour (
  _In_ SOCKET                s,
  _In_ const struct sockaddr *name,
  _In_ int                   namelen
)
{
  if (inet_ntoa != nullptr) {
    if (name->sa_family == AF_INET) {
      dll_log.Log (L" [Winsock]: Game established connection to (%hs)",
                   inet_ntoa (*(struct in_addr *)name->sa_data) );
    }
  }
  return connect_Original (s, name, namelen);
}


typedef int (WINAPI *WSAConnect_pfn)(
  _In_  SOCKET                s,
  _In_  const struct sockaddr *name,
  _In_  int                   namelen,
  _In_  LPWSABUF              lpCallerData,
  _Out_ LPWSABUF              lpCalleeData,
  _In_  LPQOS                 lpSQOS,
  _In_  LPQOS                 lpGQOS
);

WSAConnect_pfn WSAConnect_Original = nullptr;

int
WINAPI
WSAConnect_Detour(
  _In_  SOCKET                s,
  _In_  const struct sockaddr *name,
  _In_  int                   namelen,
  _In_  LPWSABUF              lpCallerData,
  _Out_ LPWSABUF              lpCalleeData,
  _In_  LPQOS                 lpSQOS,
  _In_  LPQOS                 lpGQOS
)
{
  if (inet_ntoa != nullptr) {
    if (name->sa_family == AF_INET) {
      dll_log.Log (L" [Winsock]: Game established connection to (%hs)",
                   inet_ntoa (*(struct in_addr *)name->sa_data) );
    }
  }
  return WSAConnect_Original (s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS);
}


typedef int (WINAPI *WSASend_pfn)(
  _In_  SOCKET                             s,
  _In_  LPWSABUF                           lpBuffers,
  _In_  DWORD                              dwBufferCount,
  _Out_ LPDWORD                            lpNumberOfBytesSent,
  _In_  DWORD                              dwFlags,
  _In_  LPWSAOVERLAPPED                    lpOverlapped,
  _In_  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

WSASend_pfn WSASend_Original = nullptr;

int
WINAPI
WSASend_Detour (
  _In_  SOCKET                             s,
  _In_  LPWSABUF                           lpBuffers,
  _In_  DWORD                              dwBufferCount,
  _Out_ LPDWORD                            lpNumberOfBytesSent,
  _In_  DWORD                              dwFlags,
  _In_  LPWSAOVERLAPPED                    lpOverlapped,
  _In_  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
) {
  int ret = WSASend_Original (s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
  dll_log.Log (L" [Winsock] Sent %d bytes of data <WSASend (...)>", *lpNumberOfBytesSent);
  return ret;
}


typedef int (WINAPI *send_pfn)(
  _In_       SOCKET s,
  _In_ const char   *buf,
  _In_       int    len,
  _In_       int    flags
);

send_pfn send_Original = nullptr;

int
WINAPI
send_Detour (_In_       SOCKET s,
             _In_ const char  *buf,
             _In_       int    len,
             _In_       int    flags)
{
  dll_log.Log (L" [Winsock] Sent %d bytes of data <send (...)>", len);

  return send_Original (s, buf, len, flags);
}

void
tsf::WinsockHook::Init (void)
{
  TSFix_CreateDLLHook ( L"Ws2_32.dll", "send",
                        send_Detour,
             (LPVOID *)&send_Original );

  TSFix_CreateDLLHook ( L"Ws2_32.dll", "connect",
                        connect_Detour,
             (LPVOID *)&connect_Original );

  TSFix_CreateDLLHook ( L"Ws2_32.dll", "WSASend",
                        WSASend_Detour,
             (LPVOID *)&WSASend_Original );

  TSFix_CreateDLLHook ( L"Ws2_32.dll", "WSAConnect",
                        WSAConnect_Detour,
             (LPVOID *)&WSAConnect_Original );

  HMODULE hModWS2 = GetModuleHandle (L"Ws2_32.dll");
  inet_ntoa =
    (inet_ntoa_pfn)
      GetProcAddress (hModWS2, "inet_ntoa");
}

void
tsf::WinsockHook::Shutdown (void)
{
}