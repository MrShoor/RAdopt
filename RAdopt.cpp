#include "pch.h"
#include "RAdopt.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <d3dcompiler.h>
#include "DX11TypeConverter.h"

namespace RA {
    uint32_t MurmurHash2(const void* key, int len, uint32_t seed)
    {
        const uint32_t m = 0x5bd1e995;
        const int r = 24;
        uint32_t h = seed ^ len;
        const unsigned char* data = (const unsigned char*)key;
        while (len >= 4)
        {
            uint32_t k = *(uint32_t*)data;
            k *= m;
            k ^= k >> r;
            k *= m;
            h *= m;
            h ^= k;
            data += 4;
            len -= 4;
        }
        switch (len)
        {
        [[fallthrough]];
        case 3: h ^= data[2] << 16;
        [[fallthrough]];
        case 2:  h ^= data[1] << 8;
        [[fallthrough]];
        case 1:  h ^= data[0];
            h *= m;
        };
        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;
        return h;
    }

    int PixelsSize(TextureFmt fmt)
    {
        switch (fmt) {
        case TextureFmt::R8: return 1;
        case TextureFmt::RG8: return 2;
        case TextureFmt::RGBA8: return 4;
        case TextureFmt::R16: return 2;
        case TextureFmt::RG16: return 4;
        case TextureFmt::RGBA16: return 8;
        case TextureFmt::R16f: return 2;
        case TextureFmt::RG16f: return 4;
        case TextureFmt::RGBA16f: return 8;
        case TextureFmt::R32: return 4;
        case TextureFmt::RG32: return 8;
        case TextureFmt::RGB32: return 12;
        case TextureFmt::RGBA32: return 16;
        case TextureFmt::R32f: return 4;
        case TextureFmt::RG32f: return 8;
        case TextureFmt::RGB32f: return 12;
        case TextureFmt::RGBA32f: return 16;
        case TextureFmt::D16: return 2;
        case TextureFmt::D24_S8: return 4;
        case TextureFmt::D32f: return 4;
        case TextureFmt::D32f_S8: return 8;
        default:
            return 0;
        }
    }

    std::filesystem::path ExePath()
    {
        std::vector<wchar_t> pathBuf;
        DWORD copied = 0;
        do {
            pathBuf.resize(pathBuf.size() + MAX_PATH);
            copied = GetModuleFileNameW(0, &pathBuf.at(0), DWORD(pathBuf.size()));
        } while (copied >= pathBuf.size());
        pathBuf.resize(copied);
        return std::filesystem::path(pathBuf.begin(), pathBuf.end()).parent_path();
    }

    class LayoutBuilder : public LayoutBuilderIntf {
    private:
        struct hash_fn {
            std::size_t operator() (const Layout& l) const {
                return l.hash();
            }
        };
    private:
        std::unordered_map<Layout, std::unique_ptr<Layout>, hash_fn> m_cache;
        Layout m_curr_layout;
    public:
        LayoutBuilderIntf* Reset() {
            m_curr_layout.fields.clear();
            m_curr_layout.stride = 0;
            return this;
        };
        LayoutBuilderIntf* Add(const std::string& name, LayoutType type, int num_fields, bool do_norm = true, int offset = -1, int array_size = 1) override {
            LayoutField f;
            f.name = name;
            f.type = type;
            f.num_fields = num_fields;
            f.do_norm = do_norm;
            f.offset = (offset < 0) ? m_curr_layout.stride : offset;
            f.array_size = array_size;
            m_curr_layout.fields.push_back(f);
            m_curr_layout.stride += f.Size();
            return this;
        }
        const Layout* Finish(int stride_size = -1) override {
            if (stride_size > 0) m_curr_layout.stride = stride_size;
            auto it = m_cache.find(m_curr_layout);
            if (it == m_cache.end()) {
                m_cache.insert({ m_curr_layout, std::make_unique<Layout>(m_curr_layout) });
                it = m_cache.find(m_curr_layout);
            }
            return it->second.get();
        }
    };

    class FrameBufferBuilder : public FrameBufferBuilderIntf {
    private:
        FrameBufferPtr m_new_fbo;
        int slot;
    public:
        FrameBufferBuilderIntf* Reset(const DevicePtr& dev) override {
            m_new_fbo = std::make_shared<FrameBuffer>(dev);
            slot = 0;
            return this;
        }
        FrameBufferBuilderIntf* Color(TextureFmt fmt) override {
            Texture2DPtr new_tex = std::make_shared<Texture2D>(m_new_fbo->GetDevice());
            new_tex->SetState(fmt);
            m_new_fbo->SetSlot(slot, new_tex);
            slot++;
            return this;
        }
        FrameBufferBuilderIntf* Depth(TextureFmt fmt) override {
            Texture2DPtr new_tex = std::make_shared<Texture2D>(m_new_fbo->GetDevice());
            new_tex->SetState(fmt);
            m_new_fbo->SetDS(new_tex);
            return this;
        };
        FrameBufferPtr Finish() override {
            slot = 0;
            return std::move(m_new_fbo);
        }
        FrameBufferBuilder() : slot(0) {};
    };

    int LayoutField::Size() const
    {
        switch (type) {
        case LayoutType::Byte: return 1 * num_fields * array_size;
        case LayoutType::Word: return 2 * num_fields * array_size;
        case LayoutType::UInt: return 4 * num_fields * array_size;
        case LayoutType::Float: return 4 * num_fields * array_size;
        default:
            assert(false);
        }
        return 0;
    }
    bool LayoutField::operator==(const LayoutField& l) const
    {
        return
            (name == l.name) &&
            (type == l.type) &&
            (num_fields == l.num_fields) &&
            (do_norm == l.do_norm) &&
            (offset == l.offset) &&
            (array_size == l.array_size);
    }
    std::size_t LayoutField::hash() const
    {
        return
            std::hash<std::string>()(name) ^
            std::hash<int>()(num_fields) ^
            std::hash<bool>()(do_norm) ^
            std::hash<int>()(offset) ^
            std::hash<int>()(array_size) ^
            std::hash<LayoutType>()(type);
    }
    bool Layout::operator==(const Layout& l) const
    {
        if (fields.size() != l.fields.size()) return false;
        if (stride != l.stride) return false;
        for (size_t i = 0; i < fields.size(); i++) {
            if (!(fields[i] == l.fields[i])) return false;
        }
        return true;
    }
    std::size_t Layout::hash() const
    {
        std::size_t n = std::hash<int>()(stride);
        for (size_t i = 0; i < fields.size(); i++) {
            n ^= fields[i].hash();
        }
        return n;
    }

    LayoutBuilder gvLB;
    LayoutBuilderIntf* LB()
    {
        return gvLB.Reset();
    }

    FrameBufferBuilder gvFBB;
    FrameBufferBuilderIntf* FBB(const DevicePtr& dev)
    {
        return gvFBB.Reset(dev);
    }

