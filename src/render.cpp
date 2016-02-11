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

#include <stdint.h>

#include <comdef.h>

#include <d3d9.h>
#include <d3d9types.h>

BeginScene_pfn                    D3D9BeginScene_Original               = nullptr;
SetScissorRect_pfn                D3D9SetScissorRect_Original           = nullptr;
CreateTexture_pfn                 D3D9CreateTexture_Original            = nullptr;
SetViewport_pfn                   D3D9SetViewport_Original              = nullptr;
SetRenderState_pfn                D3D9SetRenderState_Original           = nullptr;
SetVertexShaderConstantF_pfn      D3D9SetVertexShaderConstantF_Original = nullptr;
SetSamplerState_pfn               D3D9SetSamplerState_Original          = nullptr;

BMF_SetPresentParamsD3D9_pfn      BMF_SetPresentParamsD3D9_Original     = nullptr;
BMF_EndBufferSwap_pfn             BMF_EndBufferSwap                     = nullptr;

tsf::RenderFix::tracer_s
  tsf::RenderFix::tracer;

struct tsf_draw_states_s {
  bool         has_aniso      = false; // Has he game even once set anisotropy?!
  bool         has_msaa       = false;
  bool         use_msaa       = true;  // Allow MSAA toggle via console
                                       //  without changing the swapchain.
  D3DVIEWPORT9 vp             = { 0 };
  bool         postprocessing = false;
  bool         fullscreen     = false;
} draw_state;

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9EndFrame_Post (HRESULT hr, IUnknown* device)
{
  // Ignore anything that's not the primary render device.
  if (device != tsf::RenderFix::pDevice)
    return BMF_EndBufferSwap (hr, device);

  tsf::RenderFix::dwRenderThreadID = GetCurrentThreadId ();

  if (tsf::RenderFix::tracer.log && tsf::RenderFix::tracer.count > 0) {
    dll_log.Log (L" --- SwapChain Present ---");
    if (--tsf::RenderFix::tracer.count <= 0)
      tsf::RenderFix::tracer.log = false;
  }

  void TSFix_DrawCommandConsole (void);
  TSFix_DrawCommandConsole ();

  hr = BMF_EndBufferSwap (hr, device);

  return hr;
}

