#pragma once
#ifndef CUSTOM_RENDER_PIPELINE_H
#define CUSTOM_RENDER_PIPELINE_H

#include "engine.h"

#include <cglm/cglm.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <xmmintrin.h>

typedef struct
{
    vec4 * rect;
    vec3 * norm;

    uint32_t x;
    uint32_t y;
    uint32_t h;
    uint32_t w;
} Fragments;

void
rasterize ( Framebuffer * f, vec3 * v, vec4 * c );

void
merge ( Framebuffer * f );

#endif /* CUSTOM_RENDER_PIPELINE_H */
