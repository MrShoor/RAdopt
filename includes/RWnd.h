#pragma once
#include "RAdopt.h"
#include "RTypes.h"
#include "RControls.h"
#include "RUtils.h"

#define NOMINMAX
#include <Windows.h>
#include <string>
#include <functional>

namespace RA {
    using WndProcCallback = std::function<LRESULT(UINT, WPARAM, LPARAM)>;

    class Window {
    private:
        std::wstring m_class_name;
        ATOM m_wndclass;
        HWND m_hWnd;
    protected:
        bool m_isMainWindow;
    public:
        Window(std::wstring class_name, std::wstring wnd_caption, bool isMainWindow);
        virtual ~Window();

        HWND Handle() const;
        void Invalidate();
        virtual void MouseMove(const glm::ivec2& crd, const ShiftState& ss);
        virtual void MouseDown(int btn, const glm::ivec2& crd, const ShiftState& ss);
        virtual void MouseUp(int btn, const glm::ivec2& crd, const ShiftState& ss);
        virtual void MouseWheel(const glm::ivec2& crd, int delta, const ShiftState& ss);
        virtual void MouseDblClick(int btn, const glm::ivec2& crd, const ShiftState& ss);
        virtual void KeyDown(uint32_t vKey, bool duplicate);
        virtual void KeyUp(uint32_t vKey, bool duplicate);
        virtual void Paint(bool* processed);
        virtual void WindowResized(const glm::ivec2& new_size);
        virtual LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };

    class RenderWindow : public Window {
    private:
    protected:
        DevicePtr m_device;
    public:
        const DevicePtr& GetDevice() const;

        RenderWindow(std::wstring caption, bool isMainWindow, bool sRGB);
        virtual void RenderScene();
        void Paint(bool* processed) override;
    };

    class UIRenderWindow : public RenderWindow {
    protected:
        std::unique_ptr<UICamera> m_ui_camera;
        CanvasCommonObjectPtr m_canvas_common;
        std::unique_ptr<ControlGlobal> m_control_global;
        RA::FrameBufferPtr m_fbo;

        virtual void ControlsDraw_After();
        virtual void ControlsDraw_Before();        
    public:
        void MouseMove(const glm::ivec2& crd, const ShiftState& ss) override;
        void MouseDown(int btn, const glm::ivec2& crd, const ShiftState& ss) override;
        void MouseUp(int btn, const glm::ivec2& crd, const ShiftState& ss) override;
        void MouseWheel(const glm::ivec2& crd, int delta, const ShiftState& ss) override;
        void MouseDblClick(int btn, const glm::ivec2& crd, const ShiftState& ss) override;
        void KeyDown(uint32_t vKey, bool duplicate) override;
        void KeyUp(uint32_t vKey, bool duplicate) override;
        void WindowResized(const glm::ivec2& new_size) override;
        void RenderScene() override;
        UIRenderWindow(std::wstring caption, bool isMainWindow, bool sRGB);
    };

    void MessageLoop(const std::function<void()> idle_proc);
    void MessageLoop();
}