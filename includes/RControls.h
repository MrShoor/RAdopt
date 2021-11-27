#pragma once
#include "RAdopt.h"
#include "RCanvas.h"
#include "RTypes.h"

namespace RA {
    class Control;

    class ControlGlobal {
        friend class Control;
    private:
        CanvasCommonObjectPtr m_canvas_common;

        Control* m_moved;
        Control* m_captured;
        Control* m_focused;

        bool m_in_drag[5] = { false, false, false, false, false };
        glm::vec2 m_drag_point[5];

        std::unique_ptr<Control> m_root;

        std::vector<Control*> m_ups_subs;

        uint64_t m_last_time;
        RA::QPC m_timer;

        Control* UpdateMovedState(const glm::vec2& pt);

        glm::vec2 ConvertEventCoord(Control* ctrl, const glm::vec2& pt);
    public:        
        DevicePtr Device();

        ControlGlobal(const CanvasCommonObjectPtr& canvas_common);

        CanvasCommonObjectPtr CanvasCommon();

        Control* Root() const;

        Control* Moved() const;
        Control* Captured() const;
        Control* Focused() const;

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
    protected:
        virtual void Notify_MouseEnter() {};
        virtual void Notify_MouseLeave() {};
        virtual void Notify_MouseMove(const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts);
        virtual void Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts);
        virtual void Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseDblClick(int btn, const glm::vec2& pt, const ShiftState& shifts) {};

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
    protected:
        void DoValidate() override;
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
        std::function<void(Control*)> m_onclick;
    protected:
        void HitTestLocal(const glm::vec2& local_pt, Control*& hit_control) override;
        void Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts) override;
        void Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts) override;
    public:
        bool Downed() const;
        std::wstring Text() const;
        void SetText(std::wstring text);

        void Set_OnClick(const std::function<void(Control*)>& callback);
    };
}