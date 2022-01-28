#pragma once

#include <Math\Matrix4.h>

struct Trafo
{
    Math::Matrix4 m_normalTransformation;
    Math::Matrix4 m_inverseTransformation;
    Math::Matrix4 m_transformation;
    Math::Matrix4 m_translation;
    Math::Matrix4 m_world;
    Math::Matrix4 m_rotation;

    float m_worldScaleFactor;
    float m_worldScaleFactorY;
    float m_viewScaleFactor;

    Math::Vector4 m_screenMin;
    Math::Vector4 m_screenMax;
    Math::Vector3 m_scaleVector;

    Math::Vector4 m_screenMin_WithoutTranslation;
    Math::Vector4 m_screenMax_WithoutTranslation;

    Trafo() :
        m_normalTransformation(Math::Matrix4(Math::kIdentity)),
        m_inverseTransformation(Math::Matrix4(Math::kIdentity)),
        m_transformation(Math::Matrix4(Math::kIdentity)),
        m_translation(Math::Matrix4(Math::kIdentity)),
        m_world(Math::Matrix4(Math::kIdentity)),
        m_rotation(Math::Matrix4(Math::kIdentity)),
        m_worldScaleFactor(1.0f),
        m_worldScaleFactorY(1.0f),
        m_viewScaleFactor(1.0f),
        m_screenMin(Math::Vector4(0, 0, 0, 0)),
        m_screenMax(Math::Vector4(0, 0, 0, 0)),
        m_scaleVector(Math::Vector3(0, 0, 0)),
        m_screenMin_WithoutTranslation(Math::Vector4(0, 0, 0, 0)),
        m_screenMax_WithoutTranslation(Math::Vector4(0, 0, 0, 0))
    {
    }
};

struct World
{
    float m_worldXMin;
    float m_worldXMax;
    float m_worldYMin;
    float m_worldYMax;
    float m_worldSizeX;
    float m_worldSizeY;

    Trafo screen;

    Trafo* trafos[1];

    World()
    {
        trafos[0] = &screen;
    }
};


class Trafos
{
public:
    Trafos();

public:
    virtual void ZoomIn(int x, int y) ;
    virtual void ZoomOut(int x, int y) ;

    virtual void Translate(float deltaInPixelX, float deltaInPixelY) ;

    virtual void SetZoomValue(float zoomValue) ;
    virtual float GetZoomValue() ;

    virtual void ZoomToMousePosition(float zoomValue, int x, int y) ;

public:
    void SetWorldSize(const std::tuple<float, float, float, float>& ws);

    float GetTessellationFactor() const;

    void ScrollToWorld(float x, float y);
    void SetViewPortSize(float width, float height);

    void ScaleRelativeAroundCenter(float centerX, float centerY, float scaleFactor);
    void GetTranslateOffset(float& dx, float& dy) const;
    float GetViewScaleFactor() const;

    const Math::Matrix4& GetTransformation() const;
    
    Math::Vector4 WorldToScreen(Math::Vector4 v) const;
    Math::Vector4 ScreenToWorld(Math::Vector4 v) const;
    float GetScaleFactor() const;
    Math::Vector3 GetScaleVector() const;

    World m_worlds;
    World* currentWorld;
    Trafo* currentTrafo;

private:
    float m_viewPortWidth;
    float m_viewPortHeight;
    void Init();

    void CalculateSurroundingRectangleInScreenCoordinates(const World& world, Trafo& trafo);
    void UpdateTransformation();
    void CheckAndSetTranslation(Math::Matrix4 newTranslation);

    Math::Vector4 WorldToScreen(const Trafo&, Math::Vector4 v) const;
    void ZoomRelative(int mouseX, int mouseY, float scaleFactor);

    const float ZoomStepSize = 0.3f;
};
