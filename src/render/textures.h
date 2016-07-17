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
#ifndef __TSFIX__TEXTURES_H__
#define __TSFIX__TEXTURES_H__

#include "../render.h"

#include <set>
#include <map>

namespace tsf {
namespace RenderFix {
#if 0
  class Sampler {
    int                id;
    IDirect3DTexture9* pTex;
  };
#endif

  extern std::set <UINT> active_samplers;

  class Texture {
  public:
    uint32_t           crc32;
    size_t             size;
    int                refs;
    float              load_time;
    IDirect3DTexture9* d3d9_tex;
  };

  class TextureManager {
  public:
    void Init     (void);
    void Shutdown (void);

    bool                     contains      (IDirect3DTexture9* pTex);
    void                     removeTexture (IDirect3DTexture9* pTex);
    void                     decRef        (IDirect3DTexture9* pTex);

    tsf::RenderFix::Texture* getTexture (uint32_t crc32);
    void                     addTexture (uint32_t crc32, tsf::RenderFix::Texture* pTex, size_t size);

    // Record a cached reference
    void                     refTexture (tsf::RenderFix::Texture* pTex);

    void                     reset (void);
    void                     purge (void); // WIP

    int                      numTextures (void) {
      return textures.size ();
    }
    int                      numCustomTextures (void);

    size_t                   cacheSizeTotal  (void);
    size_t                   cacheSizeBasic  (void);
    size_t                   cacheSizeCustom (void);

    int                      numMSAASurfs (void);

    void                     addCustom (size_t size) {
      custom_count++;
      custom_size += size;
    }

    std::string              osdStats  (void) { return osd_stats; }
    void                     updateOSD (void);

  private:
    std::unordered_map <uint32_t, tsf::RenderFix::Texture*> textures;
    std::unordered_map <IDirect3DTexture9*, uint32_t>       rev_textures;
    float                                                   time_saved;
    int                                                     hits;

    size_t                                                  basic_size;
    size_t                                                  custom_size;
    int                                                     custom_count;

    std::string                                             osd_stats;
  } extern tex_mgr;
}
}

typedef enum D3DXIMAGE_FILEFORMAT { 
  D3DXIFF_BMP          = 0,
  D3DXIFF_JPG          = 1,
  D3DXIFF_TGA          = 2,
  D3DXIFF_PNG          = 3,
  D3DXIFF_DDS          = 4,
  D3DXIFF_PPM          = 5,
  D3DXIFF_DIB          = 6,
  D3DXIFF_HDR          = 7,
  D3DXIFF_PFM          = 8,
  D3DXIFF_FORCE_DWORD  = 0x7fffffff
} D3DXIMAGE_FILEFORMAT, *LPD3DXIMAGE_FILEFORMAT;

#define D3DX_DEFAULT ((UINT) -1)
typedef 
struct D3DXIMAGE_INFO {
  UINT                 Width;
  UINT                 Height;
  UINT                 Depth;
  UINT                 MipLevels;
  D3DFORMAT            Format;
  D3DRESOURCETYPE      ResourceType;
  D3DXIMAGE_FILEFORMAT ImageFileFormat;
} D3DXIMAGE_INFO, *LPD3DXIMAGE_INFO;
typedef HRESULT (STDMETHODCALLTYPE *D3DXCreateTextureFromFileInMemoryEx_pfn)
(
  _In_        LPDIRECT3DDEVICE9  pDevice,
  _In_        LPCVOID            pSrcData,
  _In_        UINT               SrcDataSize,
  _In_        UINT               Width,
  _In_        UINT               Height,
  _In_        UINT               MipLevels,
  _In_        DWORD              Usage,
  _In_        D3DFORMAT          Format,
  _In_        D3DPOOL            Pool,
  _In_        DWORD              Filter,
  _In_        DWORD              MipFilter,
  _In_        D3DCOLOR           ColorKey,
  _Inout_opt_ D3DXIMAGE_INFO     *pSrcInfo,
  _Out_opt_   PALETTEENTRY       *pPalette,
  _Out_       LPDIRECT3DTEXTURE9 *ppTexture
);

typedef HRESULT (STDMETHODCALLTYPE *D3DXSaveTextureToFile_pfn)(
  _In_           LPCWSTR                 pDestFile,
  _In_           D3DXIMAGE_FILEFORMAT    DestFormat,
  _In_           LPDIRECT3DBASETEXTURE9  pSrcTexture,
  _In_opt_ const PALETTEENTRY           *pSrcPalette
);

