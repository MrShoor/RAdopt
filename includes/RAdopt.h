#pragma once
#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <wrl.h>
#include <stdexcept>
#include <string>
#include <memory>
#include "GLM.h"
#include <vector>
#include <unordered_map>
#include <iostream>

namespace RA {
    using namespace Microsoft::WRL;

    uint32_t MurmurHash2(const void* key, int len, uint32_t seed = 0x9747b28c);

    enum class TextureFmt { None,
                            R8, RG8, RGBA8, 
                            R16, RG16, RGBA16, 
                            R16f, RG16f, RGBA16f, 
                            R32, RG32, RGB32, RGBA32, 
                            R32f, RG32f, RGB32f, RGBA32f,
                            D16, D24_S8, D32f, D32f_S8, 
    };
    enum class PrimTopology {
        Point,
        Line,
        Linestrip,
        Triangle,
        Trianglestrip, 
    };

    enum class CullMode { none, back, front };
    enum class Compare { never, less, equal, less_equal, greater, not_equal, greater_equal, always };
    enum class StencilOp { keep, zero, replace, inc_clamp, dec_clamp, ind, dec };
    enum class BlendFunc { add, sub, rev_sub, min, max };
    enum class Blend { zero, one, src_alpha, inv_src_alpha, dst_alpha, inv_dst_alpha, src_color, inv_src_color, dst_color, inv_dst_color };

    enum class TexFilter { none, point, linear };
    enum class TexWrap { repeat, mirror, clamp, clamp_border };
    struct Sampler {
        TexFilter filter;
        TexFilter mipfilter;
        int anisotropy;
        TexWrap wrap_x;
        TexWrap wrap_y;
        TexWrap wrap_z;
        glm::vec4 border;
        Compare comparison;
        bool operator==(const Sampler& p) const {
            return memcmp(this, &p, sizeof(Sampler))==0;
        }
        std::size_t operator() (const Sampler& s) const
        {
            return MurmurHash2(this, sizeof(Sampler));
        }
    };
    const static Sampler cSampler_Linear { 
        TexFilter::linear , TexFilter::linear , 16 , 
        TexWrap::repeat, TexWrap::repeat, TexWrap::repeat, 
        {0,0,0,0},
        Compare::never
    };

    struct DrawIndexedCmd {
        UINT IndexCount;
        UINT InstanceCount;
        UINT StartIndex;
        INT  BaseVertex;
        UINT BaseInstance;
    };

    class States {
    private:
        struct StateData {
            D3D11_RASTERIZER_DESC m_r_desc;
            ComPtr<ID3D11RasterizerState> m_r_state;
            UINT m_stencil_ref;
            D3D11_DEPTH_STENCIL_DESC m_d_desc;
            ComPtr<ID3D11DepthStencilState> m_d_state;
            D3D11_BLEND_DESC m_b_desc;
            ComPtr<ID3D11BlendState> m_b_state;
            inline StateData(States* st) {
                m_r_desc = st->m_r_desc;
                m_d_desc = st->m_d_desc;
                m_b_desc = st->m_b_desc;
                m_r_state = st->m_r_state;
                m_d_state = st->m_d_state;
                m_b_state = st->m_b_state;
                m_stencil_ref = st->m_stencil_ref;
            }
        };
    private:
        ID3D11Device* m_device;
        ID3D11DeviceContext* m_deviceContext;

        D3D11_RASTERIZER_DESC m_r_desc;
        ComPtr<ID3D11RasterizerState> m_r_state;

        UINT m_stencil_ref;
        D3D11_DEPTH_STENCIL_DESC m_d_desc;
        ComPtr<ID3D11DepthStencilState> m_d_state;

        D3D11_BLEND_DESC m_b_desc;
        ComPtr<ID3D11BlendState> m_b_state;

        std::vector<StateData> m_states;
    private:
        void SetDefaultStates();        
    public:
        void Push();
        void Pop();

        void SetWireframe(bool wire);
        void SetCull(CullMode cm);

        void SetDepthEnable(bool enable);
        void SetDepthWrite(bool enable);
        void SetDepthFunc(Compare cmp);

