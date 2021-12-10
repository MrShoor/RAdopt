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
            glm::ivec2 crd = toCoord(lParam);
            POINT pt;
            pt.x = crd.x;
            pt.y = crd.y;
            ScreenToClient(hwnd, &pt);
            crd.x = pt.x;
            crd.y = pt.y;
            MouseWheel(crd, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA, ToShiftState(wParam));
            return 0;
        case WM_LBUTTONDBLCLK:
            MouseDblClick(0, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_RBUTTONDBLCLK:
            MouseDblClick(1, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_MBUTTONDBLCLK:
            MouseDblClick(2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_XBUTTONDBLCLK:
            MouseDblClick(int(wParam >> 16) + 2, toCoord(lParam), ToShiftState(wParam));
            return 0;
        case WM_KEYDOWN:
            KeyDown(uint32_t(wParam), (lParam & (1 << 30)) != 0);
            return 0;
        case WM_KEYUP:
            KeyUp(uint32_t(wParam), (lParam & (1 << 30)) != 0);
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
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
    void Window::MouseDblClick(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
    }
    void Window::KeyDown(uint32_t vKey, bool duplicate)
    {
    }
    void Window::KeyUp(uint32_t vKey, bool duplicate)
    {
    }
    void Window::Paint(bool* processed)
    {
    }
    void Window::WindowResized(const glm::ivec2& new_size)
    {
    }
    const DevicePtr& RenderWindow::GetDevice() const
    {
        return m_device;
    }
    RenderWindow::RenderWindow(std::wstring caption, bool isMainWindow, bool sRGB) : Window(L"RenderWndClass", caption, isMainWindow)
    {
        m_device = std::make_shared<Device>(Handle(), sRGB);
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
        m_device->States()->SetBlend(true, RA::Blend::one, RA::Blend::inv_src_alpha);
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
    void UIRenderWindow::ControlsDraw_After()
    {
    }
    void UIRenderWindow::ControlsDraw_Before()
    {
    }
    void UIRenderWindow::UpdateDPIScale()
    {
        if (!m_control_global) return;
        float dpi = GetDPIScale();
        if (dpi != m_last_dpi_scale) {
            m_last_dpi_scale = dpi;
            m_control_global->SetDPIScale(m_last_dpi_scale);
            m_control_global->Root()->SetSize(m_control_global->Root()->Size() * m_control_global->GetDPIScale());
        }
    }
    float UIRenderWindow::GetDPIScale() const
    {
        return 1.0f;
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
    void UIRenderWindow::MouseDblClick(int btn, const glm::ivec2& crd, const ShiftState& ss)
    {
        m_control_global->Process_MouseDblClick(btn, crd, ss);
    }
    void UIRenderWindow::KeyDown(uint32_t vKey, bool duplicate)
    {
        m_control_global->Process_KeyDown(vKey, duplicate);
    }
    void UIRenderWindow::KeyUp(uint32_t vKey, bool duplicate)
    {
        m_control_global->Process_KeyUp(vKey, duplicate);
    }
    void UIRenderWindow::WindowResized(const glm::ivec2& new_size)
    {
        m_control_global->Root()->SetSize(glm::vec2(new_size) * m_control_global->GetDPIScale());
    }
    void UIRenderWindow::RenderScene()
    {
        RenderWindow::RenderScene();
        m_control_global->SetDPIScale(GetDPIScale());

        RECT rct;
        GetClientRect(m_device->Window(), &rct);
        if ((rct.right - rct.left) * (rct.bottom - rct.top) == 0) return;

        m_ui_camera->UpdateFromWnd();

        m_fbo->SetSizeFromWindow();
        m_device->SetFrameBuffer(m_fbo);
        m_fbo->Clear(0, { 0,0,0,0 });

        ControlsDraw_Before();
        m_control_global->Draw(m_ui_camera.get());
        ControlsDraw_After();

        m_fbo->BlitToDefaultFBO(0);
    }
    LRESULT UIRenderWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (m_control_global)
            m_control_global->Process_XInput();
        UpdateDPIScale();
        return RenderWindow::WndProc(hwnd, uMsg, wParam, lParam);
    }
    UIRenderWindow::UIRenderWindow(std::wstring caption, bool isMainWindow, bool sRGB) : RenderWindow(caption, isMainWindow, sRGB)
    {
        m_canvas_common = std::make_shared<CanvasCommonObject>(m_device);
        m_control_global = std::make_unique<ControlGlobal>(m_canvas_common);
        m_control_global->SetDPIScale(GetDPIScale());

        RECT rct;
        GetClientRect(Handle(), &rct);
        m_control_global->Root()->SetSize(glm::vec2(rct.right, rct.bottom) * m_control_global->GetDPIScale());

        m_ui_camera = std::make_unique<UICamera>(m_device);

        m_fbo = FBB(m_device)->Color(sRGB ? RA::TextureFmt::RGBA8_SRGB : RA::TextureFmt::RGBA8)->Finish();
    }
}