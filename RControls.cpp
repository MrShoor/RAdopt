#include "pch.h"
#include "RControls.h"
#include "RCanvas.h"

namespace RA {
    ControlGlobal* Control::Global()
    {
        if (!m_cglobal) {
            if (m_parent) m_cglobal = m_parent->Global();
        }
        return m_cglobal;
    }
    void Control::Validate()
    {
        if (m_valid) return;
        DoValidate();
        m_valid = true;
    }
    void Control::Invalidate()
    {
        m_valid = false;
    }
    void Control::DrawControl(const glm::mat3& transform, Camera* camera)
    {
        Validate();
    }
    void Control::DrawRecursive(const glm::mat3& parent_transform, Camera* camera)
    {
        if (!m_visible) return;
        glm::mat3 m = parent_transform * Transform();
        DrawControl(m, camera);
        for (const auto& child : m_childs) {
            child->DrawRecursive(m, camera);
        }
    }
    Control* Control::Parent() const
    {
        return m_parent;
    }
    void Control::SetParent(Control* parent)
    {
        if (m_parent == parent) return;
        m_cglobal = nullptr;
        if (m_parent) {
            if (m_child_idx != int(m_parent->m_childs.size() - 1)) {
                m_parent->m_childs[m_child_idx] = m_parent->m_childs.back();
                m_parent->m_childs[m_child_idx]->m_child_idx = m_child_idx;
            }
            m_parent->m_childs.pop_back();
            m_child_idx = -1;
        }
        m_parent = parent;
        if (m_parent) {
            m_child_idx = int(m_parent->m_childs.size());
            m_parent->m_childs.push_back(this);
        }
    }
    glm::vec2 Control::Pos() const
    {
        return m_pos;
    }
    void Control::SetPos(const glm::vec2& pos)
    {
        m_pos = pos;
    }
    glm::vec2 Control::Size() const
    {
        return m_size;
    }
    void Control::SetSize(const glm::vec2& size)
    {
        m_size = size;
    }
    glm::vec2 Control::Origin() const
    {
        return m_origin;
    }
    void Control::SetOrigin(const glm::vec2& origin)
    {
        m_origin = origin;
    }
    float Control::Angle() const
    {
        return m_angle;
    }
    void Control::SetAngle(float angle)
    {
        m_angle = angle;
    }
    int Control::GetTabIndex() const
    {
        return m_tab_idx;
    }
    void Control::SetTabIndex(int idx)
    {
        m_tab_idx = idx;
    }
    float Control::DragThreshold() const
    {
        return m_dragthreshold;
    }
    bool Control::Visible() const
    {
        return m_visible;
    }
    bool Control::AllowFocus() const
    {
        return m_allow_focus;
    }
    bool Control::PassScrollToParent() const
    {
        return m_pass_scroll_to_parent;
    }
    void Control::SetDragThreshold(float drag_threshold)
    {
        m_dragthreshold = drag_threshold;
    }
    void Control::SetVisible(bool visible)
    {
        m_visible = visible;
    }
    void Control::SetAllowFocus(bool allow_focus)
    {
        m_allow_focus = allow_focus;
    }
    void Control::SetPassScrollToParent(bool pass_scroll)
    {
        m_pass_scroll_to_parent = pass_scroll;
    }
    glm::mat3 Control::Transform()
    {
        glm::mat3 to_origin(1.0f);
        to_origin[2] = glm::vec3(m_size * m_origin, 1.0);
        
        float sn = glm::sin(m_angle);
        float cs = glm::cos(m_angle);
        glm::mat3 rot_pos(1.0f);
        rot_pos[0] = glm::vec3(cs, sn, 0.0f);
        rot_pos[1] = glm::vec3(-sn, cs, 0.0f);
        rot_pos[2] = glm::vec3(m_pos.x, m_pos.y, 1.0f);

        return rot_pos * to_origin;  
    }
    glm::mat3 Control::TransformInv()
    {
        return glm::inverse(Transform());
    }
    glm::mat3 Control::AbsTransform()
    {
        if (m_parent) {
            return m_parent->Transform() * Transform();
        }
        else {
            return Transform();
        }
    }
    glm::mat3 Control::AbsTransformInv()
    {
        return glm::inverse(AbsTransform());
    }
    void Control::Draw(Camera* camera)
    {
        DrawRecursive(glm::mat3(1.0f), camera);
    }
    Control::Control() : m_visible(true)
    {
    }
    Control::~Control()
    {
        SetParent(nullptr);
    }
    void CustomControl::DoValidate()
    {
        if (!m_canvas) {
            m_canvas = std::make_shared<RA::Canvas>(*Global()->CanvasCommon().get());
        }
        Control::DoValidate();
        m_canvas->Clear();
    }
    void CustomControl::DrawControl(const glm::mat3& transform, Camera* camera)
    {
        Control::DrawControl(transform, camera);
        m_canvas->Render(*camera, transform);
    }
    Canvas* CustomControl::Canvas()
    {
        return m_canvas.get();
    }
    bool CustomControl::Moved()
    {
        return m_moved;
    }
    bool CustomControl::Focused()
    {
        return m_focused;
    }
    bool CustomButton::Downed() const
    {
        return m_downed;
    }
    std::wstring CustomButton::Text() const
    {
        return m_text;
    }
    void CustomButton::SetText(std::wstring text)
    {
        m_text = std::move(text);
    }
    void CustomButton::Set_OnClick(const std::function<void(Control*)>& callback)
    {
        m_onclick = callback;
    }
    ControlGlobal::ControlGlobal(const CanvasCommonObjectPtr& canvas_common) :
        m_root(std::make_unique<Control>()), 
        m_canvas_common(canvas_common),
        m_moved(nullptr),
        m_captured(nullptr),
        m_focused(nullptr)
    {
        m_root->m_cglobal = this;
    }
    CanvasCommonObjectPtr ControlGlobal::CanvasCommon()
    {
        return m_canvas_common;
    }
    Control* ControlGlobal::Root() const
    {
        return m_root.get();
    }
    Control* ControlGlobal::Moved() const
    {
        return m_moved;
    }
    Control* ControlGlobal::Captured() const
    {
        return m_captured;
    }
    Control* ControlGlobal::Focused() const
    {
        return m_focused;
    }
    void ControlGlobal::SetCaptured(const Control* ctrl)
    {
        
    }
    void ControlGlobal::SetFocused(const Control* ctrl)
    {
    }
    void ControlGlobal::Draw(Camera* camera)
    {
        m_root->Draw(camera);
    }
}