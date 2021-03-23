#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Direct3D 12 header that contains all of the Direct3D 12 objects (Device, CommandQueue, CommandList, etc...)
#include <d3d12.h>

// Microsoft DirectX Graphics Infrastructure. Used to manage the low-level tasks such as enumerating GPU adapters,
//      presenting the rendered image to the screen, and handling full-screen transitions, that are not necessarily
//      part of the DirectX rendering API. DXGI 1.6 adds functionality in order to detect HDR displays.
#include <dxgi1_6.h>

// Contains functions to compile HLSL code at runtime. It is recommended to compile HLSL shaders at compile time, but it
//      may be more convenient to allow runtime compilation of HLSL shaders
//#include <d3dcompiler.h>

// DirectX Math library, provides SIMD-friendly C++ types and functions for commonly used graphics related programming.
#include <DirectXMath.h>

// OPTIONAL. Not required to work with DirectX 12 but it provides some useful classes that will help simplify things.
//      The d3dx12.h header file is not included as part of the Windows 10 SDK and needs to be downloaded separately 
//      from the Microsoft DirectX repository on GitHub (https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/Libraries/D3DX12)
//      https://github.com/microsoft/DirectX-Headers/blob/main/include/directx/d3dx12.h
#include <d3dx12.h>

// Unsure
#include <dxcapi.h>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// Use glm w/ DirectX Settings
#define GLM_FORCE_SSE42 1
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 1
#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// STL headers
#include <thread>
#include <chrono>
using namespace std::chrono;

#include <fmt/os.h>
#include <fstream>
#include <vector>

#include <optional>

bool RUNNING;

struct
{
    glm::mat4 projectionMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
} uboVS;

class App
{
private:
    // Window
    glm::ivec2 _windowSize = { 800, 600 };

    HWND _window;

    // Misc
    unsigned int _frameIndex = 0;

    HANDLE _fenceEvent;

    uint64_t _fenceValue;

    static const unsigned _bufferCount = 2;

    unsigned int _rtvDescriptorHeapSize = 0;

    std::vector<uint8_t> _vert;

    std::vector<uint8_t> _pixel;

    uint8_t* _mappedUniformBuffer;

    // DirectX
    ComPtr<ID3D12Debug1> _dxDebug;

    ComPtr<IDXGIFactory6> _dxFactory;
    
    ComPtr<IDXGIAdapter1> _dxAdapter;

    ComPtr<ID3D12Device2> _dxDevice;

    ComPtr<ID3D12CommandQueue> _dxCommandQueue;

    ComPtr<ID3D12CommandAllocator> _dxCommandAllocator;

    ComPtr<ID3D12Fence> _dxFence;

    ComPtr<IDXGISwapChain3> _dxSwapChain;

    ComPtr<ID3D12DescriptorHeap> _dxRtvDescriptorHeap;

    ComPtr<ID3D12RootSignature> _dxRootSignature;

    D3D12_VERTEX_BUFFER_VIEW _dxVertexBufferView;

    D3D12_INDEX_BUFFER_VIEW _dxIndexBufferView;

    ComPtr<ID3D12Resource> _dxUniformBuffer;

    ComPtr<ID3D12DescriptorHeap> _dxUniformBufferHeap;

    ComPtr<ID3D12PipelineState> _dxPipelineState;

    ComPtr<ID3D12GraphicsCommandList> _dxCommandList;

    D3D12_VIEWPORT _dxViewport;

    D3D12_RECT _dxSurfaceSize;

    ComPtr<ID3D12Resource> _dxRenderTargets[_bufferCount];

    ComPtr<ID3D12Resource> _dxVertexBuffer;

public:
    App() = default;

    ~App() = default;

