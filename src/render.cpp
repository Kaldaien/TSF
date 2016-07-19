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
#include "render.h"
#include "config.h"
#include "log.h"
#include "window.h"
#include "timing.h"
#include "hook.h"

#include <stdint.h>

#include <comdef.h>

#include <d3d9.h>
#include <d3d9types.h>

#include <dwmapi.h>
#pragma comment (lib, "Dwmapi.lib")


BeginScene_pfn                          D3D9BeginScene_Original                      = nullptr;
EndScene_pfn                            D3D9EndScene_Original                        = nullptr;
SetScissorRect_pfn                      D3D9SetScissorRect_Original                  = nullptr;
SetViewport_pfn                         D3D9SetViewport_Original                     = nullptr;
SetRenderState_pfn                      D3D9SetRenderState_Original                  = nullptr;
SetVertexShaderConstantF_pfn            D3D9SetVertexShaderConstantF_Original        = nullptr;
SetSamplerState_pfn                     D3D9SetSamplerState_Original                 = nullptr;

DrawPrimitive_pfn                       D3D9DrawPrimitive_Original                   = nullptr;
DrawIndexedPrimitive_pfn                D3D9DrawIndexedPrimitive_Original            = nullptr;
DrawPrimitiveUP_pfn                     D3D9DrawPrimitiveUP_Original                 = nullptr;
DrawIndexedPrimitiveUP_pfn              D3D9DrawIndexedPrimitiveUP_Original          = nullptr;

BMF_SetPresentParamsD3D9_pfn            BMF_SetPresentParamsD3D9_Original            = nullptr;
BMF_BeginBufferSwap_pfn                 BMF_BeginBufferSwap                          = nullptr;
BMF_EndBufferSwap_pfn                   BMF_EndBufferSwap                            = nullptr;

tsf::RenderFix::tracer_s
  tsf::RenderFix::tracer;

tsf::RenderFix::tsf_draw_states_s
  tsf::RenderFix::draw_state;

// One copy per-frame is enough.
int blur_bypass = 0;

#include <map>
std::unordered_map <LPVOID, uint32_t> vs_checksums;
std::unordered_map <LPVOID, uint32_t> ps_checksums;

IDirect3DVertexShader9* g_pVS;
IDirect3DPixelShader9*  g_pPS;

// Store the CURRENT shader's checksum instead of repeatedly
//   looking it up in the above hashmaps.
uint32_t vs_checksum = 0;
uint32_t ps_checksum = 0;

extern uint32_t
crc32 (uint32_t crc, const void *buf, size_t size);

typedef HRESULT (STDMETHODCALLTYPE *SetVertexShader_t)
  (IDirect3DDevice9*       This,
   IDirect3DVertexShader9* pShader);

SetVertexShader_t D3D9SetVertexShader_Original = nullptr;

HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShader_Detour (IDirect3DDevice9*       This,
                            IDirect3DVertexShader9* pShader)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice)
    return D3D9SetVertexShader_Original (This, pShader);

  if (g_pVS != pShader) {
    if (pShader != nullptr) {
      if (vs_checksums.find (pShader) == vs_checksums.end ()) {
        UINT len;
        pShader->GetFunction (nullptr, &len);

        void* pbFunc = malloc (len);

        if (pbFunc != nullptr) {
          pShader->GetFunction (pbFunc, &len);

          vs_checksums [pShader] = crc32 (0, pbFunc, len);

          free (pbFunc);
        }
      }
    }
    else {
      vs_checksum = 0;
    }
  }

  vs_checksum = vs_checksums [pShader];

  g_pVS = pShader;
  return D3D9SetVertexShader_Original (This, pShader);
}


typedef HRESULT (STDMETHODCALLTYPE *SetPixelShader_t)
  (IDirect3DDevice9*      This,
   IDirect3DPixelShader9* pShader);

SetPixelShader_t D3D9SetPixelShader_Original = nullptr;

HRESULT
STDMETHODCALLTYPE
D3D9SetPixelShader_Detour (IDirect3DDevice9*      This,
                           IDirect3DPixelShader9* pShader)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice)
    return D3D9SetPixelShader_Original (This, pShader);

  if (g_pPS != pShader) {
    if (pShader != nullptr) {
      if (ps_checksums.find (pShader) == ps_checksums.end ()) {
        UINT len;
        pShader->GetFunction (nullptr, &len);

        void* pbFunc = malloc (len);

        if (pbFunc != nullptr) {
          pShader->GetFunction (pbFunc, &len);

          ps_checksums [pShader] = crc32 (0, pbFunc, len);

          free (pbFunc);
        }
      }
    } else {
      ps_checksum = 0;
    }
  }

  ps_checksum = ps_checksums [pShader];

  g_pPS = pShader;
  return D3D9SetPixelShader_Original (This, pShader);
}

// This is used to set the draw_state at the end of each frame,
//   instead of mid-frame.
bool use_msaa = true;

