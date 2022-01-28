#include "pch.h"
#include "Trafos.h"
#include <cmath>
#include <random>
#include <Math/Functions.inl>
#include <Math/Vector.h>

Trafos::Trafos() :
    currentWorld(nullptr),
    currentTrafo(nullptr),
    m_viewPortWidth(0),
    m_viewPortHeight(0)
{
}

void Trafos::SetWorldSize(const std::tuple<float, float, float, float> &ws)
{
    this->Init();

    const auto viewCoordinateSize = 2.0f;

    int i = 0;
	{
        auto& world = this->m_worlds;
        std::tie(world.m_worldXMin, world.m_worldXMax, world.m_worldYMin, world.m_worldYMax) = ws;

        world.m_worldXMin -= 0.5f;
        world.m_worldXMax += 0.5f;
        world.m_worldYMin -= 0.5f;
        world.m_worldYMax += 0.5f;

        world.m_worldSizeX = world.m_worldXMax - world.m_worldXMin;
        world.m_worldSizeY = world.m_worldYMax - world.m_worldYMin;
    }

    {
        auto& world = this->m_worlds;
        auto trafo = &world.screen;

        // platzbedarf eigentlich numberOfRows - 1
        // für erste reihe flottungen + 0.5
        // für köpfe der letzten reihe + 0.5
        // für oberste dummy reihe + 1
        auto sizeY = world.m_worldSizeY;// + 2.0f;

        // platzbedarf eigentlich numberOfColumns - 1
        // mittellinen der rechten maschen sind von 0 bis numberOfColumns-1 
        // mittellinen der linken  maschen sind von 0.5 bis numberOfColumns-1 + 0.5
        // die mittellinen gehen somit von 0 bis numberOfColumns - 0.5
        // deshalb
        // für die linke hälfte der ersten rechten masche + 0.5
        // für die rechte hälfte der letzten linken maschen + 0.5
        auto sizeX = world.m_worldSizeX;// +1.0f;

        const auto xScaleFactor = viewCoordinateSize / sizeX;
        const auto yScaleFactor = viewCoordinateSize / sizeY;
        trafo->m_worldScaleFactor  = Math::Min(xScaleFactor, yScaleFactor);
        trafo->m_worldScaleFactorY = yScaleFactor;

        Math::Vector3 scaleVector = Math::Vector3(trafo->m_worldScaleFactor, trafo->m_worldScaleFactor, trafo->m_worldScaleFactor);
        if ((i & 1) == 0)
        {
            scaleVector = Math::Vector3(trafo->m_worldScaleFactor, trafo->m_worldScaleFactor, trafo->m_worldScaleFactor);
        }
        else
        {
            // is Bitmap Matrix
            scaleVector = Math::Vector3(xScaleFactor, yScaleFactor, trafo->m_worldScaleFactor);
        }

        trafo->m_scaleVector = scaleVector;

        const auto shiftMinToZero = Math::Matrix4(Math::XMMatrixTranslation(-world.m_worldXMin, -world.m_worldYMin, 0.0f));

        const Math::Matrix4 scaleToSizeTwo = Math::Matrix4::MakeScale(scaleVector);

        const auto shiftZeroToMinusOne = Math::Matrix4(Math::XMMatrixTranslation(-1.0f, -1.0f, 0.0f));

        //float fx = xScaleFactor / this->m_worldScaleFactor[i];
        //float fy = yScaleFactor / this->m_worldScaleFactor[i];
        //const auto shiftZeroToMinusOne = Math::Matrix4(Math::XMMatrixTranslation(-1.0f / fx,-1.0f / fy,0.0f));
        
        //                           3.              2.               1.
        trafo->m_world = shiftZeroToMinusOne * scaleToSizeTwo * shiftMinToZero;
    }

    this->UpdateTransformation();

    this->currentWorld = &this->m_worlds;
    this->currentTrafo = &this->currentWorld->screen;

    this->CheckAndSetTranslation(this->currentTrafo->m_translation);
}

