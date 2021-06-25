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

#include <fmt/os.h>

// STL headers
#include <vector>
#include <fstream>

bool Running;

class App
{
private:
    // Window
    glm::ivec2 _windowSize = { 800, 600 };

    HWND _window = {};

    // Misc
    unsigned int _frameIndex = 0;

    HANDLE _fenceEvent = nullptr;

    uint64_t _fenceValue = 0;

    static const unsigned _bufferCount = 2;

    unsigned int _rtvDescriptorHeapSize = 0;

    std::vector<uint8_t> _vert = {};

    std::vector<uint8_t> _pixel = {};

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

    D3D12_VERTEX_BUFFER_VIEW _dxVertexBufferView = {};

    ComPtr<ID3D12DescriptorHeap> _dxUniformBufferHeap;

    ComPtr<ID3D12PipelineState> _dxPipelineState;

    ComPtr<ID3D12GraphicsCommandList> _dxCommandList;

    D3D12_VIEWPORT _dxViewport = {};

    D3D12_RECT _dxSurfaceSize = {};

    ComPtr<ID3D12Resource> _dxRenderTargets[_bufferCount] = {};

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
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;

            App* app = static_cast<App*>(cs->lpCreateParams);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT ProcessMessage(HWND hwnd, unsigned msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_CLOSE) {
            Running = false;
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
    }

    void InitFactory()
    {
        HRESULT hResult;

        int dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

        hResult = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&_dxFactory));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateDXGIFactory2() Failed");
        }

        int allowTearing = 0;
        hResult = _dxFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CheckFeatureSupport() Failed");
        }

        hResult = _dxFactory->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CheckFeatureSupport() Failed");
        }
    }

    bool IsDeviceSuitable(IDXGIAdapter1* adapter)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        adapter->GetDesc1(&adapterDesc);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            fmt::print("oof. Software Renderer.\n");
            return false;
        }

        return true;
    }

    void InitDevice()
    {
        HRESULT hResult;

        for (unsigned i = 0; ; ++i) {
            hResult = _dxFactory->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&_dxAdapter)
            );

            if (hResult == DXGI_ERROR_NOT_FOUND) {
                break;
            }

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
                        D3D12_MESSAGE_SEVERITY Severities[] =
                        {
                            D3D12_MESSAGE_SEVERITY_INFO
                        };

                        D3D12_MESSAGE_ID DenyIds[] = {
                            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
                        };

                        D3D12_INFO_QUEUE_FILTER NewFilter = {};
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
    }

    void InitCommandQueue()
    {
        HRESULT hResult;

        D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        hResult = _dxDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&_dxCommandQueue));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "CreateCommandQueue() Failed");
        }
    }

    void InitSwapChain()
    {
        HRESULT hResult;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
            .Width = static_cast<UINT>(_windowSize.x),
            .Height = static_cast<UINT>(_windowSize.y),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
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
            .Flags = 0,
        };

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
            dxSwapChain1.As<IDXGISwapChain3>(&_dxSwapChain);
        }

        _frameIndex = _dxSwapChain->GetCurrentBackBufferIndex();
    }

    void InitRenderTargetViews()
    {
        HRESULT hResult;

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

        _rtvDescriptorHeapSize = _dxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(_dxRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < _bufferCount; ++i)
        {
            hResult = _dxSwapChain->GetBuffer(i, IID_PPV_ARGS(&_dxRenderTargets[i]));
            if (FAILED(hResult)) {
                throw fmt::windows_error(hResult, "GetBuffer() Failed");
            }

            _dxDevice->CreateRenderTargetView(_dxRenderTargets[i].Get(), nullptr, RtvHandle);

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
        _vert = LoadFromFile("../../resources/shader.vert.cso");

        _pixel = LoadFromFile("../../resources/shader.pixel.cso");
    }

    void InitPipelineState()
    {
        HRESULT hResult;

        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_SHADER_BYTECODE vsByteCode = {
            .pShaderBytecode = _vert.data(),
            .BytecodeLength = _vert.size(),
        };

        D3D12_SHADER_BYTECODE psByteCode = {
            .pShaderBytecode = _pixel.data(),
            .BytecodeLength = _pixel.size(),
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
            .pRootSignature = _dxRootSignature.Get(),
            .VS = vsByteCode,
            .PS = psByteCode,
            .DS = 0,
            .HS = 0,
            .GS = 0,
            .StreamOutput = 0,
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = {
                .DepthEnable = FALSE,
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
                .DepthFunc = D3D12_COMPARISON_FUNC_NEVER,
                .StencilEnable = FALSE,
                .StencilReadMask = 0,
                .StencilWriteMask = 0,
                .FrontFace = {
                    .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                    .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                    .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                    .StencilFunc = D3D12_COMPARISON_FUNC_NEVER,
                },
                .BackFace = {
                    .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                    .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                    .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                    .StencilFunc = D3D12_COMPARISON_FUNC_NEVER,
                },
            },
            .InputLayout = {
                .pInputElementDescs = inputElementDescs,
                .NumElements = _countof(inputElementDescs),
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = DXGI_FORMAT_R8G8B8A8_UNORM,
            .DSVFormat = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .NodeMask = 0,
            .CachedPSO = 0,
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

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

        D3D12_RANGE vertexReadRange = {
            .Begin = 0,
            .End = 0,
        };

        uint8_t* ptr = nullptr;

        hResult = _dxVertexBuffer->Map(0, &vertexReadRange, reinterpret_cast<void**>(&ptr));
        if (FAILED(hResult)) {
            throw fmt::windows_error(hResult, "Map() Failed");
        }

        memcpy(ptr, vertexBufferData, sizeof(vertexBufferData));

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

        auto renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            _dxRenderTargets[_frameIndex].Get(), 
            D3D12_RESOURCE_STATE_PRESENT, 
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

        _dxCommandList->ResourceBarrier(1, &renderTargetBarrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_dxRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), _frameIndex, _rtvDescriptorHeapSize);
        _dxCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        _dxCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        _dxCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        _dxCommandList->IASetVertexBuffers(0, 1, &_dxVertexBufferView);
        _dxCommandList->DrawInstanced(3, 1, 0, 0);

        auto presentBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            _dxRenderTargets[_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );

        _dxCommandList->ResourceBarrier(1, &presentBarrier);

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
            throw fmt::windows_error(hResult, "GetDeviceRemovedReason()");
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

    while (Running)
    {
        MSG msg;
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
    printf("Argv[1]: %s", argv[1]);


    FILE* file = fopen("LastRun.log", "wt");

    std::vector<FILE*> files;

    Running = true;

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
