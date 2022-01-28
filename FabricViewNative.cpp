
#include "pch.h"
#include "FabricViewNative.h"
#include "DirectX12/Display.h"
#include "Ui/Win32Application.h"

class CFabricViewNative::Impl
{
public:

    Impl() : pDisplay(nullptr)
    {
    }

    ~Impl()
    {
        delete pDisplay;
    }

    Display* GetOrCreateDisplay(void* hWnd)
    {
        if (this->pDisplay == nullptr)
        {
            this->pDisplay = new Display(hWnd);
        }
        return this->pDisplay;
    }

    Display* GetExistingDisplay() const
    {
        return this->pDisplay;
    }

    bool IsDisplayReady() const
    {
        return this->pDisplay != nullptr;
    }
private:
    Display* pDisplay;
};

CFabricViewNative* CFabricViewNative::CreateFabricViewNative()
{
    return new CFabricViewNative();
}

/// <summary>
/// Constructor: Create an Impl
/// </summary>
CFabricViewNative::CFabricViewNative()
{
    this->pImpl = new CFabricViewNative::Impl();
}

/// <summary>
/// Destructor: Destroy our Impl
/// </summary>
CFabricViewNative::~CFabricViewNative()
{
    delete this->pImpl;
}

Display* CFabricViewNative::GetOrCreateDisplay(void* hWnd) const
{
    return this->pImpl->GetOrCreateDisplay(hWnd);
}

Display* CFabricViewNative::GetExistingDisplay() const
{
    return this->pImpl->GetExistingDisplay();
}

bool CFabricViewNative::IsDisplayReady() const
{
    return this->pImpl->IsDisplayReady();
}
