#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <fmt/os.h>

// Use glm w/ DirectX Settings
#define GLM_FORCE_SSE42 1
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 1
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

bool _running = true;

glm::ivec2 _windowSize = { 800, 600 };

HWND _window = {};

static LRESULT CALLBACK WndProc(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            _running = false;
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char** argv)
{
    DWORD dwStyle = CS_VREDRAW | CS_HREDRAW;

    HINSTANCE instance = GetModuleHandle(nullptr);

    const char* windowClassName = "HelloWindowClass";

    WNDCLASSEX wndClass = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = dwStyle,
        .lpfnWndProc = &WndProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = instance,
        .hIcon = LoadIcon(nullptr, IDI_APPLICATION),
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszMenuName = nullptr,
        .lpszClassName = windowClassName,
        .hIconSm = nullptr,
    };

    if (!RegisterClassEx(&wndClass))
    {
        throw fmt::windows_error(GetLastError(), "RegisterClassEx() Failed");
    }

    RECT windowRect = {0, 0, _windowSize.x, _windowSize.y};

    // Accounts for the title bar and such
    // We want the window to be the size we gave it, this makes sure it actually is
    AdjustWindowRect(&windowRect, dwStyle, false);

    const char* title = "HelloWindow";

    _window = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        windowClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!_window)
    {
        throw fmt::windows_error(GetLastError(), "Window was not created successfully");
    }

    ShowWindow(_window, SW_SHOWNORMAL);

    // Sends 1 event through the loop, kickstarts it.
    UpdateWindow(_window);

    while (_running)
    {
        MSG msg;
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);

            // This calls WndProc
            DispatchMessage(&msg);
        }
    }

    return 0;
}