COM_DECLSPEC_NOTHROW
D3DPRESENT_PARAMETERS*
__stdcall
BMF_SetPresentParamsD3D9_Detour (IDirect3DDevice9*      device,
                                 D3DPRESENT_PARAMETERS* pparams)
{
  D3DPRESENT_PARAMETERS present_params;

  //
  // TODO: Figure out what the hell is doing this when RTSS is allowed to use
  //         custom D3D libs. 1x1@0Hz is obviously NOT for rendering!
  //
  if ( pparams->BackBufferWidth            == 1 &&
       pparams->BackBufferHeight           == 1 &&
       pparams->FullScreen_RefreshRateInHz == 0 ) {
    dll_log.Log (L" * Fake D3D9Ex Device Detected... Ignoring!");
    return BMF_SetPresentParamsD3D9_Original (device, pparams);
  }

  if (pparams != nullptr) {
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
          dll_log.Log ( L" >> Render device has %d quality level(s) avaialable "
                        L"for %d-Sample MSAA.",
                          dwQualityLevels, config.render.msaa_samples );

          if (dwQualityLevels > 0) {
            pparams->SwapEffect         = D3DSWAPEFFECT_DISCARD;
            pparams->MultiSampleType    = (D3DMULTISAMPLE_TYPE)
                                            config.render.msaa_samples;
            pparams->MultiSampleQuality = min ( dwQualityLevels-1,
                                                  config.render.msaa_quality );

            dll_log.Log ( L" (*) Selected %dxMSAA Quality Level: %d",
                            pparams->MultiSampleType,
                              pparams->MultiSampleQuality );
            draw_state.has_msaa = true;
          } else {
            dll_log.Log ( L" ### Requested %dxMSAA Quality Level: %d invalid",
                            config.render.msaa_samples,
                              config.render.msaa_quality );
          }
        }
        pD3D9->Release ();
      }
    }

    memcpy (&present_params, pparams, sizeof D3DPRESENT_PARAMETERS);

    if (device != nullptr) {
      dll_log.Log ( L" %% Caught D3D9 Swapchain :: Fullscreen=%s "
                    L" (%lux%lu@%lu Hz) "
                    L" [Device Window: 0x%04X]",
                      pparams->Windowed ? L"False" :
                                          L"True",
                        pparams->BackBufferWidth,
                          pparams->BackBufferHeight,
                            pparams->FullScreen_RefreshRateInHz,
                              pparams->hDeviceWindow );
    }

    // Some games don't set this even though it's supposed to be
    //   required by the D3D9 API...
    if (pparams->hDeviceWindow != nullptr)
      tsf::RenderFix::hWndDevice = pparams->hDeviceWindow;

    tsf::RenderFix::pDevice    = device;
    tsf::RenderFix::fullscreen = (! pparams->Windowed);

    tsf::RenderFix::width  = present_params.BackBufferWidth;
    tsf::RenderFix::height = present_params.BackBufferHeight;

    //
    // Fullscreen mode while allow_background is enabled must be implemented as
    //   a window, or Alt+Tab will not work.
    //
    if (config.render.allow_background && tsf::RenderFix::fullscreen)
      config.render.borderless = true;

    if (config.render.borderless) {
      // The game will draw in windowed mode, but it will think it's fullscreen
      pparams->Windowed  = true;

      if (tsf::RenderFix::fullscreen) {
        DEVMODE devmode = { 0 };
        devmode.dmSize  = sizeof DEVMODE;

        EnumDisplaySettings (nullptr, ENUM_CURRENT_SETTINGS, &devmode);

        if ( devmode.dmPelsHeight       < tsf::RenderFix::height ||
             devmode.dmPelsWidth        < tsf::RenderFix::width  ||
             devmode.dmDisplayFrequency != pparams->FullScreen_RefreshRateInHz ) {
          devmode.dmPelsWidth        = tsf::RenderFix::width;
          devmode.dmPelsHeight       = tsf::RenderFix::height;
          devmode.dmDisplayFrequency = pparams->FullScreen_RefreshRateInHz;

          devmode.dmDisplayFlags |= DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

          ChangeDisplaySettings (&devmode, CDS_FULLSCREEN);
        }

        pparams->FullScreen_RefreshRateInHz = 0;
      } else {
      }
    }
  }

  if (config.render.borderless)
    tsf::WindowManager::border.Disable ();
  else
    tsf::WindowManager::border.Enable  ();

  if (! tsf::RenderFix::fullscreen) {
    //GetWindowRect      (tsf::RenderFix::hWndDevice, &window_rect)
    //AdjustWindowRectEx (&window_rect, style, FALSE, style_ex);

    tsf::window.window_rect.left = 0;
    tsf::window.window_rect.top  = 0;

    tsf::window.window_rect.right  = tsf::RenderFix::width;
    tsf::window.window_rect.bottom = tsf::RenderFix::height;
    SetWindowPos_Original (
      tsf::RenderFix::hWndDevice, HWND_TOP,
        0, 0,
          tsf::RenderFix::width, tsf::RenderFix::height,
            SWP_FRAMECHANGED );
  }

  return BMF_SetPresentParamsD3D9_Original (device, pparams);
}

