#include "hlsl.h"
#include "camera.h"

struct VS_Input {
	uint S_VertexID(vid);
};

struct VS_Output {
    float4 S_Position(pos);
    float4 S_(color);
};

static const float2 quad[4] = {{0,0}, {0,1}, {1,0}, {1,1}};

VS_Output VS(VS_Input In) {
	VS_Output res;
	res.pos.xy = quad[In.vid] * 2.0 - 1.0;
	res.pos.zw = float2(0.0, 1.0);
	res.color.xy = quad[In.vid];
    res.color.zw = float2(0.0, 1.0);
    return res;
}

///////////////////////////////////////////////

struct PS_Output {
    float4 S_Target0(color);
};

PS_Output PS(VS_Output In) {
    PS_Output res;
    res.color = In.color;
    return res;
}