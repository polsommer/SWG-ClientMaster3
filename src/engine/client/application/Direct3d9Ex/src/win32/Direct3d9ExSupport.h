// ======================================================================
//
// Direct3d9ExSupport.h
// Helper declarations to enable Direct3D 9Ex support when building
// against legacy DirectX 9 headers.  In addition to the interface shims
// the header now exposes a small utility namespace that dynamically
// resolves the 9Ex entry points at runtime.
//
// ======================================================================

#ifndef INCLUDED_Direct3d9ExSupport_H
#define INCLUDED_Direct3d9ExSupport_H

#ifndef DIRECT3D_VERSION
#include <d3d9.h>
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifndef D3DERR_DEVICEREMOVED
#define D3DERR_DEVICEREMOVED MAKE_D3DHRESULT(2160)
#endif

#ifndef D3DERR_DEVICEHUNG
#define D3DERR_DEVICEHUNG MAKE_D3DHRESULT(2164)
#endif

#ifndef S_PRESENT_OCCLUDED
#define S_PRESENT_OCCLUDED MAKE_D3DSTATUS(2162)
#endif

#ifndef D3DSCANLINEORDERING_DEFINED
#define D3DSCANLINEORDERING_DEFINED
typedef enum _D3DSCANLINEORDERING
{
        D3DSCANLINEORDERING_UNKNOWN      = 0,
        D3DSCANLINEORDERING_PROGRESSIVE  = 1,
        D3DSCANLINEORDERING_INTERLACED   = 2,
        D3DSCANLINEORDERING_FORCE_DWORD  = 0x7fffffff
} D3DSCANLINEORDERING;
#endif

#ifndef D3DCOMPOSERECTSOP_DEFINED
#define D3DCOMPOSERECTSOP_DEFINED
typedef enum _D3DCOMPOSERECTSOP
{
        D3DCOMPOSERECTS_COPY             = 1,
        D3DCOMPOSERECTS_OR               = 2,
        D3DCOMPOSERECTS_AND              = 3,
        D3DCOMPOSERECTS_NEG              = 4,
        D3DCOMPOSERECTS_FORCE_DWORD      = 0x7fffffff
} D3DCOMPOSERECTSOP;
#endif

#ifndef D3DDISPLAYROTATION_DEFINED
#define D3DDISPLAYROTATION_DEFINED
typedef enum _D3DDISPLAYROTATION
{
        D3DDISPLAYROTATION_IDENTITY      = 1,
        D3DDISPLAYROTATION_90            = 2,
        D3DDISPLAYROTATION_180           = 3,
        D3DDISPLAYROTATION_270           = 4,
        D3DDISPLAYROTATION_FORCE_DWORD   = 0x7fffffff
} D3DDISPLAYROTATION;
#endif

#ifndef D3DDISPLAYMODEEX_DEFINED
#define D3DDISPLAYMODEEX_DEFINED
typedef struct _D3DDISPLAYMODEEX
{
        UINT                    Size;
        UINT                    Width;
        UINT                    Height;
        UINT                    RefreshRate;
        D3DFORMAT               Format;
        D3DSCANLINEORDERING     ScanLineOrdering;
} D3DDISPLAYMODEEX;
#endif

#ifndef D3DDISPLAYMODEFILTER_DEFINED
#define D3DDISPLAYMODEFILTER_DEFINED
typedef struct _D3DDISPLAYMODEFILTER
{
        UINT                    Size;
        D3DFORMAT               Format;
        D3DSCANLINEORDERING     ScanLineOrdering;
} D3DDISPLAYMODEFILTER;
#endif

#ifndef D3DPRESENTSTATS_DEFINED
#define D3DPRESENTSTATS_DEFINED
typedef struct _D3DPRESENTSTATS
{
        UINT PresentCount;
        UINT PresentRefreshCount;
        UINT SyncRefreshCount;
        UINT SyncQPCTime;
        UINT SyncGPUTime;
} D3DPRESENTSTATS;
#endif