    void States::SetDefaultStates()
    {
        m_r_desc.FillMode = D3D11_FILL_SOLID;
        m_r_desc.CullMode = D3D11_CULL_BACK;
        m_r_desc.FrontCounterClockwise = true;
        m_r_desc.DepthBias = 0;
        m_r_desc.DepthBiasClamp = 0;
        m_r_desc.SlopeScaledDepthBias = 0;
        m_r_desc.DepthClipEnable = true;
        m_r_desc.ScissorEnable = false;
        m_r_desc.MultisampleEnable = false;
        m_r_desc.AntialiasedLineEnable = false;
        m_r_state = nullptr;

        m_stencil_ref = 0xff;
        m_d_desc.DepthEnable = false;
        m_d_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        m_d_desc.DepthFunc = D3D11_COMPARISON_GREATER;
        m_d_desc.StencilEnable = false;
        m_d_desc.StencilReadMask = 0xff;
        m_d_desc.StencilWriteMask = 0xff;
        m_d_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
        m_d_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        m_d_desc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
        m_d_state = nullptr;

        m_b_desc.AlphaToCoverageEnable = false;
        m_b_desc.IndependentBlendEnable = true;
        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            m_b_desc.RenderTarget[i].BlendEnable = false;
            m_b_desc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
            m_b_desc.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
            m_b_desc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
            m_b_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            m_b_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            m_b_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            m_b_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        m_b_state = nullptr;
    }
    void States::Push()
    {
        m_states.emplace_back(this);
        ComPtr<ID3D11RasterizerState> m_r_state = nullptr;
        ComPtr<ID3D11DepthStencilState> m_d_state = nullptr;
        ComPtr<ID3D11BlendState> m_b_state = nullptr;
    }
    void States::Pop()
    {
        const auto& last = m_states.back();
        m_r_desc = last.m_r_desc;
        m_d_desc = last.m_d_desc;
        m_b_desc = last.m_b_desc;
        m_r_state = last.m_r_state;
        m_d_state = last.m_d_state;
        m_b_state = last.m_b_state;
        m_stencil_ref = last.m_stencil_ref;
        if (m_r_state)
            m_deviceContext->RSSetState(m_r_state.Get());
        if (m_d_state)
            m_deviceContext->OMSetDepthStencilState(m_d_state.Get(), m_stencil_ref);
        if (m_b_state)
            m_deviceContext->OMSetBlendState(m_b_state.Get(), nullptr, 0xffffffff);
    }
    void States::ValidateStates()
    {
        if (!m_r_state) {
            CheckD3DErr(m_device->CreateRasterizerState(&m_r_desc, &m_r_state));
            m_deviceContext->RSSetState(m_r_state.Get());
        }
        if (!m_d_state) {
            CheckD3DErr(m_device->CreateDepthStencilState(&m_d_desc, &m_d_state));
            m_deviceContext->OMSetDepthStencilState(m_d_state.Get(), m_stencil_ref);
        }
        if (!m_b_state) {
            CheckD3DErr(m_device->CreateBlendState(&m_b_desc, &m_b_state));
            m_deviceContext->OMSetBlendState(m_b_state.Get(), nullptr, 0xffffffff);
        }
    }
    void States::SetWireframe(bool wire)
    {
        if ((m_r_desc.FillMode == D3D11_FILL_WIREFRAME) != wire) {
            m_r_desc.FillMode = wire ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
            m_r_state = nullptr;
        }
    }
    void States::SetCull(CullMode cm)
    {
        D3D11_CULL_MODE dx_cm = D3D11_CULL_NONE;
        switch (cm) {
        case CullMode::none: dx_cm = D3D11_CULL_NONE; break;
        case CullMode::back: dx_cm = D3D11_CULL_BACK; break;
        case CullMode::front: dx_cm = D3D11_CULL_FRONT; break;
        }
        if (m_r_desc.CullMode != dx_cm) {
            m_r_desc.CullMode = dx_cm;
            m_r_state = nullptr;
        }
    }
    void States::SetDepthEnable(bool enable)
    {
        if (bool(m_d_desc.DepthEnable) != enable) {
            m_d_desc.DepthEnable = enable;
            m_d_state = nullptr;
        }
    }
    void States::SetDepthWrite(bool enable)
    {
        if ((m_d_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL) != enable) {
            m_d_desc.DepthWriteMask = enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            m_d_state = nullptr;
        }
    }
    void States::SetDepthFunc(Compare cmp)
    {
        D3D11_COMPARISON_FUNC dx_cmp = ToDX(cmp);
        if (m_d_desc.DepthFunc != dx_cmp) {
            m_d_desc.DepthFunc = dx_cmp;
            m_d_state = nullptr;
        }
    }
    void States::SetBlend(bool enable, Blend src, Blend dst, int rt_index, BlendFunc bf) 
    {
        SetBlendSeparateAlpha(enable, src, dst, bf, src, dst, bf, rt_index);
    }
    void States::SetBlendSeparateAlpha(bool enable, Blend src_color, Blend dst_color, BlendFunc bf_color,
        Blend src_alpha, Blend dst_alpha, BlendFunc bf_alpha, int rt_index) 
    {
        if (m_b_desc.IndependentBlendEnable != (rt_index >= 0)) {
            m_b_desc.IndependentBlendEnable = (rt_index >= 0);
            m_b_state = nullptr;
        }

        D3D11_BLEND dx_src_blend = enable ? ToDX(src_color) : D3D11_BLEND_ONE;
        D3D11_BLEND dx_dst_blend = enable ? ToDX(dst_color) : D3D11_BLEND_ONE;
        D3D11_BLEND_OP dx_blend_op = enable ? ToDX(bf_color) : D3D11_BLEND_OP_ADD;
        D3D11_BLEND dx_src_alpha_blend = enable ? ToDX(src_alpha) : D3D11_BLEND_ONE;
        D3D11_BLEND dx_dst_alpha_blend = enable ? ToDX(dst_alpha) : D3D11_BLEND_ONE;
        D3D11_BLEND_OP dx_blend_alpha_op = enable ? ToDX(bf_alpha) : D3D11_BLEND_OP_ADD;

        int n = (rt_index < 0) ? 0 : rt_index;
        if (bool(m_b_desc.RenderTarget[n].BlendEnable) != enable) {
            m_b_desc.RenderTarget[n].BlendEnable = enable;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].SrcBlend != dx_src_blend) {
            m_b_desc.RenderTarget[n].SrcBlend = dx_src_blend;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].DestBlend != dx_dst_blend) {
            m_b_desc.RenderTarget[n].DestBlend = dx_dst_blend;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].BlendOp != dx_blend_op) {
            m_b_desc.RenderTarget[n].BlendOp = dx_blend_op;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].SrcBlendAlpha != dx_src_alpha_blend) {
            m_b_desc.RenderTarget[n].SrcBlendAlpha = dx_src_alpha_blend;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].DestBlendAlpha != dx_dst_alpha_blend) {
            m_b_desc.RenderTarget[n].DestBlendAlpha = dx_dst_alpha_blend;
            m_b_state = nullptr;
        }
        if (m_b_desc.RenderTarget[n].BlendOpAlpha != dx_blend_alpha_op) {
            m_b_desc.RenderTarget[n].BlendOpAlpha = dx_blend_alpha_op;
            m_b_state = nullptr;
        }

        if (rt_index < 0) {
            for (int i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                m_b_desc.RenderTarget[i] = m_b_desc.RenderTarget[0];
        }
    }
    void States::SetColorWrite(bool enable, int rt_index)
    {
        if (m_b_desc.IndependentBlendEnable != (rt_index >= 0)) {
            m_b_desc.IndependentBlendEnable = (rt_index >= 0);
            m_b_state = nullptr;
        }
        int n = (rt_index < 0) ? 0 : rt_index;
        if (bool(m_b_desc.RenderTarget[n].RenderTargetWriteMask) != enable) {
            m_b_desc.RenderTarget[n].RenderTargetWriteMask = enable ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
            m_b_state = nullptr;
        }
        if (rt_index < 0) {
            for (int i = 1; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                m_b_desc.RenderTarget[i] = m_b_desc.RenderTarget[0];
        }
    }
    States::States(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        m_device = device;
        m_deviceContext = device_context;
        SetDefaultStates();
    }
    ID3D11SamplerState* Device::ObtainSampler(const Sampler& s)
    {
        auto it = m_samplers.find(s);
        if (it == m_samplers.end()) {
            D3D11_SAMPLER_DESC desc;

            bool iscomparison = s.comparison != Compare::never;

            if (s.anisotropy > 1) {
                desc.Filter = iscomparison ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;
            }
            else {
                if (iscomparison) {
                    if (s.filter == TexFilter::linear) {
                        if (s.mipfilter == TexFilter::linear) {
                            desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
                        }
                        else {
                            desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
                        }
                    }
                    else
                    {
                        if (s.mipfilter == TexFilter::linear) {
                            desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
                        }
                        else {
                            desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
                        }
                    }
                }
                else {
                    if (s.filter == TexFilter::linear) {
                        if (s.mipfilter == TexFilter::linear) {
                            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                        }
                        else {
                            desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                        }
                    }
                    else
                    {
                        if (s.mipfilter == TexFilter::linear) {
                            desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                        }
                        else {
                            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                        }
                    }
                }
            }
            desc.AddressU = ToDX(s.wrap_x);
            desc.AddressV = ToDX(s.wrap_x);
            desc.AddressW = ToDX(s.wrap_x);
            desc.MipLODBias = 0;
            desc.MaxAnisotropy = s.anisotropy;
            desc.ComparisonFunc = ToDX(s.comparison);
            desc.BorderColor[0] = s.border.x;
            desc.BorderColor[1] = s.border.y;
            desc.BorderColor[2] = s.border.z;
            desc.BorderColor[3] = s.border.w;
            desc.MinLOD = 0;
            desc.MaxLOD = s.mipfilter == TexFilter::none ? 0 : D3D11_FLOAT32_MAX;

            ComPtr<ID3D11SamplerState> sampler;
            CheckD3DErr(m_device->CreateSamplerState(&desc, &sampler));
            m_samplers.insert({s, sampler});
            return sampler.Get();
        }
        return it->second.Get();
    }
    void Device::SetDefaultFBO()
    {
        ID3D11RenderTargetView* tmp = m_RTView.Get();
        m_deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &tmp, nullptr, 0, 0, nullptr, nullptr);
    }
    void Device::SetViewport(const glm::vec2& size)
    {
        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = size.x;
        vp.Height = size.y;
        vp.MinDepth = 0.0;
        vp.MaxDepth = 1.0;
        m_deviceContext->RSSetViewports(1, &vp);
    }
    Device::Device(HWND wnd)
    {
        m_active_program = nullptr;

        m_wnd = wnd;
        RECT rct;
        GetClientRect(wnd, &rct);

        m_last_wnd_size = glm::ivec2(rct.right - rct.left, rct.bottom - rct.top);

        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 1;
        sd.BufferDesc.Width = m_last_wnd_size.x;
        sd.BufferDesc.Height = m_last_wnd_size.y;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = wnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        CheckD3DErr(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &sd,
            &m_swapChain,
            &m_device,
            nullptr,
            &m_deviceContext)
        );

        CheckD3DErr(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &m_backBuffer));
        CheckD3DErr(m_device->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_RTView));

        m_states = std::make_unique<RA::States>(m_device.Get(), m_deviceContext.Get());
    }
    HWND Device::Window() const
    {
        return m_wnd;
    }
    FrameBufferPtr Device::SetFrameBuffer(const FrameBufferPtr& fbo, bool update_viewport)
    {
        FrameBufferPtr prev_fbo = m_active_fbo.lock();
        std::weak_ptr<FrameBuffer> m_new_fbo = fbo;
        if (m_active_fbo_ptr != fbo.get()) {
            m_active_fbo = m_new_fbo;
            m_active_fbo_ptr = fbo.get();
            if (m_active_fbo_ptr) {
                m_active_fbo_ptr->PrepareViews();
                m_deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
                    UINT(m_active_fbo_ptr->m_rtv_count),
                    m_active_fbo_ptr->m_colors_to_bind.data(),
                    m_active_fbo_ptr->m_depth_view.Get(),
                    UINT(m_active_fbo_ptr->m_rtv_count),
                    UINT(glm::clamp<int>(int(m_active_fbo_ptr->m_uav_to_bind.size()) - m_active_fbo_ptr->m_rtv_count, 0, 7)),
                    m_active_fbo_ptr->m_uav_to_bind.data(),
                    m_active_fbo_ptr->m_uav_initial_counts.data()
                    );
            }
            else {
                SetDefaultFBO();
            }
        }
        if (update_viewport) {
            SetViewport(CurrentFrameBufferSize());
        }

        return prev_fbo;
    }
    glm::ivec2 Device::CurrentFrameBufferSize() const
    {
        if (m_active_fbo_ptr) {
            return m_active_fbo_ptr->GetSize();
        }
        else {
            RECT rct;
            GetClientRect(m_wnd, &rct);
            return glm::ivec2(rct.right - rct.left, rct.bottom - rct.top);
        }
    }
    States* Device::States()
    {
        return m_states.get();
    }
    FrameBufferPtr Device::ActiveFrameBuffer() const
    {
        return m_active_fbo.lock();
    }
    Program* Device::ActiveProgram()
    {
        return m_active_program;
    }
    FrameBufferPtr Device::Create_FrameBuffer() {
        return std::make_shared<FrameBuffer>(shared_from_this());
    }
    Texture2DPtr Device::Create_Texture2D() {
        return std::make_shared<Texture2D>(shared_from_this());
    }
    Texture3DPtr Device::Create_Texture3D()
    {
        return std::make_shared<Texture3D>(shared_from_this());
    }
    VertexBufferPtr Device::Create_VertexBuffer() {
        return std::make_shared<VertexBuffer>(shared_from_this());
    }
    StructuredBufferPtr Device::Create_StructuredBuffer() {
        return std::make_shared<StructuredBuffer>(shared_from_this());
    }
    IndexBufferPtr Device::Create_IndexBuffer() {
        return std::make_shared<IndexBuffer>(shared_from_this());
    }
    ProgramPtr Device::Create_Program() {
        return std::make_shared<Program>(shared_from_this());
    }
    UniformBufferPtr Device::Create_UniformBuffer() {
        return std::make_shared<UniformBuffer>(shared_from_this());
    }

    void Device::BeginFrame()
    {
        RECT rct;
        GetClientRect(m_wnd, &rct);
        glm::ivec2 new_wnd_size = glm::ivec2(rct.right - rct.left, rct.bottom - rct.top);
        if (m_last_wnd_size != new_wnd_size) {
            m_last_wnd_size = new_wnd_size;

            ID3D11RenderTargetView* tmp = nullptr;
            m_deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &tmp, nullptr, 0, 0, nullptr, nullptr);
            m_RTView = nullptr;
            m_backBuffer = nullptr;
            CheckD3DErr(m_swapChain->ResizeBuffers(1, m_last_wnd_size.x, m_last_wnd_size.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
            CheckD3DErr(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &m_backBuffer));
            CheckD3DErr(m_device->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_RTView));
        }
        m_active_fbo = std::shared_ptr<FrameBuffer>(nullptr);
        SetDefaultFBO();

        const float rgba[4] = { 0,0,0,0 };
        m_deviceContext->ClearRenderTargetView(m_RTView.Get(), rgba);
    }
    void Device::PresentToWnd()
    {
        SetFrameBuffer(nullptr);
        m_swapChain->Present(0, 0);
        m_active_program = nullptr;
    }
    DevChild::DevChild(const DevicePtr& device) : m_device(device)
    {
    }
    DevicePtr DevChild::GetDevice()
    {
        return m_device;
    }
    ComPtr<ID3D11RenderTargetView> Texture2D::BuildTargetView(int mip, int slice_start, int slice_count) const
    {
        if (!m_handle) return nullptr;
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Format = ToDXGI_SRVFmt(m_fmt);
        desc.Texture2DArray.MipSlice = mip;
        desc.Texture2DArray.FirstArraySlice = slice_start;
        desc.Texture2DArray.ArraySize = slice_count;
        ComPtr<ID3D11RenderTargetView> res;
        CheckD3DErr(m_device->m_device->CreateRenderTargetView(m_handle.Get(), &desc, &res));
        return res;
    }
    ComPtr<ID3D11DepthStencilView> Texture2D::BuildDepthStencilView(int mip, int slice_start, int slice_count, bool read_only) const
    {
        if (!m_handle) return nullptr;
        ComPtr<ID3D11DepthStencilView> res;
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        switch (m_fmt) {
        case TextureFmt::D16: desc.Format = DXGI_FORMAT_D16_UNORM; break;
        case TextureFmt::D24_S8: desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
        case TextureFmt::D32f: desc.Format = DXGI_FORMAT_D32_FLOAT; break;
        case TextureFmt::D32f_S8: desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; break;
        default:
            throw std::runtime_error("unsupported depth texture format");
        }        
        desc.Texture2DArray.MipSlice = mip;
        desc.Texture2DArray.FirstArraySlice = slice_start;
        desc.Texture2DArray.ArraySize = slice_count;
        desc.Flags = read_only ? (D3D11_DSV_READ_ONLY_DEPTH) : 0;
        CheckD3DErr(m_device->m_device->CreateDepthStencilView(m_handle.Get(), &desc, &res));
        return res;
    }
    ComPtr<ID3D11ShaderResourceView> Texture2D::GetShaderResourceView(bool as_array, bool as_cubemap)
    {
        as_array = as_array || (m_slices > 1);
        int srv_idx = (as_array ? 1 : 0) | (as_cubemap ? 2 : 0);
        if (!m_srv[srv_idx]) {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc;
            desc.Format = ToDXGI_SRVFmt(m_fmt);
            if (as_array) {
                if (as_cubemap) {
                    desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBEARRAY;
                    desc.TextureCubeArray.MipLevels = m_mips_count;
                    desc.TextureCubeArray.MostDetailedMip = 0;
                    desc.TextureCubeArray.First2DArrayFace = 0;
                    desc.TextureCubeArray.NumCubes = m_slices / 6;
                }
                else {
                    desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2DARRAY;
                    desc.Texture2DArray.MipLevels = m_mips_count;
                    desc.Texture2DArray.MostDetailedMip = 0;
                    desc.Texture2DArray.ArraySize = m_slices;
                    desc.Texture2DArray.FirstArraySlice = 0;
                }
            }
            else {
                if (as_cubemap) {
                    desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURECUBE;
                    desc.TextureCube.MipLevels = m_mips_count;
                    desc.TextureCube.MostDetailedMip = 0;
                }
                else {
                    desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
                    desc.Texture2D.MipLevels = m_mips_count;
                    desc.Texture2D.MostDetailedMip = 0;
                }
            }
            CheckD3DErr( m_device->m_device->CreateShaderResourceView(m_handle.Get(), &desc, &m_srv[srv_idx]) );
        }
        return m_srv[srv_idx];
    }
    ComPtr<ID3D11UnorderedAccessView> Texture2D::GetUnorderedAccessView(int mip, int slice_start, int slice_count, bool as_array)
    {
        glm::ivec3 key(mip, slice_start, slice_count);
        auto it = m_uav.find(key);
        if (it == m_uav.end()) {
            ComPtr<ID3D11UnorderedAccessView> new_view;
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
            desc.Format = ToDXGI_Fmt(m_fmt);
            bool is_array = (slice_count > 1) || as_array;
            desc.ViewDimension = is_array ? D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D;
            if (is_array) {
                desc.Texture2DArray.MipSlice = mip;
                desc.Texture2DArray.FirstArraySlice = slice_start;
                desc.Texture2DArray.ArraySize = slice_count;
            }
            else {
                desc.Texture2D.MipSlice = mip;
            }               
            CheckD3DErr( m_device->m_device->CreateUnorderedAccessView(m_handle.Get(), &desc, &new_view) );
            m_uav.insert({ key, new_view });
            return new_view;
        }
        return it->second;
    }
    TextureFmt Texture2D::Format() const
    {
        return m_fmt;
    }
    glm::ivec2 Texture2D::Size() const
    {
        return m_size;
    }
    int Texture2D::SlicesCount() const
    {
        return m_slices;
    }
    int Texture2D::MipsCount() const
    {
        return m_mips_count;
    }
    void Texture2D::SetState(TextureFmt fmt)
    {
        m_fmt = fmt;
        m_size = glm::ivec2(0, 0);
        m_slices = 0;
        m_mips_count = 0;
        m_handle = nullptr;
        m_srv[0] = nullptr;
        m_srv[1] = nullptr;
        m_srv[2] = nullptr;
        m_srv[3] = nullptr;
    }
    void Texture2D::SetState(TextureFmt fmt, glm::ivec2 size, int mip_levels, int slices, const void* data)
    {
        m_fmt = fmt;
        m_size = size;
        m_slices = slices;
        m_mips_count = glm::clamp(mip_levels, 1, MipLevelsCount(size.x, size.y));

        D3D11_TEXTURE2D_DESC desc;        
        desc.Width = m_size.x;
        desc.Height = m_size.y;
        desc.MipLevels = m_mips_count;
        desc.ArraySize = m_slices;
        desc.Format = ToDXGI_Fmt(m_fmt);
        desc.SampleDesc = {1, 0};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = 0;
        if (CanBeShaderRes(m_fmt)) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        if (CanBeRenderTarget(m_fmt)) desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        if (CanBeDepthTarget(m_fmt)) desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        if (CanBeUAV(m_fmt)) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        if ((m_slices % 6 == 0) && CanBeShaderRes(m_fmt)) desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        if ((m_mips_count > 0) && CanBeRenderTarget(m_fmt)) desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

        if (data) {
            D3D11_SUBRESOURCE_DATA d3ddata;
            d3ddata.pSysMem = data;
            d3ddata.SysMemPitch = PixelsSize(m_fmt) * m_size.x;
            d3ddata.SysMemSlicePitch = d3ddata.SysMemPitch * m_size.y;
            CheckD3DErr(m_device->m_device->CreateTexture2D(&desc, &d3ddata, &m_handle));
        }
        else {
            CheckD3DErr(m_device->m_device->CreateTexture2D(&desc, nullptr, &m_handle));
        }

        m_srv[0] = nullptr;
        m_srv[1] = nullptr;
        m_srv[2] = nullptr;
        m_srv[3] = nullptr;
        m_uav.clear();
    }
    void Texture2D::SetSubData(const glm::ivec2& offset, const glm::ivec2& size, int slice, int mip, const void* data)
    {
        assert(m_handle);

        UINT res_idx = D3D11CalcSubresource(mip, slice, m_mips_count);
        D3D11_BOX box;
        box.left = offset.x;
        box.top = offset.y;
        box.right = offset.x + size.x;
        box.bottom = offset.y + size.y;
        box.front = 0;
        box.back = 1;
        m_device->m_deviceContext->UpdateSubresource(m_handle.Get(), res_idx, &box, data, PixelsSize(m_fmt) * size.x, PixelsSize(m_fmt) * size.x * size.y);
    }
    void Texture2D::GenerateMips()
    {
        m_device->m_deviceContext->GenerateMips(GetShaderResourceView(false, false).Get());
    }
    void Texture2D::ReadBack(void* data, int mip, int array_slice)
    {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = m_size.x >> mip;
        desc.Height = m_size.y >> mip;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = ToDXGI_Fmt(m_fmt);
        desc.SampleDesc = { 1, 0 };
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ComPtr<ID3D11Texture2D> tmp_tex;
        CheckD3DErr(m_device->m_device->CreateTexture2D(&desc, nullptr, &tmp_tex));

        D3D11_BOX src_box;
        src_box.left = 0;
        src_box.top = 0;
        src_box.right = desc.Width;
        src_box.bottom = desc.Height;
        src_box.front = 0;
        src_box.back = 1;
        m_device->m_deviceContext->CopySubresourceRegion(
            tmp_tex.Get(), 0, 0, 0, 0,
            m_handle.Get(), D3D11CalcSubresource(mip, array_slice, m_mips_count), &src_box);

        D3D11_MAPPED_SUBRESOURCE map;
        CheckD3DErr(m_device->m_deviceContext->Map(tmp_tex.Get(), 0, D3D11_MAP_READ, 0, &map));
        int row_size = desc.Width * PixelsSize(m_fmt);
        for (size_t y = 0; y < desc.Height; y++) {
            memcpy(
                &((char*)data)[y * row_size],
                &((char*)map.pData)[y * map.RowPitch],
                row_size
            );
        }
        m_device->m_deviceContext->Unmap(tmp_tex.Get(), 0);
    }
    Texture2D::Texture2D(const DevicePtr& device) : DevChild(device)
    {
        m_fmt = TextureFmt::RGBA8;
        m_size = glm::ivec2(0,0);
        m_slices = 1;
        m_mips_count = 1;
    }

    const Layout* AutoReflectCB(ID3D11ShaderReflectionConstantBuffer* cb_ref)
    {
        D3D11_SHADER_BUFFER_DESC cb_desc;
        cb_ref->GetDesc(&cb_desc);
        assert(cb_desc.Type == D3D_CT_CBUFFER);

        std::vector<char> raw_data;
        raw_data.resize(cb_desc.Size);
        LayoutBuilderIntf* lb = LB();
        for (UINT vidx = 0; vidx < cb_desc.Variables; vidx++) {
            ID3D11ShaderReflectionVariable* var_ref;
            var_ref = cb_ref->GetVariableByIndex(vidx);
            D3D11_SHADER_VARIABLE_DESC var_desc;
            var_ref->GetDesc(&var_desc);

            ID3D11ShaderReflectionType* type_ref = var_ref->GetType();
            D3D11_SHADER_TYPE_DESC type_desc;
            type_ref->GetDesc(&type_desc);

            LayoutType lt;
            switch (type_desc.Type) {
            case D3D_SVT_INT: lt = LayoutType::UInt; break;
            case D3D_SVT_UINT: lt = LayoutType::UInt; break;
            case D3D_SVT_FLOAT: lt = LayoutType::Float; break;
            default:
                throw std::runtime_error("unsupported layout type");
            }
            int arr_size = type_desc.Elements == 0 ? 1 : type_desc.Elements;
            lb->Add(var_desc.Name, lt, type_desc.Rows * type_desc.Columns, false, var_desc.StartOffset, arr_size);

            if (var_desc.DefaultValue)
                memcpy(&raw_data[var_desc.StartOffset], var_desc.DefaultValue, var_desc.Size);
        }
        return lb->Finish(cb_desc.Size);
    }

    int Program::ObtainSlotIdx(Program::SlotKind kind, const std::string& name, const Layout* layout)
    {
        for (size_t i = 0; i < m_slots.size(); i++) { 
            if (m_slots[i].name == name) {
                assert(m_slots[i].kind == kind);
                assert((name != "$Globals") && (m_slots[i].layout == layout));
                return static_cast<int>(i);
            }
        }
        ShaderSlot newSlot;
        newSlot.kind = kind;
        newSlot.name = name;
        newSlot.layout = layout;
        m_slots.push_back(newSlot);
        return static_cast<int>(m_slots.size() - 1);
    }

    int Program::FindSlot(const char* name)
    {
        for (size_t i = 0; i < m_slots.size(); i++) {
            if (m_slots[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    ID3D11InputLayout* Program::ObtainLayout(const Layout* vertices, const Layout* instances, int step_rate)
    {
        for (const auto& l : m_layouts) {
            if ((l.vertices == vertices) && (l.instances == instances) && (l.step_rate == step_rate)) {
                return l.m_dx_layout.Get();
            }
        }
        InputLayoutData new_l;
        new_l.vertices = vertices;
        new_l.instances = instances;
        new_l.step_rate = step_rate;
        new_l.RebuildLayout(m_device->m_device.Get(), m_shader_data[int(ShaderType::vertex)]);
        m_layouts.push_back(new_l);
        return new_l.m_dx_layout.Get();
    }

    void Program::AutoReflect(const void* data, int data_size, ShaderType st)
    {
        ComPtr<ID3D11ShaderReflection> ref;
        D3DReflect(data, data_size, __uuidof(ID3D11ShaderReflection), &ref);
        D3D11_SHADER_DESC shader_desc;
        ref->GetDesc(&shader_desc);

        for (UINT i = 0; i < shader_desc.BoundResources; i++) {
            D3D11_SHADER_INPUT_BIND_DESC res_desc;
            ref->GetResourceBindingDesc(i, &res_desc);

            SlotKind kind;
            switch (res_desc.Type) {
            case D3D_SIT_CBUFFER: kind = SlotKind::uniform; break;
            case D3D_SIT_TEXTURE: kind = SlotKind::texture; break;
            case D3D_SIT_STRUCTURED: kind = SlotKind::buffer; break;
            case D3D_SIT_SAMPLER: kind = SlotKind::sampler; break;
            default:
                continue;
                //throw std::runtime_error("unsupported resource type");
            }
            const Layout* l = res_desc.Type == D3D_SIT_CBUFFER ? AutoReflectCB(ref->GetConstantBufferByName(res_desc.Name)) : nullptr;
            ShaderSlot& slot = m_slots[ObtainSlotIdx(kind, std::string(res_desc.Name), l)];
            slot.bindPoints[int(st)] = res_desc.BindPoint;

            if (slot.name == "$Globals") {
                m_globals[int(st)] = std::make_shared<UniformBuffer>(m_device);
                m_globals[int(st)]->SetState(slot.layout, 1);
                slot.buffer = m_globals[int(st)]->m_handle;
            }
        }
    }

    void Program::Load(std::istream& stream)
    {
        struct Chunk {
            uint32_t id;
            uint32_t size;
        };
        auto readChunk = [&stream]()->Chunk {
            Chunk res;
            stream.read((char*)&res, sizeof(res));
            if (!stream) {
                if (!stream.eof()) throw std::runtime_error("can't read chunk data");
            }
            return res;
        };
        auto readString = [&stream]()->std::string {
            std::string res;
            uint32_t size;
            stream.read((char*)&size, sizeof(size));
            res.resize(size);
            if (size) {
                stream.read((char*)res.data(), size);
            }
            return res;
        };
        while (!stream.eof()) {
            Chunk shader_ch;
            shader_ch = readChunk();
            if (stream.eof()) break;
            ShaderType st;
            switch (shader_ch.id) {
            case 0x54524556: st = ShaderType::vertex; break;
            case 0x4E4F4354: st = ShaderType::hull; break;
            case 0x4C564554: st = ShaderType::domain; break;
            case 0x4D4F4547: st = ShaderType::geometry; break;
            case 0x47415246: st = ShaderType::pixel; break;
            case 0x504D4F43: st = ShaderType::compute; break;
            default:
                throw std::runtime_error("unknown shader type");
            }
            for (int i = 0; i < 2; i++) {
                Chunk part_ch;
                part_ch = readChunk();
                if (part_ch.id == MAKEFOURCC('C', 'O', 'D', 'E')) {
                    uint32_t code_size;
                    stream.read((char*)&code_size, sizeof(code_size));
                    std::vector<char>& raw_data = m_shader_data[int(st)];
                    raw_data.resize(code_size);
                    stream.read(raw_data.data(), code_size);

                    ID3D11DeviceChild** tmp = &m_shaders[int(st)];
                    switch (st) {
                    case ShaderType::vertex:
                        CheckD3DErr(m_device->m_device->CreateVertexShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11VertexShader**)tmp));
                        break;
                    case ShaderType::hull:
                        CheckD3DErr(m_device->m_device->CreateHullShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11HullShader**)tmp));
                        break;
                    case ShaderType::domain:
                        CheckD3DErr(m_device->m_device->CreateDomainShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11DomainShader**)tmp));
                        break;
                    case ShaderType::geometry:
                        CheckD3DErr(m_device->m_device->CreateGeometryShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11GeometryShader**)tmp));
                        break;
                    case ShaderType::pixel:
                        CheckD3DErr(m_device->m_device->CreatePixelShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11PixelShader**)tmp));
                        break;
                    case ShaderType::compute:
                        CheckD3DErr(m_device->m_device->CreateComputeShader(raw_data.data(), raw_data.size(), nullptr, (ID3D11ComputeShader**)tmp));
                        break;
                    }
                    AutoReflect(raw_data.data(), int(raw_data.size()), st);
                    continue;
                }
                if (part_ch.id == MAKEFOURCC('U', 'B', 'L', 'K')) {
                    stream.seekg(part_ch.size, std::ios::cur);
                    continue;
                }
                throw std::runtime_error("unknown shader part block");
            }
        }
    }

    void Program::SelectProgram()
    {
        if (m_globals_dirty) {
            for (int i = 0; i < 6; i++)
                if (m_globals[i]) m_globals[i]->ValidateDynamicData();
        }

        if (!IsProgramActive()) {
            m_device->m_active_program = this;
            for (const auto& slot : m_slots) {
                slot.Select(m_device->m_deviceContext.Get());
            }
            SelectInputBuffers();

            ID3D11DeviceChild* tmp;           
            tmp = m_shaders[int(ShaderType::vertex)] ? m_shaders[int(ShaderType::vertex)].Get() : nullptr;
            m_device->m_deviceContext->VSSetShader((ID3D11VertexShader*)tmp, nullptr, 0);
            tmp = m_shaders[int(ShaderType::hull)] ? m_shaders[int(ShaderType::hull)].Get() : nullptr;
            m_device->m_deviceContext->HSSetShader((ID3D11HullShader*)tmp, nullptr, 0);
            tmp = m_shaders[int(ShaderType::domain)] ? m_shaders[int(ShaderType::domain)].Get() : nullptr;
            m_device->m_deviceContext->DSSetShader((ID3D11DomainShader*)tmp, nullptr, 0);
            tmp = m_shaders[int(ShaderType::geometry)] ? m_shaders[int(ShaderType::geometry)].Get() : nullptr;
            m_device->m_deviceContext->GSSetShader((ID3D11GeometryShader*)tmp, nullptr, 0);
            tmp = m_shaders[int(ShaderType::pixel)] ? m_shaders[int(ShaderType::pixel)].Get() : nullptr;
            m_device->m_deviceContext->PSSetShader((ID3D11PixelShader*)tmp, nullptr, 0);
            tmp = m_shaders[int(ShaderType::compute)] ? m_shaders[int(ShaderType::compute)].Get() : nullptr;
            m_device->m_deviceContext->CSSetShader((ID3D11ComputeShader*)tmp, nullptr, 0);
        }
    }

    void Program::SelectTopology(PrimTopology pt)
    {
        D3D11_PRIMITIVE_TOPOLOGY dx_pt = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        switch (pt) {
        case PrimTopology::Point: dx_pt = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        case PrimTopology::Line: dx_pt = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
        case PrimTopology::Linestrip: dx_pt = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
        case PrimTopology::Triangle: dx_pt = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
        case PrimTopology::Trianglestrip: dx_pt = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        }
        m_device->m_deviceContext->IASetPrimitiveTopology(dx_pt);
    }

    void Program::SelectInputBuffers()
    {
        ID3D11Buffer* dxbuf = m_selected_vbo ? m_selected_vbo->m_handle.Get() : nullptr;
        UINT stride = m_selected_vbo ? m_selected_vbo->m_layout->stride : 0;
        UINT offset = 0;
        m_device->m_deviceContext->IASetVertexBuffers(0, 1, &dxbuf, &stride, &offset);

        dxbuf = m_selected_instances ? m_selected_instances->m_handle.Get() : nullptr;
        stride = m_selected_instances ? m_selected_instances->m_layout->stride : 0;
        offset = 0;
        m_device->m_deviceContext->IASetVertexBuffers(1, 1, &dxbuf, &stride, &offset);

        dxbuf = m_selected_ibo ? m_selected_ibo->m_handle.Get() : nullptr;
        m_device->m_deviceContext->IASetIndexBuffer(dxbuf, DXGI_FORMAT_R32_UINT, 0);

        const Layout* vl = m_selected_vbo ? m_selected_vbo->GetLayout() : nullptr;
        const Layout* il = m_selected_instances ? m_selected_instances->GetLayout() : nullptr;        
        m_device->m_deviceContext->IASetInputLayout(ObtainLayout(vl, il, m_selected_insatnce_steprate));
    }

    bool Program::IsProgramActive() const
    {
        return m_device->m_active_program == this;
    }

    void Program::Load(const std::string& name, bool from_resources, const std::string& dir)
    {
        if (from_resources) {
            auto cvt = [](const std::string & s) {
                std::wstring wsTmp(s.begin(), s.end());
                return wsTmp;
            };
            std::wstring wstr = L"DX_" + cvt(name);
            HRSRC hResource = FindResourceW(0, wstr.c_str(), RT_RCDATA);
            if (hResource) {
                HGLOBAL hLoadedResource = LoadResource(0, hResource);
                if (hLoadedResource) {
                    LPVOID pLockedResource = LockResource(hLoadedResource);
                    if (pLockedResource) {
                        DWORD dwResourceSize = SizeofResource(0, hResource);
                        if (0 != dwResourceSize)
                        {
                            std::stringstream ss;
                            ss.write((const char*)pLockedResource, dwResourceSize);
                            ss.seekg(0, ss.beg);
                            Load(ss);
                        }
                        FreeResource(pLockedResource);
                    }
                }
            }
        }
        else {
            std::string fullpath = dir + "\\DX_" + name + ".hlsl";
            std::ifstream fs (fullpath, std::ifstream::in | std::ifstream::binary);
            if (!fs) {
                throw std::runtime_error("can't open file: " + fullpath);
            }
            Load(fs);
        }
    }
    void Program::SetValue(const char* name, float v)
    {
        for (int i = 0; i < 6; i++)
            if (m_globals[i]) m_globals[i]->SetValue(name, v);
        m_globals_dirty = true;
    }
    void Program::SetValue(const char* name, int i)
    {
        for (int j = 0; j < 6; j++)
            if (m_globals[j]) m_globals[j]->SetValue(name, i);
        m_globals_dirty = true;
    }
    void Program::SetValue(const char* name, const glm::vec2& v)
    {
        for (int i = 0; i < 6; i++)
            if (m_globals[i]) m_globals[i]->SetValue(name, v);
        m_globals_dirty = true;
    }
    void Program::SetValue(const char* name, const glm::vec3& v)
    {
        for (int i = 0; i < 6; i++)
            if (m_globals[i]) m_globals[i]->SetValue(name, v);
        m_globals_dirty = true;
    }
    void Program::SetValue(const char* name, const glm::vec4& v)
    {
        for (int i = 0; i < 6; i++)
            if (m_globals[i]) m_globals[i]->SetValue(name, v);
        m_globals_dirty = true;
    }
    void Program::SetValue(const char* name, const glm::mat4& m)
    {
        for (int i = 0; i < 6; i++)
            if (m_globals[i]) m_globals[i]->SetValue(name, m);
        m_globals_dirty = true;
    }
    void Program::SetResource(const char* name, const UniformBufferPtr& ubo)
    {
        int idx = FindSlot(name);
        if (idx < 0) return;
        ShaderSlot& slot = m_slots[idx];
        if ((slot.buffer ? slot.buffer.Get() : nullptr) != (ubo ? ubo->m_handle.Get() : nullptr)) {
            slot.buffer = ubo ? ubo->m_handle : nullptr;
            if (IsProgramActive()) {
                slot.Select(m_device->m_deviceContext.Get());
            }
        }
    }
    void Program::SetResource(const char* name, const StructuredBufferPtr& sbo)
    {
        int idx = FindSlot(name);
        if (idx < 0) return;
        ShaderSlot& slot = m_slots[idx];
        ComPtr<ID3D11ShaderResourceView> srv = sbo ? sbo->GetShaderResourceView() : nullptr;
        if ((slot.view ? slot.view.Get() : nullptr) != srv.Get()) {
            slot.view = std::move(srv);
            if (IsProgramActive()) {
                slot.Select(m_device->m_deviceContext.Get());
            }
        }
    }
    void Program::SetResource(const char* name, const Texture2DPtr& tex, bool as_array, bool as_cubemap)
    {
        int idx = FindSlot(name);
        if (idx < 0) return;
        ShaderSlot& slot = m_slots[idx];
        ComPtr<ID3D11ShaderResourceView> srv = tex ? tex->GetShaderResourceView(as_array, as_cubemap) : nullptr;
        if ((slot.view ? slot.view.Get() : nullptr) != srv.Get()) {
            slot.view = std::move(srv);
            if (IsProgramActive()) {
                slot.Select(m_device->m_deviceContext.Get());
            }
        }
    }
    void Program::SetResource(const char* name, const Texture3DPtr& tex)
    {
        int idx = FindSlot(name);
        if (idx < 0) return;
        ShaderSlot& slot = m_slots[idx];
        ComPtr<ID3D11ShaderResourceView> srv = tex ? tex->GetShaderResourceView() : nullptr;
        if ((slot.view ? slot.view.Get() : nullptr) != srv.Get()) {
            slot.view = std::move(srv);
            if (IsProgramActive()) {
                slot.Select(m_device->m_deviceContext.Get());
            }
        }
    }
    void Program::SetResource(const char* name, const Sampler& s)
    {
        int idx = FindSlot(name);
        if (idx < 0) return;
        ShaderSlot& slot = m_slots[idx];
        ID3D11SamplerState* new_sampler = m_device->ObtainSampler(s);
        if (slot.sampler != new_sampler) {
            slot.sampler = new_sampler;
            if (IsProgramActive()) {
                slot.Select(m_device->m_deviceContext.Get());
            }
        }
    }
    void Program::SetInputBuffers(const VertexBufferPtr& vbo, const IndexBufferPtr& ibo, const VertexBufferPtr& instances, int instanceStepRate)
    {
        m_selected_vbo = vbo;
        m_selected_ibo = ibo;
        m_selected_instances = instances;
        m_selected_insatnce_steprate = instanceStepRate;
        if (IsProgramActive()) SelectInputBuffers();
    }
    void Program::DrawIndexed(PrimTopology pt, const DrawIndexedCmd& cmd)
    {
        DrawIndexed(pt, cmd.StartIndex, cmd.IndexCount, cmd.InstanceCount, cmd.BaseVertex, cmd.BaseInstance);
    }
    void Program::DrawIndexed(PrimTopology pt, const std::vector<DrawIndexedCmd>& cmd_buf)
    {
        for (const auto& cmd : cmd_buf)
            DrawIndexed(pt, cmd);
    }
    void Program::DrawIndexed(PrimTopology pt, int index_start, int index_count, int instance_count, int base_vertex, int base_instance)
    {
        SelectProgram();
        SelectTopology(pt);
        m_device->States()->ValidateStates();
        if (index_count < 0) index_count = m_selected_ibo->IndexCount();
        if (instance_count < 0) instance_count = m_selected_instances ? m_selected_instances->VertexCount() : 0;

        if (instance_count) {
            m_device->m_deviceContext->DrawIndexedInstanced(index_count, instance_count, index_start, base_vertex, base_instance);
        }
        else {
            m_device->m_deviceContext->DrawIndexed(index_count, index_start, base_vertex);
        }        
    }
    void Program::Draw(PrimTopology pt, int vert_start, int vert_count, int instance_count, int base_instance)
    {
        SelectProgram();
        SelectTopology(pt);
        m_device->States()->ValidateStates();
        if (vert_count < 0) vert_count = m_selected_vbo->VertexCount();
        if (instance_count < 0) instance_count = m_selected_instances ? m_selected_instances->VertexCount() : 0;

        if (instance_count) {
            m_device->m_deviceContext->DrawInstanced(vert_count, instance_count, vert_start, base_instance);
        }
        else {
            m_device->m_deviceContext->Draw(vert_count, vert_start);
        }
    }
    void Program::CS_SetUAV(int slot, const Texture2DPtr& tex, int mip, int slice_start, int slice_count, bool as_array)
    {
        ID3D11UnorderedAccessView* view = tex ? tex->GetUnorderedAccessView(mip, slice_start, slice_count, as_array).Get() : nullptr;
        m_views_uav[slot] = view;
        UINT counter = -1;
        m_device->m_deviceContext->CSSetUnorderedAccessViews(slot, 1, &view, &counter);
    }
    void Program::CS_SetUAV(int slot, const Texture3DPtr& tex, int mip, int z_start, int z_count)
    {
        ID3D11UnorderedAccessView* view = tex ? tex->GetUnorderedAccessView(mip, z_start, z_count).Get() : nullptr;
        m_views_uav[slot] = view;
        UINT counter = -1;
        m_device->m_deviceContext->CSSetUnorderedAccessViews(slot, 1, &view, &counter);
    }
    void Program::CS_SetUAV(int slot, const StructuredBufferPtr& buf, int initial_counter)
    {
        ID3D11UnorderedAccessView* view = buf ? buf->GetUnorderedAccessView().Get() : nullptr;
        m_views_uav[slot] = view;
        UINT counter = initial_counter;
        m_device->m_deviceContext->CSSetUnorderedAccessViews(slot, 1, &view, &counter);
    }
    void Program::CS_ClearUAV(int slot, uint32_t v)
    {       
        UINT iv[4] = { v,v,v,v };
        m_device->m_deviceContext->ClearUnorderedAccessViewUint(m_views_uav[slot], iv);
    }
    void Program::CS_ClearUAV(int slot, glm::vec4 v)
    {
        m_device->m_deviceContext->ClearUnorderedAccessViewFloat(m_views_uav[slot], (float*)&v);
    }
    void Program::Dispatch(glm::ivec3 groups)
    {
        SelectProgram();
        groups = glm::max(groups, { 0,0,0 });
        m_device->m_deviceContext->Dispatch(groups.x, groups.y, groups.z);
    }
    Program::Program(const DevicePtr& device) : DevChild(device)
    {
        m_globals_dirty = false;
        m_selected_insatnce_steprate = 0;
    }

    Program::~Program()
    {
        if (m_device->m_active_program == this) m_device->m_active_program = nullptr;
    }
    int VertexBuffer::VertexCount()
    {
        return m_vert_count;
    }
    const Layout* VertexBuffer::GetLayout() const
    {
        return m_layout;
    }

    void VertexBuffer::SetState(const Layout* layout, int vertex_count, const void* data)
    {
        m_layout = layout;
        m_vert_count = vertex_count;

        if (!m_vert_count) {
            m_handle = nullptr;
            return;
        }

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = vertex_count * m_layout->stride;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = m_layout->stride;

        if (data) {
            D3D11_SUBRESOURCE_DATA dxdata;
            dxdata.pSysMem = data;
            dxdata.SysMemPitch = desc.ByteWidth;
            dxdata.SysMemSlicePitch = desc.ByteWidth;
            CheckD3DErr( m_device->m_device->CreateBuffer(&desc, &dxdata, &m_handle) );
        }
        else {
            CheckD3DErr( m_device->m_device->CreateBuffer(&desc, nullptr, &m_handle) );
        }
    }
    void VertexBuffer::SetSubData(int start_vertex, int num_vertices, const void* data)
    {
        assert(m_handle);
        D3D11_BOX box;
        box.left = start_vertex * m_layout->stride;
        box.top = 0;
        box.right = box.left + num_vertices * m_layout->stride;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        m_device->m_deviceContext->UpdateSubresource(m_handle.Get(), 0, &box, data, 0, 0);
    }
    VertexBuffer::VertexBuffer(const DevicePtr& device) : DevChild(device)
    {
        m_layout = nullptr;
        m_vert_count = 0;
    }
    ComPtr<ID3D11ShaderResourceView> StructuredBuffer::GetShaderResourceView()
    {
        if (!m_vert_count) return nullptr;
        if (!m_srv) 
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = glm::max(m_vert_count, 1);
            CheckD3DErr( m_device->m_device->CreateShaderResourceView(m_handle.Get(), &desc, &m_srv) );
        }
        return m_srv;
    }
    ComPtr<ID3D11UnorderedAccessView> StructuredBuffer::GetUnorderedAccessView()
    {
        assert(m_UAV_access);
        if (!m_uav)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = m_vert_count;
            desc.Buffer.Flags = m_UAV_with_counter ? D3D11_BUFFER_UAV_FLAG_COUNTER : 0;
            CheckD3DErr(m_device->m_device->CreateUnorderedAccessView(m_handle.Get(), &desc, &m_uav));
        }
        return m_uav;
    }
    int StructuredBuffer::Stride()
    {
        return m_stride;
    }
    int StructuredBuffer::VertexCount()
    {
        return m_vert_count;
    }
    void StructuredBuffer::SetState(int stride, int vertex_count, bool UAV, bool UAV_with_counter, const void* data)
    {
        m_uav = nullptr;
        m_srv = nullptr;

        m_stride = stride;
        m_vert_count = vertex_count;
        m_UAV_access = UAV;
        m_UAV_with_counter = UAV_with_counter;

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = glm::max(vertex_count, 1) * m_stride;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (UAV) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        //if (UAV) desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        desc.StructureByteStride = m_stride;

        if (data) {
            D3D11_SUBRESOURCE_DATA dxdata;
            dxdata.pSysMem = data;
            dxdata.SysMemPitch = desc.ByteWidth;
            dxdata.SysMemSlicePitch = desc.ByteWidth;
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, &dxdata, &m_handle));
        }
        else {
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, nullptr, &m_handle));
        }
    }
    void StructuredBuffer::SetSubData(int start_vertex, int num_vertices, const void* data)
    {
        assert(m_handle);
        if (num_vertices <= 0) return;
        D3D11_BOX box;
        box.left = start_vertex * m_stride;
        box.top = 0;
        box.right = box.left + num_vertices * m_stride;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        m_device->m_deviceContext->UpdateSubresource(m_handle.Get(), 0, &box, data, 0, 0);
    }
    void StructuredBuffer::ReadBack(void* data)
    {
        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = m_vert_count * m_stride;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = m_stride;

        ComPtr<ID3D11Buffer> tmp_buf;
        CheckD3DErr(m_device->m_device->CreateBuffer(&desc, nullptr, &tmp_buf));

        m_device->m_deviceContext->CopyResource(tmp_buf.Get(), m_handle.Get());

        D3D11_MAPPED_SUBRESOURCE map;
        CheckD3DErr( m_device->m_deviceContext->Map(tmp_buf.Get(), 0, D3D11_MAP_READ, 0, &map) );
        memcpy(data, map.pData, m_vert_count * m_stride);
        m_device->m_deviceContext->Unmap(tmp_buf.Get(), 0);
    }
    StructuredBuffer::StructuredBuffer(const DevicePtr& device) : DevChild(device)
    {
        m_stride = 0;
        m_vert_count = 0;
        m_UAV_access = false;
        m_UAV_with_counter = false;
    }
    int IndexBuffer::IndexCount()
    {
        return m_ind_count;
    }
    void IndexBuffer::SetState(int ind_count, const void* data)
    {
        m_ind_count = ind_count;

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = m_ind_count * 4;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 4;

        if (data) {
            D3D11_SUBRESOURCE_DATA dxdata;
            dxdata.pSysMem = data;
            dxdata.SysMemPitch = desc.ByteWidth;
            dxdata.SysMemSlicePitch = desc.ByteWidth;
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, &dxdata, &m_handle));
        }
        else {
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, nullptr, &m_handle));
        }
    }
    void IndexBuffer::SetSubData(int start_idx, int num_indices, const void* data)
    {
        assert(m_handle);
        if (num_indices <= 0) return;
        D3D11_BOX box;
        box.left = start_idx * 4;
        box.top = 0;
        box.right = box.left + num_indices * 4;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        m_device->m_deviceContext->UpdateSubresource(m_handle.Get(), 0, &box, data, 0, 0);
    }
    IndexBuffer::IndexBuffer(const DevicePtr& device) : DevChild(device)
    {
        m_ind_count = 0;
    }
    void* UniformBuffer::FindValueDest(const char* name, int element_idx)
    {
        for (const auto& f : m_layout->fields) {
            if (f.name == name) {
                int offset = f.offset + m_layout->stride * element_idx;
                return &m_data[offset];
            }
        }
        return nullptr;
    }
    void UniformBuffer::SetValue(void* dest, const void* data, int datasize)
    {
        memcpy(dest, data, datasize);
        m_valid = false;
    }
    void UniformBuffer::ValidateDynamicData()
    {
        if (m_valid) return;
        m_valid = true;
        D3D11_MAPPED_SUBRESOURCE map_res;
        CheckD3DErr( m_device->m_deviceContext->Map(m_handle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map_res) );
        memcpy(map_res.pData, m_data.data(), m_data.size());
        m_device->m_deviceContext->Unmap(m_handle.Get(), 0);
    }
    const Layout* UniformBuffer::GetLayout() const
    {
        return m_layout;
    }
    void UniformBuffer::SetState(const Layout* layout, int elemets_count, const void* data)
    {
        m_layout = layout;
        m_elements_count = elemets_count;
        m_data.resize(static_cast<size_t>(layout->stride) * elemets_count);
        assert(m_data.size());
        if (data)
            memcpy(m_data.data(), data, m_data.size());

        D3D11_BUFFER_DESC desc;
        desc.ByteWidth = UINT(m_data.size());
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;
        desc.StructureByteStride = layout->stride;

        if (data) {
            D3D11_SUBRESOURCE_DATA dxdata;
            dxdata.pSysMem = data;
            dxdata.SysMemPitch = desc.ByteWidth;
            dxdata.SysMemSlicePitch = desc.ByteWidth;
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, &dxdata, &m_handle));
        }
        else {
            CheckD3DErr(m_device->m_device->CreateBuffer(&desc, nullptr, &m_handle));
        }
        m_valid = true;
    }
    void UniformBuffer::SetSubData(int start_element, int num_elements, const void* data)
    {
        memcpy(&m_data[static_cast<size_t>(m_layout->stride) * start_element], data, static_cast<size_t>(m_layout->stride) * num_elements);
        m_valid = false;
    }
    void UniformBuffer::SetValue(const char* name, float v, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &v, sizeof(v));
    }
    void UniformBuffer::SetValue(const char* name, int i, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &i, sizeof(i));
    }
    void UniformBuffer::SetValue(const char* name, const glm::vec2& v, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &v, sizeof(v));
    }
    void UniformBuffer::SetValue(const char* name, const glm::vec3& v, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &v, sizeof(v));
    }
    void UniformBuffer::SetValue(const char* name, const glm::vec4& v, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &v, sizeof(v));
    }
    void UniformBuffer::SetValue(const char* name, const glm::mat4& m, int element_idx)
    {
        void* dst = FindValueDest(name, element_idx);
        if (dst) SetValue(dst, &m, sizeof(m));
    }
    UniformBuffer::UniformBuffer(const DevicePtr& device) : DevChild(device)
    {
        m_elements_count = 0;
        m_layout = nullptr;
        m_valid = true;
    }

    DXGI_FORMAT ConvertToDX(const LayoutField& l) {
        assert((l.num_fields > 0) && (l.num_fields < 5));
        switch (l.type) {
        case LayoutType::Byte: {
            switch (l.num_fields) {
            case 1: return l.do_norm ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
            case 2: return l.do_norm ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
            case 3: return DXGI_FORMAT_UNKNOWN;
            case 4: return l.do_norm ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
            }
        }
        case LayoutType::Word: {
            switch (l.num_fields) {
            case 1: return l.do_norm ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
            case 2: return l.do_norm ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
            case 3: return DXGI_FORMAT_UNKNOWN;
            case 4: return l.do_norm ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;
            }
        }
        case LayoutType::UInt: {
            switch (l.num_fields) {
            case 1: return DXGI_FORMAT_R32_UINT;
            case 2: return DXGI_FORMAT_R32G32_UINT;
            case 3: return DXGI_FORMAT_R32G32B32_UINT;
            case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
            }
        }
        case LayoutType::Float: {
            switch (l.num_fields) {
            case 1: return DXGI_FORMAT_R32_FLOAT;
            case 2: return DXGI_FORMAT_R32G32_FLOAT;
            case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
            case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
        }
        default:
            throw std::runtime_error("unsupported format");
        }
    }

    Program::InputLayoutData::InputLayoutData()
    {
        vertices = nullptr;
        instances = nullptr;
        step_rate = 1;
    }

    void Program::InputLayoutData::RebuildLayout(ID3D11Device* device, const std::vector<char>& vertex_code)
    {
        std::vector<D3D11_INPUT_ELEMENT_DESC> descs;
        if (vertices) {
            for (const auto& f : vertices->fields) {
                D3D11_INPUT_ELEMENT_DESC d;
                d.SemanticName = f.name.c_str();
                d.SemanticIndex = 0;
                d.Format = ConvertToDX(f);
                d.InputSlot = 0;
                d.AlignedByteOffset = f.offset;
                d.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                d.InstanceDataStepRate = 0;
                descs.push_back(d);
            }
        }
        if (instances) {
            for (const auto& f : instances->fields) {
                D3D11_INPUT_ELEMENT_DESC d;
                d.SemanticName = f.name.c_str();
                d.SemanticIndex = 0;
                d.Format = ConvertToDX(f);
                d.InputSlot = 1;
                d.AlignedByteOffset = f.offset;
                d.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
                d.InstanceDataStepRate = step_rate;
                descs.push_back(d);
            }
        }
        if (descs.size()) {
            CheckD3DErr(device->CreateInputLayout(descs.data(), UINT(descs.size()), vertex_code.data(), vertex_code.size(), &m_dx_layout));
        }
        else {
            m_dx_layout = nullptr;
        }
    }
    void Program::ShaderSlot::Select(ID3D11DeviceContext* dev) const
    {
        ID3D11Buffer* b = buffer ? buffer.Get() : nullptr;
        ID3D11ShaderResourceView* v = view ? view.Get() : nullptr;

        if (bindPoints[int(ShaderType::vertex)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->VSSetConstantBuffers(bindPoints[int(ShaderType::vertex)], 1, &b); break; }
            case SlotKind::texture: { dev->VSSetShaderResources(bindPoints[int(ShaderType::vertex)], 1, &v); break; }
            case SlotKind::buffer: { dev->VSSetShaderResources(bindPoints[int(ShaderType::vertex)], 1, &v); break; }
            case SlotKind::sampler: { dev->VSSetSamplers(bindPoints[int(ShaderType::vertex)], 1, &sampler); break; }
            }
        }
        if (bindPoints[int(ShaderType::hull)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->HSSetConstantBuffers(bindPoints[int(ShaderType::hull)], 1, &b); break; }
            case SlotKind::texture: { dev->HSSetShaderResources(bindPoints[int(ShaderType::hull)], 1, &v); break; }
            case SlotKind::buffer: { dev->HSSetShaderResources(bindPoints[int(ShaderType::hull)], 1, &v); break; }
            case SlotKind::sampler: { dev->HSSetSamplers(bindPoints[int(ShaderType::hull)], 1, &sampler); break; }
            }
        }
        if (bindPoints[int(ShaderType::domain)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->DSSetConstantBuffers(bindPoints[int(ShaderType::domain)], 1, &b); break; }
            case SlotKind::texture: { dev->DSSetShaderResources(bindPoints[int(ShaderType::domain)], 1, &v); break; }
            case SlotKind::buffer: { dev->DSSetShaderResources(bindPoints[int(ShaderType::domain)], 1, &v); break; }
            case SlotKind::sampler: { dev->DSSetSamplers(bindPoints[int(ShaderType::domain)], 1, &sampler); break; }
            }
        }
        if (bindPoints[int(ShaderType::geometry)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->GSSetConstantBuffers(bindPoints[int(ShaderType::geometry)], 1, &b); break; }
            case SlotKind::texture: { dev->GSSetShaderResources(bindPoints[int(ShaderType::geometry)], 1, &v); break; }
            case SlotKind::buffer: { dev->GSSetShaderResources(bindPoints[int(ShaderType::geometry)], 1, &v); break; }
            case SlotKind::sampler: { dev->GSSetSamplers(bindPoints[int(ShaderType::geometry)], 1, &sampler); break; }
            }
        }
        if (bindPoints[int(ShaderType::pixel)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->PSSetConstantBuffers(bindPoints[int(ShaderType::pixel)], 1, &b); break; }
            case SlotKind::texture: { dev->PSSetShaderResources(bindPoints[int(ShaderType::pixel)], 1, &v); break; }
            case SlotKind::buffer: { dev->PSSetShaderResources(bindPoints[int(ShaderType::pixel)], 1, &v); break; }
            case SlotKind::sampler: { dev->PSSetSamplers(bindPoints[int(ShaderType::pixel)], 1, &sampler); break; }
            }
        }
        if (bindPoints[int(ShaderType::compute)] >= 0) {
            switch (kind) {
            case SlotKind::uniform: { dev->CSSetConstantBuffers(bindPoints[int(ShaderType::compute)], 1, &b); break; }
            case SlotKind::texture: { dev->CSSetShaderResources(bindPoints[int(ShaderType::compute)], 1, &v); break; }
            case SlotKind::buffer: { dev->CSSetShaderResources(bindPoints[int(ShaderType::compute)], 1, &v); break; }
            case SlotKind::sampler: { dev->CSSetSamplers(bindPoints[int(ShaderType::compute)], 1, &sampler); break; }
            }
        }
    }
    void FrameBuffer::PrepareViews()
    {
        if (m_colors_to_bind.size() == 0) {
            m_rtv_count = 0;
            for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
                if (m_tex[i]) {
                    m_rtv_count = i + 1;
                    if (!m_color_views[i]) m_color_views[i] = m_tex[i]->BuildTargetView(m_tex_params[i].mip, m_tex_params[i].slice_start, m_tex_params[i].slice_count);
                    m_colors_to_bind.push_back(m_color_views[i].Get());
                }
                else {
                    m_colors_to_bind.push_back(nullptr);
                }
            }
        }
        if (m_uav_to_bind_count < 0) {
            m_uav_to_bind_count = 0;
            for (int i = m_rtv_count; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_PS_CS_UAV_REGISTER_COUNT; i++) {
                if (m_uav[i].kind != UAV_slot_kind::empty) m_uav_to_bind_count++;
            }

        }

        m_uav_to_bind.clear();
        m_uav_initial_counts.clear();
        if (m_uav_to_bind_count > 0) {
            for (int i = m_rtv_count; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_PS_CS_UAV_REGISTER_COUNT; i++) {
                switch (m_uav[i].kind) {
                case (UAV_slot_kind::tex):
                    m_uav_to_bind.push_back(m_uav[i].tex->GetUnorderedAccessView(m_uav[i].tex_params.mip, m_uav[i].tex_params.slice_start, m_uav[i].tex_params.slice_count, m_uav[i].tex_params.as_array).Get());
                    m_uav_initial_counts.push_back(m_uav[i].initial_counter);
                    break;
                case (UAV_slot_kind::buf):
                    m_uav_to_bind.push_back(m_uav[i].buf->GetUnorderedAccessView().Get());
                    m_uav_initial_counts.push_back(m_uav[i].initial_counter);
                    break;
                case (UAV_slot_kind::empty):
                    m_uav_to_bind.push_back(nullptr);
                    m_uav_initial_counts.push_back(-1);
                    break;
                }
            }
        }

        if (m_depth && (!m_depth_view)) m_depth_view = m_depth->BuildDepthStencilView(m_depth_params.mip, m_depth_params.slice_start, m_depth_params.slice_count, m_depth_params.read_only);
    }
    void FrameBuffer::SetSizeFromWindow()
    {
        RECT rct;
        GetClientRect(m_device->m_wnd, &rct);
        SetSize(glm::ivec2(rct.right - rct.left, rct.bottom - rct.top));
    }
    void FrameBuffer::SetSize(const glm::ivec2& xy)
    {
        if (m_size != xy) {
            for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                m_color_views[i] = nullptr;
            m_depth_view = nullptr;
            m_colors_to_bind.clear();
        }
        m_size = xy;
        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            if (!m_tex[i]) continue;
            if (m_tex[i]->Size() != xy) {
                m_tex[i]->SetState(m_tex[i]->Format(), xy, 1, 1);
                m_color_views[i] = nullptr;
                m_colors_to_bind.clear();
            }
        }
        if (m_depth) {
            if (m_depth->Size() != xy) {
                m_depth->SetState(m_depth->Format(), xy, 1, 1);
                m_depth_view = nullptr;
            }
        }
    }
    glm::ivec2 FrameBuffer::GetSize() const
    {
        return m_size;
    }
    void FrameBuffer::Clear(int slot, const glm::vec4& color)
    {
        m_device->m_deviceContext->ClearRenderTargetView(m_color_views[slot].Get(), (float*)&color);
    }
    void FrameBuffer::ClearDS(float depth, bool clear_depth, char stencil, bool clear_stencil)
    {
        if (m_depth_view) {
            UINT flags = 0;
            flags |= clear_depth ? D3D11_CLEAR_DEPTH : 0;
            flags |= clear_stencil ? D3D11_CLEAR_STENCIL : 0;
            m_device->m_deviceContext->ClearDepthStencilView(m_depth_view.Get(), flags, depth, stencil);
        }
    }
    void FrameBuffer::SetSlot(int slot, const Texture2DPtr& tex, int mip, int slice_start, int slice_count)
    {
        if (m_tex[slot] != tex) {
            m_tex[slot] = tex;
            m_color_views[slot] = nullptr;
            m_colors_to_bind_dirty = true;
            m_colors_to_bind.clear();
        }
        Tex2D_params new_params(mip, slice_start, slice_count, false, false);
        if (!(m_tex_params[slot] == new_params)) {
            m_tex_params[slot] = new_params;
            m_color_views[slot] = nullptr;
            m_colors_to_bind_dirty = true;
            m_colors_to_bind.clear();
        }
    }
    void FrameBuffer::SetDS(const Texture2DPtr& tex, int mip, int slice_start, int slice_count, bool readonly)
    {
        if (m_depth != tex) {
            m_depth = tex;
            m_depth_view = nullptr;
        }
        Tex2D_params new_params(mip, slice_start, slice_count, readonly, false);
        if (!(m_depth_params == new_params)) {
            m_depth_params = new_params;
            m_depth_view = nullptr;
        }
    }
    Texture2DPtr FrameBuffer::GetSlot(int slot) const
    {
        return m_tex[slot];
    }
    Texture2DPtr FrameBuffer::GetDS() const
    {
        return m_depth;
    }
    void FrameBuffer::ClearUAV(int slot, uint32_t v)
    {
        UINT clear_value[4] = { v,v,v,v };
        m_device->m_deviceContext->ClearUnorderedAccessViewUint(m_uav_to_bind[slot], clear_value);
    }
    void FrameBuffer::SetUAV(int slot, const Texture2DPtr& tex, int mip, int slice_start, int slice_count, bool as_array)
    {
        m_uav_to_bind_count = -1;
        m_uav_to_bind.clear();
        m_uav[slot] = UAV_slot(tex, mip, slice_start, slice_count, as_array);
    }
    void FrameBuffer::SetUAV(int slot, const StructuredBufferPtr& buf, int initial_counter)
    {
        m_uav_to_bind_count = -1;
        m_uav_to_bind.clear();
        m_uav[slot] = UAV_slot(buf, initial_counter);
    }
    void FrameBuffer::BlitToDefaultFBO(int from_slot)
    {
        if (!m_tex[from_slot]) return;
        if (!m_tex[from_slot]->m_handle) return;
        ID3D11Texture2D* tex = m_tex[from_slot]->m_handle.Get();
        D3D11_BOX src_box;
        src_box.left = 0;
        src_box.right = m_tex[from_slot]->m_size.x >> m_tex_params[from_slot].mip;
        src_box.top = 0;
        src_box.bottom = m_tex[from_slot]->m_size.y >> m_tex_params[from_slot].mip;
        src_box.front = 0;
        src_box.back = 1;
        UINT src_subres = D3D11CalcSubresource(m_tex_params[from_slot].mip, m_tex_params[from_slot].slice_start, m_tex[from_slot]->m_mips_count);
        ComPtr<ID3D11Resource> dest_res;
        m_device->m_RTView->GetResource(&dest_res);
        m_device->m_deviceContext->CopySubresourceRegion(dest_res.Get(), 0, 0, 0, 0, tex, src_subres, &src_box);
    }
    FrameBuffer::FrameBuffer(const DevicePtr& device) : DevChild(device)
    {
        m_colors_to_bind_dirty = false;
        m_uav_to_bind_count = 0;
        m_rtv_count = 0;
    }
    FrameBuffer::~FrameBuffer()
    {
        if (m_device->m_active_fbo_ptr == this) m_device->SetFrameBuffer(nullptr);
    }
    FrameBuffer::Tex2D_params::Tex2D_params()
    {
        mip = 0;
        slice_start = 0;
        slice_count = 0;
        read_only = false;
    }
    FrameBuffer::Tex2D_params::Tex2D_params(int m, int s_start, int s_count, bool ronly, bool as_array)
    {
        mip = m;
        slice_start = s_start;
        slice_count = s_count;
        read_only = ronly;
        this->as_array = as_array;
    }
    bool FrameBuffer::Tex2D_params::operator==(const Tex2D_params& b)
    {
        return (mip == b.mip)&&(slice_start == b.slice_start)&&(slice_count == b.slice_count)&&(read_only == b.read_only);
    }
    ComPtr<ID3D11ShaderResourceView> Texture3D::GetShaderResourceView()
    {
        if (!m_srv) {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc;
            desc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE3D;
            desc.Format = ToDXGI_SRVFmt(m_fmt);
            desc.Texture3D.MipLevels = m_mips_count;
            desc.Texture3D.MostDetailedMip = 0;
            CheckD3DErr(m_device->m_device->CreateShaderResourceView(m_handle.Get(), &desc, &m_srv));
        }
        return m_srv;
    }
    ComPtr<ID3D11UnorderedAccessView> Texture3D::GetUnorderedAccessView(int mip, int z_start, int z_count)
    {
        glm::ivec3 key(mip, z_start, z_count);
        auto it = m_uav.find(key);
        if (it == m_uav.end()) {
            ComPtr<ID3D11UnorderedAccessView> new_view;
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
            desc.Format = ToDXGI_Fmt(m_fmt);
            desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MipSlice = mip;
            desc.Texture3D.FirstWSlice = z_start;
            desc.Texture3D.WSize = z_count;
            CheckD3DErr(m_device->m_device->CreateUnorderedAccessView(m_handle.Get(), &desc, &new_view));
            m_uav.insert({ key, new_view });
            return new_view;
        }
        return it->second;
    }
    void Texture3D::ClearStoredViews()
    {
        m_uav.clear();
        m_srv = nullptr;
    }
    TextureFmt Texture3D::Format() const
    {
        return m_fmt;
    }
    glm::ivec3 Texture3D::Size() const
    {
        return m_size;
    }
    int Texture3D::MipsCount() const
    {
        return m_mips_count;
    }
    void Texture3D::SetState(TextureFmt fmt, glm::ivec3 size, int mip_levels, const void* data)
    {
        m_fmt = fmt;
        m_size = size;
        m_mips_count = glm::clamp(mip_levels, 1, MipLevelsCount(size.x, size.y, size.z));

        D3D11_TEXTURE3D_DESC desc;
        desc.Width = m_size.x;
        desc.Height = m_size.y;
        desc.Depth = m_size.z;
        desc.MipLevels = m_mips_count;
        desc.Format = ToDXGI_Fmt(m_fmt);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = 0;
        if (CanBeShaderRes(m_fmt)) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        if (CanBeRenderTarget(m_fmt)) desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        if (CanBeDepthTarget(m_fmt)) desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        if (CanBeUAV(m_fmt)) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        if (data) {
            D3D11_SUBRESOURCE_DATA d3ddata;
            d3ddata.pSysMem = data;
            d3ddata.SysMemPitch = PixelsSize(m_fmt) * m_size.x;
            d3ddata.SysMemSlicePitch = d3ddata.SysMemPitch * m_size.y;
            CheckD3DErr(m_device->m_device->CreateTexture3D(&desc, &d3ddata, &m_handle));
        }
        else {
            CheckD3DErr(m_device->m_device->CreateTexture3D(&desc, nullptr, &m_handle));
        }

        ClearStoredViews();
    }
    Texture3D::Texture3D(const DevicePtr& device) : DevChild(device)
    {
        m_fmt = TextureFmt::RGBA8;
        m_size = glm::ivec3(0, 0, 0);
        m_mips_count = 1;
    }
}