
#include "DirectX12/Engine/pchDirectX.h"
#include "DirectX12/CompiledShaders/AllShaders.h"
#include "DirectX12/Engine/PipelineState.h"
#include "DirectX12/Engine/GpuBuffer.h"
#include "DirectX12/VertexBuffer.h"
#include "DirectX12/ConstantBuffer.h"
#include <d3d12.h>

#include "BezierByGraficRenderer.h"

#include <iostream>

struct Vertex
{
    Vertex() = default;

    Vertex(const Math::Vector3& v)
    {
        this->PosX = v.GetX();
        this->PosY = v.GetY();
        this->PosZ = v.GetZ();
        this->unUsedFloat = 0.0f;
    }
    float PosX;
    float PosY;
    float PosZ;
    float unUsedFloat;
};

#pragma pack(push, 1)
struct PrimitiveData
{
    PrimitiveData() :
        unUsedFloat(0.0f),
        mustBe5(0)
    {
        unUsedFloats1[0] = 0.0f;
        unUsedFloats1[1] = 0.0f;
        unUsedFloats1[2] = 0.0f;
        unUsedFloats1[3] = 0.0f;

        unUsedFloats2[0] = 0.0f;
        unUsedFloats2[1] = 0.0f;
        unUsedFloats2[2] = 0.0f;
        unUsedFloats2[3] = 0.0f;
    }

#include "DirectX12/Shaders/BezierByGrafic/Shared/PrimitiveData.hlsli"
};
#pragma pack(pop)

class PSO_Collection
{
	public:
    GraphicsPSO m_PSO;

    PSO_Collection(GraphicsCore& core) :
        m_PSO(core)
    {	    
    }
};

class BezierByGraficRenderer::Impl
{
private:
    VertexBuffer* m_VertexBuffer = nullptr;
    StructuredBuffer* m_PrimitiveBuffer = nullptr;

public:

    GraphicsCore& m_Core;
    IPrepareGraphicsContext* m_PrepareGraphicsContext;

    PSO_Collection m_PSO;

    std::shared_ptr<ConstantBuffer> m_ConstantBuffer;

    Impl(
        GraphicsCore& core,
        IPrepareGraphicsContext* iPrepareGraphicsContext
        ) :
        m_Core(core),
        m_PrepareGraphicsContext(iPrepareGraphicsContext),
        m_PSO(core)
    {
    }

    ~Impl()
    {
        delete m_VertexBuffer;
        delete m_PrimitiveBuffer;
    }


    void InitPSOs(
        IPreparePipelineState* iPreparePipelineState, PSO_Collection& pso, bool zWriteEnable)
    {
        iPreparePipelineState->PreparePipelineState(pso.m_PSO, zWriteEnable);
    }

    void Init(
	    IPreparePipelineState* iPreparePipelineState,
	    std::shared_ptr<ConstantBuffer> sp_ConstantBuffer
    )
    {
        InitPSOs(iPreparePipelineState, m_PSO, true);

        this->m_ConstantBuffer = sp_ConstantBuffer;

        this->CompletePipelineStates(m_PSO);
    }

    std::vector<Vertex> m_Vertexes;
    std::vector<PrimitiveData> m_PrimitiveFlags;

    void CreateBezier(const Math::Vector3& p1, const Math::Vector3& p2)
    {
        auto v = p2 - p1;
        auto leftNormal = Math::Vector3(v.y, -v.x, 0);

        this->m_Vertexes.emplace_back(p1);
        this->m_Vertexes.emplace_back(p1 + leftNormal);
        this->m_Vertexes.emplace_back(p2 + leftNormal);
        this->m_Vertexes.emplace_back(p2);

        this->m_PrimitiveFlags.emplace_back();
    }

    void CreateSquare(float x, float y)
    {
        Math::Vector3 p1(x, y, 0);
        Math::Vector3 p2(x + 1.0f, y, 0);
        Math::Vector3 p3(x + 1.0f, y + 1.0f, 0);
        Math::Vector3 p4(x, y + 1.0f, 0);

        CreateBezier(p1, p2);
        CreateBezier(p2, p3);
        CreateBezier(p3, p4);
        CreateBezier(p4, p1);
    }