//
// TODO: Move to Render Utilities
//
const wchar_t*
SK_D3D9_RenderStateToStr (D3DRENDERSTATETYPE rs)
{
  switch (rs)
  {
    case D3DRS_ZENABLE                   : return L"D3DRS_ZENABLE";
    case D3DRS_FILLMODE                  : return L"D3DRS_FILLMODE";
    case D3DRS_SHADEMODE                 : return L"D3DRS_SHADEMODE";
    case D3DRS_ZWRITEENABLE              : return L"D3DRS_ZWRITEENABLE";
    case D3DRS_ALPHATESTENABLE           : return L"D3DRS_ALPHATESTENABLE";
    case D3DRS_LASTPIXEL                 : return L"D3DRS_LASTPIXEL";
    case D3DRS_SRCBLEND                  : return L"D3DRS_SRCBLEND";
    case D3DRS_DESTBLEND                 : return L"D3DRS_DESTBLEND";
    case D3DRS_CULLMODE                  : return L"D3DRS_CULLMODE";
    case D3DRS_ZFUNC                     : return L"D3DRS_ZFUNC";
    case D3DRS_ALPHAREF                  : return L"D3DRS_ALPHAREF";
    case D3DRS_ALPHAFUNC                 : return L"D3DRS_ALPHAFUNC";
    case D3DRS_DITHERENABLE              : return L"D3DRS_DITHERENABLE";
    case D3DRS_ALPHABLENDENABLE          : return L"D3DRS_ALPHABLENDENABLE";
    case D3DRS_FOGENABLE                 : return L"D3DRS_FOGENABLE";
    case D3DRS_SPECULARENABLE            : return L"D3DRS_SPECULARENABLE";
    case D3DRS_FOGCOLOR                  : return L"D3DRS_FOGCOLOR";
    case D3DRS_FOGTABLEMODE              : return L"D3DRS_FOGTABLEMODE";
    case D3DRS_FOGSTART                  : return L"D3DRS_FOGSTART";
    case D3DRS_FOGEND                    : return L"D3DRS_FOGEND";
    case D3DRS_FOGDENSITY                : return L"D3DRS_FOGDENSITY";
    case D3DRS_RANGEFOGENABLE            : return L"D3DRS_RANGEFOGENABLE";
    case D3DRS_STENCILENABLE             : return L"D3DRS_STENCILENABLE";
    case D3DRS_STENCILFAIL               : return L"D3DRS_STENCILFAIL";
    case D3DRS_STENCILZFAIL              : return L"D3DRS_STENCILZFAIL";
    case D3DRS_STENCILPASS               : return L"D3DRS_STENCILPASS";
    case D3DRS_STENCILFUNC               : return L"D3DRS_STENCILFUNC";
    case D3DRS_STENCILREF                : return L"D3DRS_STENCILREF";
    case D3DRS_STENCILMASK               : return L"D3DRS_STENCILMASK";
    case D3DRS_STENCILWRITEMASK          : return L"D3DRS_STENCILWRITEMASK";
    case D3DRS_TEXTUREFACTOR             : return L"D3DRS_TEXTUREFACTOR";
    case D3DRS_WRAP0                     : return L"D3DRS_WRAP0";
    case D3DRS_WRAP1                     : return L"D3DRS_WRAP1";
    case D3DRS_WRAP2                     : return L"D3DRS_WRAP2";
    case D3DRS_WRAP3                     : return L"D3DRS_WRAP3";
    case D3DRS_WRAP4                     : return L"D3DRS_WRAP4";
    case D3DRS_WRAP5                     : return L"D3DRS_WRAP5";
    case D3DRS_WRAP6                     : return L"D3DRS_WRAP6";
    case D3DRS_WRAP7                     : return L"D3DRS_WRAP7";
    case D3DRS_CLIPPING                  : return L"D3DRS_CLIPPING";
    case D3DRS_LIGHTING                  : return L"D3DRS_LIGHTING";
    case D3DRS_AMBIENT                   : return L"D3DRS_AMBIENT";
    case D3DRS_FOGVERTEXMODE             : return L"D3DRS_FOGVERTEXMODE";
    case D3DRS_COLORVERTEX               : return L"D3DRS_COLORVERTEX";
    case D3DRS_LOCALVIEWER               : return L"D3DRS_LOCALVIEWER";
    case D3DRS_NORMALIZENORMALS          : return L"D3DRS_NORMALIZENORMALS";
    case D3DRS_DIFFUSEMATERIALSOURCE     : return L"D3DRS_DIFFUSEMATERIALSOURCE";
    case D3DRS_SPECULARMATERIALSOURCE    : return L"D3DRS_SPECULARMATERIALSOURCE";
    case D3DRS_AMBIENTMATERIALSOURCE     : return L"D3DRS_AMBIENTMATERIALSOURCE";
    case D3DRS_EMISSIVEMATERIALSOURCE    : return L"D3DRS_EMISSIVEMATERIALSOURCE";
    case D3DRS_VERTEXBLEND               : return L"D3DRS_VERTEXBLEND";
    case D3DRS_CLIPPLANEENABLE           : return L"D3DRS_CLIPPLANEENABLE";
    case D3DRS_POINTSIZE                 : return L"D3DRS_POINTSIZE";
    case D3DRS_POINTSIZE_MIN             : return L"D3DRS_POINTSIZE_MIN";
    case D3DRS_POINTSPRITEENABLE         : return L"D3DRS_POINTSPRITEENABLE";
    case D3DRS_POINTSCALEENABLE          : return L"D3DRS_POINTSCALEENABLE";
    case D3DRS_POINTSCALE_A              : return L"D3DRS_POINTSCALE_A";
    case D3DRS_POINTSCALE_B              : return L"D3DRS_POINTSCALE_B";
    case D3DRS_POINTSCALE_C              : return L"D3DRS_POINTSCALE_C";
    case D3DRS_MULTISAMPLEANTIALIAS      : return L"D3DRS_MULTISAMPLEANTIALIAS";
    case D3DRS_MULTISAMPLEMASK           : return L"D3DRS_MULTISAMPLEMASK";
    case D3DRS_PATCHEDGESTYLE            : return L"D3DRS_PATCHEDGESTYLE";
    case D3DRS_DEBUGMONITORTOKEN         : return L"D3DRS_DEBUGMONITORTOKEN";
    case D3DRS_POINTSIZE_MAX             : return L"D3DRS_POINTSIZE_MAX";
    case D3DRS_INDEXEDVERTEXBLENDENABLE  : return L"D3DRS_INDEXEDVERTEXBLENDENABLE";
    case D3DRS_COLORWRITEENABLE          : return L"D3DRS_COLORWRITEENABLE";
    case D3DRS_TWEENFACTOR               : return L"D3DRS_TWEENFACTOR";
    case D3DRS_BLENDOP                   : return L"D3DRS_BLENDOP";
    case D3DRS_POSITIONDEGREE            : return L"D3DRS_POSITIONDEGREE";
    case D3DRS_NORMALDEGREE              : return L"D3DRS_NORMALDEGREE";
    case D3DRS_SCISSORTESTENABLE         : return L"D3DRS_SCISSORTESTENABLE";
    case D3DRS_SLOPESCALEDEPTHBIAS       : return L"D3DRS_SLOPESCALEDEPTHBIAS";
    case D3DRS_ANTIALIASEDLINEENABLE     : return L"D3DRS_ANTIALIASEDLINEENABLE";
    case D3DRS_MINTESSELLATIONLEVEL      : return L"D3DRS_MINTESSELLATIONLEVEL";
    case D3DRS_MAXTESSELLATIONLEVEL      : return L"D3DRS_MAXTESSELLATIONLEVEL";
    case D3DRS_ADAPTIVETESS_X            : return L"D3DRS_ADAPTIVETESS_X";
    case D3DRS_ADAPTIVETESS_Y            : return L"D3DRS_ADAPTIVETESS_Y";
    case D3DRS_ADAPTIVETESS_Z:             return L"D3DRS_ADAPTIVETESS_Z";
    case D3DRS_ADAPTIVETESS_W:             return L"D3DRS_ADAPTIVETESS_W";
    case D3DRS_ENABLEADAPTIVETESSELLATION: return L"D3DRS_ENABLEADAPTIVETESSELLATION";
    case D3DRS_TWOSIDEDSTENCILMODE:        return L"D3DRS_TWOSIDEDSTENCILMODE";
    case D3DRS_CCW_STENCILFAIL:            return L"D3DRS_CCW_STENCILFAIL";
    case D3DRS_CCW_STENCILZFAIL:           return L"D3DRS_CCW_STENCILZFAIL";
    case D3DRS_CCW_STENCILPASS:            return L"D3DRS_CCW_STENCILPASS";
    case D3DRS_CCW_STENCILFUNC:            return L"D3DRS_CCW_STENCILFUNC";
    case D3DRS_COLORWRITEENABLE1:          return L"D3DRS_COLORWRITEENABLE1";
    case D3DRS_COLORWRITEENABLE2:          return L"D3DRS_COLORWRITEENABLE2";
    case D3DRS_COLORWRITEENABLE3:          return L"D3DRS_COLORWRITEENABLE3";
    case D3DRS_BLENDFACTOR:                return L"D3DRS_BLENDFACTOR";
    case D3DRS_SRGBWRITEENABLE:            return L"D3DRS_SRGBWRITEENABLE";
    case D3DRS_DEPTHBIAS:                  return L"D3DRS_DEPTHBIAS";
    case D3DRS_WRAP8:                      return L"D3DRS_WRAP8";
    case D3DRS_WRAP9:                      return L"D3DRS_WRAP9";
    case D3DRS_WRAP10:                     return L"D3DRS_WRAP10";
    case D3DRS_WRAP11:                     return L"D3DRS_WRAP11";
    case D3DRS_WRAP12:                     return L"D3DRS_WRAP12";
    case D3DRS_WRAP13:                     return L"D3DRS_WRAP13";
    case D3DRS_WRAP14:                     return L"D3DRS_WRAP14";
    case D3DRS_WRAP15:                     return L"D3DRS_WRAP15";
    case D3DRS_SEPARATEALPHABLENDENABLE:   return L"D3DRS_SEPARATEALPHABLENDENABLE";
    case D3DRS_SRCBLENDALPHA:              return L"D3DRS_SRCBLENDALPHA"; 
    case D3DRS_DESTBLENDALPHA:             return L"D3DRS_DESTBLENDALPHA";
    case D3DRS_BLENDOPALPHA:               return L"D3DRS_BLENDOPALPHA";
  }

  static wchar_t wszUnknown [32];
  wsprintf (wszUnknown, L"UNKNOWN: %lu", rs);
  return wszUnknown;
}

bool early_resolve = false;

#include "render/textures.h"

COM_DECLSPEC_NOTHROW
void
STDMETHODCALLTYPE
D3D9EndFrame_Pre (void)
{
  // Add a constant amount of MSAA work at the end of each frame,
  //   in practive this is not a valid optimization. Deferring the resolve
  //     until late in the next frame generally works better...
  if (early_resolve) {
    extern void ResolveAllDirty (void);
    ResolveAllDirty ();
  }

  tsf::RenderFix::dwRenderThreadID = GetCurrentThreadId ();

  void TSFix_DrawCommandConsole (void);
  TSFix_DrawCommandConsole ();

  return BMF_BeginBufferSwap ();
}


COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9EndFrame_Post (HRESULT hr, IUnknown* device)
{
  // Ignore anything that's not the primary render device.
  if (device != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: D3D9 frame ended on unknown IDirect3DDevice9! << ");

    return BMF_EndBufferSwap (hr, device);
  }

  hr = BMF_EndBufferSwap (hr, device);

  if (tsf::RenderFix::tracer.log && tsf::RenderFix::tracer.count > 0) {
    dll_log.Log (L"[FrameTrace] --- SwapChain Present ---");
    if (--tsf::RenderFix::tracer.count <= 0)
      tsf::RenderFix::tracer.log = false;
  }

  blur_bypass = 0;
  tsf::RenderFix::draw_state.frames++;

  // Push any changes to this state at the very end of a frame.
  tsf::RenderFix::draw_state.use_msaa = use_msaa &&
                                        tsf::RenderFix::draw_state.has_msaa;

  //  Is the window in the foreground? (NV compatibility hack)
  if ((! tsf::window.active) && config.window.disable_bg_msaa)
    tsf::RenderFix::draw_state.use_msaa = false;

  extern void TSFix_LoadQueuedTextures (void);
  TSFix_LoadQueuedTextures ();

  return hr;
}

