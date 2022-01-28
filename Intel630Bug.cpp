// TestFabricViewNativeConsole.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
// Windows Header Files

#include <string>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <windows.h>

#include "FabricViewNative.h"

#include "UI/Win32Application.h"


int main(int argc, char* argv[])
{
    CFabricViewNative* fn = CFabricViewNative::CreateFabricViewNative();

    auto hInstance = GetModuleHandle(NULL);

    Win32Application::CreateWindowAndLoadGraphic(fn, hInstance);
    Win32Application::Run(fn->GetExistingDisplay(), hInstance);

    delete fn;
}