typedef HRESULT (WINAPI *D3DXSaveSurfaceToFile_pfn)
(
  _In_           LPCWSTR               pDestFile,
  _In_           D3DXIMAGE_FILEFORMAT  DestFormat,
  _In_           LPDIRECT3DSURFACE9    pSrcSurface,
  _In_opt_ const PALETTEENTRY         *pSrcPalette,
  _In_opt_ const RECT                 *pSrcRect
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

typedef HRESULT (STDMETHODCALLTYPE *CreateRenderTarget_pfn)
(
  IDirect3DDevice9     *This,
  UINT                  Width,
  UINT                  Height,
  D3DFORMAT             Format,
  D3DMULTISAMPLE_TYPE   MultiSample,
  DWORD                 MultisampleQuality,
  BOOL                  Lockable,
  IDirect3DSurface9   **ppSurface,
  HANDLE               *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *CreateDepthStencilSurface_pfn)
(
  IDirect3DDevice9     *This,
  UINT                  Width,
  UINT                  Height,
  D3DFORMAT             Format,
  D3DMULTISAMPLE_TYPE   MultiSample,
  DWORD                 MultisampleQuality,
  BOOL                  Discard,
  IDirect3DSurface9   **ppSurface,
  HANDLE               *pSharedHandle
);

typedef HRESULT (STDMETHODCALLTYPE *SetTexture_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ DWORD                  Sampler,
  _In_ IDirect3DBaseTexture9 *pTexture
);

typedef HRESULT (STDMETHODCALLTYPE *SetRenderTarget_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ DWORD                  RenderTargetIndex,
  _In_ IDirect3DSurface9     *pRenderTarget
);

typedef HRESULT (STDMETHODCALLTYPE *SetDepthStencilSurface_pfn)(
  _In_ IDirect3DDevice9      *This,
  _In_ IDirect3DSurface9     *pNewZStencil
);

const GUID IID_SKTextureD3D9 = { 0xace1f81b, 0x5f3f, 0x45f4, 0xbf, 0x9f, 0x1b, 0xaf, 0xdf, 0xba, 0x11, 0x9b };

interface ISKTextureD3D9 : public IDirect3DTexture9
{
public:
     ISKTextureD3D9 (IDirect3DTexture9 **ppTex, SIZE_T size) {
         pTexOverride  = nullptr;
         override_size = 0;
         pTex          = *ppTex;
       *ppTex          =  this;
         tex_size      = size;
         refs          =  pTex->AddRef ();
     };

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) {
      if (IsEqualGUID (riid, IID_SKTextureD3D9)) {
        return S_OK;
      }

      if ( IsEqualGUID (riid, IID_IUnknown)              ||
           IsEqualGUID (riid, IID_IDirect3DTexture9)     ||
           IsEqualGUID (riid, IID_IDirect3DBaseTexture9)    )
      {
        AddRef ();
        *ppvObj = this;
        return S_OK;
      }

      return E_FAIL;
      //return pTex->QueryInterface (riid, ppvObj);
    }
    STDMETHOD_(ULONG,AddRef)(THIS) {
      refs++;

      ULONG ret = pTex->AddRef ();

      //if (pTexOverride != nullptr) {
        //if (pTexOverride->AddRef () != ret) {
          /// Mismatch
        //}
      //}

      if (refs != ret) {
        /// Mismatch
      }

      return ret;
    }
    STDMETHOD_(ULONG,Release)(THIS) {
      //if (pTexOverride != nullptr) {
        //if (pTexOverride->Release () == 0)
          //pTexOverride = nullptr;
      //}

      refs--;

      ULONG ret = pTex->Release ();

      if (ret != refs) {
        /// Mismatch
      }

      if (ret == 0) {
        if (pTexOverride != nullptr) {
          if (pTexOverride->Release () == 0)
            pTexOverride = nullptr;
        }
      }

      return ret;
    }

    /*** IDirect3DBaseTexture9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice) {
      return pTex->GetDevice (ppDevice);
    }
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags) {
      return pTex->SetPrivateData (refguid, pData, SizeOfData, Flags);
    }
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData) {
      return pTex->GetPrivateData (refguid, pData, pSizeOfData);
    }
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid) {
      return pTex->FreePrivateData (refguid);
    }
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew) {
      return pTex->SetPriority (PriorityNew);
    }
    STDMETHOD_(DWORD, GetPriority)(THIS) {
      return pTex->GetPriority ();
    }
    STDMETHOD_(void, PreLoad)(THIS) {
      pTex->PreLoad ();
    }
    STDMETHOD_(D3DRESOURCETYPE, GetType)(THIS) {
      return pTex->GetType ();
    }
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew) {
      return pTex->SetLOD (LODNew);
    }
    STDMETHOD_(DWORD, GetLOD)(THIS) {
      return pTex->GetLOD ();
    }
    STDMETHOD_(DWORD, GetLevelCount)(THIS) {
      return pTex->GetLevelCount ();
    }
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3DTEXTUREFILTERTYPE FilterType) {
      return pTex->SetAutoGenFilterType (FilterType);
    }
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS) {
      return pTex->GetAutoGenFilterType ();
    }
    STDMETHOD_(void, GenerateMipSubLevels)(THIS) {
      pTex->GenerateMipSubLevels ();
    }
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3DSURFACE_DESC *pDesc) {
      return pTex->GetLevelDesc (Level, pDesc);
    }
    STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level,IDirect3DSurface9** ppSurfaceLevel) {
      return pTex->GetSurfaceLevel (Level, ppSurfaceLevel);
    }
    STDMETHOD(LockRect)(THIS_ UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags) {
      return pTex->LockRect (Level, pLockedRect, pRect, Flags);
    }
    STDMETHOD(UnlockRect)(THIS_ UINT Level) {
      return pTex->UnlockRect (Level);
    }
    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect) {
      return pTex->AddDirtyRect (pDirtyRect);
    }

    IDirect3DTexture9* pTex;          // The original texture data
    SIZE_T             tex_size;      //   Original size

    IDirect3DTexture9* pTexOverride;  // The overridden texture data (nullptr if unchanged)
    SIZE_T             override_size; //   Override data size

    int                refs;
    LARGE_INTEGER      last_used;     // The last time this texture was used (for rendering)
                                      //   different from the last time referenced, this is
                                      //     set when SetTexture (...) is called.
};

#endif /* __TSFIX__TEXTURES_H__ */