typedef HRESULT (__stdcall *Reset_pfn)(
  IDirect3DDevice9     *This,
 D3DPRESENT_PARAMETERS *pPresentationParameters
);

Reset_pfn D3D9Reset_Original = nullptr;

COM_DECLSPEC_NOTHROW
HRESULT
__stdcall
D3D9Reset_Detour ( IDirect3DDevice9      *This,
                   D3DPRESENT_PARAMETERS *pPresentationParameters )
{
  if (This != tsf::RenderFix::pDevice)
    return D3D9Reset_Original (This, pPresentationParameters);

  tsf::RenderFix::tex_mgr.reset         ();

  HRESULT hr =
    D3D9Reset_Original (This, pPresentationParameters);

  // We override this for D3D9, but the game's menu will not work
  //   properly without doing this...
  pPresentationParameters->Windowed = (! tsf::RenderFix::fullscreen);

  return hr;
}

#define __PTR_SIZE   sizeof LPCVOID 
#define __PAGE_PRIVS PAGE_EXECUTE_READWRITE 
 
#define D3D9_VIRTUAL_OVERRIDE(_Base,_Index,_Name,_Override,_Original,_Type) {  \
   void** vftable = *(void***)*_Base;                                          \
                                                                               \
   if (vftable [_Index] != _Override) {                                        \
     DWORD dwProtect;                                                          \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, __PAGE_PRIVS, &dwProtect); \
                                                                               \
       if (_Original == NULL)                                                  \
       _Original = (##_Type)vftable [_Index];                                  \
                                                                               \
     vftable [_Index] = _Override;                                             \
                                                                               \
     VirtualProtect (&vftable [_Index], __PTR_SIZE, dwProtect, &dwProtect);    \
                                                                               \
  }                                                                            \
 }

