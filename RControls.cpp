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
    void Control::MoveFocusToSibling(const glm::ivec2& dir)
    {
        Control* p = Parent();
        if (!p) return;
        glm::ivec2 sd = glm::sign(dir);
        float min_dist = 1000000000;
        Control* next_control = nullptr;
        if (dir.x) {
            for (int i = 0; i < int(p->m_childs.size()); i++) {
                if (i == m_child_idx) continue;
                if (!p->m_childs[i]->AllowFocus()) continue;
                glm::vec2 cdir = p->m_childs[i]->Pos() - Pos();
                float dist = cdir.x * sd.x;
                if (dist <= 0) continue;
                if (glm::abs(cdir.y) >= glm::abs(cdir.x)) continue;
                if (dist < min_dist) {
                    min_dist = dist;
                    next_control = p->m_childs[i];
                }
            }
        }
        else {
            for (int i = 0; i < int(p->m_childs.size()); i++) {
                if (i == m_child_idx) continue;
                if (!p->m_childs[i]->AllowFocus()) continue;
                glm::vec2 cdir = p->m_childs[i]->Pos() - Pos();
                float dist = cdir.y * sd.y;
                if (dist <= 0) continue;
                if (glm::abs(cdir.x) >= glm::abs(cdir.y)) continue;
                if (dist < min_dist) {
                    min_dist = dist;
                    next_control = p->m_childs[i];
                }
            }
        }
        if (next_control) {
            next_control->SetFocus();
        }
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
        hit_control = this;
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
    float Control::DPIScale()
    {
        auto g = Global();
        return g ? g->GetDPIScale() : 1.0f;
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
    void Control::Notify_InputKindChanged(InputKind new_input_kind)
    {
        for (int i = int(m_childs.size()) - 1; i >= 0; i--) {
            m_childs[i]->Notify_InputKindChanged(new_input_kind);
        }
    }
    bool Control::InDrag() const
    {
        if (m_cglobal)
            if (m_cglobal->m_captured == this) {            
                for (int i = 0; i < 5; i++) {
                    if (m_cglobal->m_in_drag[i]) return true;
                }
            }
        return false;
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
            if (curr_global->m_captured == this) curr_global->SetCaptured(nullptr);
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
            AlignChilds();
            Invalidate();
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
    void CustomControl::TryGiftFocus(const glm::ivec2& dir)
    {
        if (!m_focus_auto_gift) return;
        MoveFocusToSibling(dir);
    }
    void CustomControl::PrepareCanvas()
    {
        if (!m_canvas) {
            auto g = Global();
            if (!g) return;
            m_canvas = std::make_shared<RA::Canvas>(*g->CanvasCommon().get());
        }
    }
    void CustomControl::DoValidate()
    {
        PrepareCanvas();
        Control::DoValidate();
        m_canvas->Clear();
    }
    void CustomControl::Notify_MouseEnter()
    {
        if (m_invalidate_on_move) Invalidate();
    }
    void CustomControl::Notify_MouseLeave()
    {
        if (m_invalidate_on_move) Invalidate();
    }
    void CustomControl::Notify_FocusSet()
    {
        Control::Notify_FocusSet();
        if (m_invalidate_on_focus) Invalidate();
    }
    void CustomControl::Notify_FocusLost()
    {
        if (m_invalidate_on_focus) Invalidate();
    }
    void CustomControl::Notify_KeyDown(uint32_t vKey, bool duplicate)
    {
        Control::Notify_KeyDown(vKey, duplicate);
        if (vKey == VK_DOWN) {
            TryGiftFocus({ 0,1 });
            return;
        }
        if (vKey == VK_UP) {
            TryGiftFocus({ 0,-1 });
            return;
        }
        if (vKey == VK_RIGHT) {
            TryGiftFocus({ 1,0 });
            return;
        }
        if (vKey == VK_LEFT) {
            TryGiftFocus({ -1,0 });
            return;
        }
    }
    void CustomControl::Notify_XInputKey(int pad, XInputKey key, bool down)
    {
        Control::Notify_XInputKey(pad, key, down);
        if ((key == XInputKey::Up || key == XInputKey::LStickMoveUp) && down) {
            TryGiftFocus({ 0,-1 });
            return;
        }
        if ((key == XInputKey::Down || key == XInputKey::LStickMoveDown) && down) {
            TryGiftFocus({ 0, 1 });
            return;
        }
        if ((key == XInputKey::Left || key == XInputKey::LStickMoveLeft) && down) {
            TryGiftFocus({ -1, 0 });
            return;
        }
        if ((key == XInputKey::Right || key == XInputKey::LStickMoveRight) && down) {
            TryGiftFocus({ 1, 0 });
            return;
        }
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
        ControlGlobal* g = Global();
        if (!g) return false;
        return g->Moved() == this;
    }
    bool CustomControl::Focused()
    {
        ControlGlobal* g = Global();
        if (!g) return false;
        return g->Focused() == this;
    }
    void CustomButton::HitTestLocal(const glm::vec2& local_pt, Control*& hit_control)
    {
        hit_control = this;
    }
    void CustomButton::Notify_MouseDown(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        CustomControl::Notify_MouseDown(btn, pt, shifts);
        m_downed = true;
        Invalidate();
    }
    void CustomButton::Notify_MouseUp(int btn, const glm::vec2& pt, const ShiftState& shifts)
    {
        CustomControl::Notify_MouseUp(btn, pt, shifts);
        if (!m_downed) return;
        m_downed = false;
        Control* ctrl;
        HitTestLocal(pt, ctrl);
        if (ctrl == this) {
            DoOnClick();
        }
        Invalidate();
    }
    void CustomButton::Notify_KeyUp(uint32_t vKey, bool duplicate)
    {
        CustomControl::Notify_KeyUp(vKey, duplicate);
        if (vKey == VK_RETURN) {
            DoOnClick();
        }
        if (vKey == VK_BACK || vKey == VK_ESCAPE) {
            DoOnBack();
        }
    }
    void CustomButton::Notify_XInputKey(int pad, XInputKey key, bool down)
    {
        CustomControl::Notify_XInputKey(pad, key, down);
        if (key == XInputKey::A && !down) {
            DoOnClick();
        }
        if (key == XInputKey::B && !down) {
            DoOnBack();
        }
    }
    void CustomButton::DoOnClick()
    {
        if (m_onclick) {
            m_onclick(this);
        }
    }
    void CustomButton::DoOnBack()
    {
        if (m_onback) {
            m_onback(this);
        }
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
    void CustomButton::Set_OnClick(const std::function<void(CustomButton*)>& callback)
    {
        m_onclick = callback;
    }
    void CustomButton::Set_OnBack(const std::function<void(CustomButton*)>& callback)
    {
        m_onback = callback;
    }
    CustomButton::CustomButton()
    {
        m_invalidate_on_move = true;
    }
    ControlGlobal::ControlGlobal(const CanvasCommonObjectPtr& canvas_common) :
        m_root(std::make_unique<Control>()), 
        m_canvas_common(canvas_common),
        m_moved(nullptr),
        m_captured(nullptr),
        m_focused(nullptr)
    {
        m_canvas_common->SetDPIScale(m_dpi_scale);
        m_root->m_cglobal = this;
        for (auto& xstate : m_last_xinput) {
            ZeroMemory(&xstate, sizeof(XINPUT_STATE));
        }
        for (auto& t : m_last_failed_time) {
            t = -cXInputCheckDelay;
        }

        for (auto& pad : m_xinput_key_down) {
            for (auto& key : pad) {
                key = false;
            }
        }
        for (auto& pad : m_xinput_stick_pos) {
            for (auto& stick : pad) {
                stick = 0.0f;
            }
        }
    }
    float ControlGlobal::GetDPIScale() const
    {
        return m_dpi_scale;
    }
    void ControlGlobal::SetDPIScale(float dpi_scale)
    {
        m_dpi_scale = dpi_scale;
        m_canvas_common->SetDPIScale(dpi_scale);
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
    void ControlGlobal::SetInputKind(InputKind new_kind)
    {
        if (m_last_input_kind == new_kind) return;
        m_last_input_kind = new_kind;
        m_root->Notify_InputKindChanged(new_kind);
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
    InputKind ControlGlobal::LastInput() const
    {
        return m_last_input_kind;
    }
    void ControlGlobal::SetCaptured(Control* ctrl)
    {
        if (m_captured != ctrl) {
            if (m_captured) {
                for (int i = 0; i < 5; i++) {
                    if (m_in_drag[i]) {
                        ShiftState ss;
                        ss.ctrl = GetKeyState(VK_CONTROL) < 0;
                        for (int j = 0; j < 5; j++) ss.mouse_btn[j] = false;
                        ss.mouse_btn[i] = true;
                        ss.shift = GetKeyState(VK_SHIFT) < 0;
                        m_captured->Notify_DragStop(i, m_drag_point[i], m_drag_point[i], ss);
                    }
                }
            }
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
    void ControlGlobal::Process_MouseMove(glm::vec2 pt, const ShiftState& shifts)
    {
        pt *= GetDPIScale();
        Control* ctrl = UpdateMovedState(pt);
        if (ctrl) {            
            if (Captured() == ctrl) {
                for (int i = 0; i < 5; i++) {
                    if (!m_in_drag[i]) {
                        glm::vec2 dir = m_drag_point[i] - pt;
                        float dist = ctrl->DragThreshold();
                        if (glm::dot(dir, dir) > dist * dist) {
                            m_in_drag[i] = true;
                            ctrl->Notify_DragStart(i, pt, m_drag_point[i], shifts);
                        }
                    }
                    if (m_in_drag[i]) {
                        ctrl->Notify_DragMove(i, pt, m_drag_point[i], shifts);
                    }
                }
            }
            ctrl->Notify_MouseMove(ConvertEventCoord(ctrl, pt), shifts);
        }        
    }
    void ControlGlobal::Process_MouseWheel(glm::vec2 pt, int delta, const ShiftState& shifts)
    {
        pt *= GetDPIScale();
        Control* m = UpdateMovedState(pt);
        if (m) m->Notify_MouseWheel(ConvertEventCoord(m, pt), delta, shifts);
    }
    void ControlGlobal::Process_MouseDown(int btn, glm::vec2 pt, const ShiftState& shifts)
    {
        pt *= GetDPIScale();
        Control* m = UpdateMovedState(pt);
        if (m) {
            m->Notify_MouseDown(btn, ConvertEventCoord(m, pt), shifts);
            if (m->AllowFocus()) SetFocused(m);
            Control* cap = Captured();
            if (cap == m) {
                m_drag_point[btn] = pt;
            }
        }
        else {
            SetFocused(nullptr);
        }
    }
    void ControlGlobal::Process_MouseUp(int btn, glm::vec2 pt, const ShiftState& shifts)
    {
        pt *= GetDPIScale();
        Control* m = UpdateMovedState(pt);        
        if (m) {
            m->Notify_MouseUp(btn, ConvertEventCoord(m, pt), shifts);
            if (Captured() == m) {
                for (int i = 0; i < 5; i++) {
                    if (m_in_drag[i])
                        m->Notify_DragStop(btn, pt, m_drag_point[i], shifts);
                    m_in_drag[i] = false;
                }
            }
        }
        SetCaptured(nullptr);
    }
    void ControlGlobal::Process_MouseDblClick(int btn, glm::vec2 pt, const ShiftState& shifts)
    {
        pt *= GetDPIScale();
        Control* m = UpdateMovedState(pt);
        if (m) m->Notify_MouseDblClick(btn, ConvertEventCoord(m, pt), shifts);
    }
    void ControlGlobal::Process_KeyDown(uint32_t vKey, bool duplicate)
    {
        SetInputKind(InputKind::Keyboard);
        if (!m_focused) return;
        m_focused->Notify_KeyDown(vKey, duplicate);
    }
    void ControlGlobal::Process_KeyUp(uint32_t vKey, bool duplicate)
    {
        SetInputKind(InputKind::Keyboard);
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
    void ControlGlobal::Process_XInputStick(int pad, XInputStick stick, float new_pos)
    {
        Control* c = Focused();
        m_xinput_stick_pos[pad][int(stick)] = new_pos;
        if (!c) return;
        c->Notify_XInputStick(pad, stick, new_pos);
    }
    void ControlGlobal::Process_XInputKey(int pad, XInputKey key, bool down)
    {
        SetInputKind(InputKind::Gamepad);
        m_xinput_key_down[pad][int(key)] = down;
        Control* c = Focused();
        if (!c) return;
        c->Notify_XInputKey(pad, key, down);        
    }
    void ControlGlobal::Process_XInput()
    {
        DWORD dwResult;
        int64_t curr_time = int64_t(m_timer.Time());
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
        {
            if (curr_time - m_last_failed_time[i] < cXInputCheckDelay) continue;

            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));
            dwResult = XInputGetState(i, &state);
            if (dwResult == ERROR_SUCCESS)
            {              
                if (state.dwPacketNumber != m_last_xinput[i].dwPacketNumber) {
                    auto StickToFloat = [](SHORT x) {
                        return x < 0 ? float(x) / 32768.0f : float(x) / 32767.0f;
                    };
                    auto TriggerToFloat = [](BYTE b) {
                        return float(b) / 255.0f;
                    };
                    auto KeyChanged = [](WORD btn_old, WORD btn_new, WORD mask, bool& down)->bool {
                        WORD oldm = btn_old & mask;
                        WORD newm = btn_new & mask;
                        if (oldm == newm) return false;
                        down = newm;
                        return true;
                    };

                    if (m_last_xinput[i].Gamepad.bLeftTrigger != state.Gamepad.bLeftTrigger) {
                        Process_XInputStick(i, RA::XInputStick::LT, TriggerToFloat(state.Gamepad.bLeftTrigger));                            
                        bool old_up = TriggerToFloat(m_last_xinput[i].Gamepad.bLeftTrigger) > cDeadZoneTolerance;
                        bool new_up = TriggerToFloat(state.Gamepad.bLeftTrigger) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::LT, new_up);
                    }
                    if (m_last_xinput[i].Gamepad.bRightTrigger != state.Gamepad.bRightTrigger) {
                        Process_XInputStick(i, RA::XInputStick::RT, TriggerToFloat(state.Gamepad.bRightTrigger));
                        bool old_up = TriggerToFloat(m_last_xinput[i].Gamepad.bRightTrigger) > cDeadZoneTolerance;
                        bool new_up = TriggerToFloat(state.Gamepad.bRightTrigger) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::RT, new_up);
                    }
                    if (m_last_xinput[i].Gamepad.sThumbLX != state.Gamepad.sThumbLX) {
                        Process_XInputStick(i, RA::XInputStick::LX, StickToFloat(state.Gamepad.sThumbLX));
                        bool old_up = StickToFloat(m_last_xinput[i].Gamepad.sThumbLX) > cDeadZoneTolerance;
                        bool new_up = StickToFloat(state.Gamepad.sThumbLX) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::LStickMoveRight, new_up);
                        bool old_down = StickToFloat(m_last_xinput[i].Gamepad.sThumbLX) < -cDeadZoneTolerance;
                        bool new_down = StickToFloat(state.Gamepad.sThumbLX) < -cDeadZoneTolerance;
                        if (old_down != new_down) Process_XInputKey(i, RA::XInputKey::LStickMoveLeft, new_down);
                    }
                    if (m_last_xinput[i].Gamepad.sThumbLY != state.Gamepad.sThumbLY)
                    {
                        Process_XInputStick(i, RA::XInputStick::LY, StickToFloat(state.Gamepad.sThumbLY));
                        bool old_up = StickToFloat(m_last_xinput[i].Gamepad.sThumbLY) > cDeadZoneTolerance;
                        bool new_up = StickToFloat(state.Gamepad.sThumbLY) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::LStickMoveUp, new_up);
                        bool old_down = StickToFloat(m_last_xinput[i].Gamepad.sThumbLY) < -cDeadZoneTolerance;
                        bool new_down = StickToFloat(state.Gamepad.sThumbLY) < -cDeadZoneTolerance;
                        if (old_down != new_down) Process_XInputKey(i, RA::XInputKey::LStickMoveDown, new_down);
                    }
                    if (m_last_xinput[i].Gamepad.sThumbRX != state.Gamepad.sThumbRX) {
                        Process_XInputStick(i, RA::XInputStick::RX, StickToFloat(state.Gamepad.sThumbRX));
                        bool old_up = StickToFloat(m_last_xinput[i].Gamepad.sThumbRX) > cDeadZoneTolerance;
                        bool new_up = StickToFloat(state.Gamepad.sThumbRX) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::RStickMoveRight, new_up);
                        bool old_down = StickToFloat(m_last_xinput[i].Gamepad.sThumbRX) < -cDeadZoneTolerance;
                        bool new_down = StickToFloat(state.Gamepad.sThumbRX) < -cDeadZoneTolerance;
                        if (old_down != new_down) Process_XInputKey(i, RA::XInputKey::RStickMoveLeft, new_down);
                    }
                    if (m_last_xinput[i].Gamepad.sThumbRY != state.Gamepad.sThumbRY) {
                        Process_XInputStick(i, RA::XInputStick::RY, StickToFloat(state.Gamepad.sThumbRY));
                        bool old_up = StickToFloat(m_last_xinput[i].Gamepad.sThumbRY) > cDeadZoneTolerance;
                        bool new_up = StickToFloat(state.Gamepad.sThumbRY) > cDeadZoneTolerance;
                        if (old_up != new_up) Process_XInputKey(i, RA::XInputKey::RStickMoveUp, new_up);
                        bool old_down = StickToFloat(m_last_xinput[i].Gamepad.sThumbRY) < -cDeadZoneTolerance;
                        bool new_down = StickToFloat(state.Gamepad.sThumbRY) < -cDeadZoneTolerance;
                        if (old_down != new_down) Process_XInputKey(i, RA::XInputKey::RStickMoveDown, new_down);
                    }

                    bool down;
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_UP, down))
                        Process_XInputKey(i, RA::XInputKey::Up, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_DOWN, down))
                        Process_XInputKey(i, RA::XInputKey::Down, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_LEFT, down))
                        Process_XInputKey(i, RA::XInputKey::Left, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_DPAD_RIGHT, down))
                        Process_XInputKey(i, RA::XInputKey::Right, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_START, down))
                        Process_XInputKey(i, RA::XInputKey::Start, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_BACK, down))
                        Process_XInputKey(i, RA::XInputKey::Back, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_A, down))
                        Process_XInputKey(i, RA::XInputKey::A, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_B, down))
                        Process_XInputKey(i, RA::XInputKey::B, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_X, down))
                        Process_XInputKey(i, RA::XInputKey::X, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_Y, down))
                        Process_XInputKey(i, RA::XInputKey::Y, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_THUMB, down))
                        Process_XInputKey(i, RA::XInputKey::LStick, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_THUMB, down))
                        Process_XInputKey(i, RA::XInputKey::RStick, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, down))
                        Process_XInputKey(i, RA::XInputKey::LB, down);
                    if (KeyChanged(m_last_xinput[i].Gamepad.wButtons, state.Gamepad.wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, down))
                        Process_XInputKey(i, RA::XInputKey::RB, down);
                }
                m_last_xinput[i] = state;                
            }
            else
            {
                m_last_failed_time[i] = curr_time;
            }
        }
    }
    int ControlGlobal::XInputPadsCount() const
    {
        return XUSER_MAX_COUNT;
    }
    bool ControlGlobal::Is_XInputKeyDown(int pad, XInputKey key) const
    {
        return m_xinput_key_down[pad][int(key)];
    }
    float ControlGlobal::XInputStickPos(int pad, XInputStick stick) const
    {
        return m_xinput_stick_pos[pad][int(stick)];
    }
    void GridAlign_FixedSize(
        const std::vector<Control*>& controls, 
        const glm::AABR& item_box, 
        const glm::vec2& min_step,
        float preferred_y_step,
        bool force_fit_y,
        glm::vec2& grid_step,
        glm::vec2& grid_size)
    {
        int num_controls = int(controls.size());
        if (num_controls == 0) {
            grid_size = { 0,0 };
            grid_step = { 0,0 };
            return;
        }
        glm::vec2 box_size = item_box.Size();
        box_size = glm::max(box_size, glm::vec2(0));
        int num_col = int(glm::floor((box_size.x / min_step.x)) + 1);
        int num_row = (num_controls + num_col - 1) / num_col;
        grid_step.x = (num_col == 1) ? 0 : box_size.x / float(num_col - 1);
        if (num_row == 1)
            grid_step.y = 0;
        else
            grid_step.y = box_size.y / float(num_row - 1);
        if (!force_fit_y) {
            grid_step.y = glm::max(grid_step.y, min_step.y);
        }
        grid_step.y = glm::min(grid_step.y, preferred_y_step);
        grid_size = grid_step * glm::vec2(num_col - 1, num_row - 1);
        int n = 0;
        glm::vec2 start_xy = item_box.min;
        if (num_col == 1) start_xy.x = item_box.Center().x * 0.5f;
        for (int y = 0; y < num_row; y++) {
            for (int x = 0; x < num_col; x++) {
                controls[n]->SetPos(item_box.min + grid_step * glm::vec2(x, y));
                n++;
                if (n == num_controls) return;
            }
        }
    }
}