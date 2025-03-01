#include "pipeline.h"

/*
 * Framebuffer, and [ ] are pixels.
 * We need to define how fill pixels... efficiently.
 * for each pixel:
 * [ ] [ ] ... [ ]
 * [ ] [ ] ... [ ]
 * ... ... ... ...
 * [ ] [ ] ... [ ]
 *  ^
 *  |
 *  +---------------+
 *  | color: uint32 |
 *  | ------------- | --> linked list -->
 *  | z: float      |
 *  +---------------+
 *
 *  BUT:
 *  1. linked list is sorted: go until the right spot and insert into OR:
 *  2. store it DESC, so we could check and discard list => NULL IF z > z_max.
 *  3. QUESTION: how to deallocate:
 *  preallocate linked list items and have a pointer to full memory range
 *  AND FREE(range) ALL AT ONCE.
 *
 *  ==================== PIPELINE =====================
 *                                    ┌──────────┐
 *  V  1. Vertex Buffer + Index Buffer│+ Normals │
 *  E     ─────────────┬──────────────└──────────┘
 *  R                  │
 *  T                  │
 *  E                  ▼
 *  X         ┌─────────────────┐
 *            │                 │
 *  S         │ Culling & Clip  │
 *  H         │                 │
 *  A         └────────┬────────┘
 *  D                  │
 *  E                  │
 *  R                  ▼
 * 2. Viewport + Camera space
 *     3. Rasterizer
 *     4. Fragment Shader + Lightning & Normal Maps
 *        Texturing + Alpha Blending + Z buffering
 *             + Depth Test
 */

/* before applying rasterizer make sure you stretched vectors anal:
 * viewportMat   *  projMat*cameraView*worldView*vert
 * ^^ [ScrH,ScrW]   ^^ [-1, 1]
 * */

/* len(v) and len(c) should be EXACLY 3 */

#define ALPHA_IDX     0
#define OPAQUE_THRSHD 0.98

