
// Core Windows Functionality
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Logging and Exception Handling
#include <fmt/os.h>

// Configure GLM for DirectX's coordinate system
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Used to track when the window should close
bool _running = true;

// Used to track the initial and current window size
glm::ivec2 _windowSize = { 800, 600 };

// A Handle used to refer to the window when calling Windows APIs
HWND _window = nullptr;

// The Window Event Handler callback, used to track changes to the window
static LRESULT CALLBACK WndProc(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        // The window should be closed
        case WM_CLOSE:
            _running = false;
            break;

        // The window has been resized
        case WM_SIZE:
            _windowSize.x = LOWORD(lParam);
            _windowSize.y = HIWORD(lParam);
            break;
    }

    // The Default Window Event Handler callback
    // We call this so that unhandled events can be processed by Windows
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void InitWindow()
{
    // Redraw the window if the width (CS_HREDRAW) or height (CS_VREDRAW) changes
    // https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
    DWORD dwStyleClass = CS_HREDRAW | CS_VREDRAW;

    // A Handle to this process
    HINSTANCE instance = GetModuleHandle(nullptr);

    // Used to identify a class of similar windows
    // Even though we only have one window, we still need to register a class
    const char* windowClassName = "HelloWindowClass";

    // Window class settings
    WNDCLASSEX wndClass = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = dwStyleClass,
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

    RECT windowRect = { 0, 0, _windowSize.x, _windowSize.y };

    // Accounts for the title bar and borders
    // We want the window to be the size we gave it, this makes sure it actually is
    AdjustWindowRect(&windowRect, dwStyleClass, false);

    const char* title = "HelloWindow";

    // The window doesn't look like it's from Windows 95 (WS_EX_CLIENTEDGE)
    // https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
    DWORD dwExStyle = WS_EX_CLIENTEDGE;

    // A Regular Window with a title and border (WS_OVERLAPPEDWINDOW)
    // https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;

    // Create the window with our settings
    _window = CreateWindowEx(
        dwExStyle,
        windowClassName,
        title,
        dwStyle,
        CW_USEDEFAULT, // Starting X Location
        CW_USEDEFAULT, // Starting Y Location
        windowRect.right - windowRect.left, // Starting Width
        windowRect.bottom - windowRect.top, // Starting Height
        nullptr, // No parent window
        nullptr, // No menubar
        instance, // Tie the window to this process
        nullptr
    );

    if (!_window)
    {
        throw fmt::windows_error(GetLastError(), "CreateWindowEx() failed.");
    }

    // Present the window to the user
    ShowWindow(_window, SW_SHOWNORMAL);

    // Sends 1 event through the loop, kickstarts it.
    UpdateWindow(_window);
}

void CleanupWindow()
{
    DestroyWindow(_window);
    _window = nullptr;
}

void Init()
{
    InitWindow();
}

void Cleanup()
{
    CleanupWindow();
}

void Update()
{
    // Update any logic, game logic, or any thing that needs to be updated during runtime.
}

void Render()
{
    // Render new images and present them to the window.
}

int main(int argc, char** argv)
{
    Init();

    MSG msg;
    while (_running)
    {
        // Handle all events, if this isn't done in a timely manner, the window is marked
        // as "unresponsive" and Windows asks if you want to kill it
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // Translates virtual-key messages into WM_CHAR messages
            TranslateMessage(&msg);

            // This calls WndProc
            DispatchMessage(&msg);
        }

        Update();

        Render();
    }

    Cleanup();

    return 0;
}