__declspec (noinline)
D3DPRESENT_PARAMETERS*
__stdcall
BMF_SetPresentParamsD3D9_Detour (IDirect3DDevice9*      device,
                                 D3DPRESENT_PARAMETERS* pparams)
{
  static D3DPRESENT_PARAMETERS present_params;

  //
  // TODO: Figure out what the hell is doing this when RTSS is allowed to use
  //         custom D3D libs. 1x1@0Hz is obviously NOT for rendering!
  //
  if ( pparams->BackBufferWidth            == 1 &&
       pparams->BackBufferHeight           == 1 &&
       pparams->FullScreen_RefreshRateInHz == 0 ) {
    dll_log.Log (L"[Render Fix] * Fake D3D9Ex Device Detected... Ignoring!");
    return BMF_SetPresentParamsD3D9_Original (device, pparams);
  }

  memcpy (&present_params, pparams, sizeof D3DPRESENT_PARAMETERS);

  // We must let this go, it's not valid anymore.
  tsf::RenderFix::draw_state.blur_proxy.first = nullptr;
  tsf::RenderFix::active_samplers.clear ();
  //tsf::RenderFix::tex_mgr.reset         ();

  if (pparams != nullptr) {
    tsf::RenderFix::fullscreen = (! pparams->Windowed);

    BOOL win7 = (LOBYTE (LOWORD (GetVersion ())) == 6  &&
                 HIBYTE (LOWORD (GetVersion ())) >= 1) ||
                 LOBYTE (LOWORD (GetVersion () > 6));

    if (win7 && config.render.allow_flipex && ( (! tsf::RenderFix::fullscreen)  ||
                                                 config.render.allow_background ||
                                                 config.window.borderless ) ) {
      dll_log.Log ( L"[Render Fix] Opt-In:  D3D9Ex FlipEx Model  (Windows 7+ Detected)");
      dll_log.Log ( L"[Render Fix]          ^^ %lu Backbuffers ^^", config.render.backbuffers);

      pparams->SwapEffect           = D3DSWAPEFFECT_FLIPEX;
      pparams->BackBufferCount      = config.render.backbuffers;
      pparams->PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }

    // Do not use Flip EX in fullscreen mode
    else {
      pparams->SwapEffect           = D3DSWAPEFFECT_FLIP;
      pparams->BackBufferCount      = 1;
      pparams->PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }

    //
    // MSAA Override
    //
    if (config.render.msaa_samples > 0 && device != nullptr) {
      IDirect3D9* pD3D9 = nullptr;
      if (SUCCEEDED (device->GetDirect3D (&pD3D9))) {
        DWORD dwQualityLevels;
        if (SUCCEEDED ( pD3D9->CheckDeviceMultiSampleType (
                0, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8,
                  false /* Full-Scene */,
                    (D3DMULTISAMPLE_TYPE)config.render.msaa_samples,
                      &dwQualityLevels )
                      )
           ) {
          dll_log.Log ( L"[Render Fix] >> Render device has %d quality level(s) available "
                        L"for %d-Sample MSAA.",
                          dwQualityLevels, config.render.msaa_samples );

          if (dwQualityLevels > 0) {
            // After range restriction, write the correct value back to the config
            //   file
            config.render.msaa_quality = min ( dwQualityLevels-1,
                                                  config.render.msaa_quality );

            dll_log.Log ( L"[Render Fix]  (*) Selected %dxMSAA Quality Level: %d",
                            config.render.msaa_samples,
                              config.render.msaa_quality );
            tsf::RenderFix::draw_state.has_msaa = true;
          }
        }

        else {
            dll_log.Log ( L"[Render Fix]  ### Requested %dxMSAA Quality Level: %d invalid",
                            config.render.msaa_samples,
                              config.render.msaa_quality );
        }

        pD3D9->Release ();
      }
    }

    if (device != nullptr) {
      dll_log.Log ( L"[Render Fix] %% Caught D3D9 Swapchain :: Fullscreen=%s "
                    L" (%lux%lu@%lu Hz) "
                    L" [Device Window: 0x%04X, Pointer: %ph]",
                      pparams->Windowed ? L"False" :

                       L"True",
                        pparams->BackBufferWidth,
                          pparams->BackBufferHeight,
                            pparams->FullScreen_RefreshRateInHz,
                              pparams->hDeviceWindow,
                                device );
    }

    // Some games don't set this even though it's supposed to be
    //   required by the D3D9 API...
    if (pparams->hDeviceWindow != nullptr)
      tsf::RenderFix::hWndDevice = pparams->hDeviceWindow;

    tsf::window.hwnd = tsf::RenderFix::hWndDevice;

    // Setup window message detouring as soon as a window is created..
    if (tsf::window.WndProc_Original == nullptr) {
      tsf::window.WndProc_Original =
        (WNDPROC)GetWindowLong (tsf::RenderFix::hWndDevice, GWL_WNDPROC);

      extern LRESULT
      CALLBACK
      DetourWindowProc ( _In_  HWND   hWnd,
                         _In_  UINT   uMsg,
                         _In_  WPARAM wParam,
                         _In_  LPARAM lParam );

      SetWindowLongA_Original ( tsf::RenderFix::hWndDevice,
                                  GWL_WNDPROC,
                                    (LONG)DetourWindowProc );
    }

    tsf::RenderFix::pDevice    = device;

    tsf::RenderFix::width  = pparams->BackBufferWidth;
    tsf::RenderFix::height = pparams->BackBufferHeight;

    DEVMODE devmode = { 0 };
    devmode.dmSize  = sizeof DEVMODE;

    // We have to call this once with a value of 0, or Windows is not going to be happy...
    EnumDisplaySettings (nullptr, 0,                     &devmode);
    EnumDisplaySettings (nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    //
    // Fullscreen mode while allow_background is enabled must be implemented as
    //   a window, or Alt+Tab will not work.
    //
    if (config.render.allow_background && tsf::RenderFix::fullscreen)
      config.window.borderless = true;

    pparams->FullScreen_RefreshRateInHz = config.render.refresh_rate;

    if ( devmode.dmPelsHeight       < tsf::RenderFix::height ||
         devmode.dmPelsWidth        < tsf::RenderFix::width  ||
        (devmode.dmDisplayFrequency != pparams->FullScreen_RefreshRateInHz &&
         0                          != pparams->FullScreen_RefreshRateInHz) ) {
      ZeroMemory (&devmode, sizeof DEVMODE);
           devmode.dmSize = sizeof DEVMODE;

      devmode.dmBitsPerPel       = 32; // Win 8+ cannot use anything else
      devmode.dmFields           = DM_BITSPERPEL;

      devmode.dmFields |= (devmode.dmPelsWidth  < tsf::RenderFix::width  ? DM_PELSWIDTH        : 0) |
                          (devmode.dmPelsHeight < tsf::RenderFix::height ? DM_PELSHEIGHT       : 0) |
                          (pparams->FullScreen_RefreshRateInHz != 0      ? DM_DISPLAYFREQUENCY : 0);

      devmode.dmPelsWidth        = tsf::RenderFix::width;
      devmode.dmPelsHeight       = tsf::RenderFix::height;
      devmode.dmDisplayFrequency = pparams->FullScreen_RefreshRateInHz;

      dll_log.LogEx (true, L"[Render Fix] Requested display mode requires display mode change... ");

      if (DISP_CHANGE_SUCCESSFUL != ChangeDisplaySettings (&devmode, CDS_FULLSCREEN)) {
        dll_log.LogEx (false, L"failed!\n");
      } else {
        dll_log.LogEx (false, L"success!\n");
      }

      EnumDisplaySettings (nullptr, 0,                     &devmode);
      EnumDisplaySettings (nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    }

    if (config.window.borderless || (! tsf::RenderFix::fullscreen)) {
      DwmEnableMMCSS (TRUE);

      // The game will draw in windowed mode, but it will think it's fullscreen
      pparams->Windowed  = true;
    } else {
      DwmEnableMMCSS (FALSE);
    }

    if (! tsf::RenderFix::fullscreen) {
      bool shrunk = false;

      if (devmode.dmPelsHeight < tsf::RenderFix::height ||
          devmode.dmPelsWidth  < tsf::RenderFix::width) {
        tsf::RenderFix::height    = devmode.dmPelsHeight;
        pparams->BackBufferHeight = devmode.dmPelsHeight;

        tsf::RenderFix::width     = devmode.dmPelsWidth;
        pparams->BackBufferWidth  = devmode.dmPelsWidth;

        shrunk                    = true;
      }

      if (shrunk) {
        dll_log.Log ( L"[Render Fix] Original window dimensions (%lux%lu) were "
                      L"impossible given desktop -- Shrunk to (%lux%lu)",
                        present_params.BackBufferWidth,
                        present_params.BackBufferHeight,
                          tsf::RenderFix::width,
                          tsf::RenderFix::height );
      }
    }
  }

  BringWindowToTop (tsf::RenderFix::hWndDevice);
  SetActiveWindow  (tsf::RenderFix::hWndDevice);

  // Reset will fail without this
  if (pparams->Windowed)
    pparams->FullScreen_RefreshRateInHz = 0;

  if (config.window.borderless)
    tsf::WindowManager::border.Disable ();
  else
    tsf::WindowManager::border.Enable  ();

  return BMF_SetPresentParamsD3D9_Original (device, pparams);
}

#include "hook.h"

void
TSF_ComputeAspectCoeffs (float& x, float& y, float& xoff, float& yoff)
{
  yoff = 0.0f;
  xoff = 0.0f;

  x = 1.0f;
  y = 1.0f;

  //if (! (config.render.aspect_correction || config.render.blackbar_videos))
    //return;

  const float aspect_ratio = (float)tsf::RenderFix::width / (float)tsf::RenderFix::height;
  float       rescale      =                 (1.77777778f / aspect_ratio);

  // No correction needed if we are _close_ to 16:9
  if (aspect_ratio <= 1.7778f && aspect_ratio >= 1.7776f) {
    return;
  }

  // Wider
  if (aspect_ratio > 1.7777f) {
    int width = (16.0f / 9.0f) * tsf::RenderFix::height;
    int x_off = (tsf::RenderFix::width - width) / 2;

    x    = (float)tsf::RenderFix::width / (float)width;
    xoff = x_off;
  } else {
    int height = (9.0f / 16.0f) * tsf::RenderFix::width;
    int y_off  = (tsf::RenderFix::height - height) / 2;

    y    = (float)tsf::RenderFix::height / (float)height;
    yoff = y_off;
  }
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetViewport_Detour (IDirect3DDevice9* This,
                  CONST D3DVIEWPORT9*     pViewport)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: D3D9 command came from unknown IDirect3DDevice9! << ");
    return D3D9SetViewport_Original (This, pViewport);
  }

  tsf::RenderFix::draw_state.vp = *pViewport;

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetViewport     - [%lu,%lu] / [%lu,%lu] :: [%f - %f]",
                    pViewport->X, pViewport->Y,
                      pViewport->Width, pViewport->Height,
                        pViewport->MinZ, pViewport->MaxZ );
  }

  // ...
  if (pViewport->Width == 1280 && pViewport->Height == 720) {
    tsf::RenderFix::draw_state.postprocessing = false;
    tsf::RenderFix::draw_state.fullscreen     = true; // TODO: Check viewport against rendertarget

    D3DVIEWPORT9 newView = *pViewport;

    float x_scale = (float)tsf::RenderFix::width  / 1280.0f;
    float y_scale = (float)tsf::RenderFix::height / 720.0f;

    newView.Width  = newView.Width  * x_scale - (newView.X * 2.0f);
    newView.Height = newView.Height * y_scale - (newView.Y * 2.0f);

    return D3D9SetViewport_Original (This, &newView);
  }

  // Effects
  if (pViewport->Width == 512 && pViewport->Height == 256) {
    tsf::RenderFix::draw_state.postprocessing = true;
    tsf::RenderFix::draw_state.fullscreen     = false;

    D3DVIEWPORT9 newView = *pViewport;

    newView.Width  = tsf::RenderFix::width  * config.render.postproc_ratio;
    newView.Height = tsf::RenderFix::height * config.render.postproc_ratio;

    return D3D9SetViewport_Original (This, &newView);
  }

  // (Other?) Effects
  if (pViewport->Width == 256 && pViewport->Height == 256) {
    tsf::RenderFix::draw_state.postprocessing = true;
    tsf::RenderFix::draw_state.fullscreen     = false;

    D3DVIEWPORT9 newView = *pViewport;

    newView.Width  = pViewport->Width;//  * config.render.postproc_ratio;
    newView.Height = pViewport->Height;// * config.render.postproc_ratio;

    return D3D9SetViewport_Original (This, &newView);
  }


  return D3D9SetViewport_Original (This, pViewport);
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetVertexShaderConstantF_Detour (IDirect3DDevice9* This,
                                     UINT              StartRegister,
                                     CONST float*      pConstantData,
                                     UINT              Vector4fCount)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: D3D9 command came from unknown IDirect3DDevice9! << ");

    return D3D9SetVertexShaderConstantF_Original ( This,
                                                     StartRegister,
                                                       pConstantData,
                                                         Vector4fCount );
  }

  tsf::RenderFix::draw_state.last_vs_vec4 = Vector4fCount;

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetVertexShaderConstantF\n"
                  L"                         [FrameTrace]                 "
                  L"- StartRegister: %lu, Vector4fCount: %lu",
                    StartRegister, Vector4fCount );
    for (int i = 0; i < Vector4fCount; i++) {
      dll_log.LogEx ( false, L"                         [FrameTrace]"
                             L"                 - %11.6f %11.6f "
                                                L"%11.6f %11.6f\n",
                       pConstantData [i*4+0],pConstantData [i*4+1],
                       pConstantData [i*4+2],pConstantData [i*4+3] );
    }
  }

  if (StartRegister == 240 && Vector4fCount == 1) {
    D3DVIEWPORT9 vp;
    This->GetViewport (&vp);

    float vals [4] = { -1.0f / vp.Width, 1.0f / vp.Height,
                        0.0f,            0.0f              };

    return
      D3D9SetVertexShaderConstantF_Original (
        This,
          StartRegister,
            vals,
              Vector4fCount
        );
  }

  return
    D3D9SetVertexShaderConstantF_Original (
          This,
            StartRegister,
              pConstantData,
                Vector4fCount 
      );
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9BeginScene_Detour (IDirect3DDevice9* This)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: D3D9 BeginScene came from unknown IDirect3DDevice9! << ");

    return D3D9BeginScene_Original (This);
  }

  tsf::RenderFix::draw_state.draws = 0;

  bool msaa = false;

  if (config.render.msaa_samples > 0 && tsf::RenderFix::draw_state.has_msaa &&
                                        tsf::RenderFix::draw_state.use_msaa) {
    msaa = true;
  }

  HRESULT result = D3D9BeginScene_Original (This);

  // Don't do this until resources are allocated.
  if (tsf::RenderFix::tex_mgr.numMSAASurfs () > 0) {
    // Turn this on/off at the beginning of every frame
    D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                    D3DRS_MULTISAMPLEANTIALIAS,
                                      msaa );
  }

  return result;
}


COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9EndScene_Detour (IDirect3DDevice9* This)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9EndScene_Original (This);
  }

  // Don't use MSAA for stuff drawn directly to the framebuffer
  D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                  D3DRS_MULTISAMPLEANTIALIAS,
                                    false );

  return D3D9EndScene_Original (This);
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetScissorRect_Detour ( _In_       IDirect3DDevice9* This,
                            _In_ const RECT*             pRect )
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: SetScissorRect came from unknown IDirect3DDevice9! << ");

    return D3D9SetScissorRect_Original (This, pRect);
  }

  if (tsf::RenderFix::tracer.log && pRect != nullptr) {
    dll_log.Log ( L"[FrameTrace] SetScissorRect  - [%lu,%lu] / [%lu,%lu]",
                    pRect->left, pRect->top,
                      pRect->right, pRect->bottom );
  }

  RECT newR;

  // TOBE REMVOED - This is here to demonstrate the problems, since they
  //                  can be hard to spot in all but a few well-known places.
  if (config.render.durante_scissor) {
    newR = *pRect;

    double sFactor = tsf::RenderFix::width / 1280.0;

    newR.right  = static_cast <long> (newR.right  * sFactor);
    newR.bottom = static_cast <long> (newR.bottom * sFactor);
  } else {
    if (! tsf::RenderFix::draw_state.postprocessing) {
      struct {
        uint32_t width  = tsf::RenderFix::draw_state.vp.Width;
        uint32_t height = tsf::RenderFix::draw_state.vp.Height;
      } in;

      struct {
        uint32_t width  = tsf::RenderFix::width;
        uint32_t height = tsf::RenderFix::height;
      } out;

#if 0
      dll_log.Log ( L"Scissor --Viewport-- Width: %f, Height: %f :: (x: %f, y: %f)",
                      in.width, in.height, in.x, in.y );

      // I imagine querying this from the D3D9 (usermode) driver every frame is
      //   inefficient ... consider caching surface descriptors
      IDirect3DSurface9* pSurf = nullptr;
      if (SUCCEEDED (This->GetRenderTarget (0, &pSurf)) && pSurf != nullptr) {
        D3DSURFACE_DESC desc;
        pSurf->GetDesc (&desc);
        pSurf->Release ();

        out.width  = desc.Width;
        out.height = desc.Height;

        dll_log.Log ( L"Scissor --Surface-- Width: %f, Height: %f",
                        screen_width, screen_height );
      }
#endif

      struct {
        float left, right;
        float top,  bottom;
      } ndc_scissor;

      ndc_scissor.left  =
        2.0f * ((float)(pRect->left)  / (float)in.width) - 1.0f;
      ndc_scissor.right =
        2.0f * ((float)(pRect->right) / (float)in.width) - 1.0f;

      ndc_scissor.top  =
        2.0f * ((float)(pRect->top)    / (float)in.height) - 1.0f;
      ndc_scissor.bottom =
        2.0f * ((float)(pRect->bottom) / (float)in.height) - 1.0f;

      // Add +/- 1% to left/right
      if (pRect->left == 188 && pRect->top == 114 /*&&
          in.width    == 720 && in.height  == 1280*/) {
        ndc_scissor.left  -= 0.01f;
        ndc_scissor.right += 0.01f;
      }

      // Synopsis screen
      if (pRect->left  == 0    && pRect->top    == 125 &&
          pRect->right == 1280 && pRect->bottom == 645 ) {
        ndc_scissor.bottom += 0.1f;
      }

      newR.right  = (ndc_scissor.right  * out.width  + out.width)  / 2;
      newR.left   = (ndc_scissor.left   * out.width  + out.width)  / 2;

      newR.top    = (ndc_scissor.top    * out.height + out.height) / 2;
      newR.bottom = (ndc_scissor.bottom * out.height + out.height) / 2;
    } else {
      dll_log.Log (L"[Render Fix] >> Scissor Rectangle Applied During Post-Processing!");
    }
  }

  return D3D9SetScissorRect_Original (This, &newR);
}

// Optimal depth offsets
float depth_offset1 =  0.000001f; // Constant
float depth_offset2 = -0.666666f; // Slope
float depth_offset  =  0.000255f;

bool doing_outlines = false;

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitive_Detour (IDirect3DDevice9* This,
                          D3DPRIMITIVETYPE  PrimitiveType,
                          UINT              StartVertex,
                          UINT              PrimitiveCount)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: DrawPrimitive came from unknown IDirect3DDevice9! << ");

    return D3D9DrawPrimitive_Original ( This, PrimitiveType,
                                                 StartVertex, PrimitiveCount );
  }

  tsf::RenderFix::draw_state.draws++;

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] DrawPrimitive - %X, StartVertex: %lu, PrimitiveCount: %lu",
                      PrimitiveType, StartVertex, PrimitiveCount );
  }

  return D3D9DrawPrimitive_Original ( This, PrimitiveType,
                                        StartVertex, PrimitiveCount );
}

const wchar_t*
SK_D3D9_PrimitiveTypeToStr (D3DPRIMITIVETYPE pt)
{
  switch (pt)
  {
    case D3DPT_POINTLIST             : return L"D3DPT_POINTLIST";
    case D3DPT_LINELIST              : return L"D3DPT_LINELIST";
    case D3DPT_LINESTRIP             : return L"D3DPT_LINESTRIP";
    case D3DPT_TRIANGLELIST          : return L"D3DPT_TRIANGLELIST";
    case D3DPT_TRIANGLESTRIP         : return L"D3DPT_TRIANGLESTRIP";
    case D3DPT_TRIANGLEFAN           : return L"D3DPT_TRIANGLEFAN";
  }

  return L"Invalid Primitive";
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitive_Detour (IDirect3DDevice9* This,
                                 D3DPRIMITIVETYPE  Type,
                                 INT               BaseVertexIndex,
                                 UINT              MinVertexIndex,
                                 UINT              NumVertices,
                                 UINT              startIndex,
                                 UINT              primCount)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: DrawIndexedPrimitive came from unknown IDirect3DDevice9! << ");

    return D3D9DrawIndexedPrimitive_Original ( This, Type,
                                                 BaseVertexIndex, MinVertexIndex,
                                                   NumVertices, startIndex,
                                                     primCount );
  }

  tsf::RenderFix::draw_state.draws++;

  if (config.render.remove_blur && 
    /*Type == D3DPT_TRIANGLELIST && NumVertices == 3 && primCount == 1 &&*/
      (ps_checksum == 0x1e8447cc && vs_checksum == 0x2def1491)) {
    IDirect3DTexture9* pTex = nullptr;

    This->GetTexture (0, (IDirect3DBaseTexture9 **)&pTex);

    if (pTex != nullptr)
    {
      IDirect3DSurface9* pSrc = nullptr;

      pTex->GetSurfaceLevel (0, &pSrc);

      if (pSrc != nullptr)
      {
        IDirect3DSurface9* pDst = nullptr;

        This->GetRenderTarget (0, &pDst);

        if (pDst != nullptr)
        {
          if (tsf::RenderFix::tracer.log)
            dll_log.Log ( L"[FrameTrace] ### Removed Blur (Readbuffer: %ph, Drawbuffer: %ph) ###",
                            pTex, pDst );

           IDirect3DTexture9* pProxyTex = nullptr;

           // Since StretchRect is most expensive when used with MSAA,
           //   as a last resort let's use the MSAA texture map to see
           //     if we can bypass the blur without any actual work.
           extern IDirect3DTexture9*
           GetMSAABackingTexture (IDirect3DSurface9* pSurf);

           pProxyTex = GetMSAABackingTexture (pDst);

           // No MSAA backing texture, let's try the driver.
           if (pProxyTex == nullptr) {
             pDst->GetContainer (__uuidof (IDirect3DTexture9), (void **)&pProxyTex);

             if (pProxyTex != nullptr)
               pProxyTex->Release ();
           }

           if (pProxyTex != nullptr)
           {
             tsf::RenderFix::draw_state.blur_proxy.first  = pProxyTex;
             tsf::RenderFix::draw_state.blur_proxy.second = pTex;

             // We cannot do this indefinitely, we need at least one copy per-frame.
             if (blur_bypass == 1) {
               D3DVIEWPORT9 vp;
               This->GetViewport (&vp);

               RECT rectOut = { vp.X,            vp.Y,
                                vp.X + vp.Width, vp.Y + vp.Height };

               This->StretchRect (pSrc, nullptr, pDst, &rectOut, D3DTEXF_NONE);
             }

             blur_bypass++;
           }

           else {
             if (tsf::RenderFix::tracer.log)
               dll_log.Log (L"[FrameTrace] >>  Blur Proxy Impossible; Resorting to Blit  <<");

               D3DVIEWPORT9 vp;
               This->GetViewport (&vp);

               RECT rectOut = { vp.X,            vp.Y,
                                vp.X + vp.Width, vp.Y + vp.Height };

               This->StretchRect (pSrc, nullptr, pDst, &rectOut, D3DTEXF_NONE);
           }

          pDst->Release ();
        }

        pSrc->Release ();
      }

      pTex->Release ();
    }

    return S_OK;
  }

  // These are outlines that would not normally be detected because the polygon
  //   winding direction is not reversed...
  bool weapon_outline = false;

