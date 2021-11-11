#include "hlsl.h"
#include "camera.h"

struct VS_Input {
    uint S_VertexID(vid);

    float2 S_(pos);
    float2 S_(size);
    float4 S_(color);
    float S_(halign);
    float S_(sdfoffset);

    float2 S_(sprite_xy);
    float2 S_(sprite_size);
    int S_(slice_idx);
    float S_(dummy);

    float4 S_(bounds2d);
    float S_(valign);
};

float4x4 transform_2d;
float3 pos3d;
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

VS_Output VS(VS_Input In) {
    VS_Output res;
    res.color = In.color;
    res.sdfoffset = In.sdfoffset;
    res.slice_idx = In.slice_idx;

    float2 aligned_pos = lerp(In.bounds2d.xy, In.bounds2d.zw, float2(In.halign, In.valign)) + In.pos;

    res.pos = mul(viewproj, float4(pos3d, 1.0));
    res.pos.xy /= res.pos.w;
    res.pos.xy /= view_pixel_size;
    float2 offset2d = (quad[In.vid] * In.size - In.size * 0.5 + aligned_pos);
    offset2d = mul(transform_2d, float4(offset2d, 0.0, 1.0)).xy;
    res.pos.xy += float2(offset2d.x, -offset2d.y);
    res.pos.xy *= view_pixel_size;
    res.pos.xy *= res.pos.w;

    float w, h, e;
    atlas.GetDimensions(w, h, e);
    res.uv = In.sprite_xy / float2(w,h) + In.sprite_size / float2(w,h) * quadUV[In.vid];

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