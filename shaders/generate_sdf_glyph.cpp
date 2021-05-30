#include "hlsl.h"

float4 rect;
StructuredBuffer<float4> segments;
uint segments_count;
int slice;

RWTexture2DArray<float> out_tex register_(u0);

float cross2D(float2 p1, float2 p2) {
    return p1.x * p2.y - p1.y * p2.x;
}

float SegDistSqr(float4 seg, float2 pt) {
    float2 seg_dir = seg.zw - seg.xy;
    float2 dir1 = pt - seg.xy;
    float2 dir2 = pt - seg.zw;
    if (dot(dir1, seg_dir) <= 0) return dot(dir1, dir1);
    if (dot(dir2, seg_dir) >= 0) return dot(dir2, dir2);
    float s = cross2D(dir1, seg_dir);
    return s * s / dot(seg_dir, seg_dir);
}

bool PtIn(float4 seg, float2 pt) {
    float4 s = (seg.x > seg.z) ? seg.zwxy : seg;
    if (pt.x < s.x) return false;
    if (pt.x >= s.z) return false;
    return cross2D(s.zw - s.xy, pt - s.xy) < 0;
}

[numthreads(32,32,1)]
void CS(uint3 S_DispatchThreadID(id)) {
    if (id.x >= uint(rect.z)) return;
    if (id.y >= uint(rect.w)) return;
    float2 pt = id.xy + rect.xy;
    float dist = 100000000.0;
    uint summ = 0;
    for (uint i = 0; i < segments_count; i++) {
        float4 seg = segments[i] + rect.xyxy;
        dist = min(dist, SegDistSqr(seg, pt));
        summ += PtIn(seg, pt) ? 1 : 0;
    }
    dist = sqrt(abs(dist));
    if (summ % 2) dist = -dist;
    uint3 pix = uint3(pt, slice);
    out_tex[pix] = dist;
}