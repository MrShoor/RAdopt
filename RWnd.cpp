#include "pch.h"
#include "RWnd.h"

namespace RA {
    LRESULT CALLBACK DefWndProc(
        _In_ HWND   hwnd,
        _In_ UINT   uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam
    ) {
        if (uMsg == WM_NCCREATE) {
            SetPropW(hwnd, L"WndPtr", ((tagCREATESTRUCTW*)lParam)->lpCreateParams);
            return true;
        }
        Window* w = (Window*)GetPropW(hwnd, L"WndPtr");
        if (w) {
            return w->WndProc(hwnd, uMsg, wParam, lParam);
        }
        else {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }

    LRESULT Window::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        auto toCoord = [](LPARAM lParam)->glm::ivec2 {
            glm::ivec2 p;
            p.x = int(lParam & 0xffff);
            p.y = int(lParam >> 16);
            return p;
        };

        switch (uMsg) {
        case WM_CLOSE:
            DestroyWindow(m_hWnd);
            m_hWnd = 0;
            return 0;
        case WM_DESTROY:
            if (m_isMainWindow)
                PostQuitMessage(0);
            return 0;
        case WM_LBUTTONDOWN:
            MouseDown(0, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_RBUTTONDOWN:
            MouseDown(1, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_MBUTTONDOWN:
            MouseDown(2, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_XBUTTONDOWN:
            MouseDown(int(wParam >> 16) + 2, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_LBUTTONUP:
            MouseUp(0, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_RBUTTONUP:
            MouseUp(1, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_MBUTTONUP:
            MouseUp(2, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_XBUTTONUP:
            MouseUp(int(wParam >> 16) + 2, toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_MOUSEMOVE:
            MouseMove(toCoord(lParam), ShiftState(wParam));
            return 0;
        case WM_MOUSEWHEEL:
            MouseWhell(toCoord(lParam), int(wParam >> 16) / WHEEL_DELTA, ShiftState(wParam));
            return 0;
        case WM_PAINT:
            bool processed = false;
            Paint(&processed);
            if (processed) {
                ValidateRect(hwnd, nullptr);
                return 0;                
            }
            break;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    Window::Window(std::wstring class_name, std::wstring wnd_caption, bool isMainWindow) {
        m_isMainWindow = isMainWindow;
        m_class_name = class_name;
        WNDCLASSEXW wcex = { };
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = DefWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = GetModuleHandle(nullptr);
        wcex.hIcon = LoadIcon(GetModuleHandle(nullptr), IDI_APPLICATION);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = 0;// (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = m_class_name.c_str();
        wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);
        m_wndclass = RegisterClassExW(&wcex);
        m_hWnd = CreateWindowExW(
            WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW,
            m_class_name.c_str(),
            wnd_caption.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            GetModuleHandle(nullptr),
            this
        );
    }
    Window::~Window() 
    {
        if (m_hWnd) DestroyWindow(m_hWnd);
        UnregisterClassW(m_class_name.c_str(), GetModuleHandle(nullptr));
    }
    HWND Window::Handle() const
    {
        return m_hWnd;
    }
    void Window::Invalidate()
    {
        InvalidateRect(m_hWnd, nullptr, false);
    }
    void Window::MouseMove(const glm::ivec2& crd, const ShiftState& ss)
    {
    }
    void Window::MouseDown(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
    }
    void Window::MouseUp(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
    }
    void Window::MouseWhell(const glm::ivec2& crd, int delta, const ShiftState& ss)
    {
    }
    void Window::Paint(bool* processed)
    {
    }
    RenderWindow::RenderWindow(std::wstring caption, bool isMainWindow) : Window(L"RenderWndClass", caption, isMainWindow)
    {
        m_device = std::make_shared<Device>(Handle());
    }
    void RenderWindow::RenderScene()
    {        
    }
    void RenderWindow::Paint(bool* processed)
    {
        *processed = true;
        RECT rct;
        GetClientRect(Handle(), &rct);
        if (rct.right - rct.left <= 0) return;
        if (rct.bottom - rct.top <= 0) return;

        m_device->BeginFrame();
        RenderScene();
        m_device->PresentToWnd();        
    }
    ShiftState::ShiftState(WPARAM wParam)
    {
        ctrl = wParam & MK_CONTROL;
        shift = wParam & MK_SHIFT;
        mouse_btn[0] = wParam & MK_LBUTTON;
        mouse_btn[1] = wParam & MK_RBUTTON;
        mouse_btn[2] = wParam & MK_MBUTTON;
        mouse_btn[3] = wParam & MK_XBUTTON1;
        mouse_btn[4] = wParam & MK_XBUTTON2;
    }
    void MessageLoop(const std::function<void()> idle_proc)
    {
        MSG msg;
        while (true) {
            while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) return;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            idle_proc();
        }
    }
    void MessageLoop()
    {
        MSG msg;
        while (GetMessageW(&msg, 0, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}