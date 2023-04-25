//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pchDirectX.h"
#include "GraphicsCore.h"
#include "GpuTimeManager.h"
#include "ColorBuffer.h"
#include "CommandContext.h"

#if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>    // For WARP
#endif
#include <winreg.h>        // To read the registry


//#include "../CompiledShaders/GenerateMipsLinearCS.h"
//#include "../CompiledShaders/GenerateMipsLinearOddCS.h"
//#include "../CompiledShaders/GenerateMipsLinearOddXCS.h"
//#include "../CompiledShaders/GenerateMipsLinearOddYCS.h"
//#include "../CompiledShaders/GenerateMipsGammaCS.h"
//#include "../CompiledShaders/GenerateMipsGammaOddCS.h"
//#include "../CompiledShaders/GenerateMipsGammaOddXCS.h"
//#include "../CompiledShaders/GenerateMipsGammaOddYCS.h"

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

using namespace Math;
using namespace Microsoft::WRL;

#define DEVICE_REMOVE_HANDLING

#ifndef RELEASE
    const GUID WKPDID_D3DDebugObjectName = { 0x429b8c22,0x9188,0x4b0c, { 0x87,0x42,0xac,0xb0,0xbf,0x85,0xc2,0x00 } };
#endif

GraphicsCore* GraphicsCore::s_pCore = nullptr;

GraphicsCore* GraphicsCore::Reserve(HWND g_hWnd)
{
    if(s_pCore == nullptr)
    {
        s_pCore = new GraphicsCore(g_hWnd);
    }

    ++s_pCore->refCount;

    return s_pCore;
}

void GraphicsCore::Release()
{
    --s_pCore->refCount;

    if(s_pCore->refCount == 0)
    {
        s_pCore->Shutdown();
        delete s_pCore;
        s_pCore = nullptr;
    }
}

GraphicsCore::GraphicsCore(HWND g_hWnd) :
    m_pContextManager(new ContextManager()),
    m_pCommandManager(new CommandListManager(*this)),
    m_pDynamicDescriptorHeapStatics(new DynamicDescriptorHeapStatics(*this)),
    m_pLinearAllocatorStatics(new LinearAllocatorStatics(*this)),
    m_pDisplayPlanes(new ColorBuffer[3 /*SWAP_CHAIN_BUFFER_COUNT*/]{ {*this, DXGI_FORMAT_UNKNOWN}, {*this, DXGI_FORMAT_UNKNOWN}, {*this, DXGI_FORMAT_UNKNOWN} }),
    m_CurrentBufferIndex(0),
    m_GenerateMipsRS(*this),
    m_pGenerateMipsLinearPSOs(new ComputePSO[4]{ *this, *this, *this, *this }),
    m_pGenerateMipsGammaPSOs( new ComputePSO[4]{ *this, *this, *this, *this }),
    m_pDescriptorAllocatorStatics(new DescriptorAllocatorStatics(*this)),
    m_pDescriptorAllocators(new DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, *this->m_pDescriptorAllocatorStatics),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, *this->m_pDescriptorAllocatorStatics),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, *this->m_pDescriptorAllocatorStatics),
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, *this->m_pDescriptorAllocatorStatics) }),
    m_DisplayWidth(0),
    m_DisplayHeight(0),
    refCount(0),

    DispatchIndirectCommandSignature(1),
    DrawIndirectCommandSignature(1)
{
    this->Initialize(g_hWnd);
}

GraphicsCore::~GraphicsCore()
{
    delete this->m_pGpuTimeManager;
    this->m_pGpuTimeManager = nullptr;

    delete this->m_pContextManager;
    delete this->m_pCommandManager;
    delete this->m_pDynamicDescriptorHeapStatics;
    delete this->m_pLinearAllocatorStatics;
    delete[] this->m_pDisplayPlanes;
    delete[] m_pGenerateMipsLinearPSOs;
    delete[] m_pGenerateMipsGammaPSOs;
    delete m_pDescriptorAllocatorStatics;
    delete[] m_pDescriptorAllocators;
}

Microsoft::WRL::ComPtr<IDXGIAdapter1> GraphicsCore::GetRecommendedAdapter() const
{
    return this->m_RecommendedAdapter;
}