	void CreateData()
    {
        const int numX = static_cast<int>(SizeX);
        const int numY = static_cast<int>(SizeY);
        const int numPrimitives = numX * numY * 4;
        const int numVertices = numPrimitives * 4;

        this->m_Vertexes.reserve(numVertices);
        this->m_PrimitiveFlags.reserve(numPrimitives);

        for (int y = 0; y < numY; ++y)
        {
            for (int x = 0; x < numX; ++x)
            {
                CreateSquare(static_cast<float>(x), static_cast<float>(y));
            }
        }

        // clear old stuff
        delete this->m_VertexBuffer;
        this->m_VertexBuffer = nullptr;

        delete this->m_PrimitiveBuffer;
        this->m_PrimitiveBuffer = nullptr;

        // create the vertex buffer
        this->m_VertexBuffer = new VertexBuffer(this->m_Core, L"BezierByGraficVertexVertices", m_Vertexes);

        this->m_PrimitiveBuffer = new StructuredBuffer(this->m_Core);

        this->m_PrimitiveBuffer->Create(
            L"BezierByGraficPrimitiveFlags",
            static_cast<unsigned int>(m_PrimitiveFlags.size()),
            sizeof(m_PrimitiveFlags[0]),
            m_PrimitiveFlags.data());
    }

    static void PreparePipelineState(GraphicsPSO& pso)
    {
        // Input element Descriptor for Bezier Points
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            {"SV_POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };
        pso.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
        pso.SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF);
        pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
    }

    static void SetPipelineStateShader(GraphicsPSO& pso)
    {
        pso.SetVertexShader(c_pVS, c_sVS);
    	pso.SetHullShader(c_pHS, c_sHS);
        pso.SetDomainShader(c_pDS, c_sDS);
        pso.SetGeometryShader(c_pGS, c_sGS);
    }

    void CompletePipelineStates(PSO_Collection& pso)
    {
        this->PreparePipelineState(pso.m_PSO);
        this->SetPipelineStateShader(pso.m_PSO);
        pso.m_PSO.SetPixelShader(c_pPS, c_sPS);
        pso.m_PSO.Finalize(this->m_Core.m_pDevice);
    }

    void PrepareContext(RenderContext& renderContext)
    {
        renderContext.graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

        renderContext.graphicsContext->SetDynamicDescriptor(RootSignature_PrimitiveBuffer_Index, 0, this->m_PrimitiveBuffer->GetSRV());

        renderContext.graphicsContext->SetVertexBuffers(0, 1, &this->m_VertexBuffer->GetView());
    }
        
    uint64_t Render(RenderContext& renderContext)
    {
        uint64_t fence = 0;
        if (this->m_VertexBuffer == nullptr)
        {
            return fence;
        }

        this->PrepareContext(renderContext);

        this->m_PrepareGraphicsContext->FinishGraphicContext(renderContext);
        this->m_PrepareGraphicsContext->CreateAndInitGraphicContext(L"Render", renderContext);
        this->PrepareContext(renderContext);
        renderContext.graphicsContext->SetPipelineState(this->m_PSO.m_PSO);
        renderContext.graphicsContext->SetDynamicConstantBufferView(RootSignature_ConstantBuffer_Index, sizeof(*this->m_ConstantBuffer), &*this->m_ConstantBuffer);
        renderContext.graphicsContext->Draw(static_cast<UINT>(this->m_VertexBuffer->GetNumElements()), 0);
        
        return fence;
    }
};

BezierByGraficRenderer::BezierByGraficRenderer(
    GraphicsCore& core,
    IPrepareGraphicsContext* iPrepareGraphicsContext) :
    pImpl(new Impl(core, iPrepareGraphicsContext))
{
}

BezierByGraficRenderer::~BezierByGraficRenderer()
{
    delete this->pImpl;
}

void BezierByGraficRenderer::CreateData()
{
    this->pImpl->CreateData();
}

void BezierByGraficRenderer::Init(
	IPreparePipelineState* iPreparePipelineState,
	std::shared_ptr<ConstantBuffer> sp_ConstantBuffer)
{
    this->pImpl->Init(
	    iPreparePipelineState,
	    sp_ConstantBuffer);
}

uint64_t BezierByGraficRenderer::Render(RenderContext& renderContext)
{
    return this->pImpl->Render(renderContext);
}

std::shared_ptr<ConstantBuffer> BezierByGraficRenderer::GetConstantBuffer() const
{
    return this->pImpl->m_ConstantBuffer;
}

bool BezierByGraficRenderer::GetIsEnable() const
{
    return true;
}