void Trafos::ZoomToMousePosition(float zoomValue, int mouseX, int mouseY)
{
    // 0 ~ 1
    float x = mouseX / this->m_viewPortWidth;
    float y = mouseY / this->m_viewPortHeight;
    
    // -1 ~ 1
    x = 2 * x - 1;
    y = 2 * y - 1;

    // World Position ermitteln
    const Math::Vector4 mouseScreenPosition { x, y, 0.5f, 1.0f };
    const auto mouseWorldPosition = this->ScreenToWorld(mouseScreenPosition);

    // Zoom setzen
    this->SetZoomValue(zoomValue);

    // Zur World Position scrollen
    this->ScrollToWorld(mouseWorldPosition.GetX(), mouseWorldPosition.GetY());
}


float Trafos::GetTessellationFactor() const
{
    const float MinTessFactor = 3.0f;
    const float MaxTessFactor = 64.0f;

    float anyFloat = std::abs(this->GetScaleFactor() * this->m_viewPortHeight);
    float tessFactor = anyFloat / 3.0f;
    if (tessFactor < MinTessFactor)
    {
        tessFactor = MinTessFactor;
    }
    else if (tessFactor > MaxTessFactor)
    {
        tessFactor = MaxTessFactor;
    }

    return tessFactor;
}

void Trafos::ScrollToWorld(float x, float y)
{
    const Math::Vector4 targetScreenPosition{ 0.0f, 0.0f, 0.5f, 1.0f };
    const Math::Vector4 targetWorldPosition = { x, y, 0.1f, 1.0f};
    
    const Math::Vector4 currentScreenPosition = WorldToScreen(targetWorldPosition);

    // calculate the offset / difference between screen positions
    const auto deltaScreenPosition = targetScreenPosition - currentScreenPosition;

    const auto translationMatrix = Math::XMMatrixTranslation(deltaScreenPosition.GetX(), deltaScreenPosition.GetY(), deltaScreenPosition.GetZ());

    // fix the translation after scaling is applied
    auto newTranslation = Math::Matrix4(translationMatrix) * currentTrafo->m_translation;
    this->CheckAndSetTranslation(newTranslation);
}

void Trafos::SetViewPortSize(float width, float height)
{
    this->m_viewPortWidth = width;
    this->m_viewPortHeight = height;
    this->CheckAndSetTranslation(currentTrafo->m_translation);
    this->UpdateTransformation();
}

void Trafos::ScaleRelativeAroundCenter(float centerX, float centerY, float scaleFactor)
{
    const Math::Vector4 oldScreenPosition{ centerX, centerY, 0.5f, 1.0f };
    // get old world position
    const Math::Vector4 oldWorldPosition = ScreenToWorld(oldScreenPosition);

    // update the scaling matrix
    currentTrafo->m_viewScaleFactor *= scaleFactor;

    // update transformation after scaling change
    this->UpdateTransformation();

    // get the new world position (after scaling change)
    const Math::Vector4 newScreenPosition = WorldToScreen(oldWorldPosition);

    // calculate the offset / difference between screen positions after scaling
    const auto deltaScreenPosition = oldScreenPosition - newScreenPosition;

    const auto translationMatrix = Math::XMMatrixTranslation(deltaScreenPosition.GetX(), deltaScreenPosition.GetY(), deltaScreenPosition.GetZ());

    // fix the translation after scaling is applied
    auto newTranslation = Math::Matrix4(translationMatrix) * currentTrafo->m_translation;
    this->CheckAndSetTranslation(newTranslation);
}

void Trafos::Translate(float deltaInPixelX, float deltaInPixelY)
{
    const float dx = deltaInPixelX / this->m_viewPortWidth * 2.0f;
    // 0-1
    const float dy = deltaInPixelY / this->m_viewPortHeight * 2.0f;
    // 0-1

    const auto translationMatrix = Math::Matrix4(Math::XMMatrixTranslation(dx, dy, 0.0f));

    auto newTranslation = translationMatrix * currentTrafo->m_translation;
    this->CheckAndSetTranslation(newTranslation);
}