void
rasterize ( Framebuffer * f, vec3 * v, vec4 * c )
{

#define X1 v[ 0 ][ 0 ]
#define X2 v[ 1 ][ 0 ]
#define X3 v[ 2 ][ 0 ]
#define Y1 v[ 0 ][ 1 ]
#define Y2 v[ 1 ][ 1 ]
#define Y3 v[ 2 ][ 1 ]
#define Z1 v[ 0 ][ 2 ]
#define Z2 v[ 1 ][ 2 ]
#define Z3 v[ 2 ][ 2 ]

#define FREAK_CMP <
    /* 03.01.25 ::: NOTE ::: Trying to reduce aliasing: < . */

#define MAX( a, b, c )                                  \
    ( ( a ) > ( b ) ? ( ( a ) > ( c ) ? ( a ) : ( c ) ) \
                    : ( ( b ) > ( c ) ? ( b ) : ( c ) ) )
#define MIN( a, b, c )                                  \
    ( ( a ) < ( b ) ? ( ( a ) < ( c ) ? ( a ) : ( c ) ) \
                    : ( ( b ) < ( c ) ? ( b ) : ( c ) ) )
#define CLIP( val, min, max )     \
    ( ( val ) < ( min ) ? ( min ) \
                        : ( ( val ) > ( max ) ? ( max ) : ( val ) ) )

    uint32_t xmin, xmax, ymin, ymax;
    xmin = ( uint32_t ) CLIP ( MIN ( X1, X2, X3 ), 0, f->surface->w );
    ymin = ( uint32_t ) CLIP ( MIN ( Y1, Y2, Y3 ), 0, f->surface->h );
    xmax = ( uint32_t ) CLIP ( MAX ( X1, X2, X3 ), 0, f->surface->w );
    ymax = ( uint32_t ) CLIP ( MAX ( Y1, Y2, Y3 ), 0, f->surface->h );

    int        start_idx = ( ymin * f->surface->w ) + xmin;
    DBuffer ** faint_ll  = ( f->transparent + start_idx );
    vec4 *     curr_c    = ( f->opaque_c + start_idx );
    float *    curr_z    = ( f->opaque_z + start_idx );

    vec4  ctemp;
    float denom, w1, w2, w3;

    denom = ( X1 - X3 ) * ( Y2 - Y3 ) - ( X2 - X3 ) * ( Y1 - Y3 );

    // if ( fabsf ( ( float ) denom ) < 1e-6 ) return; /* Degenerate */

    float dw1x = ( Y2 - Y3 );
    float dw2x = ( Y3 - Y1 );
    int   dypx = f->surface->w - ( xmax - xmin );
    for ( uint32_t py = ymin; py < ymax; py += 1 )
    {
        /* go to current row */
        w1 = ( ( Y2 - Y3 ) * ( xmin - 1.0f - X3 ) +
               ( X3 - X2 ) * ( py - Y3 ) );
        w2 = ( ( Y3 - Y1 ) * ( xmin - 1.0f - X3 ) +
               ( X1 - X3 ) * ( py - Y3 ) );

        for ( uint32_t px = xmin; px < xmax; px++ )
        {
            w1 += dw1x;
            w2 += dw2x;

            if ( w1 FREAK_CMP 0 ) goto l_next_pixel;
            if ( w2 FREAK_CMP 0 ) goto l_next_pixel;
            w3 = denom - w1 - w2;
            if ( w3 FREAK_CMP 0 ) goto l_next_pixel;

            float z_px = ( w1 * Z1 + w2 * Z2 + w3 * Z3 ) / denom;

            vec4 cpx;
            glm_vec4_scale ( c[ 0 ], w1, cpx );
            glm_vec4_scale ( c[ 1 ], w2, ctemp );
            glm_vec4_add ( cpx, ctemp, cpx );
            glm_vec4_scale ( c[ 2 ], w3, ctemp );
            glm_vec4_add ( cpx, ctemp, cpx );
            glm_vec4_divs ( cpx, denom, cpx );

            if ( z_px >= *curr_z )
            {
                if ( cpx[ ALPHA_IDX ] >= OPAQUE_THRSHD )
                {
                    glm_vec4_copy ( cpx, *curr_c );
                    *curr_z = z_px;
                }
                else
                {
                    DBuffer * newBuf = getAuxDBuffer ( f );
                    glm_vec4_copy ( cpx, newBuf->color );
                    newBuf->z    = z_px;
                    newBuf->next = NULL;

                    if ( ! ( *faint_ll ) )
                    {
                        ( *faint_ll ) = newBuf;
                        goto l_next_pixel;
                    }
                    while ( ( *faint_ll )->next &&
                            ( *faint_ll )->next->z >= z_px )
                    {
                        ( *faint_ll ) = ( *faint_ll )->next;
                    }
                    newBuf->next        = ( *faint_ll )->next;
                    ( *faint_ll )->next = newBuf;
                }
            }

        l_next_pixel:
            faint_ll++;
            curr_c++;
            curr_z++;
        }
        faint_ll += dypx;
        curr_c += dypx;
        curr_z += dypx;
    }
}

void
merge ( Framebuffer * f )
{

    vec4 * curr_c = f->opaque_c;
    // float * curr_z = f->opaque_z;

    uint32_t * curr_out = f->surface->pixels;
    const int  num_px   = f->surface->h * f->surface->w;
    for ( int i = 0; i < num_px; i++ )
    {
        vec4 out_color;

        glm_vec4_zero ( out_color );
        glm_vec4_copy ( *curr_c, out_color );
        glm_vec4_scale ( out_color, 255.0f, out_color );

        __m128  color     = _mm_loadu_ps ( out_color );
        __m128i color_int = _mm_cvtps_epi32 ( color );

        color_int = _mm_packs_epi32 ( color_int, color_int );
        color_int = _mm_packus_epi16 ( color_int, color_int );

        *curr_out = _mm_cvtsi128_si32 ( color_int );
        //  __m128  color     = _mm_loadu_ps ( out_color );
        //  __m128i color_int = _mm_cvttps_epi32 ( color );

        //  int r = _mm_extract_epi32 ( color_int, 0 );
        //  int g = _mm_extract_epi32 ( color_int, 1 );
        //  int b = _mm_extract_epi32 ( color_int, 2 );
        //  int a = _mm_extract_epi32 ( color_int, 3 );

        //  *curr_out = ( a << 24 ) | ( b << 16 ) | ( g << 8 ) | r;

        //  uint32_t r = ( uint32_t ) ( *out_color_temp++ );
        //  uint32_t g = ( uint32_t ) ( *out_color_temp++ );
        //  uint32_t b = ( uint32_t ) ( *out_color_temp++ );
        //  uint32_t a = ( uint32_t ) ( *out_color );
        //  *curr_out = ( r ) | ( g << 8 ) | ( b << 16 ) | ( a << 24 );

        curr_c++;
        // curr_z++;
        curr_out++;
    }
}