void GraphicsCore::Resize(uint32_t width, uint32_t height)
{
    ASSERT(m_pSwapChain1 != nullptr);

    // Check for invalid window dimensions
    if (width == 0 || height == 0)
        return;

    // Check for an unneeded resize
    if (width == m_DisplayWidth && height == m_DisplayHeight)
        return;

    m_pCommandManager->IdleGPU();

    m_DisplayWidth = width;
    m_DisplayHeight = height;

    DEBUGPRINT("Changing display resolution to %ux%u", width, height);

    for (uint32_t i = 0; i < this->SWAP_CHAIN_BUFFER_COUNT; ++i)
        m_pDisplayPlanes[i].Destroy();

    ASSERT_SUCCEEDED(m_pSwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

    for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> DisplayPlane;
        ASSERT_SUCCEEDED(m_pSwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
        m_pDisplayPlanes[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
    }

    m_CurrentBufferIndex = 0;

    m_pCommandManager->IdleGPU();
}

// D3D12_AUTO_BREADCRUMB_OP Enum To Strings
static const wchar_t* Breadcrumb_Op_ToStringMap[]  {
L"D3D12_AUTO_BREADCRUMB_OP_SETMARKER",
L"D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT",
L"D3D12_AUTO_BREADCRUMB_OP_ENDEVENT",
L"D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED",
L"D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED",
L"D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT",
L"D3D12_AUTO_BREADCRUMB_OP_DISPATCH",
L"D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION",
L"D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION",
L"D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE",
L"D3D12_AUTO_BREADCRUMB_OP_COPYTILES",
L"D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE",
L"D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW",
L"D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW",
L"D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW",
L"D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER",
L"D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE",
L"D3D12_AUTO_BREADCRUMB_OP_PRESENT",
L"D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA",
L"D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION",
L"D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION",
L"D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME",
L"D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES",
L"D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT",
L"D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64",
L"D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION",
L"D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE",
L"D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1",
L"D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION",
L"D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2",
L"D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1",
L"D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE",
L"D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO",
L"D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE",
L"D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS",
L"D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND",
L"D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND",
L"D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION",
L"D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP",
L"D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1",
L"D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND",
L"D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND",
};

void PrintBreadcrumb(const D3D12_AUTO_BREADCRUMB_NODE* node)
{
    if (node == nullptr)
    {
        return;
    }
    
    std::wstring temp;

    if (node->pCommandListDebugNameW != nullptr)
    {
        temp = std::wstring(L"CommandList:") + node->pCommandListDebugNameW + L"\n";
        OutputDebugString(temp.c_str());
    }
    if (node->pCommandQueueDebugNameW != nullptr)
    {
        temp = std::wstring(L"CommandQueue:") + node->pCommandQueueDebugNameW + L"\n";
        OutputDebugString(temp.c_str());
    }

    temp = std::wstring(L"CommandHistory:\n");
    for (UINT32 i = 0; i < node->BreadcrumbCount; i++)
    {
        const auto commandHistoryEntry = node->pCommandHistory[i];
        temp = Breadcrumb_Op_ToStringMap[commandHistoryEntry] ;

        if(node->pLastBreadcrumbValue != nullptr)
        if (i == *node->pLastBreadcrumbValue)
        {
            temp += L" <<< LastBreadcrumbValue";
        }
        temp += L"\n";
        OutputDebugString(temp.c_str());
    }

    temp = L"\n";
    OutputDebugString(temp.c_str());
}

void GraphicsCore::HandleDeviceRemoved() const
{
#ifndef DEVICE_REMOVE_HANDLING
    return;
#endif

    ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
    this->m_pDevice->QueryInterface(IID_PPV_ARGS(&pDred));

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
    D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
    auto hr = pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
    if (SUCCEEDED(hr))
    {
        if (DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode != nullptr)
        {
            const D3D12_AUTO_BREADCRUMB_NODE* currentNode = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
            
            do
            {
                PrintBreadcrumb(currentNode);
                currentNode = currentNode->pNext;
            }
            while (currentNode != nullptr);
        }
    }
    hr = pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput);

    if (SUCCEEDED(hr))
    {
        if (DredPageFaultOutput.pHeadExistingAllocationNode != nullptr)
        {
            Utility::Print(DredPageFaultOutput.pHeadExistingAllocationNode->ObjectNameA);
            Utility::Print(DredPageFaultOutput.pHeadRecentFreedAllocationNode->ObjectNameA);
        }
    }
}

// Initialize the DirectX resources required to run.
void GraphicsCore::Initialize(HWND g_hWnd)
{
    ASSERT(m_pSwapChain1 == nullptr, "Graphics has already been initialized");

    Microsoft::WRL::ComPtr<ID3D12Device> pDevice;

#if TRUE 
    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    if (SUCCEEDED(D3D12GetDebugInterface(MY_IID_PPV_ARGS(&debugInterface))))
    {
        debugInterface->EnableDebugLayer();
    }
    else
    {
        Utility::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
    }

    // https://devblogs.microsoft.com/directx/dred-v1-2-supports-pix-marker-and-event-strings-in-auto-breadcrumbs/
    // DRED = Device Removed Extended Data
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
    if (SUCCEEDED(D3D12GetDebugInterface(MY_IID_PPV_ARGS(&pDredSettings))))
    {
        // Turn on AutoBreadcrumbs and Page Fault reporting
        pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pDredSettings->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
#endif

    // Obtain the DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;

    ASSERT_SUCCEEDED(CreateDXGIFactory2(0, MY_IID_PPV_ARGS(&dxgiFactory)));

    // Create the D3D graphics device
    Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;

    static const bool bUseWarpDriver = false;

    {
        SIZE_T MaxSize = 0;

        for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            std::wstring description = desc.Description;

            if (description.find(L"Intel") != std::string::npos)
            {
                const HRESULT hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr);
                if (SUCCEEDED(hr))
                {
                    Utility::Printf(L"D3D12-capable hardware found:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
            
                    m_RecommendedAdapter = pAdapter;
                    MaxSize = desc.DedicatedVideoMemory;
            
                    //this->m_IsIntelDevice = false;
            
                    // ALR: Erstmal den 0ten Adapter nehmen
                    break;
                }
            }

            {
                const HRESULT hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr);
                if (SUCCEEDED(hr))
                {
                    if (desc.DedicatedVideoMemory > MaxSize)
                    {
                        Utility::Printf(L"D3D12-capable hardware found:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);

                        m_RecommendedAdapter = pAdapter;
                        MaxSize = desc.DedicatedVideoMemory;

                        // ALR: Erstmal den 0ten Adapter nehmen
                        //break;
                    }
                    else
                    {
                        Utility::Printf(L"D3D12-capable hardware skipped:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
                    }
                }
            }
        }

        if (MaxSize > 0)
        {
            const HRESULT hr = D3D12CreateDevice(m_RecommendedAdapter.Get(), D3D_FEATURE_LEVEL_12_0, MY_IID_PPV_ARGS(&pDevice));
            if(SUCCEEDED(hr))
            {
                Microsoft::WRL::ComPtr<ID3D12Device4> pDevice4;
                pDevice.As(&pDevice4);

                m_pDevice = pDevice4.Detach();

                {
                    auto luid = m_pDevice->GetAdapterLuid();

                    UINT i = 0;
                    ComPtr<IDXGIAdapter> tempAdapter;
                    while (dxgiFactory->EnumAdapters(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND)
                    {
                        DXGI_ADAPTER_DESC desc;
                        tempAdapter->GetDesc(&desc);
                        if (desc.AdapterLuid.HighPart == luid.HighPart && desc.AdapterLuid.LowPart == luid.LowPart)
                        {
                            Utility::Printf(L"D3D12-capable hardware used:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
                            std::wstring description = desc.Description;
                        }
                        ++i;
                    }
                }
            }
            else
            {
                throw "Failed create D3D12 Device: ";
            }
        }
    }
        
    // We like to do read-modify-write operations on UAVs during post processing.  To support that, we
    // need to either have the hardware do typed UAV loads of R11G11B10_FLOAT or we need to manually
    // decode an R32_UINT representation of the same buffer.  This code determines if we get the hardware
    // load support.
    D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
    if (SUCCEEDED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
    {
        //if (FeatureData.TypedUAVLoadAdditionalFormats)
        //{
        //    D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
        //    {
        //        DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
        //    };

        //    if (SUCCEEDED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
        //        (Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
        //    {
        //        //g_bTypedUAVLoadSupport_R11G11B10_FLOAT = true;
        //    }

        //    Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

        //    if (SUCCEEDED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
        //        (Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
        //    {
        //        // g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
        //    }
        //}
    }

    m_pCommandManager->Create();

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_DisplayWidth;
    swapChainDesc.Height = m_DisplayHeight;
    swapChainDesc.Format = SwapChainFormat;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    if(g_hWnd != 0)
    {
        ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(m_pCommandManager->GetCommandQueue(), g_hWnd, &swapChainDesc, nullptr, nullptr, &m_pSwapChain1));
    
        for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        {
            ComPtr<ID3D12Resource> DisplayPlane;
            ASSERT_SUCCEEDED(m_pSwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
            m_pDisplayPlanes[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
        }
    }
    else
    {
    }

    // Common state was moved to GraphicsCommon.*
    this->InitializeCommonState();

    this->m_GenerateMipsRS.Reset(3, 1);
    this->m_GenerateMipsRS[0].InitAsConstants(0, 4);
    this->m_GenerateMipsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    this->m_GenerateMipsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
    this->m_GenerateMipsRS.InitStaticSampler(0, this->SamplerLinearClampDesc);
    this->m_GenerateMipsRS.Finalize(this->m_pDevice, L"Generate Mips");

#define CreatePSO(ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(m_GenerateMipsRS); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize(this->m_pDevice);

    //CreatePSO(m_pGenerateMipsLinearPSOs[0], g_pGenerateMipsLinearCS);
    //CreatePSO(m_pGenerateMipsLinearPSOs[1], g_pGenerateMipsLinearOddXCS);
    //CreatePSO(m_pGenerateMipsLinearPSOs[2], g_pGenerateMipsLinearOddYCS);
    //CreatePSO(m_pGenerateMipsLinearPSOs[3], g_pGenerateMipsLinearOddCS);

    //CreatePSO(m_pGenerateMipsGammaPSOs[0], g_pGenerateMipsGammaCS);
    //CreatePSO(m_pGenerateMipsGammaPSOs[1], g_pGenerateMipsGammaOddXCS);
    //CreatePSO(m_pGenerateMipsGammaPSOs[2], g_pGenerateMipsGammaOddYCS);
    //CreatePSO(m_pGenerateMipsGammaPSOs[3], g_pGenerateMipsGammaOddCS);

    m_pCommandManager->IdleGPU();

    this->m_pGpuTimeManager = new GpuTimeManager();
    this->m_pGpuTimeManager->Initialize(*this, 2, 1);
    this->m_GpuTimerIndexRendering = this->m_pGpuTimeManager->NewTimer();
    this->m_GpuTimerIndexTexture = this->m_pGpuTimeManager->NewTimer();
    this->m_GpuTimerPipelineQueryIndex = this->m_pGpuTimeManager->NewPipelineQuery();
}

void GraphicsCore::Shutdown(void)
{
    this->m_pGpuTimeManager->Shutdown();
    m_pCommandManager->IdleGPU();

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (m_pSwapChain1 != nullptr)
    {
        m_pSwapChain1->SetFullscreenState(FALSE, nullptr);
    }
#endif

    this->m_pDynamicDescriptorHeapStatics->Destroy();
    this->m_pLinearAllocatorStatics->DestroyAll();
    m_pContextManager->DestroyAllContexts();

    m_pCommandManager->Shutdown();
    if (m_pSwapChain1 != nullptr)
    {
        m_pSwapChain1->Release();
    }
    m_pSwapChain1 = nullptr;

    // PSO::DestroyAll();
    this->m_GraphicsPSOHashMap.clear();
    this->m_ComputePSOHashMap.clear();

    //RootSignature::DestroyAll();
    this->m_RootSignatureHashMap.clear();

    this->m_pDescriptorAllocatorStatics->DestroyAll();

    for (size_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
    {
        m_pDescriptorAllocators[i].Reset();
    }

    this->DestroyCommonState();

    for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        m_pDisplayPlanes[i].Destroy();

#if defined(_DEBUG)
    ID3D12DebugDevice* debugInterface;
    if (SUCCEEDED(m_pDevice->QueryInterface(&debugInterface)))
    {
        debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        debugInterface->Release();
    }
#endif

    SAFE_RELEASE(m_pDevice);
}

void GraphicsCore::Present(void)
{
    m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % SWAP_CHAIN_BUFFER_COUNT;

    UINT PresentInterval = 0;

    m_pSwapChain1->Present(PresentInterval, 0);

    m_pCommandManager->IdleGPU();
}

int GraphicsCore::GetMultiSampleQuality()
{
    int msaaSampleCount = 1;
    std::array<int, 4> msaaSampleCounts{ 16, 8, 4, 2 };
    for (auto sc : msaaSampleCounts)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        msLevels.SampleCount = sc;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        this->m_pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels));

        if (msLevels.NumQualityLevels >= 1)
        {
            msaaSampleCount = msLevels.SampleCount;

            break;
        }
    }

    return msaaSampleCount;
}

void GraphicsCore::InitializeCommonState()
{
    SamplerLinearClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerLinearClampDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    SamplerLinearClamp = SamplerLinearClampDesc.CreateDescriptor(*this);

    DispatchIndirectCommandSignature[0].Dispatch();
    DispatchIndirectCommandSignature.Finalize(this->m_pDevice);

    DrawIndirectCommandSignature[0].Draw();
    DrawIndirectCommandSignature.Finalize(this->m_pDevice);
}

void GraphicsCore::DestroyCommonState()
{
    DispatchIndirectCommandSignature.Destroy();
    DrawIndirectCommandSignature.Destroy();
}