        void SetBlend(bool enable, Blend src = Blend::one, Blend dst = Blend::one, int rt_index = -1, BlendFunc bf = BlendFunc::add);
        void SetBlendSeparateAlpha(bool enable, Blend src_color = Blend::one, Blend dst_color = Blend::one, BlendFunc bf_color = BlendFunc::add,
                                                Blend src_alpha = Blend::one, Blend dst_alpha = Blend::one, BlendFunc bf_alpha = BlendFunc::add,
                                   int rt_index = -1);
        void SetColorWrite(bool enable, int rt_index = -1);

        void ValidateStates();
        States(ID3D11Device* device, ID3D11DeviceContext* device_context);
    };

    class FrameBuffer;
    class Texture2D;
    class VertexBuffer;
    class StructuredBuffer;
    class IndexBuffer;
    class Program;
    class UniformBuffer;

    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;
    using Texture2DPtr = std::shared_ptr<Texture2D>;
    using VertexBufferPtr = std::shared_ptr<VertexBuffer>;
    using StructuredBufferPtr = std::shared_ptr<StructuredBuffer>;
    using IndexBufferPtr = std::shared_ptr<IndexBuffer>;
    using ProgramPtr = std::shared_ptr<Program>;
    using UniformBufferPtr = std::shared_ptr<UniformBuffer>;   

    class Device : public std::enable_shared_from_this<Device> {
        friend class Texture2D;
        friend class VertexBuffer;
        friend class StructuredBuffer;
        friend class IndexBuffer;
        friend class Program;
        friend class UniformBuffer;
        friend class FrameBuffer;
    private:
        HWND m_wnd;
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_deviceContext;
        ComPtr<IDXGISwapChain> m_swapChain;
        ComPtr<ID3D11Texture2D> m_backBuffer;
        ComPtr<ID3D11RenderTargetView> m_RTView;
        glm::ivec2 m_last_wnd_size;

        Program* m_active_program;

        std::weak_ptr<FrameBuffer> m_active_fbo;
        FrameBuffer* m_active_fbo_ptr;

        std::unique_ptr<States> m_states;

        std::unordered_map<Sampler, ComPtr<ID3D11SamplerState>, Sampler> m_samplers;
        ID3D11SamplerState* ObtainSampler(const Sampler& s);
        void SetDefaultFBO();
        void SetViewport(const glm::vec2& size);
    public:
        Device(HWND wnd);
        ~Device(){}
    public:
        HWND Window() const;
        FrameBufferPtr SetFrameBuffer(const FrameBufferPtr& fbo, bool update_viewport = true);
        glm::ivec2 CurrentFrameBufferSize() const;

        States* States();

        Program* ActiveProgram();

        FrameBufferPtr Create_FrameBuffer();
        Texture2DPtr Create_Texture2D();
        VertexBufferPtr Create_VertexBuffer();
        StructuredBufferPtr Create_StructuredBuffer();
        IndexBufferPtr Create_IndexBuffer();
        ProgramPtr Create_Program();
        UniformBufferPtr Create_UniformBuffer();

        void BeginFrame();
        void PresentToWnd();
    };
    using DevicePtr = std::shared_ptr<Device>;

    class DevChild {
    protected:
        DevicePtr m_device;
    public:
        DevChild(const DevicePtr& device);
        DevicePtr GetDevice();
    };

    class Texture2D : public DevChild {
        friend class FrameBuffer;
        friend class Program;
    private:
        struct ivec3_hasher {
            std::size_t operator() (const glm::ivec3& v) const {
                return std::hash<int>()(v.x) ^ std::hash<int>()(v.y) ^ std::hash<int>()(v.z);
            }
        };
    private:
        TextureFmt m_fmt;
        glm::ivec2 m_size;
        int m_slices;
        int m_mips_count;
        ComPtr<ID3D11Texture2D> m_handle;
        ComPtr<ID3D11ShaderResourceView> m_srv[4];

        std::unordered_map<glm::ivec3, ComPtr<ID3D11UnorderedAccessView>, ivec3_hasher> m_uav;

