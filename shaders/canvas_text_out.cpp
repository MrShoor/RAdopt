#include "hlsl.h"
#include "camera.h"

struct TextGlyphVertex3D {
    float2 pos;
    float2 size;
    float4 color;
    float halign;
    float sdfoffset;

    float2 sprite_xy;
    float2 sprite_size;
    int slice_idx;
    float dummy;

    float4 bounds2d;
    float valign;
};

float4x4 transform_2d;
float3 pos3d;
StructuredBuffer<TextGlyphVertex3D> glyphs;
float2 view_pixel_size;
Texture2DArray atlas; SamplerState atlasSampler;
float flipY;

struct VS_Output {
    float4 S_Position(pos);
    float2 S_(uv);
    int S_(slice_idx);
    float4 S_(color);
    float S_(sdfoffset);
};

static const float2 quad[4] = {{0,0}, {0,1}, {1,0}, {1,1}};
static const float2 quadUV[4] = {{0,0}, {0,1}, {1,0}, {1,1}};

VS_Output VS(uint S_VertexID(vid), uint S_InstanceID(iid)) {
    VS_Output res;
    TextGlyphVertex3D gv = glyphs[iid];
    res.color = gv.color;
    res.sdfoffset = gv.sdfoffset;
    res.slice_idx = gv.slice_idx;    

    float2 aligned_pos = lerp(gv.bounds2d.xy, gv.bounds2d.zw, float2(gv.halign, gv.valign)) + gv.pos;

    res.pos = mul(viewproj, float4(pos3d, 1.0));
    res.pos.xy /= res.pos.w;
    res.pos.xy /= view_pixel_size;
    float2 offset2d = (quad[vid] * gv.size - gv.size * 0.5 + aligned_pos);
    offset2d = mul(transform_2d, float4(offset2d, 0.0, 1.0)).xy;
    res.pos.xy += float2(offset2d.x, -offset2d.y);
    res.pos.xy *= view_pixel_size;
    res.pos.xy *= res.pos.w;

    float w, h, e;
    atlas.GetDimensions(w, h, e);
    res.uv = gv.sprite_xy / float2(w,h) + gv.sprite_size / float2(w,h) * quadUV[vid];

    return res;
}

///////////////////////////////////////////////

struct PS_Output {
    float4 S_Target0(color);
};

PS_Output PS(VS_Output In) {
    PS_Output res;
    res.color = In.color;
    res.color.a *= saturate(0.5 - atlas.Sample(atlasSampler, float3(In.uv, In.slice_idx)).r + In.sdfoffset);
    return res;
}