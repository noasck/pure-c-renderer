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

static inline __attribute__ ( ( always_inline ) ) int
fast_min ( int a, int b )
{
    return b ^ ( ( a ^ b ) & -( a < b ) );
}

static inline __attribute__ ( ( always_inline ) ) int
fast_max ( int a, int b )
{
    return a ^ ( ( a ^ b ) & -( a < b ) );
}

static inline __attribute__ ( ( always_inline ) ) int
min3 ( int a, int b, int c )
{
    return fast_min ( fast_min ( a, b ), c );
}

static inline __attribute__ ( ( always_inline ) ) int
max3 ( int a, int b, int c )
{
    return fast_max ( fast_max ( a, b ), c );
}

static inline __attribute__ ( ( always_inline ) ) int
clip ( int val, int min, int max )
{
    return fast_max ( min, fast_min ( max, val ) );
}

static inline __attribute__ ( ( always_inline ) ) int
fast_floorf ( float x )
{
    int i = ( int ) x;
    return ( x < 0 && x != ( float ) i ) ? i - 1 : i;
}

static inline __attribute__ ( ( always_inline ) ) int
fast_ceilf ( float x )
{
    int i = ( int ) x;
    return ( x > 0 && x != ( float ) i ) ? i + 1 : i;
}

void
draw_edgef ( float x0,
             float y0,
             float x1,
             float y1,
             int * l_x,
             int * r_x,
             int   min_y,
             int   max_y,
             int   min_x,
             int   max_x )
{
    if ( y0 > y1 )
    {
        float tx = x0, ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }

    float dy = y1 - y0;
    if ( dy == 0.0f ) return;

    float dx      = ( x1 - x0 ) / dy;
    int   y_start = ( int ) ceilf ( y0 );
    int   y_end   = ( int ) floorf ( y1 );
    if ( y_end < min_y || y_start > max_y ) return;

    if ( y_start < min_y ) y_start = min_y;
    if ( y_end > max_y ) y_end = max_y;

    float x = x0 + dx * ( y_start - y0 );
    for ( int y = y_start; y <= y_end; y++ )
    {
        int xi = ( int ) ( x + 0.5f );
        if ( xi < min_x ) xi = min_x;
        if ( xi > max_x ) xi = max_x;
        if ( xi < l_x[ y ] ) l_x[ y ] = xi;
        if ( xi > r_x[ y ] ) r_x[ y ] = xi;
        x += dx;
    }
}

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

    if ( X1 < 0 || X2 < 0 || X3 < 0 || Y1 < 0 || Y2 < 0 || Y3 < 0 ||
         X1 > f->surface->w - 1 || X2 > f->surface->w - 1 ||
         X3 > f->surface->w - 1 || Y1 > f->surface->h - 1 ||
         Y2 > f->surface->h - 1 || Y3 > f->surface->h - 1 )
    {
        return;
    }

#define FREAK_CMP <=
    /* 03.01.25 ::: NOTE ::: Trying to reduce aliasing: < . */

    int xmin = ( int ) fast_floorf ( min3 ( X1, X2, X3 ) );
    int ymin = ( int ) fast_floorf ( min3 ( Y1, Y2, Y3 ) );
    int xmax = ( int ) fast_ceilf ( max3 ( X1, X2, X3 ) );
    int ymax = ( int ) fast_ceilf ( max3 ( Y1, Y2, Y3 ) );

    for ( int i = ymin; i <= ymax; i++ )
    {
        // fill with sentinel value:
        f->min_x[ i ] = xmax + 1;
        f->max_x[ i ] = xmin - 1;
    }

    draw_edgef ( X1, Y1, X2, Y2, f->min_x, f->max_x, ymin, ymax, xmin, xmax );
    draw_edgef ( X2, Y2, X3, Y3, f->min_x, f->max_x, ymin, ymax, xmin, xmax );
    draw_edgef ( X3, Y3, X1, Y1, f->min_x, f->max_x, ymin, ymax, xmin, xmax );

    DBuffer ** faint_ll;
    vec4 *     curr_c;
    float *    curr_z;

    vec4  ctemp;
    float denom, w1, w2, w3;

    denom = ( X1 - X3 ) * ( Y2 - Y3 ) - ( X2 - X3 ) * ( Y1 - Y3 );

    if ( fabsf ( ( float ) denom ) < 1e-6 ) return; /* Degenerate */

    float dw1x  = ( Y2 - Y3 );
    float dw2x  = ( Y3 - Y1 );
    float dw3x  = -dw1x - dw2x;
    float y2sy3 = ( Y2 - Y3 );
    float y3sy1 = ( Y3 - Y1 );
    float x3sx2 = ( X3 - X2 );
    float x1sx3 = ( X1 - X3 );

    for ( int py = ymin; py <= ymax; py++ )
    {
        int l_x = f->min_x[ py ], r_x = f->max_x[ py ];
        /* go to current row */
        float fx = ( l_x + 0.5f ) - X3;
        float fy = ( py + 0.5f ) - Y3;

        w1 = y2sy3 * fx + x3sx2 * fy;
        w2 = y3sy1 * fx + x1sx3 * fy;
        w3 = denom - w1 - w2;

        faint_ll = ( f->transparent + ( py * f->surface->w ) + l_x );
        curr_c   = ( f->opaque_c + ( py * f->surface->w ) + l_x );
        curr_z   = ( f->opaque_z + ( py * f->surface->w ) + l_x );

        for ( int px = l_x; px <= r_x; px++ )
        {
            if ( w1 < 0 || w2 < 0 || w3 < 0 ) { goto l_next_pixel; }

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