void Trafos::CheckAndSetTranslation(Math::Matrix4 newTranslation)
{
    auto w = newTranslation.GetW();
    this->UpdateTransformation();

    const float minX = -currentTrafo->m_screenMax_WithoutTranslation.GetX() + 1.0f;
    const float maxX = -currentTrafo->m_screenMin_WithoutTranslation.GetX() - 1.0f;
    const float minY = -currentTrafo->m_screenMax_WithoutTranslation.GetY() + 1.0f;
    const float maxY = -currentTrafo->m_screenMin_WithoutTranslation.GetY() - 1.0f;

    if (maxX <= minX)
    {
        w.SetX((minX + maxX) / 2.0f);
    }
    else
    {
        if (w.GetX() > maxX) 
        {
            w.SetX(maxX);
        }

        if (w.GetX() < minX) 
        {
            w.SetX(minX);
        }
    }

    if (maxY <= minY)
    {
        w.SetY((minY + maxY) / 2.0f);
    }
    else
    {
        if (w.GetY() > maxY)
        {
            w.SetY(maxY);
        }

        if (w.GetY() < minY)
        {
            w.SetY(minY);
        }
    }

    newTranslation.SetW(w);

    currentTrafo->m_translation = newTranslation;
    // update transformation after translation change
    this->UpdateTransformation();
}

void Trafos::GetTranslateOffset(float& dx, float& dy) const
{
    dx = -currentTrafo->m_translation.GetW().GetX();
    dy = -currentTrafo->m_translation.GetW().GetY();
}


float Trafos::GetViewScaleFactor() const
{
    return currentTrafo->m_viewScaleFactor;
}

const Math::Matrix4& Trafos::GetTransformation() const
{
    return currentTrafo->m_transformation;
}

Math::Vector4 Trafos::WorldToScreen(Math::Vector4 v) const
{
    return currentTrafo->m_transformation * v;
}

Math::Vector4 Trafos::ScreenToWorld(Math::Vector4 v) const
{
    return currentTrafo->m_inverseTransformation * v;
}

Math::Vector4 Trafos::WorldToScreen(const Trafo& trafo, Math::Vector4 v) const
{
    return trafo.m_transformation * v;
}

float Trafos::GetScaleFactor() const
{
    return currentTrafo->m_viewScaleFactor * currentTrafo->m_worldScaleFactor;
}

Math::Vector3 Trafos::GetScaleVector() const
{
    return currentTrafo->m_scaleVector;
}

void Trafos::SetZoomValue(float zoomValue)
{
    currentTrafo->m_viewScaleFactor = zoomValue;
    this->UpdateTransformation();
}

float Trafos::GetZoomValue()
{
    return currentTrafo->m_viewScaleFactor;
}

void Trafos::UpdateTransformation()
{
    int i = 0;
    {
        auto& world = this->m_worlds;
        auto trafo = &world.screen;

        const Math::Matrix4 zTranslationMatrix = Math::Matrix4(Math::XMMatrixTranslation(0.0f, 0.0f, 0.5f));

        // bitmap bzw. full size mode
        if ((i & 1) != 0)
        {
            trafo->m_transformation =
                zTranslationMatrix *
                trafo->m_world;
        }
        else
        {
            float aspectRatio = 1.0f;
            if (this->m_viewPortWidth > 0)
            {
                aspectRatio = this->m_viewPortHeight / this->m_viewPortWidth;
                if (aspectRatio < 0.01 || aspectRatio > 100)
                {
                    aspectRatio = 1.0f;
                }
            }

            const Math::Matrix4 aspectRatioScalingMatrix = Math::Matrix4::MakeScale({ aspectRatio, 1.0f, 1.0f });
            const Math::Matrix4 scaling = Math::Matrix4::MakeScale(Math::Vector3(trafo->m_viewScaleFactor, trafo->m_viewScaleFactor, 0.25f + 0.03125f));

            trafo->m_transformation =
                trafo->m_translation *
                aspectRatioScalingMatrix *
                zTranslationMatrix *
                scaling *
                trafo->m_rotation *
                trafo->m_world;
        }

        trafo->m_inverseTransformation = Math::Invert(trafo->m_transformation);
        trafo->m_normalTransformation = Math::Transpose(trafo->m_inverseTransformation);

        this->CalculateSurroundingRectangleInScreenCoordinates(world, *trafo);
    }
}

