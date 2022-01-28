#pragma once

class GraphicsPSO;

class IPreparePipelineState
{
public:
    virtual void PreparePipelineState(GraphicsPSO& sp_PipelineState, bool zWriteEnable) const = 0;
};

struct RenderContext
{
    int64_t lastFenceValue;

    GraphicsContext* graphicsContext;
    int numDrawsCalled;

    CD3DX12_VIEWPORT* viewPort;
    CD3DX12_RECT* scissorRect;

    ColorBuffer* colorBuffer;

    RenderContext() :
      lastFenceValue(0),
      graphicsContext(nullptr),
      numDrawsCalled(0),
      viewPort(nullptr),
      scissorRect(nullptr),
      colorBuffer(nullptr)
    {
    }
};

class IPrepareGraphicsContext
{
public:
    virtual void CreateGraphicContext(std::wstring id, RenderContext& renderContext) = 0;
    virtual void InitGraphicContext(RenderContext& renderContext) = 0;
    virtual void CreateAndInitGraphicContext(std::wstring id, RenderContext& renderContext) = 0;
    virtual void FinishGraphicContext(RenderContext& renderContext, bool waitForCompletion = false) = 0;
};