// This whole thing is really convoluted, in a few versions please remove it
//   ...
#if 0
  // Outlines end on the first non-triangle strip primitive drawn
  if (Type == D3DPT_TRIANGLESTRIP && config.render.outline_technique == OUTLINE_KALDAIEN) {
    if (tsf::RenderFix::draw_state.outlines)
      doing_outlines = true;

    if ( doing_outlines && 
         config.render.outline_technique == OUTLINE_KALDAIEN ) {
      if ( tsf::RenderFix::draw_state.last_vs_vec4 == 12           &&
       (! tsf::RenderFix::draw_state.alpha_test)                   &&
          tsf::RenderFix::draw_state.dstalpha      == D3DBLEND_ONE &&
          tsf::RenderFix::draw_state.srcalpha      == D3DBLEND_ZERO ) {
         weapon_outline = true;
      }
    }

    if ((! weapon_outline) && (! tsf::RenderFix::draw_state.outlines))
      doing_outlines = false;

    if (! tsf::RenderFix::draw_state.zwrite)
      weapon_outline = false;
  }
  else {
    doing_outlines = false;
  }
#endif

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] DrawIndexedPrimitive   (Type: %s)\n"
                  L"                         [FrameTrace]                 -"
                  L"    BaseIdx:     %5li, MinVtxIdx:  %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    NumVertices: %5lu, startIndex: %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    primCount:   %5lu",
                    SK_D3D9_PrimitiveTypeToStr (Type),
                      BaseVertexIndex, MinVertexIndex,
                        NumVertices, startIndex, primCount );

    if (tsf::RenderFix::draw_state.outlines)
      dll_log.Log ( L"[FrameTrace]  ** Outline        (vs=%X, ps=%X) **",
                      vs_checksum, ps_checksum );

    else if (weapon_outline)
      dll_log.Log ( L"[FrameTrace]  ** Weapon Outline (vs=%X, ps=%X) **",
                      vs_checksum, ps_checksum );
    else if (Type == D3DPT_TRIANGLESTRIP && primCount > 1)
      dll_log.Log ( L"[FrameTrace]  ** Possible Outline (vs=%X, ps=%X) **",
                      vs_checksum, ps_checksum );
  }

  if ( config.render.outline_technique == OUTLINE_KALDAIEN ) {
    bool outline_detect = false;

    if ( tsf::RenderFix::draw_state.dstblend == D3DBLEND_INVSRCALPHA &&
         tsf::RenderFix::draw_state.srcalpha == D3DBLEND_ZERO &&
         tsf::RenderFix::draw_state.dstalpha == D3DBLEND_ONE ) {
      // Regular Outlines
      if (vs_checksum == 0x39736C4E && ps_checksum == 0x49AC1497) {
        outline_detect = true;
      }

      // Weapons (and Rheairds / world geometry)
      if (vs_checksum == 0xF03FCE8D && ps_checksum != 0x99B99471) {
                                                                                                 // ^^^ Spectres
        // Avoid making weapons opaque by mistake ;)
        if (! (tsf::RenderFix::draw_state.alpha_test && ps_checksum == 0xBDCA3F2C)) {
          // Filter foliage and special effects
          if (ps_checksum != 0xA5BBB3F || (primCount > 13000 && tsf::RenderFix::draw_state.alpha_ref == 0))
          // ^^^^ At this point it would be simpler just to hard-code the vertex counts for Rheairds!
            outline_detect = true;
        }
      }
    }

    if (outline_detect && tsf::RenderFix::draw_state.zwrite) {
      weapon_outline                      = true;
      if (tsf::RenderFix::tracer.log)
        dll_log.Log (L"[FrameTrace] *** Kaldaien Outline ***");
    } else {
      weapon_outline                      = false;
      tsf::RenderFix::draw_state.outlines = false;
    }
  }

  static D3DVIEWPORT9 vp;
  static D3DVIEWPORT9 vp_orig;

  //
  // Detect Enemies with Invalid Outlines
  //
  bool  disable_outlines = false;
  float neutral_depth    = 0.0f;

  if ( weapon_outline || tsf::RenderFix::draw_state.outlines ) {
    // Slimes (Bestiary #158)
    if (NumVertices == 348 && primCount == 1811)
      disable_outlines = true;

    // Man-Eater (Bestiary #?)
    if (NumVertices == 348 && primCount == 1761)
      disable_outlines = true;

    // Sword on Fire Warrior (Bestiary #72)
    if (NumVertices == 198 && primCount == 465) {
      if (tsf::RenderFix::tracer.log) {
        dll_log.Log (L"[FrameTrace] *** Warrior Handle ***");
      }
      disable_outlines = true;
    }

    // Blob Monsters on the world map
    if ((NumVertices == 482 && primCount == 482) ||
        (NumVertices == 320 && primCount == 763) ||
        (NumVertices == 204 && primCount == 482)) {
      disable_outlines = true;
    }

    // Feathers on Cockatrice (Bestiary #123) -- Find a better solution, this kills the entire
    //                                             outline for a few minor issues...
    ////if (NumVertices == 1273 && primCount == 3734)
      ////disable_outlines = true;

    // If ONLY weapon_outline is true, this is an outline that Durante's method would have
    //   missed...
    if (! disable_outlines) {
      if (weapon_outline) {
        D3D9SetRenderState_Original (This, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        D3D9SetRenderState_Original (This, D3DRS_DESTBLEND, D3DBLEND_ZERO);
      }
    }

    // Some outlines are hopelessly broken and we need to bypass them.
    else {
      D3D9SetRenderState_Original (This, D3DRS_SRCBLEND,  tsf::RenderFix::draw_state.srcblend);
      D3D9SetRenderState_Original (This, D3DRS_DESTBLEND, tsf::RenderFix::draw_state.dstblend);
    }

    if (config.render.outline_technique == OUTLINE_KALDAIEN) {
      //D3D9SetRenderState_Original (This, D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&depth_offset2);
      //D3D9SetRenderState_Original (This, D3DRS_DEPTHBIAS,           *(DWORD *)&depth_offset1);

      This->GetViewport (&vp_orig);

      vp = vp_orig;

      vp.MaxZ += depth_offset;
      vp.MinZ += depth_offset;
    }
  }

  HRESULT hr = D3D9DrawIndexedPrimitive_Original ( This, Type,
                                                     BaseVertexIndex, MinVertexIndex,
                                                       NumVertices, startIndex,
                                                         primCount );

  if ( (weapon_outline || tsf::RenderFix::draw_state.outlines) &&
       config.render.outline_technique == OUTLINE_KALDAIEN ) {
    //D3D9SetRenderState_Original (This, D3DRS_DEPTHBIAS,           *(DWORD *)&neutral_depth);
    //D3D9SetRenderState_Original (This, D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD *)&neutral_depth);

    D3D9SetViewport_Original (This, &vp_orig);
  }

  //
  // Restore the outlines after we finish this drawcall
  //
  if (disable_outlines) {
    if (config.render.outline_technique > 0) {
      if (config.render.outline_technique == OUTLINE_KALDAIEN)
        D3D9SetRenderState_Original (This, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
      else
        D3D9SetRenderState_Original (This, D3DRS_SRCBLEND,  D3DBLEND_ONE);

      D3D9SetRenderState_Original (This, D3DRS_DESTBLEND, D3DBLEND_ZERO);
    }
  }

  // Restore normal blending
  if (weapon_outline) {
    D3D9SetRenderState_Original (This, D3DRS_SRCBLEND,  tsf::RenderFix::draw_state.srcblend);
    D3D9SetRenderState_Original (This, D3DRS_DESTBLEND, tsf::RenderFix::draw_state.dstblend);
  }

  return hr;
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9DrawPrimitiveUP_Detour ( IDirect3DDevice9* This,
                             D3DPRIMITIVETYPE  PrimitiveType,
                             UINT              PrimitiveCount,
                             const void       *pVertexStreamZeroData,
                             UINT              VertexStreamZeroStride )
{
  if (tsf::RenderFix::tracer.log && This == tsf::RenderFix::pDevice) {
    dll_log.Log ( L"[FrameTrace] DrawPrimitiveUP   (Type: %s) - PrimitiveCount: %lu"/*
                  L"                         [FrameTrace]                 -"
                  L"    BaseIdx:     %5li, MinVtxIdx:  %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    NumVertices: %5lu, startIndex: %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    primCount:   %5lu"*/,
                    SK_D3D9_PrimitiveTypeToStr (PrimitiveType),
                      PrimitiveCount/*,
                      BaseVertexIndex, MinVertexIndex,
                        NumVertices, startIndex, primCount*/ );
  }

  tsf::RenderFix::draw_state.draws++;

  return
    D3D9DrawPrimitiveUP_Original ( This,
                                     PrimitiveType,
                                       PrimitiveCount,
                                         pVertexStreamZeroData,
                                           VertexStreamZeroStride );
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9DrawIndexedPrimitiveUP_Detour ( IDirect3DDevice9* This,
                                    D3DPRIMITIVETYPE  PrimitiveType,
                                    UINT              MinVertexIndex,
                                    UINT              NumVertices,
                                    UINT              PrimitiveCount,
                                    const void       *pIndexData,
                                    D3DFORMAT         IndexDataFormat,
                                    const void       *pVertexStreamZeroData,
                                    UINT              VertexStreamZeroStride )
{
  if (tsf::RenderFix::tracer.log && This == tsf::RenderFix::pDevice) {
    dll_log.Log ( L"[FrameTrace] DrawIndexedPrimitiveUP   (Type: %s) - NumVertices: %lu, PrimitiveCount: %lu"/*
                  L"                         [FrameTrace]                 -"
                  L"    BaseIdx:     %5li, MinVtxIdx:  %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    NumVertices: %5lu, startIndex: %5lu,\n"
                  L"                         [FrameTrace]                 -"
                  L"    primCount:   %5lu"*/,
                    SK_D3D9_PrimitiveTypeToStr (PrimitiveType),
                      NumVertices, PrimitiveCount/*,
                      BaseVertexIndex, MinVertexIndex,
                        NumVertices, startIndex, primCount*/ );
  }

  tsf::RenderFix::draw_state.draws++;

  return
    D3D9DrawIndexedPrimitiveUP_Original (
      This,
        PrimitiveType,
          MinVertexIndex,
            NumVertices,
              PrimitiveCount,
                pIndexData,
                  IndexDataFormat,
                    pVertexStreamZeroData,
                      VertexStreamZeroStride );
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderState_Detour (IDirect3DDevice9*  This,
                           D3DRENDERSTATETYPE State,
                           DWORD              Value)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: SetRenderState came from unknown IDirect3DDevice9! << ");

    return D3D9SetRenderState_Original (This, State, Value);
  }

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetRenderState  - State: %24s, Value: %lu",
                    SK_D3D9_RenderStateToStr (State),
                      Value );
  }

  switch (State)
  {
    case D3DRS_CULLMODE:
      if (Value == D3DCULL_CW)
        tsf::RenderFix::draw_state.outlines = true;
      else
        tsf::RenderFix::draw_state.outlines = false;
      break;
    case D3DRS_ALPHATESTENABLE:
      tsf::RenderFix::draw_state.alpha_test = Value;
      break;
    case D3DRS_ALPHAREF:
      tsf::RenderFix::draw_state.alpha_ref = Value;
      break;
    case D3DRS_ZWRITEENABLE:
      tsf::RenderFix::draw_state.zwrite = Value;
      break;
    case D3DRS_DESTBLENDALPHA:
      tsf::RenderFix::draw_state.dstalpha = Value;
      break;
    case D3DRS_SRCBLENDALPHA:
      tsf::RenderFix::draw_state.srcalpha = Value;
      break;
    case D3DRS_DESTBLEND:
      tsf::RenderFix::draw_state.dstblend = Value;
      break;
    case D3DRS_SRCBLEND:
      tsf::RenderFix::draw_state.srcblend = Value;
      break;
    case D3DRS_MULTISAMPLEANTIALIAS:
      if ( config.render.msaa_samples > 0      &&
           tsf::RenderFix::draw_state.has_msaa &&
           tsf::RenderFix::draw_state.use_msaa )
        return D3D9SetRenderState_Original (This, State, 1);
      else
        return D3D9SetRenderState_Original (This, State, 0);
      break;
  }

  if (config.render.outline_technique > 0) {
    if (State == D3DRS_SRCBLEND || State == D3DRS_DESTBLEND) {
      if (tsf::RenderFix::draw_state.outlines) {
        switch (State)
        {
          case D3DRS_SRCBLEND:
            if (config.render.outline_technique == OUTLINE_KALDAIEN) {
              return D3D9SetRenderState_Original (This, State, D3DBLEND_SRCALPHA);
            }
            else
              return D3D9SetRenderState_Original (This, State, D3DBLEND_ONE);
          case D3DRS_DESTBLEND:
            return D3D9SetRenderState_Original (This, State, D3DBLEND_ZERO);
        }
      }
    }
  }

  return D3D9SetRenderState_Original (This, State, Value);
}

