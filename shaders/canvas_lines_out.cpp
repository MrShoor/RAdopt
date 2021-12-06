#include "hlsl.h"
#include "camera.h"

struct VS_Input {
    float4 S_(coords);
    float4 S_(normals);
    float4 S_(color);
    float2 S_(width);
    float  S_(hinting);
    uint S_VertexID(vid);
};

float4x4 transform_2d;
float3 pos3d;
float2 view_pixel_size;

struct VS_Output {
    float4 S_Position(pos);
    float4 S_(color);
};

static const float2 quad[4] = {{0,0}, {0,1}, {1,0}, {1,1}};

VS_Output VS(VS_Input In) {
    float end_k = In.vid >= 2;
    float2 crd = lerp(In.coords.xy, In.coords.zw, end_k);
    float2 n = lerp(In.normals.xy, In.normals.zw, end_k);

    VS_Output res;    
    res.color = In.color;

    res.pos = mul(viewproj, float4(pos3d, 1.0));
    res.pos.xy /= res.pos.w;
    // res.pos.xy += In.coord * view_pixel_size;
    res.pos.xy /= view_pixel_size;
    float2 offset2d = mul(transform_2d, float4(crd, 0.0, 1.0)).xy;
    res.pos.xy += float2(offset2d.x, -offset2d.y);
    if (In.hinting) res.pos.xy = round(res.pos.xy);    
    res.pos.xy += n * (In.vid % 2) * In.width.x;
    res.pos.xy *= view_pixel_size;
    res.pos.xy *= res.pos.w;

    return res;
}

///////////////////////////////////////////////

struct PS_Output {
    float4 S_Target0(color);
};

PS_Output PS(VS_Output In) {
    PS_Output res;
    res.color = In.color;
    res.color.xyz *= res.color.a;
    return res;
}