#include "Direct3d9ExSupport.h"

namespace Direct3d9ExSupport
{
        namespace
        {
                bool isValidHandle(HMODULE module)
                {
                        return module != NULL && module != reinterpret_cast<HMODULE>(INVALID_HANDLE_VALUE);
                }

                PFN_Direct3DCreate9Ex resolveCreateProc(HMODULE module)
                {
                        return module ? reinterpret_cast<PFN_Direct3DCreate9Ex>(GetProcAddress(module, "Direct3DCreate9Ex")) : NULL;
                }
        }

        HMODULE loadRuntime(bool loadIfMissing, bool *outLoaded)
        {
                if (outLoaded)
                        *outLoaded = false;

                HMODULE module = GetModuleHandle(TEXT("d3d9.dll"));
                if (!isValidHandle(module) && loadIfMissing)
                {
                        module = LoadLibrary(TEXT("d3d9.dll"));
                        if (outLoaded)
                                *outLoaded = isValidHandle(module);
                }

                return isValidHandle(module) ? module : NULL;
        }

        void unloadRuntime(HMODULE module, bool loaded)
        {
                if (loaded && isValidHandle(module))
                        FreeLibrary(module);
        }

        PFN_Direct3DCreate9Ex getCreate9ExProc(HMODULE module)
        {
                return resolveCreateProc(module);
        }

        HRESULT createInterface(PFN_Direct3DCreate9Ex createProc, UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                if (!outInterface)
                        return E_POINTER;

                *outInterface = NULL;
                if (!createProc)
                        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

                return createProc(sdkVersion, outInterface);
        }

        HRESULT createInterface(UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                bool loaded = false;
                HMODULE module = loadRuntime(true, &loaded);
                if (!module)
                        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

                PFN_Direct3DCreate9Ex const proc = resolveCreateProc(module);
                HRESULT const result = createInterface(proc, sdkVersion, outInterface);

                if (FAILED(result) && loaded)
                        unloadRuntime(module, true);

                return result;
        }

        bool isRuntimeAvailable()
        {
                HMODULE module = loadRuntime(false, NULL);
                if (!module)
                        return false;

                return resolveCreateProc(module) != NULL;
        }
}