const wchar_t*
SK_D3D9_SamplerStateTypeToStr (D3DSAMPLERSTATETYPE sst)
{
  switch (sst)
  {
    case D3DSAMP_ADDRESSU       : return L"D3DSAMP_ADDRESSU";
    case D3DSAMP_ADDRESSV       : return L"D3DSAMP_ADDRESSV";
    case D3DSAMP_ADDRESSW       : return L"D3DSAMP_ADDRESSW";
    case D3DSAMP_BORDERCOLOR    : return L"D3DSAMP_BORDERCOLOR";
    case D3DSAMP_MAGFILTER      : return L"D3DSAMP_MAGFILTER";
    case D3DSAMP_MINFILTER      : return L"D3DSAMP_MINFILTER";
    case D3DSAMP_MIPFILTER      : return L"D3DSAMP_MIPFILTER";
    case D3DSAMP_MIPMAPLODBIAS  : return L"D3DSAMP_MIPMAPLODBIAS";
    case D3DSAMP_MAXMIPLEVEL    : return L"D3DSAMP_MAXMIPLEVEL";
    case D3DSAMP_MAXANISOTROPY  : return L"D3DSAMP_MAXANISOTROPY";
    case D3DSAMP_SRGBTEXTURE    : return L"D3DSAMP_SRGBTEXTURE";

    case D3DSAMP_ELEMENTINDEX   : return L"D3DSAMP_ELEMENTINDEX";

    case D3DSAMP_DMAPOFFSET     : return L"D3DSAMP_DMAPOFFSET";
  }

  static wchar_t wszUnknown [32];
  wsprintf (wszUnknown, L"UNKNOWN: %lu", sst);
  return wszUnknown;
}

//
// TODO: Move Me to textures.cpp
//

IDirect3DTexture9* pFontTex = nullptr;

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetSamplerState_Detour (IDirect3DDevice9*   This,
                            DWORD               Sampler,
                            D3DSAMPLERSTATETYPE Type,
                            DWORD               Value)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    dll_log.Log (L"[Render Fix] >> WARNING: SetSamplerState came from unknown IDirect3DDevice9! << ");

    return D3D9SetSamplerState_Original (This, Sampler, Type, Value);
  }

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetSamplerState - %02lu Type: %22s, Value: %lu",
                    Sampler,
                      SK_D3D9_SamplerStateTypeToStr (Type),
                        Value );
  }

  //dll_log.Log ( L" [!] IDirect3DDevice9::SetSamplerState (%lu, %lu, %lu)",
                  //Sampler, Type, Value );
  if (Type == D3DSAMP_MIPFILTER ||
      Type == D3DSAMP_MINFILTER ||
      Type == D3DSAMP_MAGFILTER ||
      Type == D3DSAMP_MIPMAPLODBIAS) {
    //dll_log.Log (L" [!] IDirect3DDevice9::SetSamplerState (...)");
    if (Type < 8) {
      bool aniso_override = false;

      //if (Value != D3DTEXF_ANISOTROPIC)
        //D3D9SetSamplerState_Original (This, Sampler, D3DSAMP_MAXANISOTROPY, aniso);
      //dll_log.Log (L" %s Filter: %x", Type == D3DSAMP_MIPFILTER ? L"Mip" : Type == D3DSAMP_MINFILTER ? L"Min" : L"Mag", Value);

      if (Type == D3DSAMP_MIPFILTER) {
        //extern bool __remap_textures;

        //if (__remap_textures)
          Value = D3DTEXF_LINEAR;
        //else
          //Value = D3DTEXF_NONE;

        //if (Value != D3DTEXF_NONE)
          //aniso_override = false;
      }

      if (Type == D3DSAMP_MAGFILTER ||
          Type == D3DSAMP_MINFILTER) {
        if (Value == D3DTEXF_LINEAR) {
          aniso_override = true;
        }
      }

      if (aniso_override && config.textures.max_anisotropy != 0) {
        Value = D3DTEXF_ANISOTROPIC;
        D3D9SetSamplerState_Original (This, Sampler, D3DSAMP_MAXANISOTROPY, tsf::RenderFix::draw_state.max_aniso);
      }
    }
  }

  if (Type == D3DSAMP_MAXANISOTROPY) {
    if (tsf::RenderFix::tracer.log)
      dll_log.Log (L"[Render Fix] Max Anisotropy Set (it's a miracle!): %d", Value);
    Value = tsf::RenderFix::draw_state.max_aniso;
  }

  return D3D9SetSamplerState_Original (This, Sampler, Type, Value);
}

