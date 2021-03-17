#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include <thread>
#include <chrono>

using Microsoft::WRL::ComPtr;
using namespace std::chrono;

LRESULT CALLBACK WndProc(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            std::terminate();
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main(int argc, char ** argv)
{
    printf("Hello, duck\n");
    HINSTANCE instance = GetModuleHandle(nullptr);

    // Register the window class
    const char * WINDOW_CLASS_NAME = "SampleWindowClass";

    WNDCLASSEX wndClass = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_VREDRAW | CS_HREDRAW,
        .lpfnWndProc = &WndProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = instance,
        .hIcon = LoadIcon(nullptr, IDI_APPLICATION),
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszMenuName = nullptr,
        .lpszClassName = WINDOW_CLASS_NAME,
        .hIconSm = nullptr,
    };

    if (!RegisterClassEx(&wndClass)) {
        printf("shit\n");
        return 1;
    }

    int width = 800;
    int height = 600;
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;

    HWND window = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WINDOW_CLASS_NAME,
        "Title",
        dwStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
    printf("%d\n", GetLastError());

    if (!window) {
        return 1;
    }

    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);

    HRESULT hResult;

    ComPtr<IDXGIFactory6> dxFactory;
    ComPtr<IDXGIAdapter1> dxAdapter;

    hResult = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxFactory));

    for (unsigned i = 0; ; ++i) {
        hResult = dxFactory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxAdapter)
        );


        DXGI_ADAPTER_DESC1 desc;
        dxAdapter->GetDesc1(&desc);
        printf("oo\n");
    }

    MSG msg;
    while (true)
    {
        printf("hullo\n");
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}