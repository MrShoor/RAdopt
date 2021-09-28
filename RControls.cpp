#include "pch.h"
#include "RControls.h"
#include "RCanvas.h"

namespace RA {
    DevicePtr Control::Device()
    {
        return Global()->Device();
    }
    ControlGlobal* Control::Global()
    {
        if (!m_cglobal) {
            if (m_parent) m_cglobal = m_parent->Global();
        }
        return m_cglobal;
    }
    void Control::Notify_RootChanged()
    {
        for (auto& c : m_childs) {
            c->Notify_RootChanged();
        }
    }
    void Control::RedirectToParent_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts)
    {
        if (m_parent)
            m_parent->Notify_MouseWheel(Space_LocalToParent(pt), delta, shifts);
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
    void Control::DrawControl(const glm::mat3& transform, CameraBase* camera)
    {
        Validate();
    }
    void Control::DrawRecursive(const glm::mat3& parent_transform, CameraBase* camera)
    {
        if (!m_visible) return;
        glm::mat3 m = parent_transform * Transform();
        DrawControl(m, camera);
        for (const auto& child : m_childs) {
            child->DrawRecursive(m, camera);
        }
    }
    bool Control::LocalPtInArea(const glm::vec2& pt)
    {
        return (pt.x >= 0) && (pt.y >= 0) && (pt.x < m_size.x) && (pt.y < m_size.y);
    }
    void Control::HitTestRecursive(const glm::vec2& local_pt, Control*& hit_control)
    {
        if (!Visible()) return;
        if (!LocalPtInArea(local_pt)) return;
        for (int i = int(m_childs.size() - 1); i >= 0; i--) {
            m_childs[i]->HitTestRecursive(m_childs[i]->Space_ParentToLocal(local_pt), hit_control);
            if (hit_control) return;
        }
        HitTestLocal(local_pt, hit_control);
    }
    void Control::HitTestLocal(const glm::vec2& local_pt, Control*& hit_control)
    {
    }
    void Control::UPSSubscribe()
    {
        if (m_ups_idx >= 0) return;
        ControlGlobal* gc = Global();
        if (!gc) return;
        
        m_ups_idx = int(gc->m_ups_subs.size());
        gc->m_ups_subs.push_back(this);
    }
    void Control::UPSUnsubscribe()
    {
        if (m_ups_idx < 0) return;
        ControlGlobal* gc = Global();
        assert(gc);
        gc->m_ups_subs[m_ups_idx] = gc->m_ups_subs.back();
        gc->m_ups_subs[m_ups_idx]->m_ups_idx = m_ups_idx;
        gc->m_ups_subs.pop_back();
        m_ups_idx = -1;
    }
    void Control::OnUPS(uint64_t dt)
    {
    }
    glm::vec2 Control::Space_ParentToLocal(const glm::vec2& pt)
    {
        if (m_parent) {
            glm::mat3 transform = TransformInv();
            return (transform * glm::vec3(pt, 1.0f)).xy();
        }
        else {
            return pt; //todo check this case
        }
    }
    glm::vec2 Control::Space_RootControlToLocal(const glm::vec2& pt)
    {
        if (m_parent) {
            glm::vec2 new_pt = Space_ParentToLocal(pt);
            return m_parent->Space_RootControlToLocal(new_pt);
        }
        else {
            return pt;
        }
    }
    glm::vec2 Control::Space_LocalToRootControl(const glm::vec2& pt)
    {
        if (m_parent) {
            glm::vec2 new_pt = Space_LocalToParent(pt);
            return m_parent->Space_LocalToRootControl(new_pt);
        }
        else {
            return pt;
        }

    }
    glm::vec2 Control::Space_LocalToParent(const glm::vec2& pt)
    {
        if (m_parent) {
            glm::mat3 t = Transform();
            return (t * glm::vec3(pt, 1.0f)).xy();
        }
        else {
            return pt; //todo check this case
        }  
    }
    void Control::Notify_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts)
    {
        if (m_pass_scroll_to_parent)
            RedirectToParent_MouseWheel(pt, delta, shifts);
    }
    void Control::Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        if (AutoCapture()) {
            Global()->SetCaptured(this);
        }
    }
    const std::string& Control::Name() const
    {
        return m_name;
    }
    Control* Control::Root()
    {
        return (m_parent) ? m_parent->Root() : this;
    }
    Control* Control::Parent() const
    {
        return m_parent;
    }
    void Control::SetName(std::string name)
    {
        m_name = std::move(name);
    }
    void Control::SetParent(Control* parent)
    {
        if (m_parent == parent) return;
        
        ControlGlobal* curr_global = Global();
        if (curr_global) {
            if (curr_global->m_moved == this) curr_global->m_moved = nullptr;
            if (curr_global->m_focused == this) curr_global->m_focused = nullptr;
            if (curr_global->m_captured == this) curr_global->m_captured = nullptr;
            m_cglobal = nullptr;
        }

        Control* old_root = Root();
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
        Notify_ParentChanged();
        if (old_root != Root())
            Notify_RootChanged();
    }
    glm::vec2 Control::Pos() const
    {
        return m_pos;
    }
    void Control::SetPos(const glm::vec2& pos)
    {        
        if (m_pos != pos) {
            m_pos = pos;
            for (auto c : m_childs) {
                c->Notify_ParentPosChanged();
            }
        }
    }
    glm::vec2 Control::Size() const
    {
        return m_size;
    }
    void Control::SetSize(const glm::vec2& size)
    {
        if (m_size != size) {
            m_size = size;
            for (auto c : m_childs) {
                c->Notify_ParentSizeChanged();
            }
        }
    }
    glm::vec2 Control::Origin() const
    {
        return m_origin;
    }
    void Control::SetOrigin(const glm::vec2& origin)
    {
        if (m_origin != origin) {
            m_origin = origin;
            for (auto c : m_childs) {
                c->Notify_ParentOriginChanged();
            }
        }
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
    bool Control::AutoCapture() const
    {
        return m_auto_capture;
    }
    bool Control::Focused()
    {
        auto g = Global();
        if (!g) return false;
        return g->Focused() == this;
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
    void Control::SetAutoCapture(bool auto_capture)
    {
        m_auto_capture = auto_capture;
    }
    void Control::SetFocus()
    {
        auto g = Global();
        if (g)
            g->SetFocused(this);
    }
    glm::mat3 Control::Transform()
    {
        glm::mat3 to_origin(1.0f);
        to_origin[2] = glm::vec3(-m_size * m_origin, 1.0);
        
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
    void Control::Draw(CameraBase* camera)
    {
        DrawRecursive(glm::mat3(1.0f), camera);
    }
    Control::Control() : 
        m_visible(true), 
        m_ups_idx(-1), 
        m_auto_capture(true), 
        m_parent(nullptr),
        m_cglobal(nullptr),
        m_child_idx(-1),
        m_pos(0,0),
        m_size(0,0),
        m_origin(0,0),
        m_angle(0),
        m_tab_idx(0),
        m_dragthreshold(5),
        m_allow_focus(false),
        m_pass_scroll_to_parent(false),
        m_valid(false)
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
    void CustomControl::DrawControl(const glm::mat3& transform, CameraBase* camera)
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
    void CustomButton::HitTestLocal(const glm::vec2& local_pt, Control*& hit_control)
    {
        hit_control = this;
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
    Control* ControlGlobal::UpdateMovedState(const glm::vec2& pt)
    {
        Control* res = Captured();
        if (!res) {
            m_root->HitTestRecursive(pt, res);
        }
        if (m_moved != res) {
            if (m_moved) m_moved->Notify_MouseLeave();
            m_moved = res;
            if (m_moved) m_moved->Notify_MouseEnter();
        }
        return res;
    }
    glm::vec2 ControlGlobal::ConvertEventCoord(Control* ctrl, const glm::vec2& pt)
    {
        return (ctrl->AbsTransformInv() * glm::vec3(pt, 1.0f)).xy();
    }
    DevicePtr ControlGlobal::Device()
    {
        if (!m_canvas_common) return nullptr;
        return m_canvas_common->Device();
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
    void ControlGlobal::SetCaptured(Control* ctrl)
    {
        if (m_captured != ctrl) {
            m_captured = ctrl;
            if (m_captured)
                SetCapture(Device()->Window());
            else
                SetCapture(0);
        }
    }
    void ControlGlobal::SetFocused(Control* ctrl)
    {
        if (m_focused == ctrl) return;
        if (m_focused) {
            m_focused->Notify_FocusLost();
        }
        m_focused = ctrl;
        if (m_focused) {
            m_focused->Notify_FocusSet();
        } 
    }
    void ControlGlobal::Process_MouseMove(const glm::vec2& pt, const ShiftState& shifts)
    {
        Control* ctrl = UpdateMovedState(pt);        
        if (ctrl) ctrl->Notify_MouseMove(ConvertEventCoord(ctrl, pt), shifts);
    }
    void ControlGlobal::Process_MouseWheel(const glm::vec2& pt, int delta, const ShiftState& shifts)
    {
        Control* m = UpdateMovedState(pt);
        if (m) m->Notify_MouseWheel(ConvertEventCoord(m, pt), delta, shifts);
    }
    void ControlGlobal::Process_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        Control* m = UpdateMovedState(pt);
        if (m) {
            m->Notify_MouseDown(btn, ConvertEventCoord(m, pt), shifts);
            if (m->AllowFocus()) SetFocused(m);
        }
        else {
            SetFocused(nullptr);
        }
    }
    void ControlGlobal::Process_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        Control* m = UpdateMovedState(pt);
        if (m) m->Notify_MouseUp(btn, ConvertEventCoord(m, pt), shifts);
        SetCaptured(nullptr);
    }
    void ControlGlobal::Process_MouseDblClick(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        Control* m = UpdateMovedState(pt);
        if (m) m->Notify_MouseDblClick(btn, ConvertEventCoord(m, pt), shifts);
    }
    void ControlGlobal::Process_KeyDown(uint32_t vKey, bool duplicate)
    {
        if (!m_focused) return;
        m_focused->Notify_KeyDown(vKey, duplicate);
    }
    void ControlGlobal::Process_KeyUp(uint32_t vKey, bool duplicate)
    {
        if (!m_focused) return;
        m_focused->Notify_KeyUp(vKey, duplicate);
    }
    void ControlGlobal::Draw(CameraBase* camera)
    {
        m_root->Draw(camera);
    }
    void ControlGlobal::UpdateStates()
    {
        uint64_t t = m_timer.Time();
        uint64_t dt = t - m_last_time;
        UpdateStates(dt);
        m_last_time = t;
    }
    void ControlGlobal::UpdateStates(uint64_t dt)
    {
        if (dt > 0) {
            for (int i = int(m_ups_subs.size()) - 1; i >= 0; i--) {
                m_ups_subs[i]->OnUPS(dt);
            }
        }
    }
}