#include "hook.h"

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9CreateTexture_Detour (IDirect3DDevice9   *This,
                          UINT                Width,
                          UINT                Height,
                          UINT                Levels,
                          DWORD               Usage,
                          D3DFORMAT           Format,
                          D3DPOOL             Pool,
                          IDirect3DTexture9 **ppTexture,
                          HANDLE             *pSharedHandle)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9CreateTexture_Original ( This, Width, Height,
                                          Levels, Usage, Format,
                                            Pool, ppTexture, pSharedHandle );
  }

  // Resize the primary framebuffer
  if (Width == 1280 && Height == 720) {
    if (((Usage & D3DUSAGE_RENDERTARGET) && Format == D3DFMT_A8R8G8B8) ||
                                            Format == D3DFMT_D24S8) {
      Width  = tsf::RenderFix::width;
      Height = tsf::RenderFix::height;
    }
  } else if (Width == 512 && Height == 256 && (Usage & D3DUSAGE_RENDERTARGET)) {
    Width  = tsf::RenderFix::width  * config.render.postproc_ratio;
    Height = tsf::RenderFix::height * config.render.postproc_ratio;
  }

  return D3D9CreateTexture_Original ( This, Width, Height,
                                        Levels, Usage, Format,
                                          Pool, ppTexture, pSharedHandle );
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetViewport_Detour (IDirect3DDevice9* This,
                  CONST D3DVIEWPORT9*     pViewport)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice)
    return D3D9SetViewport_Original (This, pViewport);

  memcpy (&draw_state.vp, pViewport, sizeof D3DVIEWPORT9);

  // ...
  if (pViewport->Width == 1280 && pViewport->Height == 720) {
    draw_state.postprocessing = false;
    draw_state.fullscreen     = true; // TODO: Check viewport against rendertarget

    D3DVIEWPORT9 newView = *pViewport;

    newView.Width  = tsf::RenderFix::width;
    newView.Height = tsf::RenderFix::height;

    return D3D9SetViewport_Original (This, &newView);
  }

  // Effects
  if (pViewport->Width == 512 && pViewport->Height == 256) {
    draw_state.postprocessing = true;
    draw_state.fullscreen     = false;

    D3DVIEWPORT9 newView = *pViewport;

    newView.Width  = tsf::RenderFix::width  * config.render.postproc_ratio;
    newView.Height = tsf::RenderFix::height * config.render.postproc_ratio;

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
    return D3D9SetVertexShaderConstantF_Original ( This,
                                                     StartRegister,
                                                       pConstantData,
                                                         Vector4fCount );
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
    return D3D9BeginScene_Original (This);
  }

  // Turn this on at the beginning of every frame
  if (config.render.msaa_samples > 0 && draw_state.has_msaa && draw_state.use_msaa) {
    D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                    D3DRS_MULTISAMPLEANTIALIAS,
                                      1 );
  } else {
    D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                    D3DRS_MULTISAMPLEANTIALIAS,
                                      0 );
  }

  return D3D9BeginScene_Original (This);
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetScissorRect_Detour (IDirect3DDevice9* This,
                     const RECT*             pRect)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9SetScissorRect_Original (This, pRect);
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
    if (! draw_state.postprocessing) {
      // TODO: Use rendertarget dimensions
      float screen_width  = draw_state.vp.Width;
      float screen_height = draw_state.vp.Height;

      float ndc_left  =
        2.0f * (((float)pRect->left - draw_state.vp.X) /
                 screen_width) - 1.0f;
      float ndc_right =
        2.0f * (((float)pRect->right - draw_state.vp.X) /
                 screen_width) - 1.0f;

      float ndc_top  =
        2.0f * (((float)pRect->top - draw_state.vp.Y) /
                 screen_height) - 1.0f;
      float ndc_bottom =
        2.0f * (((float)pRect->bottom - draw_state.vp.Y) /
                 screen_height) - 1.0f;

      newR.right = (ndc_right * tsf::RenderFix::width + tsf::RenderFix::width) / 2.0f +
                    (draw_state.vp.X / screen_width)  * tsf::RenderFix::width;
      newR.left  = (ndc_left  * tsf::RenderFix::width + tsf::RenderFix::width) / 2.0f +
                    (draw_state.vp.X / screen_width)  * tsf::RenderFix::width;

      newR.top    = (ndc_top    * tsf::RenderFix::height + tsf::RenderFix::height) / 2.0f +
                      (draw_state.vp.Y / screen_height)  * tsf::RenderFix::height;
      newR.bottom = (ndc_bottom * tsf::RenderFix::height + tsf::RenderFix::height) / 2.0f +
                      (draw_state.vp.Y / screen_height)  * tsf::RenderFix::height;
    } else {
      dll_log.Log (L" >> Scissor Rectangle Applied During Post-Processing!");
    }
  }

  return D3D9SetScissorRect_Original (This, &newR);
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
    return D3D9SetRenderState_Original (This, State, Value);
  }

  if (State == D3DRS_MULTISAMPLEANTIALIAS) {
    if (config.render.msaa_samples > 0 && draw_state.has_msaa && draw_state.use_msaa)
      return D3D9SetRenderState_Original (This, State, 1);
    else
      return D3D9SetRenderState_Original (This, State, 0);
  }

  if (config.render.outline_technique > 0) {
    if (State == D3DRS_SRCBLEND || State == D3DRS_DESTBLEND) {
      D3DCULL cullmode;
      This->GetRenderState (D3DRS_CULLMODE, (DWORD *)&cullmode);

      if (cullmode == D3DCULL_CW) {
        switch (State)
        {
          case D3DRS_SRCBLEND:
            if (config.render.outline_technique == OUTLINE_KALDAIEN)
              return D3D9SetRenderState_Original (This, State, D3DBLEND_SRCALPHA);
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

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetSamplerState_Detour (IDirect3DDevice9*   This,
                            DWORD               Sampler,
                            D3DSAMPLERSTATETYPE Type,
                            DWORD               Value)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice)
    return D3D9SetSamplerState_Original (This, Sampler, Type, Value);

  static int aniso = 1;
  //dll_log.Log ( L" [!] IDirect3DDevice9::SetSamplerState (%lu, %lu, %lu)",
                  //Sampler, Type, Value );
  // Pending removal - these are not configurable tweaks and not particularly useful
  if (Type == D3DSAMP_MIPFILTER ||
      Type == D3DSAMP_MINFILTER ||
      Type == D3DSAMP_MAGFILTER ||
      Type == D3DSAMP_MIPMAPLODBIAS) {
    //dll_log.Log (L" [!] IDirect3DDevice9::SetSamplerState (...)");
    if (Type < 8) {
      //if (Value != D3DTEXF_ANISOTROPIC)
        //D3D9SetSamplerState_Original (This, Sampler, D3DSAMP_MAXANISOTROPY, aniso);
      //dll_log.Log (L" %s Filter: %x", Type == D3DSAMP_MIPFILTER ? L"Mip" : Type == D3DSAMP_MINFILTER ? L"Min" : L"Mag", Value);
      if (Type == D3DSAMP_MIPFILTER) {
        if (Value != D3DTEXF_NONE)
          Value = D3DTEXF_ANISOTROPIC;
      }
      if (Type == D3DSAMP_MAGFILTER ||
                  D3DSAMP_MINFILTER)
        if (Value != D3DTEXF_POINT)
          Value = D3DTEXF_ANISOTROPIC;
    }
  }

  if (Type == D3DSAMP_MAXANISOTROPY) {
    if (tsf::RenderFix::tracer.log)
      dll_log.Log (L" Max Anisotropy Set: %d", Value);
    //aniso = Value;
    Value = config.render.max_anisotropy;
  }

  //
  // The game apparently never sets this, so that's a problem...
  //
  D3D9SetSamplerState_Original (This, Sampler, D3DSAMP_MAXANISOTROPY, config.render.max_anisotropy);
  if (config.render.msaa_samples > 0 && draw_state.has_msaa && draw_state.use_msaa) {
    D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                    D3DRS_MULTISAMPLEANTIALIAS,
                                      1 );
  } else {
    D3D9SetRenderState_Original ( tsf::RenderFix::pDevice,
                                    D3DRS_MULTISAMPLEANTIALIAS,
                                      0 );
  }
  //
  // End things the game is supposed to, but never sets.
  //

  return D3D9SetSamplerState_Original (This, Sampler, Type, Value);
}

void
tsf::RenderFix::Init (void)
{
  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9CreateTexture_Override",
                         D3D9CreateTexture_Detour,
               (LPVOID*)&D3D9CreateTexture_Original );

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
                        "D3D9SetRenderState_Override",
                         D3D9SetRenderState_Detour,
               (LPVOID*)&D3D9SetRenderState_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9BeginScene_Override",
                         D3D9BeginScene_Detour,
               (LPVOID*)&D3D9BeginScene_Original );


  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "BMF_EndBufferSwap",
                         D3D9EndFrame_Post,
               (LPVOID*)&BMF_EndBufferSwap );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "BMF_SetPresentParamsD3D9",
                         BMF_SetPresentParamsD3D9_Detour,
              (LPVOID *)&BMF_SetPresentParamsD3D9_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetSamplerState_Override",
                         D3D9SetSamplerState_Detour,
                (LPVOID*)&D3D9SetSamplerState_Original );


  CommandProcessor*     comm_proc    = CommandProcessor::getInstance ();
  eTB_CommandProcessor* pCommandProc = SK_GetCommandProcessor        ();

  pCommandProc->AddVariable ("Stutter.Fix",         new eTB_VarStub <bool>  (&config.stutter.fix));
  pCommandProc->AddVariable ("Stutter.FudgeFactor", new eTB_VarStub <float> (&config.stutter.fudge_factor));

  pCommandProc->AddVariable ("Render.DuranteScissor",   new eTB_VarStub <bool> (&config.render.durante_scissor));
  pCommandProc->AddVariable ("Render.OutlineTechnique", new eTB_VarStub <int>  (&config.render.outline_technique));

  pCommandProc->AddVariable ("Render.MSAA",             new eTB_VarStub <bool> (&draw_state.use_msaa));

  pCommandProc->AddVariable ("Trace.NumFrames", new eTB_VarStub <int>  (&tracer.count));
  pCommandProc->AddVariable ("Trace.Enable",    new eTB_VarStub <bool> (&tracer.log));
}

void
tsf::RenderFix::Shutdown (void)
{
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