    static LRESULT CALLBACK WndProc(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
    {
        LONG_PTR userData = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        App* app = (App*)userData;
        if (app) {
            return app->ProcessMessage(hwnd, msg, wParam, lParam);
        }

        if (msg == WM_CREATE) {
            // CREATESTRUCT is the data for the event WM_CREATE
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;

            // Extract our applications pointer from lpCreateParams
            App* app = static_cast<App*>(cs->lpCreateParams);

            // Set our app into userdata
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT ProcessMessage(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_CLOSE) {
            //std::terminate();
            RUNNING = false;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void Cleanup()
    {
        WaitForPreviousFrame();

        CloseHandle(_fenceEvent);
    }

    void InitDefaults()
    {
        _dxSurfaceSize.left = 0;
        _dxSurfaceSize.top = 0;
        _dxSurfaceSize.right = static_cast<LONG>(_windowSize.x);
        _dxSurfaceSize.bottom = static_cast<LONG>(_windowSize.y);

        _dxViewport.TopLeftX = 0.0f;
        _dxViewport.TopLeftY = 0.0f;
        _dxViewport.Width = static_cast<float>(_windowSize.x);
        _dxViewport.Height = static_cast<float>(_windowSize.y);
        _dxViewport.MinDepth = 0.1f;
        _dxViewport.MaxDepth = 1000.0f;
    }

    void InitWindow()
    {
        HINSTANCE instance = GetModuleHandle(nullptr);

        // Register the window class
        const char* WINDOW_CLASS_NAME = "SampleWindowClass";

        DWORD dwStyle = CS_VREDRAW | CS_HREDRAW;

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
            .lpszClassName = WINDOW_CLASS_NAME,
            .hIconSm = nullptr,
        };

        if (!RegisterClassEx(&wndClass)) {
            throw fmt::windows_error(GetLastError(), "RegisterClassEx() Failed");
        }

        // Account for title bar
        RECT wr = { 0, 0, _windowSize.x, _windowSize.y };
        AdjustWindowRect(&wr, dwStyle, false);

        _window = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            WINDOW_CLASS_NAME,
            "Title",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            wr.right - wr.left,
            wr.bottom - wr.top,
            nullptr,
            nullptr,
            instance,
            this
        );

        if (!_window) {
            throw fmt::windows_error(GetLastError(), "Window was not created successfully");
        }

        ShowWindow(_window, SW_SHOWNORMAL);
        UpdateWindow(_window);
    }

    void InitDebugLayer()
    {
        HRESULT hResult;

        // Enable Debug Layer
        ComPtr<ID3D12Debug> dxDebug;
        hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&dxDebug));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "D3D12GetDebugInterface() Failed");
        }

        hResult = dxDebug->QueryInterface(IID_PPV_ARGS(&_dxDebug));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "QueryInterface() Failed");
        }

        _dxDebug->EnableDebugLayer();
        _dxDebug->SetEnableGPUBasedValidation(true);
        //_dxDebug->SetEnableSynchronizedCommandQueueValidation(true);
    }

    void InitFactory()
    {
        HRESULT hResult;

        int dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

        // NOTE: This has a default flag
        hResult = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&_dxFactory));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateDXGIFactory2() Failed");
        }

        // Disable V-Sync
        int allowTearing = 0;
        hResult = _dxFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CheckFeatureSupport() Failed");
        }

        // Disable ALT+ENTER
        hResult = _dxFactory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CheckFeatureSupport() Failed");
        }
    }

    bool IsDeviceSuitable(IDXGIAdapter1* adapter)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        adapter->GetDesc1(&adapterDesc);
        wprintf(L"%s\n", adapterDesc.Description);

        // Skip the basic Render driver adapter | WARP - Windows Advanced Rasterization Platform
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            fmt::print("oof. Software Renderer.\n");
            return false;
        }

        return true;
    }

    void InitDevice()
    {
        HRESULT hResult;

        // Enumerate adapter
        for (unsigned i = 0; ; ++i) {
            hResult = _dxFactory->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&_dxAdapter)
            );

            if (hResult == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            // Create device if suitable
            if (IsDeviceSuitable(_dxAdapter.Get())) {
                hResult = D3D12CreateDevice(
                    _dxAdapter.Get(),
                    D3D_FEATURE_LEVEL_12_1,
                    IID_PPV_ARGS(&_dxDevice)
                );


                if (SUCCEEDED(hResult)) {
                    ComPtr<ID3D12InfoQueue> pInfoQueue;
                    if (SUCCEEDED(_dxDevice.As(&pInfoQueue)))
                    {
                        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

                        // Suppress whole categories of messages
                        //D3D12_MESSAGE_CATEGORY Categories[] = {};

                        // Suppress messages based on their severity level
                        D3D12_MESSAGE_SEVERITY Severities[] =
                        {
                            D3D12_MESSAGE_SEVERITY_INFO
                        };

                        // Suppress individual messages by their ID
                        D3D12_MESSAGE_ID DenyIds[] = {
                            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
                        };

                        D3D12_INFO_QUEUE_FILTER NewFilter = {};
                        //NewFilter.DenyList.NumCategories = _countof(Categories);
                        //NewFilter.DenyList.pCategoryList = Categories;
                        NewFilter.DenyList.NumSeverities = _countof(Severities);
                        NewFilter.DenyList.pSeverityList = Severities;
                        NewFilter.DenyList.NumIDs = _countof(DenyIds);
                        NewFilter.DenyList.pIDList = DenyIds;

                        hResult = pInfoQueue->PushStorageFilter(&NewFilter);

                        if (FAILED(hResult)) {
                            throw fmt::windows_error(hResult, "PushStorageFilter() Failed");
                        }
                    }
                    break;
                }
            }

            _dxAdapter->Release();
        }

        if (!_dxAdapter) {
            throw fmt::format("Adapter was not created succesfully");
        }

        if (!_dxDevice) {
            throw fmt::format("Device was not created successfully");
        }

        ComPtr<ID3D12DebugDevice> dxDebugDevice;
        // NOTE: This is for debugging
        hResult = _dxDevice->QueryInterface(IID_PPV_ARGS(&dxDebugDevice));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "QueryInterface() Failed");
        }
        //dxDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }

    void InitCommandQueue()
    {
        HRESULT hResult;

        // Create commandQueueDesc
        D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        // Make sure every Command queue tracks its own fence object and fence value, and only signals it's own fence object.
        hResult = _dxDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&_dxCommandQueue));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommandQueue() Failed");
        }
    }

    void InitSwapChain()
    {
        HRESULT hResult;

        // Create Swapchain Desc
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
            .Width = static_cast<UINT>(_windowSize.x),
            .Height = static_cast<UINT>(_windowSize.y),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM, //DXGI_FORMAT_R8G8B8A8_UNORM_SRGB - this one fails when making the swapchain
            .Stereo = false,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = _bufferCount,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            // This is part of where we allow tearing if tearing is supported - CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
            .Flags = 0,
        };

        // Create Swapchain
        ComPtr<IDXGISwapChain1> dxSwapChain1;
        hResult = _dxFactory->CreateSwapChainForHwnd(
            _dxCommandQueue.Get(),
            _window,
            &swapChainDesc,
            nullptr,
            nullptr,
            &dxSwapChain1
        );
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateSwapChainForHwnd() Failed");
        }

        hResult = dxSwapChain1->QueryInterface(IID_PPV_ARGS(&_dxSwapChain));
        if (SUCCEEDED(hResult)) {
            //dxSwapChain = static_cast<IDXGISwapChain3*>(dxSwapChain1.Get());
            dxSwapChain1.As<IDXGISwapChain3>(&_dxSwapChain);
            //dxSwapChain1.Reset();
        }

        _frameIndex = _dxSwapChain->GetCurrentBackBufferIndex();
    }

    void InitRenderTargetViews()
    {
        HRESULT hResult;

        // Create Descriptor Heap
        D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = _bufferCount,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };

        hResult = _dxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_dxRtvDescriptorHeap));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateDescriptorHeap() Failed");
        }

        // Create Render Target Views
        _rtvDescriptorHeapSize = _dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(_dxRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < _bufferCount; ++i)
        {
            hResult = _dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&_dxRenderTargets[i]));
            if (FAILED(hResult)) {
                throw fmt::windows_error(hResult, "GetBuffer() Failed");
            }

            _dxDevice->CreateRenderTargetView(_dxRenderTargets[i].Get(), nullptr, RtvHandle);

            //RtvHandle.ptr += (1 * _rtvDescriptorHeapSize);
            RtvHandle.Offset(1, _rtvDescriptorHeapSize);
        }
    }

    void InitCommandAllocator()
    {
        HRESULT hResult;

        hResult = _dxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_dxCommandAllocator));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommandAllocator() Failed");
        }
    }
    
    void InitRootSignature()
    {
        HRESULT hResult;

        //D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {
        //    .HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1
        //};

        //D3D12_DESCRIPTOR_RANGE1 ranges[1];
        //ranges[0].BaseShaderRegister = 0;
        //ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        //ranges[0].NumDescriptors = 1;
        //ranges[0].RegisterSpace = 0;
        //ranges[0].OffsetInDescriptorsFromTableStart = 0;
        //ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

        //D3D12_ROOT_PARAMETER1 rootParameters[1];
        //rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        //rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        //rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        //rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

        //D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        //    .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        //    .Desc_1_1 = {
        //        .NumParameters = 1,
        //        .pParameters = rootParameters,
        //        .NumStaticSamplers = 0,
        //        .pStaticSamplers = nullptr,
        //        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        //    },
        //};

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        hResult = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "D3D12SerializeVersionedRootSignature() Failed");
        }

        hResult = _dxDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_dxRootSignature));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateRootSignature() Failed");
        }

        //if (signature) {
        //    signature->Release();
        //    signature = nullptr;
        //}
    }

    std::vector<uint8_t> LoadFromFile(std::string filename)
    {
        std::ifstream file;

        file.open(filename, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            printf("Something went wrong opening the file\n");
            return {};
        }

        file.unsetf(std::ios::skipws);

        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        auto begin = std::istreambuf_iterator<char>(file);
        auto end = std::istreambuf_iterator<char>();

        std::vector<uint8_t> tempVec;

        tempVec.reserve(size);
        tempVec.assign(begin, end);

        return tempVec;
    }

    void InitShaders()
    {
        wchar_t* cwd = _wgetcwd(NULL, _MAX_PATH);

        wprintf(L"%s\n", cwd);

        free(cwd);

        _vert = LoadFromFile("../../resources/shader.vert.cso");

        _pixel = LoadFromFile("../../resources/shader.pixel.cso");
    }

    void InitPipelineState()
    {
        HRESULT hResult;

        // define the vertex input layout
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = _dxRootSignature.Get();

        D3D12_SHADER_BYTECODE vsByteCode = {
            .pShaderBytecode = _vert.data(),
            .BytecodeLength = _vert.size(),
        };

        psoDesc.VS = vsByteCode;

        D3D12_SHADER_BYTECODE psByteCode = {
            .pShaderBytecode = _pixel.data(),
            .BytecodeLength = _pixel.size(),
        };

        psoDesc.PS = psByteCode;

        //D3D12_RASTERIZER_DESC rasterDesc = {
        //    .FillMode = D3D12_FILL_MODE_SOLID,
        //    .CullMode = D3D12_CULL_MODE_NONE,
        //    .FrontCounterClockwise = FALSE,
        //    .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        //    .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        //    .DepthClipEnable = TRUE,
        //    .MultisampleEnable = FALSE,
        //    .AntialiasedLineEnable = FALSE,
        //    .ForcedSampleCount = 0,
        //    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
        //};

        //psoDesc.RasterizerState = rasterDesc;

        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

        /*D3D12_BLEND_DESC blendDesc;
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            .BlendEnable = FALSE,
            .LogicOpEnable = FALSE,
            .SrcBlend = D3D12_BLEND_ONE,
            .DestBlend = D3D12_BLEND_ZERO,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_ZERO,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

        psoDesc.BlendState = blendDesc;*/
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        hResult = _dxDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_dxPipelineState));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateGraphicsPipelineState() Failed");
        }
    }

    void InitCommands()
    {
        HRESULT hResult;

        hResult = _dxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _dxCommandAllocator.Get(), _dxPipelineState.Get(), IID_PPV_ARGS(&_dxCommandList));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommandList() Failed");
        }

        //_dxCommandList->SetName(L"Hello Triangle Command List");

        hResult = _dxCommandList->Close();
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Close() Failed");
        }
    }

    void InitVertexBuffer()
    {
        HRESULT hResult;

        struct Vertex
        {
            float position[3];
            float color[3];
        };

        Vertex vertexBufferData[3] =
        {
            { { 1.0f, -1.0f, 0.0f }, {1.0f, 0.0f, 0.0f } },
            { { -1.0f, -1.0f, 0.0f }, {0.0f, 1.0f, 0.0f} },
            { { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(vertexBufferData);

        D3D12_HEAP_PROPERTIES vertexHeapProperties = {
            .Type = D3D12_HEAP_TYPE_UPLOAD,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };

        D3D12_RESOURCE_DESC vertexBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = vertexBufferSize,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        hResult = _dxDevice->CreateCommittedResource(
            &vertexHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &vertexBufferResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_dxVertexBuffer)
        );
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommittedResource() Failed");
        }

        // Copy the triangle data to the vertex buffer.
        uint8_t* pVertexDataBegin;

        // We do not intend to read from this resource on the CPU.
        D3D12_RANGE vertexReadRange = {
            .Begin = 0,
            .End = 0,
        };

        hResult = _dxVertexBuffer->Map(0, &vertexReadRange, reinterpret_cast<void**>(&pVertexDataBegin));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Map() Failed");
        }

        memcpy(pVertexDataBegin, vertexBufferData, sizeof(vertexBufferData));
        _dxVertexBuffer->Unmap(0, nullptr);

        _dxVertexBufferView.BufferLocation = _dxVertexBuffer->GetGPUVirtualAddress();
        _dxVertexBufferView.StrideInBytes = sizeof(Vertex);
        _dxVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    void InitFence()
    {
        HRESULT hResult;

        hResult = _dxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_dxFence));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateFence() Failed");
        }

        _fenceValue = 1;

        _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (_fenceEvent == nullptr) {
            throw fmt::format("We're owl exterminators");
        }

        WaitForPreviousFrame();
    }

    void InitIndexBuffer()
    {
        HRESULT hResult;

        uint32_t indexBufferData[3] = { 0, 1, 2 };

        ComPtr<ID3D12Resource> dxIndexBuffer;

        const unsigned int indexBufferSize = sizeof(indexBufferData);

        D3D12_HEAP_PROPERTIES indexHeapProperties = {
            .Type = D3D12_HEAP_TYPE_UPLOAD,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };

        D3D12_RESOURCE_DESC indexBufferResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = indexBufferSize,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        hResult = _dxDevice->CreateCommittedResource(
            &indexHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &indexBufferResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&dxIndexBuffer)
        );
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommittedResource() Failed");
        }

        uint8_t* pIndexDataBegin;

        D3D12_RANGE indexReadRange = {
            .Begin = 0,
            .End = 0,
        };

        hResult = dxIndexBuffer->Map(0, &indexReadRange, reinterpret_cast<void**>(&pIndexDataBegin));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Map() Failed");
        }

        memcpy(pIndexDataBegin, indexBufferData, sizeof(indexBufferData));
        dxIndexBuffer->Unmap(0, nullptr);

        _dxIndexBufferView.BufferLocation = dxIndexBuffer->GetGPUVirtualAddress();
        _dxIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        _dxIndexBufferView.SizeInBytes = indexBufferSize;
    }

    void InitUniformBuffer()
    {
        HRESULT hResult;

        uboVS.projectionMatrix = glm::perspective(45.0f, (float)_windowSize.x / (float)_windowSize.y, 0.01f, 1024.0f);

        uboVS.viewMatrix = glm::translate(glm::identity<glm::mat4>(), glm::vec3(0.0f, 0.0f, 2.5f));

        uboVS.modelMatrix = glm::identity<glm::mat4>();

        D3D12_HEAP_PROPERTIES uniformHeapProperties = {
            .Type = D3D12_HEAP_TYPE_UPLOAD,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };

        D3D12_DESCRIPTOR_HEAP_DESC uniformHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0,
        };

        hResult = _dxDevice->CreateDescriptorHeap(&uniformHeapDesc, IID_PPV_ARGS(&_dxUniformBufferHeap));

        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateDescriptorHeap() Failed");
        }

        D3D12_RESOURCE_DESC uboResourceDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = (sizeof(uboVS) + 255) & ~255,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        hResult = _dxDevice->CreateCommittedResource(
            &uniformHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &uboResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_dxUniformBuffer)
        );
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommittedResource() Failed");
        }

        _dxUniformBufferHeap->SetName(L"Constant Buffer Upload Resource Heap");

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
            .BufferLocation = _dxUniformBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = (sizeof(uboVS) + 255) & ~255 // CB size is required to be 256-byte aligned
        };

        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(_dxUniformBufferHeap->GetCPUDescriptorHandleForHeapStart());
        cbvHandle.ptr = cbvHandle.ptr + _dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 0;

        _dxDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);

        // We do not intend to read from the resource on the CPU. (End is less than or equal to begin)
        D3D12_RANGE uniformReadRange = {
            .Begin = 0,
            .End = 0,
        };

        hResult = _dxUniformBuffer->Map(0, &uniformReadRange, reinterpret_cast<void**>(&_mappedUniformBuffer));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Map() Failed");
        }

        memcpy(_mappedUniformBuffer, &uboVS, sizeof(uboVS));
        _dxUniformBuffer->Unmap(0, &uniformReadRange);
    }

    void SetupCommands()
    {
        HRESULT hResult;

        hResult = _dxCommandAllocator->Reset();
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Reset() Failed");
        }

        hResult = _dxCommandList->Reset(_dxCommandAllocator.Get(), _dxPipelineState.Get());
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Reset() Failed");
        }

        _dxCommandList->SetGraphicsRootSignature(_dxRootSignature.Get());
        _dxCommandList->RSSetViewports(1, &_dxViewport);
        _dxCommandList->RSSetScissorRects(1, &_dxSurfaceSize);

        // Indicate that the back buffer will be used as a render target.
        D3D12_RESOURCE_BARRIER renderTargetBarrier;
        renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        renderTargetBarrier.Transition.pResource = _dxRenderTargets[_frameIndex].Get();
        renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        renderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        _dxCommandList->ResourceBarrier(1, &renderTargetBarrier);

        //_dxCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_dxRenderTargets[_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_dxRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), _frameIndex, _rtvDescriptorHeapSize);
        _dxCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        _dxCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        _dxCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _dxCommandList->IASetVertexBuffers(0, 1, &_dxVertexBufferView);
        //_dxCommandList->IASetIndexBuffer(&_dxIndexBufferView);
        _dxCommandList->DrawInstanced(3, 1, 0, 0);

        //_dxCommandList->DrawIndexedInstanced(3, 1, 0, 0, 0);

        D3D12_RESOURCE_BARRIER presentBarrier;
        presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        presentBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        presentBarrier.Transition.pResource = _dxRenderTargets[_frameIndex].Get();
        presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        _dxCommandList->ResourceBarrier(1, &presentBarrier);

        //_dxCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_dxRenderTargets[_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        hResult = _dxCommandList->Close();
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Close() Failed");
        }
    }

    void WaitForPreviousFrame()
    {
        HRESULT hResult;

        const uint64_t fenceValue = _fenceValue;

        hResult = _dxCommandQueue->Signal(_dxFence.Get(), fenceValue);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Signal() Failed");
        }
        _fenceValue++;

        if (_dxFence->GetCompletedValue() < fenceValue)
        {
            hResult = _dxFence->SetEventOnCompletion(fenceValue, _fenceEvent);
            if (FAILED(hResult)) {
                throw fmt::windows_error(hResult, "SetEventOnCompletion() Failed");
            }
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
        _frameIndex = _dxSwapChain->GetCurrentBackBufferIndex();
    }

    void Render()
    {
        HRESULT hResult;

        SetupCommands();

        ID3D12CommandList* ppCommandLists[] = { _dxCommandList.Get() };
        _dxCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Debug
        hResult = _dxDevice->GetDeviceRemovedReason();
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "OOF Device: ");
        }

        // Present, then wait till finished to continue execution
        hResult = _dxSwapChain->Present(1, 0);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Present() Failed");
        }

        WaitForPreviousFrame();
    }
};

void run()
{
    App app;

    app.InitDefaults();

    app.InitWindow();

    app.InitDebugLayer();
   
    app.InitFactory();

    app.InitDevice();

    app.InitCommandQueue();

    app.InitSwapChain();

    app.InitRenderTargetViews();
   
    app.InitCommandAllocator();

    app.InitRootSignature();

    app.InitShaders();

    app.InitPipelineState();

    app.InitCommands();

    app.InitVertexBuffer();

    app.InitFence();

    // These didn't happen?

    app.InitIndexBuffer();

    app.InitUniformBuffer();


    while (RUNNING)
    {
        MSG msg;
        //printf("hullo\n");
        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        app.Render();
    }

    app.Cleanup();
}

int main(int argc, char ** argv)
{
    // Open Log File
    FILE* file = fopen("LastRun.log", "wt");

    RUNNING = true;

    try {
        run();
    }
    catch (std::exception& e) {
        fmt::print("{}\n", e.what());
        fmt::print(file, "{}\n", e.what());
    }

    fclose(file);

    return 0;
}