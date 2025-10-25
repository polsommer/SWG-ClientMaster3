#include "FirstTerrainEditor.h"

#include <tchar.h>

#include <cstddef>
#include <vector>

extern int AFXAPI AfxWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);

namespace
{
    int invokeAfxWinMain(HINSTANCE const hInstance, HINSTANCE const hPrevInstance, LPTSTR const lpCmdLine, int const nCmdShow)
    {
        return AfxWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return invokeAfxWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    if (!lpCmdLine)
    {
        return invokeAfxWinMain(hInstance, hPrevInstance, GetCommandLineA(), nCmdShow);
    }

    int const requiredCharacterCount = WideCharToMultiByte(CP_ACP, 0, lpCmdLine, -1, nullptr, 0, nullptr, nullptr);

    if (requiredCharacterCount <= 0)
    {
        return invokeAfxWinMain(hInstance, hPrevInstance, GetCommandLineA(), nCmdShow);
    }

    std::vector<char> ansiCommandLine(static_cast<std::size_t>(requiredCharacterCount));

    WideCharToMultiByte(CP_ACP, 0, lpCmdLine, -1, ansiCommandLine.data(), requiredCharacterCount, nullptr, nullptr);

    return invokeAfxWinMain(hInstance, hPrevInstance, ansiCommandLine.data(), nCmdShow);
}
