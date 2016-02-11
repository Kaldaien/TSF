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
#ifndef __TSFIX__RENDER_H__
#define __TSFIX__RENDER_H__

#include "command.h"

struct IDirect3DDevice9;
struct IDirect3DSurface9;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d9.h>

namespace tsf
{
  namespace RenderFix
  {
    struct tracer_s {
      bool log   = false;
      int  count = 0;
    } extern tracer;

    void Init     ();
    void Shutdown ();

    class CommandProcessor : public eTB_iVariableListener {
    public:
      CommandProcessor (void);

      virtual bool OnVarChange (eTB_Variable* var, void* val = NULL);

      static CommandProcessor* getInstance (void)
      {
        if (pCommProc == NULL)
          pCommProc = new CommandProcessor ();

        return pCommProc;
      }

    protected:

    private:
      static CommandProcessor* pCommProc;
    };

    extern uint32_t           width;
    extern uint32_t           height;
    extern bool               fullscreen;

    extern IDirect3DDevice9*  pDevice;
    extern HWND               hWndDevice;

    extern uint32_t           dwRenderThreadID;

    extern HMODULE            d3dx9_43_dll;
    extern HMODULE            user32_dll;
  }
}

typedef HRESULT (STDMETHODCALLTYPE *BeginScene_pfn)(
  IDirect3DDevice9 *This
);

typedef HRESULT (STDMETHODCALLTYPE *SetScissorRect_pfn)
(
        IDirect3DDevice9 *This,
  const RECT             *pRect
);

typedef HRESULT (STDMETHODCALLTYPE *CreateTexture_pfn)
(
  IDirect3DDevice9   *This,
  UINT                Width,
  UINT                Height,
  UINT                Levels,
  DWORD               Usage,
  D3DFORMAT           Format,
  D3DPOOL             Pool,
  IDirect3DTexture9 **ppTexture,
  HANDLE             *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *SetViewport_pfn)
(
        IDirect3DDevice9* This,
  CONST D3DVIEWPORT9*     pViewport);



typedef HRESULT (STDMETHODCALLTYPE *SetRenderState_pfn)
(
  IDirect3DDevice9*  This,
  D3DRENDERSTATETYPE State,
  DWORD              Value
);

typedef HRESULT (STDMETHODCALLTYPE *SetVertexShaderConstantF_pfn)
(
        IDirect3DDevice9 *This,
        UINT              StartRegister,
  CONST float            *pConstantData,
        UINT              Vector4fCount
);

typedef HRESULT (STDMETHODCALLTYPE *BMF_EndBufferSwap_pfn)
(
  HRESULT   hr,
  IUnknown *device
);

typedef D3DPRESENT_PARAMETERS* (__stdcall *BMF_SetPresentParamsD3D9_pfn)
(
  IDirect3DDevice9      *pDevice,
  D3DPRESENT_PARAMETERS *pParams
);

typedef HRESULT (STDMETHODCALLTYPE *SetSamplerState_pfn)
(
  IDirect3DDevice9    *This,
  DWORD                Sampler,
  D3DSAMPLERSTATETYPE  Type,
  DWORD                Value
);

#endif /* __TSFIX__RENDER_H__ */