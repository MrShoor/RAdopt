#pragma once
#include "RAdopt.h"
#include "RCanvas.h"
#include "RTypes.h"
#include "xinput.h"
#include <array>

namespace RA {
    class Control;

    static constexpr float cDeadZoneTolerance = 0.5f;

    enum class InputKind { Keyboard, Gamepad };
    enum class XInputKey { A, B, X, Y, RB, LB, Up, Down, Left, Right, Start, Back, LStick, RStick, 
        LStickMoveRight, LStickMoveUp, LStickMoveLeft, LStickMoveDown, RStickMoveRight, RStickMoveUp, RStickMoveLeft, RStickMoveDown, LT, RT };
    enum class XInputStick { LX, LY, RX, RY, LT, RT };

    static constexpr XInputKey XInputKeyLow() { return XInputKey::A; };
    static constexpr XInputKey XInputKeyHigh() { return XInputKey::RT; };
    static constexpr int XInputKeyCount() { return int(XInputKeyHigh()) - int(XInputKeyLow()) + 1; };

    class ControlGlobal {
        friend class Control;
        static constexpr int64_t cXInputCheckDelay = 3000;
    private:
        CanvasCommonObjectPtr m_canvas_common;

        Control* m_moved;
        Control* m_captured;
        Control* m_focused;

        bool m_in_drag[5] = { false, false, false, false, false };
        glm::vec2 m_drag_point[5] = { {0,0},{0,0},{0,0},{0,0},{0,0} };

        std::unique_ptr<Control> m_root;

        std::vector<Control*> m_ups_subs;

        uint64_t m_last_time = 0;
        RA::QPC m_timer;

        InputKind m_last_input_kind = InputKind::Keyboard;
        std::array<std::array<bool, XInputKeyCount()>, XUSER_MAX_COUNT> m_xinput_key_down;
        std::array<std::array<float, XInputKeyCount()>, XUSER_MAX_COUNT> m_xinput_stick_pos;

        std::array<XINPUT_STATE, XUSER_MAX_COUNT> m_last_xinput;
        std::array<int64_t, XUSER_MAX_COUNT> m_last_failed_time;

        Control* UpdateMovedState(const glm::vec2& pt);

        glm::vec2 ConvertEventCoord(Control* ctrl, const glm::vec2& pt);
        void SetInputKind(InputKind new_kind);
    public:        
        DevicePtr Device();

        ControlGlobal(const CanvasCommonObjectPtr& canvas_common);

        CanvasCommonObjectPtr CanvasCommon();

        Control* Root() const;

        Control* Moved() const;
        Control* Captured() const;
        Control* Focused() const;

        InputKind LastInput() const;

        void SetCaptured(Control* ctrl);
        void SetFocused(Control* ctrl);

