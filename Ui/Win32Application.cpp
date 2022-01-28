//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "Win32Application.h"
#include "FabricViewNative.h"
#include "DirectX12/Display.h"
#include "Renderer/Trafos.h"

HWND Win32Application::m_hwnd = nullptr;
#define IDD_DIALOG1                     101
#define IDC_SLIDER1                     1001
#define IDC_SLIDER2                     1002
#define IDC_SLIDER3                     1003
#define IDC_SLIDER4                     1004
#define IDC_SLIDER5                     1005
#define IDC_SLIDER6                     1006

class WindowData
{
public:
    Display* pDisplay;
    int lastMouseX;
    int lastMouseY;
    int windowWidth;
    int windowHeight;
    bool renderNecessary;
};

WindowData gWindowData;

void Win32Application::CreateWindowAndLoadGraphic(CFabricViewNative* cFabricViewNative, HINSTANCE hInstance)
{
    gWindowData.lastMouseX = -1;
    gWindowData.lastMouseY = -1;
    gWindowData.windowWidth = 1024;
    gWindowData.windowHeight = 768;
    gWindowData.renderNecessary = true;

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(gWindowData.windowWidth), static_cast<LONG>(gWindowData.windowHeight) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    int vScrollWidth = GetSystemMetrics(SM_CXVSCROLL);
    int hScrollHeight = GetSystemMetrics(SM_CYHSCROLL);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        L"Karli",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        &gWindowData);

    // Initialize the sample. OnInit is defined in each child-implementation of DXSample.

    gWindowData.pDisplay = cFabricViewNative->GetOrCreateDisplay(m_hwnd);
    gWindowData.pDisplay->Init();
    gWindowData.pDisplay->Resize(gWindowData.windowWidth, gWindowData.windowHeight);
    ShowWindow(m_hwnd, SW_SHOW);
}

int Win32Application::Run(Display* pDisplay, HINSTANCE hInstance)
{
    //ShowWindow(m_hwnd, SW_SHOW);

    auto showSliderDialog = false;

    if(showSliderDialog)
    {
        HWND hwndDialog = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), m_hwnd, Dlgproc, (size_t)pDisplay);
        ShowWindow(hwndDialog, SW_SHOW);
    }

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
////        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        if (GetMessage(&msg, NULL, 0, 0) != 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Muck: Passiert im desstructor von FabricViewNative
    // pDisplay->Destroy();
    pDisplay = nullptr;

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

INT_PTR CALLBACK Win32Application::Dlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//INT_PTR CALLBACK Dlgproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int minParam[6] = { 20,  1,  1,  1,   1, 0 };
    int maxParam[6] = { 200, 50, 30, 10, 250, 1 };
    int stdParam[6] = { 200, 20, 10,  4,  10, 1 };

    switch (message)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
        for (int sliderId = IDC_SLIDER1; sliderId <= IDC_SLIDER6; sliderId++)
        {
            int index = sliderId - IDC_SLIDER1;
            auto hWndSlider = GetDlgItem(hWnd, sliderId);
            SendMessage(hWndSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(minParam[index], maxParam[index]));
            SendMessage(hWndSlider, TBM_SETPOS, (WPARAM)1, (LPARAM)stdParam[index]);
        }
    }
    break;

    case WM_HSCROLL:
    {
    }
    break;

    case WM_CLOSE:
    {
        ShowWindow(hWnd, SW_HIDE);
    }
    break;

    default:
        break;
    }

    return 0;
}

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto isSliderDialogVisible = false;
    WindowData* pWindowData = nullptr;
    pWindowData = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    Display* pDisplay = nullptr;
    if (pWindowData != nullptr)
    {
        pDisplay = pWindowData->pDisplay;
    }

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the DXSample* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        if (pDisplay)
        {
        }
        return 0;

    case WM_KEYUP:
        if (pDisplay)
        {
            // key == c
            if (wParam == 67)
            {
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (pDisplay && pWindowData)
        {
            auto fwKeys = GET_KEYSTATE_WPARAM(wParam);
            auto zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            ScreenToClient(hWnd, &pt);
            pt.y = pWindowData->windowHeight - pt.y;

            auto trafo = pDisplay->GetTransformation();

            if (zDelta > 0) {
                // zoom in
                trafo->ZoomIn(pt.x, pt.y);
            }
            else
            {
                // zoom out
                trafo->ZoomOut(pt.x, pt.y);
            }

            pWindowData->renderNecessary = true;
            InvalidateRect(hWnd, nullptr, false);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pWindowData)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            y = pWindowData->windowHeight - y;

            if (pWindowData->lastMouseX >= 0)
            {
                auto dx = x - pWindowData->lastMouseX;
                auto dy = y - pWindowData->lastMouseY;

                auto trafo = pDisplay->GetTransformation();
                if ((wParam & 0xc) != 0)
                {
                }
                else
                {
                    trafo->Translate(static_cast<float>(dx), static_cast<float>(dy));
                }

                pWindowData->renderNecessary = true;

                InvalidateRect(hWnd, nullptr, false);

                pWindowData->lastMouseX = x;
                pWindowData->lastMouseY = y;
            }

            pWindowData->renderNecessary = true;

            InvalidateRect(hWnd, nullptr, false);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pWindowData)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            y = pWindowData->windowHeight - y;
        }
        return 0;

    case WM_LBUTTONUP:
        if (pWindowData)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            y = pWindowData->windowHeight - y;

        }
        return 0;

    case WM_RBUTTONDOWN:
        if (pWindowData)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            y = pWindowData->windowHeight - y;
            if (pWindowData != nullptr)
            {
                pWindowData->lastMouseX = x;
                pWindowData->lastMouseY = y;
            }
        }
        return 0;

    case WM_RBUTTONUP:
        if (pWindowData)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            y = pWindowData->windowHeight - y;

            if (pWindowData != nullptr)
            {
                pWindowData->lastMouseX = -1;
                pWindowData->lastMouseY = -1;
            }
        }
        return 0;

    case WM_PAINT:
    {
        RECT rcUpdate;
        auto status = GetUpdateRect(hWnd, &rcUpdate, false);
        if (status == 0)
        {
            return 0;
        }
        if (pDisplay)
        {
            if (pWindowData->renderNecessary)
            {
                pDisplay->Render();
                pWindowData->renderNecessary = false;
            }
        }
        ValidateRect(hWnd, nullptr);
    }
    return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:

        if (pWindowData)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            pDisplay->Resize(width, height);
            pWindowData->windowWidth = width;
            pWindowData->windowHeight = height;
            pWindowData->renderNecessary = true;

            InvalidateRect(hWnd, nullptr, false);
        }
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
