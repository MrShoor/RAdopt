#include "hlsl.h"
#include "camera.h"

struct VS_Input {
    float2 S_(coord);
    float2 S_(texcoord);
    float4 S_(color);
    float S_(hinting);
    uint S_(sprite_idx);
};

struct AtlasSprite {
    int slice;
    float2 xy;
    float2 size;    
};
StructuredBuffer<AtlasSprite> sprites_data;
Texture2DArray atlas; SamplerState atlasSampler;

float3x3 transform_2d;
float3 pos3d;
float2 view_pixel_size;

struct VS_Output {
    float4 S_Position(pos);
    float2 S_(uv);
    int S_(slice_idx);
    float4 S_(color);
};

VS_Output VS(VS_Input In) {
    AtlasSprite sprite = sprites_data[In.sprite_idx];

    VS_Output res;    
    res.color = In.color;
    res.slice_idx = sprite.slice;

    res.pos = mul(viewproj, float4(pos3d, 1.0));
    res.pos.xy /= res.pos.w;
    res.pos.xy /= view_pixel_size;
    res.pos.xy += mul(transform_2d, float3(In.coord, 1.0)).xy;
    if (In.hinting) res.pos.xy = round(res.pos.xy);
    res.pos.xy *= view_pixel_size;
    res.pos.xy *= res.pos.w;

    float w, h, e;
    atlas.GetDimensions(w, h, e);
    float2 tmp_uv = lerp(sprite.xy, sprite.xy + sprite.size, In.texcoord);
    res.uv = tmp_uv / float2(w,h);

    return res;
}

///////////////////////////////////////////////

struct PS_Output {
    float4 S_Target0(color);
};

PS_Output PS(VS_Output In) {
    PS_Output res;
    res.color = atlas.Sample(atlasSampler, float3(In.uv, In.slice_idx)) * In.color;
    return res;
}