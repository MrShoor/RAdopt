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

float4x4 transform_2d;
float3 pos3d;
float2 view_pixel_size;

struct VS_Output {
    float4 S_Position(pos);
    float2 S_(uv);
    nointerpolation int S_(slice_idx);
    float4 S_(color);
};

VS_Output VS(VS_Input In) {    
    AtlasSprite sprite;
    
    if (In.sprite_idx != 0xffffffff) {
        sprite = sprites_data[In.sprite_idx];
    } else {
        sprite.slice = -1;
        sprite.size = 1;
        sprite.xy = 0;
    }

    VS_Output res;    
    res.color = In.color;
    res.slice_idx = sprite.slice;

    res.pos = mul(viewproj, float4(pos3d, 1.0));
    res.pos.xy /= res.pos.w;
    res.pos.xy /= view_pixel_size;
    float2 offset2d = mul(transform_2d, float4(In.coord.x, In.coord.y, 0.0, 1.0)).xy;
    res.pos.xy += float2(offset2d.x, -offset2d.y);
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
    res.color = In.color;
    if (In.slice_idx > 0)
        res.color *= atlas.Sample(atlasSampler, float3(In.uv, In.slice_idx));
    return res;
}