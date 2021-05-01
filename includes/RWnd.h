#pragma once
#include "RAdopt.h"

#define NOMINMAX
#include <Windows.h>
#include <string>
#include <functional>

namespace RA {
    using WndProcCallback = std::function<LRESULT(UINT, WPARAM, LPARAM)>;

    struct ShiftState {
        bool ctrl;
        bool shift;
        bool mouse_btn[5];
        ShiftState(WPARAM wParam);
    };

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
        virtual void MouseWhell(const glm::ivec2& crd, int delta, const ShiftState& ss);
        virtual void Paint(bool* processed);
        virtual LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };

    class RenderWindow : public Window {
    private:
    protected:
        DevicePtr m_device;
    public:
        RenderWindow(std::wstring caption, bool isMainWindow);
        virtual void RenderScene();
        void Paint(bool* processed) override;
    };

    void MessageLoop(const std::function<void()> idle_proc);
    void MessageLoop();
}