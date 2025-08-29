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
rasterize ( Framebuffer * f, vec3 * v, uint64_t * zi, vec4 * c )
{

    if ( X1 < 0 || X2 < 0 || X3 < 0 || Y1 < 0 || Y2 < 0 || Y3 < 0 ||
         X1 > f->surface->w - 1 || X2 > f->surface->w - 1 ||
         X3 > f->surface->w - 1 || Y1 > f->surface->h - 1 ||
         Y2 > f->surface->h - 1 || Y3 > f->surface->h - 1 )
    {
        return;
    }

    int   tl = 0, lm = 0, ri = 0;
    float ymin = Y1;
    float xmin = X1;

    for ( int i = 1; i < 3; i++ )
    {
        if ( ( v[ i ][ 1 ] < ymin ) ||
             ( v[ i ][ 1 ] == ymin && v[ i ][ 0 ] < xmin ) )
        {
            tl   = i;
            xmin = v[ i ][ 0 ];
            ymin = v[ i ][ 1 ];
        }
    }

    xmin = f->surface->w;
    ymin = f->surface->h;

    for ( int i = 0; i < 3; i++ )
    {
        if ( i == tl ) continue;

        if ( ( v[ i ][ 0 ] < xmin ) ||
             ( v[ i ][ 0 ] == xmin && v[ i ][ 0 ] < ymin ) )
        {
            lm   = i;
            xmin = v[ i ][ 0 ];
            ymin = v[ i ][ 1 ];
        }
    }

    for ( int i = 0; i < 3; i++ )
    {
        if ( i != tl && i != lm )
        {
            ri = i;
            break
        }
    }

#define X1  v[ tl ][ 0 ]
#define Y1  v[ tl ][ 1 ]
#define Z1  v[ tl ][ 2 ]
#define ZI1 zi[ tl ]

#define X2  v[ lm ][ 0 ]
#define Y2  v[ lm ][ 1 ]
#define Z2  v[ lm ][ 2 ]
#define ZI2 zi[ lm ]

#define X3  v[ ri ][ 0 ]
#define Y3  v[ ri ][ 1 ]
#define Z3  v[ ri ][ 2 ]
#define ZI3 zi[ ri ]

    /*
                       _,.-'"\ top-est, left-most
    leftmost     _,.-'"       \  (X1, Y1, ...)
 (X2, Y2...),.-'"              \
     _,.-'"                     \
     '-._                        \
         '-._                     \
             '-._                  \
                 '-._               \
                     '-._            \
                         '-._         \
                             '-:_      \
                                 '-._   \
                         right       '-._)
                    (X3, Y3, Z3 ...)
      */


    DBuffer ** faint_ll;
    vec4 *     curr_c;
    uint64_t * curr_z;

    vec4  ctemp;
    float denom, w1, w2, w3;

    denom = ( X1 - X3 ) * ( Y2 - Y3 ) - ( X2 - X3 ) * ( Y1 - Y3 );

    if ( fabsf ( ( float ) denom ) < 1e-6 ) return; /* Degenerate */

    float dw1x  = ( Y2 - Y3 ) / denom;
    float dw2x  = ( Y3 - Y1 ) / denom;
    float dw3x  = -dw1x - dw2x;
    float y2sy3 = ( Y2 - Y3 ) / denom;
    float y3sy1 = ( Y3 - Y1 ) / denom;
    float x3sx2 = ( X3 - X2 ) / denom;
    float x1sx3 = ( X1 - X3 ) / denom;

    for ( int py = ymin; py <= ymax; py++ )
    {
        int l_x = f->min_x[ py ], r_x = f->max_x[ py ];
        /* go to current row */
        float fx = ( l_x + 0.5f ) - X3;
        float fy = ( py + 0.5f ) - Y3;

        w1 = ( y2sy3 * fx + x3sx2 * fy );
        w2 = ( y3sy1 * fx + x1sx3 * fy );
        w3 = ( 1 - w1 - w2 );

        faint_ll = ( f->transparent + ( py * f->surface->w ) + l_x );
        curr_c   = ( f->opaque_c + ( py * f->surface->w ) + l_x );
        curr_z   = ( f->opaque_z + ( py * f->surface->w ) + l_x );

        for ( int px = l_x; px <= r_x; px++ )
        {
            if ( w1 < 0 || w2 < 0 || w3 < 0 ) { goto l_next_pixel; }

            uint64_t z_px = ( w1 * ( float ) ZI1 + w2 * ( float ) ZI2 +
                              w3 * ( float ) ZI3 );

            vec4 cpx;
            glm_vec4_scale ( c[ 0 ], w1, cpx );
            glm_vec4_scale ( c[ 1 ], w2, ctemp );
            glm_vec4_add ( cpx, ctemp, cpx );
            glm_vec4_scale ( c[ 2 ], w3, ctemp );
            glm_vec4_add ( cpx, ctemp, cpx );
            // glm_vec4_divs ( cpx, denom, cpx );

            if ( z_px < *curr_z ) goto l_next_pixel;

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

        l_next_pixel:
            w1 += dw1x;
            w2 += dw2x;
            w3 += dw3x;

            faint_ll++;
            curr_c++;
            curr_z++;
        }
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