        void Process_MouseMove(const glm::vec2& pt, const ShiftState& shifts);
        void Process_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts);
        void Process_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts);
        void Process_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts);
        void Process_MouseDblClick(int btn, const glm::vec2& pt, const ShiftState& shifts);

        void Process_KeyDown(uint32_t vKey, bool duplicate);
        void Process_KeyUp(uint32_t vKey, bool duplicate);

        void Draw(CameraBase* camera);
        void UpdateStates();
        void UpdateStates(uint64_t dt);

        void Process_XInputStick(int pad, XInputStick stick, float new_pos);
        void Process_XInputKey(int pad, XInputKey key, bool down);
        void Process_XInput();

        int XInputPadsCount() const;
        bool Is_XInputKeyDown(int pad, XInputKey key) const;
        float XInputStickPos(int pad, XInputStick stick) const;
    };

    class Control {
        friend class ControlGlobal;
    private:
        ControlGlobal* m_cglobal;
        Control* m_parent;
        int m_child_idx;
        std::vector<Control*> m_childs;
    private:
        int m_ups_idx;
    protected:
        std::string m_name;
        glm::vec2 m_pos;
        glm::vec2 m_size;
        glm::vec2 m_origin;
        float m_angle;
        int m_tab_idx;
        float m_dragthreshold;
        bool m_visible;
        bool m_allow_focus;
        bool m_pass_scroll_to_parent;
        bool m_auto_capture;

        DevicePtr Device();
        ControlGlobal* Global();
    protected:
        virtual void AlignChilds() {};

        virtual void Notify_ChildVisibleChanged(Control* ACurrentChild) {};

        virtual void Notify_RootChanged();
        virtual void Notify_ParentChanged() {};
        virtual void Notify_ParentSizeChanged() {};
        virtual void Notify_ParentOriginChanged() {};
        virtual void Notify_ParentPosChanged() {};
        virtual void Notify_ParentAngleChanged() {};

        void RedirectToParent_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts);
    protected:
        bool m_valid;
        void Validate();
        virtual void DoValidate() {};
        virtual void DrawControl(const glm::mat3& transform, CameraBase* camera);
        virtual void DrawRecursive(const glm::mat3& parent_transform, CameraBase* camera);

        bool LocalPtInArea(const glm::vec2& pt);
        void HitTestRecursive(const glm::vec2& local_pt, Control*& hit_control);
        virtual void HitTestLocal(const glm::vec2& local_pt, Control*& hit_control);
    protected:
        void UPSSubscribe();
        void UPSUnsubscribe();
        virtual void OnUPS(uint64_t dt);
    public:
        glm::vec2 Space_ParentToLocal(const glm::vec2& pt);
        glm::vec2 Space_RootControlToLocal(const glm::vec2& pt);
        glm::vec2 Space_LocalToRootControl(const glm::vec2& pt);
        glm::vec2 Space_LocalToParent(const glm::vec2& pt);
    public:
        virtual void Notify_MouseEnter() {};
        virtual void Notify_MouseLeave() {};
        virtual void Notify_MouseMove(const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts);
        virtual void Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts);
        virtual void Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseDblClick(int btn, const glm::vec2& pt, const ShiftState& shifts) {};

        virtual void Notify_InputKindChanged(InputKind new_input_kind);
        virtual void Notify_XInputStick(int pad, XInputStick stick, float new_pos) {};
        virtual void Notify_XInputKey(int pad, XInputKey key, bool down) {};

        bool InDrag() const;
        virtual void Notify_DragStart(int btn, const glm::vec2& pt, const glm::vec2& drag_pt, const ShiftState& shifts) {};
        virtual void Notify_DragMove(int btn, const glm::vec2& pt, const glm::vec2& drag_pt, const ShiftState& shifts) {};
        virtual void Notify_DragStop(int btn, const glm::vec2& pt, const glm::vec2& drag_pt, const ShiftState& shifts) {};

        virtual void Notify_FocusSet() {};
        virtual void Notify_FocusLost() {};

        virtual void Notify_KeyDown(uint32_t vKey, bool duplicate) {};
        virtual void Notify_Char() {};
        virtual void Notify_KeyUp(uint32_t vKey, bool duplicate) {};

        virtual void NextTabStop(Control* ACurrentChild) {};
    public:
        const std::string& Name() const;

        Control* Root();
        Control* Parent() const;
        glm::vec2 Pos() const;
        glm::vec2 Size() const;
        glm::vec2 Origin() const;
        float Angle() const;
        int GetTabIndex() const;
        float DragThreshold() const;
        bool Visible() const;
        bool AllowFocus() const;
        bool PassScrollToParent() const;
        bool AutoCapture() const;
        bool Focused();

        void SetName(std::string name);
        void SetParent(Control* parent);
        void SetPos(const glm::vec2& pos);
        virtual void SetSize(const glm::vec2& size);
        void SetOrigin(const glm::vec2& origin);
        void SetAngle(float angle);
        void SetTabIndex(int idx);
        void SetDragThreshold(float drag_threshold);
        void SetVisible(bool visible);
        void SetAllowFocus(bool allow_focus);
        void SetPassScrollToParent(bool pass_scroll);
        void SetAutoCapture(bool auto_capture);
        void SetFocus();

        void Invalidate();

        void MoveFocusToSibling(const glm::ivec2& dir);
    public:
        glm::mat3 Transform();
        glm::mat3 TransformInv();
        glm::mat3 AbsTransform();
        glm::mat3 AbsTransformInv();

        void Draw(CameraBase* camera);

        Control();
        virtual ~Control();
    };

    class CustomControl : public Control {
    protected:
        CanvasPtr m_canvas;
    protected:
        bool m_invalidate_on_move;
        bool m_invalidate_on_focus;
        bool m_focus_auto_gift = false;
        virtual void TryGiftFocus(const glm::ivec2& dir);
    protected:
        void PrepareCanvas();
        void DoValidate() override;
        void Notify_MouseEnter() override;
        void Notify_MouseLeave() override;
        void Notify_FocusSet() override;
        void Notify_FocusLost() override;

        void Notify_KeyDown(uint32_t vKey, bool duplicate) override;
        void Notify_XInputKey(int pad, XInputKey key, bool down) override;

        void DrawControl(const glm::mat3& transform, CameraBase* camera) override;
    public:
        Canvas* Canvas();
        bool Moved();
        bool Focused();
    };

    class RenderControl : public Control {
    protected:
    public:
    };

    class CustomButton : public CustomControl {
    private:
    protected:
        bool m_downed;
        std::wstring m_text;
        std::function<void(CustomButton*)> m_onclick;
        std::function<void(CustomButton*)> m_onback;
    protected:
        void HitTestLocal(const glm::vec2& local_pt, Control*& hit_control) override;
        void Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts) override;
        void Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts) override;
        void Notify_KeyUp(uint32_t vKey, bool duplicate) override;
        void Notify_XInputKey(int pad, XInputKey key, bool down) override;
        virtual void DoOnClick();
        virtual void DoOnBack();
    public:
        bool Downed() const;
        std::wstring Text() const;
        void SetText(std::wstring text);

        void Set_OnClick(const std::function<void(CustomButton*)>& callback);
        void Set_OnBack(const std::function<void(CustomButton*)>& callback);

        CustomButton();
    };

    void GridAlign_FixedSize(
        const std::vector<Control*>& controls,
        const glm::AABR& item_box,
        const glm::vec2& min_step,
        float preferred_y_step,
        bool force_fit_y,
        glm::vec2& grid_step,
        glm::vec2& grid_size
    );
}