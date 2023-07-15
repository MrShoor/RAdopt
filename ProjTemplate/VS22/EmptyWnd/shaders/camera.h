#include "hlsl.h"
#ifndef CAMERA_H
#define CAMERA_H

CBuffer(camera) {
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float4x4 view_inv;
    float4x4 proj_inv;
    float4x4 view_proj_inv;
    float2 z_near_far;
    float2 dummy;
};

#endif