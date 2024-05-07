
// Core Windows Functionality
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows Runtime Library
// Provides Microsoft::WRL::ComPtr<>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Direct3D 12 header that contains all of the core Direct3D 12 objects
#include <d3d12.h>
// Provides ID3D12Debug1, ID3D12Device2

// Microsoft DirectX Graphics Infrastructure v1.6
// Used to manage the low-level tasks such as enumerating GPU adapters, presenting the rendered
// image to the screen, and handling full-screen transitions, that are not necessarily part of
// the DirectX rendering API. DXGI 1.6 adds functionality in order to detect HDR displays.
#include <dxgi1_6.h>
// Provides IDXGIFactory6, IDXGIAdapter1

// Logging and Exception Handling
#include <fmt/os.h>

// Configure GLM for DirectX's coordinate system
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#pragma region "Previous Lessons"

// Used to track when the window should close
bool _running = true;

// Used to track the initial and current window size
glm::ivec2 _windowSize = { 800, 600 };

// A Handle used to refer to the window when calling Windows APIs
HWND _window = nullptr;

#pragma endregion

// The debug layer allows you to enable debugging features
ComPtr<ID3D12Debug5> _dxDebug;

// The factory allows you to access system-specific features such as adapters and swap chains
ComPtr<IDXGIFactory6> _dxFactory;

// The adapter represents the physical graphics device, and allows us to query available features
ComPtr<IDXGIAdapter1> _dxAdapter;

// The device represents the logical graphics device, and provides the actual DirectX functionality
ComPtr<ID3D12Device2> _dxDevice;

#pragma region "Previous Lessons"

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

#pragma endregion

void InitDebugLayer()
{
    HRESULT hResult;

    // Get the generic Debug interface
    ComPtr<ID3D12Debug> dxDebug;
    hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&dxDebug));
    if (FAILED(hResult)) {
        throw fmt::windows_error(hResult, "D3D12GetDebugInterface() Failed");
    }

    // Attempt to cast the generic Debug interface into a Debug5
    hResult = dxDebug->QueryInterface(IID_PPV_ARGS(&_dxDebug));
    if (FAILED(hResult)) {
        throw fmt::windows_error(hResult, "QueryInterface() Failed");
    }

    // Causes the Device we are going to create to be created with debug layers
    // This allows tools to inspect the state of the graphics device to debug
    _dxDebug->EnableDebugLayer();

    // Enable auto-naming of Object's, useful when debugging
    _dxDebug->SetEnableAutoName(true);
    
    // Enables in-driver debug checking, which will generate warnings/errors for us
    _dxDebug->SetEnableGPUBasedValidation(true);
}

void InitInfoQueueFilter()
{
    HRESULT hResult;

    // Configure the information queue that contains the debugging information
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(_dxDevice.As(&pInfoQueue)))
    {
        // Configure the message severities we want to see
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Blacklist specific message types that we don't want to see
        D3D12_MESSAGE_ID DenyIds[] = {
            // D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.

            // These warnings occur when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        // Create the message queue filter with our settings
        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        // Filter the message queue
        hResult = pInfoQueue->PushStorageFilter(&NewFilter);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "PushStorageFilter() Failed");
        }
    }
}

void InitFactory()
{
    HRESULT hResult;

    // Enable debug information for factory functions
    int dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

    // Create a factory
    hResult = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&_dxFactory));
    if (FAILED(hResult)) {
        throw fmt::windows_error(hResult, "CreateDXGIFactory2() Failed");
    }

    // Ensure the driver supports a variable refresh rate
    int allowTearing = 0;
    hResult = _dxFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    if (FAILED(hResult)) {
        throw fmt::windows_error(hResult, "CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING) Failed");
    }

    // Tie this DirectX instance to our Window
    hResult = _dxFactory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hResult)) {
        throw fmt::windows_error(hResult, "CheckFeatureSupport(DXGI_MWA_NO_ALT_ENTER) Failed");
    }
}

void InitDevice()
{
    HRESULT hResult;

    // Look through the available adapters to find a suitable one
    for (unsigned index = 0; ; ++index) {
        hResult = _dxFactory->EnumAdapterByGpuPreference(
            index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&_dxAdapter)
        );

        if (hResult == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        // Most systems come with multiple adapters, including WARP, and we want to pick the best one
        // https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp
        bool suitable = true;
        
        // Query adapter information
        DXGI_ADAPTER_DESC1 adapterDesc;
        _dxAdapter->GetDesc1(&adapterDesc);

        // Filter out software renderers
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            fmt::print("oof. Software Renderer.\n");
            suitable = false;
        }

        if (suitable) {

            // Attempt to create a DirectX 12.2 capable device
            hResult = D3D12CreateDevice(
                _dxAdapter.Get(),
                D3D_FEATURE_LEVEL_12_2,
                IID_PPV_ARGS(&_dxDevice)
            );

            // This device did not support DirectX 12.2, keep looking
            if (FAILED(hResult)) {
                continue;
            }

            // We have found and configured a suitable device
            break;
        }

        _dxAdapter->Release();
    }

    if (!_dxAdapter) {
        throw fmt::format("Adapter was not created succesfully");
    }

    if (!_dxDevice) {
        throw fmt::format("Device was not created successfully");
    }
}

void Init()
{
    InitWindow();
    InitDebugLayer();
    InitFactory();
    InitDevice();
    InitInfoQueueFilter();
}

void Cleanup()
{
    DestroyWindow(_window);
    _window = nullptr;
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