void Trafos::Init()
{
    this->m_worlds;

    this->currentWorld = &this->m_worlds;
    this->currentTrafo = &this->currentWorld->screen;

}

void Trafos::CalculateSurroundingRectangleInScreenCoordinates(const World& world, Trafo& trafo)
{
    float worldXMin = world.m_worldXMin;
    float worldXMax = world.m_worldXMax;
    float worldYMin = world.m_worldYMin;
    float worldYMax = world.m_worldYMax;
    float worldZMin = -0.125;
    float worldZMax = +0.125;

    auto screenMin =
        Math::Vector4(std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            1.0f);
    auto screenMax =
        Math::Vector4(std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            1.0f);

    Math::Vector4 tempWorld;
    Math::Vector4 tempScreen;

    // min, min, min
    tempWorld = Math::Vector4(worldXMin, worldYMin, worldZMin, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // min, min, max
    tempWorld = Math::Vector4(worldXMin, worldYMin, worldZMax, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // min, max, min
    tempWorld = Math::Vector4(worldXMin, worldYMax, worldZMin, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // min, max, max
    tempWorld = Math::Vector4(worldXMin, worldYMax, worldZMax, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // max, min, min
    tempWorld = Math::Vector4(worldXMax, worldYMin, worldZMin, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // max, min, max
    tempWorld = Math::Vector4(worldXMax, worldYMin, worldZMax, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // max, max, min
    tempWorld = Math::Vector4(worldXMax, worldYMax, worldZMin, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    // max, max, max
    tempWorld = Math::Vector4(worldXMax, worldYMax, worldZMax, 1.0f);
    tempScreen = WorldToScreen(trafo, tempWorld);
    screenMin = Math::Min(screenMin, tempScreen);
    screenMax = Math::Max(screenMax, tempScreen);

    trafo.m_screenMin = screenMin;
    trafo.m_screenMax = screenMax;

    trafo.m_screenMin_WithoutTranslation = screenMin - trafo.m_translation.GetW();
    trafo.m_screenMax_WithoutTranslation = screenMax - trafo.m_translation.GetW();
}

void ExtractPitchYawRollFromXMMatrix(float* flt_p_PitchOut, float* flt_p_YawOut, float* flt_p_RollOut, const DirectX::XMMATRIX* XMMatrix_p_Rotation)
{
    DirectX::XMFLOAT4X4 XMFLOAT4X4_Values;
    DirectX::XMStoreFloat4x4(&XMFLOAT4X4_Values, DirectX::XMMatrixTranspose(*XMMatrix_p_Rotation));
    *flt_p_PitchOut = (float)asin(-XMFLOAT4X4_Values._23) / static_cast<float>(std::_Pi);
    *flt_p_YawOut = (float)atan2(XMFLOAT4X4_Values._13, XMFLOAT4X4_Values._33) / static_cast<float>(std::_Pi);
    *flt_p_RollOut = (float)atan2(XMFLOAT4X4_Values._21, XMFLOAT4X4_Values._22) / static_cast<float>(std::_Pi);
}

void Trafos::ZoomRelative(int mouseX, int mouseY, float scaleFactor)
{
    float x = mouseX / this->m_viewPortWidth;
    // 0-1
    float y = mouseY / this->m_viewPortHeight;
    // 0-1

    x = 2 * x - 1;
    y = 2 * y - 1;

    scaleFactor = 1 + scaleFactor;
    this->ScaleRelativeAroundCenter(x, y, scaleFactor);
}

void Trafos::ZoomIn(int x, int y)
{
    this->ZoomRelative(x, y, this->ZoomStepSize);
}

void Trafos::ZoomOut(int x, int y)
{
    this->ZoomRelative(x, y, -this->ZoomStepSize);
}
