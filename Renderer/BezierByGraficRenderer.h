#pragma once

#include "DirectX12/ConstantBuffer.h"
#include "DirectX12/Engine/CommandContext.h"
#include "DirectX12/IPreparePipelineState.h"

class BezierByGraficRenderer 
{
public:
    BezierByGraficRenderer(
        GraphicsCore& core,
        IPrepareGraphicsContext*);

    ~BezierByGraficRenderer();

    void CreateData();

    void Init(
        IPreparePipelineState*,
        std::shared_ptr<ConstantBuffer>);

    std::shared_ptr<ConstantBuffer> GetConstantBuffer() const;

    uint64_t Render(RenderContext& renderContext);

    bool GetIsEnable() const;

private:
    class Impl;

    Impl* pImpl;
};