void
tsf::RenderFix::Init (void)
{
  d3dx9_43_dll = LoadLibrary (L"D3DX9_43.DLL");

  void SK_SetupD3D9Hooks (void);
  SK_SetupD3D9Hooks ();

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_BeginBufferSwap",
                         D3D9EndFrame_Pre,
               (LPVOID*)&BMF_BeginBufferSwap );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9Reset_Override",
                         D3D9Reset_Detour,
               (LPVOID*)&D3D9Reset_Original );


  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_EndBufferSwap",
                         D3D9EndFrame_Post,
               (LPVOID*)&BMF_EndBufferSwap );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "SK_SetPresentParamsD3D9",
                         BMF_SetPresentParamsD3D9_Detour,
              (LPVOID *)&BMF_SetPresentParamsD3D9_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetSamplerState_Override",
                         D3D9SetSamplerState_Detour,
                (LPVOID*)&D3D9SetSamplerState_Original );

  CommandProcessor*     comm_proc    = CommandProcessor::getInstance ();
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor        ();

  pCommandProc->AddVariable ("Render.DuranteScissor",   new eTB_VarStub <bool> (&config.render.durante_scissor));
  pCommandProc->AddVariable ("Render.OutlineTechnique", new eTB_VarStub <int>  (&config.render.outline_technique));

  // Don't directly modify this state, switching mid-frame would be disasterous...
  pCommandProc->AddVariable ("Render.MSAA",             new eTB_VarStub <bool> (&use_msaa));//&draw_state.use_msaa));

  pCommandProc->AddVariable ("Trace.NumFrames", new eTB_VarStub <int>  (&tracer.count));
  pCommandProc->AddVariable ("Trace.Enable",    new eTB_VarStub <bool> (&tracer.log));

  pCommandProc->AddVariable ("Render.OutlineOffsetC", new eTB_VarStub <float> (&depth_offset1));
  pCommandProc->AddVariable ("Render.OutlineOffsetS", new eTB_VarStub <float> (&depth_offset2));

  draw_state.max_aniso = config.textures.max_anisotropy;
  pCommandProc->AddVariable ("Render.MaxAniso",   new eTB_VarStub <int>  (&draw_state.max_aniso));
  pCommandProc->AddVariable ("Render.RemoveBlur", new eTB_VarStub <bool> (&config.render.remove_blur));

  pCommandProc->AddVariable ("Render.ConservativeMSAA", new eTB_VarStub <bool> (&config.render.conservative_msaa));
  pCommandProc->AddVariable ("Render.EarlyResolve",     new eTB_VarStub <bool> (&early_resolve));

  pCommandProc->AddVariable ("Textures.MaxCacheSize", new eTB_VarStub <int> (&config.textures.max_cache_in_mib));
 
  tex_mgr.Init ();
}

void
tsf::RenderFix::Shutdown (void)
{
  tex_mgr.Shutdown ();

  FreeLibrary (d3dx9_43_dll);
}

float ar;

tsf::RenderFix::CommandProcessor::CommandProcessor (void)
{
  eTB_CommandProcessor* pCommandProc =
    SK_GetCommandProcessor ();

  pCommandProc->AddVariable ("Render.AllowBG",   new eTB_VarStub <bool>  (&config.render.allow_background));
}

bool
tsf::RenderFix::CommandProcessor::OnVarChange (eTB_Variable* var, void* val)
{
  return true;
}

bool draw_hud = true;

typedef int (__stdcall *interpolate_pfn)(int unk0, int unk1);
interpolate_pfn interpolate_Original;

int
__stdcall
interpolate_Detour (int unk0, int unk1)
{
#if 0
  void* This;

  __asm { pushad
          mov This, eax
          popad
        }
#endif

  return interpolate_Original (unk0, unk1);

#if 0
  __asm { pushad }

  dll_log.Log (L" Interpolate: Offset=%d [%f], This: %ph, unk1: %X  - Ret => %d",
   *(DWORD *)unk0, *(float *)((uint8_t *)This+8-0xC), This, unk1, ret);

  __asm { popad }

  return ret * 2;
#endif
}

typedef int (__cdecl *sub_611F10_pfn)(void);
sub_611F10_pfn sub_611F10_Original = nullptr;

int
__cdecl
sub_611F10_Detour (void)
{
  static int last_ret = 0;

  if (draw_hud)
    last_ret = sub_611F10_Original ();

  return last_ret;
}

void
SK_SetupD3D9Hooks (void)
{
  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetViewport_Override",
                         D3D9SetViewport_Detour,
               (LPVOID*)&D3D9SetViewport_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetScissorRect_Override",
                         D3D9SetScissorRect_Detour,
               (LPVOID*)&D3D9SetScissorRect_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetVertexShaderConstantF_Override",
                         D3D9SetVertexShaderConstantF_Detour,
               (LPVOID*)&D3D9SetVertexShaderConstantF_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9DrawPrimitive_Override",
                         D3D9DrawPrimitive_Detour,
               (LPVOID*)&D3D9DrawPrimitive_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9DrawIndexedPrimitive_Override",
                         D3D9DrawIndexedPrimitive_Detour,
               (LPVOID*)&D3D9DrawIndexedPrimitive_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9DrawPrimitiveUP_Override",
                         D3D9DrawPrimitiveUP_Detour,
               (LPVOID*)&D3D9DrawPrimitiveUP_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9DrawIndexedPrimitiveUP_Override",
                         D3D9DrawIndexedPrimitiveUP_Detour,
               (LPVOID*)&D3D9DrawIndexedPrimitiveUP_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetRenderState_Override",
                         D3D9SetRenderState_Detour,
               (LPVOID*)&D3D9SetRenderState_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9BeginScene_Override",
                         D3D9BeginScene_Detour,
               (LPVOID*)&D3D9BeginScene_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9EndScene_Override",
                         D3D9EndScene_Detour,
               (LPVOID*)&D3D9EndScene_Original );


  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetVertexShader_Override",
                         D3D9SetVertexShader_Detour,
               (LPVOID*)&D3D9SetVertexShader_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetPixelShader_Override",
                         D3D9SetPixelShader_Detour,
               (LPVOID*)&D3D9SetPixelShader_Original );

#if 0
  TSFix_CreateFuncHook ( L"Namco_DrawHUD",
                 (void *)0x611F10,
                         sub_611F10_Detour,
               (LPVOID*)&sub_611F10_Original );
  TSFix_EnableHook ((void *)0x611F10);

  TSFix_CreateFuncHook ( L"Namco_Interpolate",
                 (void *)0x628E70,
                         interpolate_Detour,
               (LPVOID*)&interpolate_Original );
  TSFix_EnableHook ((void *)0x628E70);
#endif

  eTB_CommandProcessor* pCommandProc =
    SK_GetCommandProcessor ();

  pCommandProc->AddVariable ("HUD.Show", new eTB_VarStub <bool> (&draw_hud));

}


tsf::RenderFix::CommandProcessor* 
           tsf::RenderFix::CommandProcessor::pCommProc;

IDirect3DDevice9*
         tsf::RenderFix::pDevice          = nullptr;
HWND     tsf::RenderFix::hWndDevice       = NULL;

bool     tsf::RenderFix::fullscreen       = false;
uint32_t tsf::RenderFix::width            = 0UL;
uint32_t tsf::RenderFix::height           = 0UL;
uint32_t tsf::RenderFix::dwRenderThreadID = 0UL;

HMODULE  tsf::RenderFix::d3dx9_43_dll     = 0;
HMODULE  tsf::RenderFix::user32_dll       = 0;

#if 0
  if (config.textures.log) {
    tex_log.Log ( L"Texture:   (%lu x %lu) * <LODs: %lu>",
                    Width, Height, Levels );
    tex_log.Log ( L"             Usage: %-20s - Format: %-20s",
                    SK_D3D9_UsageToStr    (Usage).c_str (),
                      SK_D3D9_FormatToStr (Format) );
    tex_log.Log ( L"               Pool: %s",
                    SK_D3D9_PoolToStr (Pool) );
  }
#endif

void
TSF_Zoom (double incr)
{
  double* pZoom = (double *)0x87dbc0;

  DWORD dwOld;
  VirtualProtect ((void *)pZoom, 8, PAGE_EXECUTE_READWRITE, &dwOld);
  if (*pZoom + incr > 0.0 && *pZoom + incr < 2.0)
    *pZoom += incr;
  VirtualProtect ((void *)pZoom, 8, dwOld, &dwOld);
}

void
TSF_ZoomEx (double val)
{
  double* pZoom = (double *)0x87dbc0;

  DWORD dwOld;
  VirtualProtect ((void *)pZoom, 8, PAGE_EXECUTE_READWRITE, &dwOld);
  *pZoom = val;
  VirtualProtect ((void *)pZoom, 8, dwOld, &dwOld);
}