#pragma once

class GraphicsCore;
class Trafos;

class Display
{

public:
    Display(void* hWnd);
    virtual ~Display();

    virtual void Init();
    virtual void Resize(int width, int height) ;
    virtual void Render() ;

    virtual Trafos* GetTransformation() ;
private:
    class Impl;
    Impl* pImpl;
};