#ifndef __IDirect3D9Ex_INTERFACE_DEFINED__
#define __IDirect3D9Ex_INTERFACE_DEFINED__
struct IDirect3DDevice9Ex;

struct IDirect3D9Ex : public IDirect3D9
{
        virtual UINT    STDMETHODCALLTYPE GetAdapterModeCountEx(UINT adapter, const D3DDISPLAYMODEFILTER *filter) = 0;
        virtual HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(UINT adapter, const D3DDISPLAYMODEFILTER *filter, UINT mode, D3DDISPLAYMODEEX *displayMode) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(UINT adapter, D3DDISPLAYMODEEX *displayMode, D3DDISPLAYROTATION *rotation) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateDeviceEx(UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS *presentationParameters, D3DDISPLAYMODEEX *fullscreenDisplayMode, IDirect3DDevice9Ex **returnedDeviceInterface) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT adapter, LUID *luid) = 0;
};
#endif

#ifndef __IDirect3DDevice9Ex_INTERFACE_DEFINED__
#define __IDirect3DDevice9Ex_INTERFACE_DEFINED__
struct IDirect3DDevice9Ex : public IDirect3DDevice9
{
        virtual HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(UINT width, UINT height, float *rowWeights, float *columnWeights) = 0;
        virtual HRESULT STDMETHODCALLTYPE ComposeRects(IDirect3DSurface9 *source, IDirect3DSurface9 *destination, IDirect3DVertexBuffer9 *srcRectDescriptors, UINT rectDescCount, IDirect3DSurface9 *dstRectDescriptors, D3DCOMPOSERECTSOP operation, int xoffset, int yoffset) = 0;
        virtual HRESULT STDMETHODCALLTYPE PresentEx(const RECT *sourceRect, const RECT *destRect, HWND destWindowOverride, const RGNDATA *dirtyRegion, DWORD flags) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *priority) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT priority) = 0;
        virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(UINT adapter) = 0;
        virtual HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DResource9 **resourceArray, UINT resourceCount) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT maxLatency) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *maxLatency) = 0;
        virtual HRESULT STDMETHODCALLTYPE CheckDeviceState(HWND destWindow) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL lockable, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL discard, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE ResetEx(D3DPRESENT_PARAMETERS *presentationParameters, D3DDISPLAYMODEEX *fullscreenDisplayMode) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx(UINT swapChain, D3DDISPLAYMODEEX *mode, D3DDISPLAYROTATION *rotation) = 0;
};
#endif

typedef HRESULT (WINAPI *PFN_Direct3DCreate9Ex)(UINT, IDirect3D9Ex **);

namespace Direct3d9ExSupport
{
        // Returns a handle to d3d9.dll. When the library is not yet loaded
        // the helper optionally loads it and reports the action through the
        // outLoaded flag. Passing loadIfMissing=false performs a non-loading
        // probe.
        HMODULE loadRuntime(bool loadIfMissing, bool *outLoaded);

        // Releases the runtime if it was loaded through loadRuntime(). Callers
        // should pass the value returned through outLoaded so that we do not
        // accidentally unload a module owned by the host application.
        void unloadRuntime(HMODULE module, bool loaded);

        // Retrieves the Direct3DCreate9Ex entry point from the supplied module.
        PFN_Direct3DCreate9Ex getCreate9ExProc(HMODULE module);

        // Invokes Direct3DCreate9Ex through the provided function pointer.
        HRESULT createInterface(PFN_Direct3DCreate9Ex createProc, UINT sdkVersion, IDirect3D9Ex **outInterface);

        // Convenience helper that loads the runtime (if necessary) and creates
        // the IDirect3D9Ex interface in one call.
        HRESULT createInterface(UINT sdkVersion, IDirect3D9Ex **outInterface);

        // Returns true when the host system exposes the Direct3D 9Ex entry
        // point. No additional runtime state is modified.
        bool isRuntimeAvailable();
}

#endif // INCLUDED_Direct3d9ExSupport_H

