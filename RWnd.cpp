#include "pch.h"
#include "RWnd.h"

namespace RA {
    ShiftState ToShiftState(WPARAM wParam)
    {
        ShiftState res;
        res.ctrl = wParam & MK_CONTROL;
        res.shift = wParam & MK_SHIFT;
        res.mouse_btn[0] = wParam & MK_LBUTTON;
        res.mouse_btn[1] = wParam & MK_RBUTTON;
        res.mouse_btn[2] = wParam & MK_MBUTTON;
        res.mouse_btn[3] = wParam & MK_XBUTTON1;
        res.mouse_btn[4] = wParam & MK_XBUTTON2;
        return res;
    }

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
            MouseDown(0, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_RBUTTONDOWN:
            MouseDown(1, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_MBUTTONDOWN:
            MouseDown(2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_XBUTTONDOWN:
            MouseDown(int(wParam >> 16) + 2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_LBUTTONUP:
            MouseUp(0, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_RBUTTONUP:
            MouseUp(1, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_MBUTTONUP:
            MouseUp(2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_XBUTTONUP:
            MouseUp(int(wParam >> 16) + 2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_MOUSEMOVE:
            MouseMove(toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_MOUSEWHEEL:
            MouseWheel(toCoord(lParam), GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA, ToShiftState(wParam));
            return 0;
        case WM_PAINT: {
                bool processed = false;
                Paint(&processed);
                if (processed) {
                    ValidateRect(hwnd, nullptr);
                    return 0;
                }
                break;
            }
        case WM_SIZE:
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            WindowResized(glm::ivec2(width, height));
            return 0;
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
    void Window::MouseWheel(const glm::ivec2& crd, int delta, const ShiftState& ss)
    {
    }
    void Window::Paint(bool* processed)
    {
    }
    void Window::WindowResized(const glm::ivec2& new_size)
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
    void UIRenderWindow::MouseMove(const glm::ivec2& crd, const ShiftState& ss)
    {
        m_control_global->Process_MouseMove(crd, ss);
    }
    void UIRenderWindow::MouseDown(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
        m_control_global->Process_MouseDown(btn, crd, ss);
    }
    void UIRenderWindow::MouseUp(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
        m_control_global->Process_MouseUp(btn, crd, ss);
    }
    void UIRenderWindow::MouseWheel(const glm::ivec2& crd, int delta, const ShiftState& ss)
    {
        m_control_global->Process_MouseWheel(crd, delta, ss);
    }
    void UIRenderWindow::WindowResized(const glm::ivec2& new_size)
    {
        m_control_global->Root()->SetSize(new_size);
    }
    void UIRenderWindow::RenderScene()
    {
        RenderWindow::RenderScene();

        RECT rct;
        GetClientRect(m_device->Window(), &rct);
        if ((rct.right - rct.left) * (rct.bottom - rct.top) == 0) return;

        m_ui_camera->UpdateFromWnd();

        m_fbo->SetSizeFromWindow();
        m_device->SetFrameBuffer(m_fbo);
        m_fbo->Clear(0, { 0,0,0,0 });

        m_control_global->Draw(m_ui_camera.get());

        m_fbo->BlitToDefaultFBO(0);
    }
    UIRenderWindow::UIRenderWindow(std::wstring caption, bool isMainWindow) : RenderWindow(caption, isMainWindow)
    {
        m_canvas_common = std::make_shared<CanvasCommonObject>(m_device);
        m_control_global = std::make_unique<ControlGlobal>(m_canvas_common);

        RECT rct;
        GetClientRect(Handle(), &rct);
        m_control_global->Root()->SetSize(glm::vec2(rct.right, rct.bottom));

        m_ui_camera = std::make_unique<UICamera>(m_device);

        m_fbo = FBB(m_device)->Color(RA::TextureFmt::RGBA8)->Finish();
    }
}