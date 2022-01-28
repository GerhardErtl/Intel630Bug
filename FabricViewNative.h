#include "DirectX12/Display.h"

class  CFabricViewNative {
public:
	static CFabricViewNative* CreateFabricViewNative();

private:
	CFabricViewNative();

public:
	~CFabricViewNative();

public:

	Display* GetOrCreateDisplay(void* hWnd) const;
	
	Display* GetExistingDisplay() const;

    bool IsDisplayReady() const;

private:
	class Impl;
	Impl* pImpl = nullptr;
};