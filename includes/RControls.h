#pragma once
#include "RAdopt.h"
#include "RCanvas.h"
#include "RWnd.h"

namespace RA {
    class Control;

    class ControlGlobal {
    private:
        CanvasCommonObjectPtr m_canvas_common;

        Control* m_moved;
        Control* m_captured;
        Control* m_focused;

        std::unique_ptr<Control> m_root;
    public:        
        ControlGlobal(const CanvasCommonObjectPtr& canvas_common);

        CanvasCommonObjectPtr CanvasCommon();

        Control* Root() const;

        Control* Moved() const;
        Control* Captured() const;
        Control* Focused() const;

        void SetCaptured(const Control* ctrl);
        void SetFocused(const Control* ctrl);

        void Draw(Camera* camera);
    };

    class Control {
        friend class ControlGlobal;
    private:
        ControlGlobal* m_cglobal;
        Control* m_parent;
        int m_child_idx;
        std::vector<Control*> m_childs;
    protected:
        glm::vec2 m_pos;
        glm::vec2 m_size;
        glm::vec2 m_origin;
        float m_angle;
        int m_tab_idx;
        float m_dragthreshold;
        bool m_visible;
        bool m_allow_focus;
        bool m_pass_scroll_to_parent;

        ControlGlobal* Global();
    protected:
        virtual void Notify_ChildVisibleChanged(Control* ACurrentChild) {};

        virtual void Notify_ParentSizeChanged() {};
        virtual void Notify_ParentOriginChanged() {};
        virtual void Notify_ParentPosChanged() {};
        virtual void Notify_ParentAngleChanged() {};
    protected:
        bool m_valid;
        void Validate();
        void Invalidate();
        virtual void DoValidate() {};
        virtual void DrawControl(const glm::mat3& transform, Camera* camera);
        virtual void DrawRecursive(const glm::mat3& parent_transform, Camera* camera);
    protected:
        virtual void Notify_MouseEnter() {};
        virtual void Notify_MouseLeave() {};
        virtual void Notify_MouseMove(const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts) {};
        virtual void Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_MouseDblClick(int btn, const glm::vec2& pt, const ShiftState& shifts) {};

        virtual void Notify_DragStart(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_DragMove(int btn, const glm::vec2& pt, const ShiftState& shifts) {};
        virtual void Notify_DragStop(int btn, const glm::vec2& pt, const ShiftState& shifts) {};

        virtual void Notify_FocusSet() {};
        virtual void Notify_FocusLost() {};

        virtual void Notify_KeyDown() {};
        virtual void Notify_Char() {};
        virtual void Notify_KeyUp() {};

        virtual void NextTabStop(Control* ACurrentChild) {};
    public:
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

        void SetParent(Control* parent);
        void SetPos(const glm::vec2& pos);
        void SetSize(const glm::vec2& size);
        void SetOrigin(const glm::vec2& origin);
        void SetAngle(float angle);
        void SetTabIndex(int idx);
        void SetDragThreshold(float drag_threshold);
        void SetVisible(bool visible);
        void SetAllowFocus(bool allow_focus);
        void SetPassScrollToParent(bool pass_scroll);
    public:
        glm::mat3 Transform();
        glm::mat3 TransformInv();
        glm::mat3 AbsTransform();
        glm::mat3 AbsTransformInv();

        void Draw(Camera* camera);

        Control();
        virtual ~Control();
    };

    class CustomControl : public Control {
    protected:
        CanvasPtr m_canvas;
        bool m_moved;
        bool m_focused;        
    protected:
        bool m_invalidate_on_move;
        bool m_invalidate_on_focus;
    protected:
        void DoValidate() override;
        void DrawControl(const glm::mat3& transform, Camera* camera) override;
    public:
        Canvas* Canvas();
        bool Moved();
        bool Focused();
    };

    class CustomButton : public CustomControl {
    private:
    protected:
        bool m_downed;
        std::wstring m_text;
        std::function<void(Control*)> m_onclick;
    public:
        bool Downed() const;
        std::wstring Text() const;
        void SetText(std::wstring text);

        void Set_OnClick(const std::function<void(Control*)>& callback);
    };
}