        ComPtr<ID3D11RenderTargetView> BuildTargetView(int mip, int slice_start, int slice_count) const;
        ComPtr<ID3D11DepthStencilView> BuildDepthStencilView(int mip, int slice_start, int slice_count) const;
        ComPtr<ID3D11ShaderResourceView> GetShaderResourceView(bool as_array, bool as_cubemap);
        ComPtr<ID3D11UnorderedAccessView> GetUnorderedAccessView(int mip, int slice_start, int slice_count);
    public:
        TextureFmt Format() const;
        glm::ivec2 Size() const;
        int SlicesCount() const;
        int MipsCount() const;
        void SetState(TextureFmt fmt);
        void SetState(TextureFmt fmt, glm::ivec2 size, int mip_levels = 0, int slices = 1, const void* data = nullptr);
        void SetSubData(const glm::ivec2& offset, const glm::ivec2& size, int slice, int mip, const void* data);
        void GenerateMips();

        void ReadBack(void* data, int mip, int array_slice);

        Texture2D(const DevicePtr& device);
    };

    class FrameBuffer : public DevChild {
        friend class Device;
    private:
        struct Tex2D_params {
            int mip;
            int slice_start;
            int slice_count;
            bool read_only;
            Tex2D_params();
            Tex2D_params(int mip, int slice_start, int slice_count, bool ronly);
            bool operator == (const Tex2D_params& b);
        };
        enum class UAV_slot_kind { empty, tex, buf };
        struct UAV_slot {
            UAV_slot_kind kind;
            Texture2DPtr tex;
            Tex2D_params tex_params;
            StructuredBufferPtr buf;
            ComPtr<ID3D11UnorderedAccessView> view;
            int initial_counter;
            UAV_slot() {
                kind = UAV_slot_kind::empty;
                this->tex = nullptr;
                this->buf = nullptr;
            }
            UAV_slot(const Texture2DPtr& tex, int mip = 0, int slice_start = 0, int slice_count = 1) {
                kind = UAV_slot_kind::tex;
                buf = nullptr;
                this->tex = tex;
                tex_params.mip = mip;
                tex_params.read_only = false;
                tex_params.slice_count = slice_count;
                tex_params.slice_start = slice_start;
                this->initial_counter = -1;
            }
            UAV_slot(const StructuredBufferPtr& buf, int initial_counter) {
                kind = UAV_slot_kind::buf;
                this->buf = buf;
                this->tex = nullptr;
                this->initial_counter = initial_counter;
            }
        };
    private:
        Texture2DPtr m_tex[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        Tex2D_params m_tex_params[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        Texture2DPtr m_depth;
        Tex2D_params m_depth_params;

        UAV_slot m_uav[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_PS_CS_UAV_REGISTER_COUNT];

        ComPtr<ID3D11RenderTargetView> m_color_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ComPtr<ID3D11DepthStencilView> m_depth_view;

        glm::ivec2 m_size;
        bool m_colors_to_bind_dirty;
        int m_rtv_count;
        std::vector<ID3D11RenderTargetView*> m_colors_to_bind;
        int m_uav_to_bind_count;
        std::vector<ID3D11UnorderedAccessView*> m_uav_to_bind;
        std::vector<UINT> m_uav_initial_counts;
        void PrepareViews();
    public:
        void SetSizeFromWindow();
        void SetSize(const glm::ivec2& xy);
        glm::ivec2 GetSize() const;

        void Clear(int slot, const glm::vec4& color);
        void ClearDS(float depth, bool clear_depth = true, char stencil = 0, bool clear_stencil = false);

        void SetSlot(int slot, const Texture2DPtr& tex, int mip = 0, int slice_start = 0, int slice_count = 1);
        void SetDS(const Texture2DPtr& tex, int mip = 0, int slice_start = 0, int slice_count = 1, bool readonly = false);

        void ClearUAV(int slot, uint32_t v);
        void SetUAV(int slot, const Texture2DPtr& tex, int mip = 0, int slice_start = 0, int slice_count = 1);
        void SetUAV(int slot, const StructuredBufferPtr& buf, int initial_counter = -1);

        void BlitToDefaultFBO(int from_slot);

        FrameBuffer(const DevicePtr& device);
        ~FrameBuffer();
    };

    enum class LayoutType {Byte, Word, UInt, Float};
    struct LayoutField {
        std::string name;
        LayoutType type = LayoutType::Byte;
        int num_fields = 0;
        bool do_norm = false;
        int offset = 0;
        int array_size = 0;
        int Size() const;
        bool operator==(const LayoutField& l) const;
        std::size_t hash() const;
    };
    struct Layout {
        std::vector<LayoutField> fields;
        int stride;
        bool operator==(const Layout& l) const;
        std::size_t hash() const;
    };

    class VertexBuffer : public DevChild {
        friend class Program;
    private:
        const Layout* m_layout;
        int m_vert_count;
        ComPtr<ID3D11Buffer> m_handle;
    public:
        int VertexCount();
        const Layout* GetLayout() const;
        void SetState(const Layout* layout, int vertex_count, const void* data = nullptr);
        void SetSubData(int start_vertex, int num_vertices, const void* data);
        VertexBuffer(const DevicePtr& device);
    };

    class StructuredBuffer : public DevChild {
        friend class Program;
        friend class FrameBuffer;
    private:
        int m_stride;
        int m_vert_count;
        bool m_UAV_access;
        bool m_UAV_with_counter;
        ComPtr<ID3D11Buffer> m_handle;
        ComPtr<ID3D11ShaderResourceView> m_srv;
        ComPtr<ID3D11UnorderedAccessView> m_uav;
        ComPtr<ID3D11ShaderResourceView> GetShaderResourceView();
        ComPtr<ID3D11UnorderedAccessView> GetUnorderedAccessView();
    public:
        int Stride();
        int VertexCount();

        void SetState(int stride, int vertex_count, bool UAV = false, bool UAV_with_counter = false, const void* data = nullptr);
        void SetSubData(int start_vertex, int num_vertices, const void* data);

        void ReadBack(void* data);
        StructuredBuffer(const DevicePtr& device);
    };

    class IndexBuffer : public DevChild {
        friend class Program;
    private:
        int m_ind_count;
        ComPtr<ID3D11Buffer> m_handle;
    public:
        int IndexCount();
        void SetState(int ind_count, const void* data = nullptr);
        void SetSubData(int start_idx, int num_indices, const void* data);
        IndexBuffer(const DevicePtr& device);
    };

    class UniformBuffer : public DevChild {
        friend class Program;
    private:
        int m_elements_count;
        bool m_valid;
        std::vector<char> m_data;
        ComPtr<ID3D11Buffer> m_handle;
        const Layout* m_layout;
        void* FindValueDest(const char* name, int element_idx);
        void SetValue(void* dest, const void* data, int datasize);        
    public:
        const Layout* GetLayout() const;
        void SetState(const Layout* layout, int elemets_count, const void* data = nullptr);
        void SetSubData(int start_element, int num_elements, const void* data);
        void SetValue(const char* name, float v, int element_idx = 0);
        void SetValue(const char* name, int i, int element_idx = 0);
        void SetValue(const char* name, const glm::vec2& v, int element_idx = 0);
        void SetValue(const char* name, const glm::vec3& v, int element_idx = 0);
        void SetValue(const char* name, const glm::vec4& v, int element_idx = 0);
        void SetValue(const char* name, const glm::mat4& m, int element_idx = 0);
        void ValidateDynamicData();
        UniformBuffer(const DevicePtr& device);        
    };

    class Program : public DevChild {
    private:
        enum class SlotKind {uniform, texture, buffer, sampler};
        struct ShaderSlot {
            SlotKind kind;
            std::string name;
            const Layout* layout;
            int bindPoints[6] = {-1,-1,-1,-1,-1,-1};

            ComPtr<ID3D11ShaderResourceView> view;
            ComPtr<ID3D11Buffer> buffer;
            ID3D11SamplerState* sampler;

            void Select(ID3D11DeviceContext* dev) const;
            ShaderSlot() : kind(SlotKind::uniform), layout(nullptr), sampler(nullptr) {}
        };
        enum class ShaderType {vertex = 0, hull = 1, domain = 2, geometry = 3, pixel = 4, compute = 5};
        const ShaderType cShaders[6] = { ShaderType::vertex, ShaderType::hull, ShaderType::domain, ShaderType::geometry, ShaderType::pixel, ShaderType::compute };

        struct InputLayoutData {
            const Layout* vertices;
            const Layout* instances;
            int step_rate;
            ComPtr<ID3D11InputLayout> m_dx_layout;
            InputLayoutData();
            void RebuildLayout(ID3D11Device* device, const std::vector<char>& vertex_code);
        };
    private:
        std::vector<char> m_shader_data[6];
        ComPtr<ID3D11DeviceChild> m_shaders[6];

        UniformBufferPtr m_globals[6];
        bool m_globals_dirty;

        std::vector<ShaderSlot> m_slots;
        int ObtainSlotIdx(SlotKind kind, const std::string& name, const Layout* layout);
        int FindSlot(const char* name);

        std::vector<InputLayoutData> m_layouts;
        ID3D11InputLayout* ObtainLayout(const Layout* vertices, const Layout* instances, int step_rate);

        VertexBufferPtr m_selected_vbo;
        IndexBufferPtr m_selected_ibo;
        VertexBufferPtr m_selected_instances;
        int m_selected_insatnce_steprate;

        ID3D11UnorderedAccessView* m_views_uav[D3D11_PS_CS_UAV_REGISTER_COUNT];

        void AutoReflect(const void* data, int data_size, ShaderType st);
        void Load(std::istream& stream);
        void SelectTopology(PrimTopology pt);
        void SelectInputBuffers();
        bool IsProgramActive() const;
    public:
        void SelectProgram();

        void Load(const std::string& name, bool from_resources = true, const std::string& dir = "");

        void SetValue(const char* name, float v);
        void SetValue(const char* name, int i);
        void SetValue(const char* name, const glm::vec2& v);
        void SetValue(const char* name, const glm::vec3& v);
        void SetValue(const char* name, const glm::vec4& v);
        void SetValue(const char* name, const glm::mat4& m);

        void SetResource(const char* name, const UniformBufferPtr& ubo);
        void SetResource(const char* name, const StructuredBufferPtr& sbo);
        void SetResource(const char* name, const Texture2DPtr& tex, bool as_array = false, bool as_cubemap = false);
        void SetResource(const char* name, const Sampler& s);

        void SetInputBuffers(const VertexBufferPtr& vbo, const IndexBufferPtr& ibo, const VertexBufferPtr& instances, int instanceStepRate = 1);

        void DrawIndexed(PrimTopology pt, const DrawIndexedCmd& cmd);
        void DrawIndexed(PrimTopology pt, const std::vector<DrawIndexedCmd>& cmd_buf);
        void DrawIndexed(PrimTopology pt, int index_start = 0, int index_count = -1, int instance_count = -1, int base_vertex = 0, int base_instance = 0);
        void Draw(PrimTopology pt, int vert_start = 0, int vert_count = -1, int instance_count = -1, int base_instance = 0);
        
        void CS_SetUAV(int slot, const Texture2DPtr& tex, int mip, int slice_start, int slice_count);
        void CS_SetUAV(int slot, const StructuredBufferPtr& buf, int initial_counter = -1);
        void CS_ClearUAV(int slot, uint32_t v);
        void CS_ClearUAV(int slot, glm::vec4 v);
        void Dispatch(glm::ivec3 groups);

        Program(const DevicePtr& device);
        ~Program();
    };

    class LayoutBuilderIntf {
    public:
        virtual LayoutBuilderIntf* Add(const std::string& name, LayoutType type, int num_fields, bool do_norm = true, int offset = -1, int array_size = 1) = 0;
        virtual const Layout* Finish(int stride_size = -1) = 0;
    };
    LayoutBuilderIntf* LB();

    class FrameBufferBuilderIntf {
    public:
        virtual FrameBufferBuilderIntf* Reset(const DevicePtr& dev) = 0;
        virtual FrameBufferBuilderIntf* Color(TextureFmt fmt) = 0;
        virtual FrameBufferBuilderIntf* Depth(TextureFmt fmt) = 0;
        virtual FrameBufferPtr Finish() = 0;
    };
    FrameBufferBuilderIntf* FBB(const DevicePtr& dev);

    int PixelsSize(TextureFmt fmt);
}