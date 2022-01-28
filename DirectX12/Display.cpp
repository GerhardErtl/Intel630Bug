#include "Engine/pchDirectX.h"
#include "Display.h"

#include "Engine/GraphicsCore.h"
#include "Engine/CommandListManager.h"
#include "Engine/CommandContext.h"
#include "Engine/ColorBuffer.h"

#include "Math/Matrix4.h"

#include "Renderer/Trafos.h"
#include "ConstantBuffer.h"

#include "Renderer/BezierByGraficRenderer.h"

#include "IPreparePipelineState.h"
#include "shellscalingapi.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Display::Impl : public IPreparePipelineState, public IPrepareGraphicsContext
{
public:
    GraphicsCore& m_Core;

    Trafos m_trafos;
private:

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    RootSignature m_rootSignature;

    BezierByGraficRenderer* m_bezierByGraficRenderer = nullptr;

    // Constants for the scene
    std::shared_ptr<ConstantBuffer> m_ConstantBuffer;

    HWND m_hWnd = 0;
public:

    Impl(void* hWnd) :
        m_viewport(0.0f, 0.0f, 0.0f, 0.0f),
        m_scissorRect(0, 0, static_cast<LONG>(0), static_cast<LONG>(0)),
        m_ConstantBuffer(std::make_shared<ConstantBuffer>()),
        m_Core(*GraphicsCore::Reserve(static_cast<HWND>(hWnd))),
        m_rootSignature(m_Core)
    {}

    void Destroy()
    {
        this->m_Core.m_pCommandManager->IdleGPU();

        delete this->m_bezierByGraficRenderer;

        this->m_Core.Release();
    }

    void UpdateBuffer()
    {
        this->m_Core.HandleDeviceRemoved();
        this->m_Core.Resize(this->m_ConstantBuffer->windowSizeXInPixels1, this->m_ConstantBuffer->windowSizeYInPixels1);
    }

    void Resize(int widthInPixel, int heightInPixel)
    {
        this->m_trafos.SetViewPortSize(static_cast<float>(widthInPixel), static_cast<float>(heightInPixel));
        this->m_viewport.Height = static_cast<float>(heightInPixel);
        this->m_viewport.Width = static_cast<float>(widthInPixel);
        this->m_scissorRect.right = widthInPixel;
        this->m_scissorRect.bottom = heightInPixel;

        this->m_ConstantBuffer->windowSizeXInPixels1 = widthInPixel;
        this->m_ConstantBuffer->windowSizeYInPixels1 = heightInPixel;

        this->UpdateBuffer();
    }

    void CreateRendererData()
    {
        this->m_bezierByGraficRenderer->CreateData();
    }

    void Present()
    {
        this->m_Core.m_CurrentBufferIndex = (this->m_Core.m_CurrentBufferIndex + 1) % this->m_Core.SWAP_CHAIN_BUFFER_COUNT;
        this->m_Core.m_pSwapChain1->Present(0, 0);
    }

    void InitGraphicContext(RenderContext& renderContext)
    {
        renderContext.graphicsContext->SetRootSignature(this->m_rootSignature);

        renderContext.graphicsContext->SetViewportAndScissor(*renderContext.viewPort, *renderContext.scissorRect);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[1];
        int numRtvs = 1;
        rtvs[0] = renderContext.colorBuffer->GetRTV();

        renderContext.graphicsContext->SetRenderTargets(numRtvs, rtvs);
    }

    void CreateGraphicContext(std::wstring id, RenderContext& renderContext)
    {
        renderContext.graphicsContext = &GraphicsContext::Begin(id, this->m_Core);
    }

    void CreateAndInitGraphicContext(std::wstring id, RenderContext& renderContext)
    {
        this->CreateGraphicContext(id, renderContext);
        this->InitGraphicContext(renderContext);
    }

    void FinishGraphicContext(RenderContext& renderContext, bool waitForCompletion)
    {
        if (renderContext.graphicsContext != nullptr)
        {
            renderContext.lastFenceValue = renderContext.graphicsContext->Finish(waitForCompletion);
        }
        renderContext.numDrawsCalled = 0;
        renderContext.graphicsContext = nullptr;

        // Timing if you want to :)
    }

    void Render()
    {
        this->m_Core.HandleDeviceRemoved();

        auto renderContext = RenderContext();
        {
            {
                this->m_ConstantBuffer->cViewProjection = this->m_trafos.GetTransformation();
                this->m_ConstantBuffer->viewScaleFactor = this->m_trafos.GetViewScaleFactor();
                this->m_ConstantBuffer->scaleVector = Math::Vector4(this->m_trafos.GetScaleVector(), 1.0f);

                this->m_ConstantBuffer->cTessellationFactor = this->m_trafos.GetTessellationFactor();
                this->m_ConstantBuffer->windowSizeXInPixels1 = this->m_scissorRect.right;
                this->m_ConstantBuffer->windowSizeYInPixels1 = this->m_scissorRect.bottom;

                renderContext.viewPort = &this->m_viewport;
                renderContext.scissorRect = &this->m_scissorRect;

                renderContext.colorBuffer = &this->m_Core.m_pDisplayPlanes[this->m_Core.m_CurrentBufferIndex];
            }
        }

        // Setup
        {
            this->CreateGraphicContext(L"Render-Setup", renderContext);
            renderContext.graphicsContext->SetRootSignature(this->m_rootSignature);

            renderContext.graphicsContext->SetViewportAndScissor(*renderContext.viewPort, *renderContext.scissorRect);

            renderContext.graphicsContext->TransitionResource(*renderContext.colorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
            renderContext.graphicsContext->FlushResourceBarriers();

            D3D12_CPU_DESCRIPTOR_HANDLE rtvs[1];
            int numRtvs = 1;
            rtvs[0] = renderContext.colorBuffer->GetRTV();

            renderContext.graphicsContext->SetRenderTargets(numRtvs, rtvs);

            // Clear the color Buffer
            renderContext.graphicsContext->ClearColor(*renderContext.colorBuffer);

            renderContext.numDrawsCalled = 0;
        }

        this->m_bezierByGraficRenderer->Render(renderContext);

        // Indicate that the back buffer will now be used to present.
        renderContext.graphicsContext->TransitionResource(*renderContext.colorBuffer, D3D12_RESOURCE_STATE_PRESENT);

        this->FinishGraphicContext(renderContext, true);
    }

    void Init()
    {
        this->CreateRootSignature();
        this->m_bezierByGraficRenderer = new BezierByGraficRenderer(this->m_Core, this);

        this->m_bezierByGraficRenderer->Init(
            this,
            this->m_ConstantBuffer);

        this->m_bezierByGraficRenderer->CreateData();

        this->m_trafos.SetWorldSize(std::make_tuple(0.0f, SizeX, 0.0f, SizeY));
    }

    void CreateRootSignature()
    {
        // Create a root signature consisting of a descriptor table with a single CBV.
        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        //m_rootSignature.Reset(3, 1);
        this->m_rootSignature.Reset(2, 0);
        this->m_rootSignature[RootSignature_ConstantBuffer_Index].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);

        this->m_rootSignature[RootSignature_PrimitiveBuffer_Index].InitAsDescriptorTable(1, D3D12_SHADER_VISIBILITY_HULL);
        this->m_rootSignature[RootSignature_PrimitiveBuffer_Index].SetTableRange(
            0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 0);

        this->m_rootSignature.Finalize(this->m_Core.m_pDevice, L"RootSignature", rootSignatureFlags);
    }

    void PreparePipelineState(GraphicsPSO& pipelineState, bool zWriteEnable) const
    {
        pipelineState.SetRootSignature(this->m_rootSignature);

        D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rasterDesc.MultisampleEnable = false;
        rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

        pipelineState.SetRasterizerState(rasterDesc);
        pipelineState.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));

        D3D12_DEPTH_STENCIL_DESC dsDesc;
        dsDesc.DepthEnable = false;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        if (zWriteEnable)
        {
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        }
        else
        {
            dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }

        dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        //dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        dsDesc.StencilEnable = FALSE;
        pipelineState.SetDepthStencilState(dsDesc);

        DXGI_FORMAT RTVFormats[]
        {
            this->m_Core.m_pDisplayPlanes[this->m_Core.m_CurrentBufferIndex].GetFormat()
        };

        //pipelineState.SetRenderTargetFormats(1, RTVFormats, this->m_SceneDepthBuffer.GetFormat(), 1, 0);
        pipelineState.SetRenderTargetFormats(1, RTVFormats, DXGI_FORMAT_UNKNOWN, 1, 0);
    }
};

Display::Display(void* hWnd)
{
    this->pImpl = new Impl(hWnd);
}

Display::~Display()
{
    this->pImpl->Destroy();
    delete this->pImpl;
}

void Display::Init()
{
    this->pImpl->Init();
}

void Display::Resize(int width, int height)
{
    this->pImpl->Resize(width, height);
}

void Display::Render()
{
    this->pImpl->Render();
    this->pImpl->Present();
}

Trafos* Display::GetTransformation()
{
    return &this->pImpl->m_trafos;
}
