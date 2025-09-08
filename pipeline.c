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
#define EPS           1e-6f

#define U8_COPY( src, dst )          \
    do {                             \
        ( dst )[ 0 ] = ( src )[ 0 ]; \
        ( dst )[ 1 ] = ( src )[ 1 ]; \
        ( dst )[ 2 ] = ( src )[ 2 ]; \
        ( dst )[ 3 ] = ( src )[ 3 ]; \
    } while ( 0 )

#define VEC4_TO_U8( src, dst )                                    \
    do {                                                          \
        ( dst )[ 0 ] = ( uint8_t ) floorf ( ( src )[ 0 ] * 255 ); \
        ( dst )[ 1 ] = ( uint8_t ) floorf ( ( src )[ 1 ] * 255 ); \
        ( dst )[ 2 ] = ( uint8_t ) floorf ( ( src )[ 2 ] * 255 ); \
        ( dst )[ 3 ] = ( uint8_t ) floorf ( ( src )[ 3 ] * 255 ); \
    } while ( 0 )

#define MAXF( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MINF( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

static inline void
draw_line ( float   x1,
            float   y1,
            vec4    c1,
            float   x2,
            float   y2,
            vec4    c2,
            int     dy,
            Bound * b,
            float ( *round ) ( float ) )
{
    int   r, cy;
    float cx, dx;
    vec4  cc, dc;
    // we will make r steps from V1 to V2

    r = floorf ( MAXF ( y1, y2 ) ) - ceilf ( MINF ( Y1, Y2 ) ) + 1;

    if ( r <= 0 ) return;
    // c = current, d = delta
    glm_vec4_sub ( c2, c1, dc );
    glm_vec4_divs ( dc, r, dc );

    glm_vec4_copy ( c1, cc );

    cy = round ( y1 );

    dx = fabsf ( y2 - y1 ) > EPS ? dy * ( x2 - x1 ) / ( y2 - y1 ) : 0;
    cx = x1 + dx * ( ( float ) round ( y1 ) - y1 );

    for ( int i = 0; i < r; i++ )
    {
        uint8_t * c = b->c[ cy ];
        VEC4_TO_U8 ( cc, c );

        b->x[ cy ] = round ( cx );

        // step
        glm_vec4_add ( cc, dc, cc );
        cy += dy;
        cx += dx;
    }
}

void
rasterize ( Framebuffer * f, vec3 * v, uint64_t * zi, vec4 * c )
{

    int w = f->surface->w;
    int h = f->surface->h;

    // bounds check all three vertices: non-cool cooling
    if ( v[ 0 ][ 0 ] < 0 || v[ 1 ][ 0 ] < 0 || v[ 2 ][ 0 ] < 0 ||
         v[ 0 ][ 1 ] < 0 || v[ 1 ][ 1 ] < 0 || v[ 2 ][ 1 ] < 0 ||
         v[ 0 ][ 0 ] > w - 1 || v[ 1 ][ 0 ] > w - 1 || v[ 2 ][ 0 ] > w - 1 ||
         v[ 0 ][ 1 ] > h - 1 || v[ 1 ][ 1 ] > h - 1 || v[ 2 ][ 1 ] > h - 1 )
    {
        return;
    }

    int   tl = 0, lm = 0, ri = 0;
    float xmin = v[ 0 ][ 0 ];
    float ymin = v[ 0 ][ 1 ];

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
            break;
        }
    }

#define X1  v[ tl ][ 0 ]
#define Y1  v[ tl ][ 1 ]
#define Z1  v[ tl ][ 2 ]
#define ZI1 zi[ tl ]
#define C1  c[ tl ]

#define X2  v[ lm ][ 0 ]
#define Y2  v[ lm ][ 1 ]
#define Z2  v[ lm ][ 2 ]
#define ZI2 zi[ lm ]
#define C2  c[ lm ]

#define X3  v[ ri ][ 0 ]
#define Y3  v[ ri ][ 1 ]
#define Z3  v[ ri ][ 2 ]
#define ZI3 zi[ ri ]
#define C3  c[ ri ]
    // printf ( "+-------------------------------+\n" );
    // printf ( "|  Tri |     X     Y     Z      |\n" );
    // printf ( "+-------------------------------+\n" );
    // printf ( "|  T1  | %5f %5f %5f |\n", X1, Y1, Z1 );
    // printf ( "|  T2  | %5f %5f %5f |\n", X2, Y2, Z2 );
    // printf ( "|  T3  | %5f %5f %5f |\n", X3, Y3, Z3 );
    // printf ( "+-------------------------------+\n" );

    //   if ( fabsf ( ( float ) ( X1 - X3 ) * ( Y2 - Y3 ) - ( X2 - X3 ) * ( Y1
    //   - Y3 ) ) < 1e-6 ) return; /* Degenerate */

    /*
                       _,.-'"\ top-est, left-most
    leftmost     _,.-'"   V1  \  (X1, Y1, ...)
 (X2, Y2...),.-'"              \
     _,.-'"                     \
     '-._ V2                     \
         '-._                     \
             '-._                  \
                 '-._               \
                     '-._            \
                         '-._         \
                             '-:_   V3 \
                                 '-._   \
                         right       '-._)
                    (X3, Y3, Z3 ...)
      */

    int   min_y = ceilf ( Y1 ), max_y = 0;
    int   r, cy, dy;
    float cx, dx;
    vec4  cc, dc;
    // we will make r steps from V1 to V2

    r = floorf ( Y2 ) - ceilf ( Y1 ) + 1;

    if ( r <= 0 ) goto draw_v2v3;
    // c = current, d = delta
    glm_vec4_sub ( C2, C1, dc );
    glm_vec4_divs ( dc, r + 1, dc );

    glm_vec4_copy ( C1, cc );

    cy = ceilf ( Y1 );
    dy = 1;

    dx = fabsf ( Y2 - Y1 ) > EPS ? dy * ( X2 - X1 ) / ( Y2 - Y1 ) : 0;
    cx = X1 + dx * ( ( float ) ceilf ( Y1 ) - Y1 );

    for ( int i = 0; i < r; i++ )
    {
        uint8_t * c = f->l.c[ cy ];
        VEC4_TO_U8 ( cc, c );

        f->l.x[ cy ] = ceilf ( cx );

        // step
        glm_vec4_add ( cc, dc, cc );
        cy += dy;
        cx += dx;
    }
draw_v2v3:
    if ( Y3 >= Y2 )
    {
        max_y = floorf ( Y3 );
        r     = floorf ( Y3 ) - ceilf ( Y2 ) + 1;

        if ( r <= 0 ) goto draw_v3v1;

        glm_vec4_sub ( C3, C2, dc );
        glm_vec4_divs ( dc, r + 1, dc );
        glm_vec4_copy ( C2, cc );

        cy = ceilf ( Y2 );
        dy = 1;
        dx = fabsf ( Y3 - Y2 ) > EPS ? dy * ( X3 - X2 ) / ( Y3 - Y2 ) : 0;
        cx = X2 + dx * ( ( float ) ceilf ( Y2 ) - Y2 );

        for ( int i = 0; i < r; i++ )
        {
            uint8_t * c = f->l.c[ cy ];
            VEC4_TO_U8 ( cc, c );

            f->l.x[ cy ] = ceilf ( cx );

            // step
            glm_vec4_add ( cc, dc, cc );
            cy += dy;
            cx += dx;
        }
    }
    else
    {
        max_y = floorf ( Y2 );
        r     = floorf ( Y2 ) - ceilf ( Y3 ) + 1;

        if ( r <= 0 ) goto draw_v3v1;

        glm_vec4_sub ( C3, C2, dc );
        glm_vec4_divs ( dc, r + 1, dc );
        glm_vec4_copy ( C2, cc );

        cy = floorf ( Y2 );
        dy = -1;
        dx = fabsf ( Y3 - Y2 ) > EPS ? dy * ( X3 - X2 ) / ( Y3 - Y2 ) : 0;
        cx = X2 + dx * ( ( float ) floorf ( Y2 ) - Y2 );

        for ( int i = 0; i < r; i++ )
        {
            uint8_t * c = f->r.c[ cy ];
            VEC4_TO_U8 ( cc, c );

            f->r.x[ cy ] = floorf ( cx );

            // step
            glm_vec4_add ( cc, dc, cc );
            cy += dy;
            cx += dx;
        }
    }

draw_v3v1:
    r = floorf ( Y3 ) - ceilf ( Y1 ) + 1;

    if ( r <= 0 ) goto end_draw_bounds;

    glm_vec4_sub ( C1, C3, dc );
    glm_vec4_divs ( dc, r + 1, dc );
    glm_vec4_copy ( C3, cc );

    cy = floorf ( Y3 );
    dy = -1;
    dx = fabsf ( Y3 - Y1 ) > EPS ? dy * ( X3 - X1 ) / ( Y3 - Y1 ) : 0;
    cx = X3 + dx * ( ( float ) floorf ( Y3 ) - Y3 );

    for ( int i = 0; i < r; i++ )
    {
        uint8_t * c = f->r.c[ cy ];
        VEC4_TO_U8 ( cc, c );

        f->r.x[ cy ] = floorf ( cx );

        // step
        glm_vec4_add ( cc, dc, cc );
        cy += dy;
        cx += dx;
    }
end_draw_bounds:

    DBuffer ** faint_ll;
    icolor_t * curr_c;
    uint64_t * curr_z;

    icolor_t ctemp;

    // printf ( "minimax: %d %d\n", min_y, max_y );
    for ( int py = min_y; py <= max_y; py++ )
    {
        int l_x = f->l.x[ py ];
        int r_x = f->r.x[ py ];
        if ( l_x > r_x ) continue;

        // faint_ll = ( f->transparent + ( py * f->surface->w ) + l_x );
        // curr_c = ( f->opaque_c + ( py * f->surface->w ) + l_x );
        // curr_z   = ( f->opaque_z + ( py * f->surface->w ) + l_x );

        curr_c = ( f->opaque_c + ( py * f->surface->w ) );
        U8_COPY ( f->l.c[ py ], *( curr_c + l_x ) );
        U8_COPY ( f->r.c[ py ], *( curr_c + r_x ) );
        //  for ( int px = l_x; px <= r_x; px++ )
        //  {
        // if ( z_px < *curr_z ) goto l_next_pixel;

        // if ( cpx[ ALPHA_IDX ] >= OPAQUE_THRSHD )
        //{
        //     glm_vec4_copy ( cpx, *curr_c );
        //     *curr_z = z_px;
        // }
        // else
        //{
        //     DBuffer * newBuf = getAuxDBuffer ( f );
        //     glm_vec4_copy ( cpx, newBuf->color );
        //     newBuf->z    = z_px;
        //     newBuf->next = NULL;

        //    if ( ! ( *faint_ll ) )
        //    {
        //        ( *faint_ll ) = newBuf;
        //        goto l_next_pixel;
        //    }
        //    while ( ( *faint_ll )->next &&
        //            ( *faint_ll )->next->z >= z_px )
        //    {
        //        ( *faint_ll ) = ( *faint_ll )->next;
        //    }
        //    newBuf->next        = ( *faint_ll )->next;
        //    ( *faint_ll )->next = newBuf;
        //}

        //   l_next_pixel:
        //       faint_ll++;
        //       curr_c++;
        //       curr_z++;
        //   }
    }
}

void
merge ( Framebuffer * f )
{

    icolor_t * curr_c = f->opaque_c;
    // float * curr_z = f->opaque_z;

    uint32_t * curr_out = f->surface->pixels;
    const int  num_px   = f->surface->h * f->surface->w;
    for ( int i = 0; i < num_px; i++ )
    {
        uint8_t * c = *curr_c;
        *curr_out =
            ( c[ 3 ] << 24 ) | ( c[ 2 ] << 16 ) | ( c[ 1 ] << 8 ) | c[ 0 ];

        curr_c++;
        curr_out++;
    }
}

/*
 *
 * 'G' ??? vim user
 ⠀⠀⠀⠀⠀⠀⠀⠀⢾⡄⠀⠘⣷⣸⣷⣄⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⣀⡀⠀⠀⠀⠀⠀⢀⣾⣿⠋⠀⠆⢀⢻⠓⢸⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠈⠳⡀⠀⠘⠿⣿⣿⣧⡀⠀⠀⠀⠺⣏⠉⠀⠀⠈⣹⠗⠀⠀⠀⢀⣾⡿⡿⠀⠀⠀⡸⣼⠀⡜⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⡄⠀⠀⠹⠛⠉⠻⠷⣄⠀⠀⠈⠓⢤⡠⠞⠁⠀⠀⣠⣴⡿⠟⢠⠇⠀⠀⢀⣷⣿⣄⡇⠀⢀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⢤⡀⠀⠀⠀⠀⠐⠦⣿⣿⣶⣄⠀⠀⠀⠀⣀⣴⣾⠿⠋⠀⠀⣼⡀⠀⢀⣾⡟⠈⣿⣷⡀⢿⡄⠀⠀⢀⣀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠲⢄⡀⠀⠀⠀⠈⠙⠿⠿⣿⣶⡶⠾⠛⠋⠀⠀⠀⠀⢠⠏⠀⣠⠋⡿⠀⢠⣿⣿⢳⡘⣇⠀⠀⣸⣿
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠑⠤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⢀⡔⠁⡼⠁⠀⠀⣿⢹⡀⣷⣹⣷⣯⢉⡛
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⢋⡴⠋⠀⡴⠁⠀⠀⠀⣿⣼⡇⠹⣿⣿⠈⠻⣭
⠀⠀⠐⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⠟⠉⠀⠀⠈⠀⠀⠀⠀⠀⡿⢹⡇⠀⢿⣿⣧⠀⠖
⠀⠀⠀⠀⢹⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡠⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⡇⣿⠁⠀⠀⢹⡇⣇⠀
⣀⠀⢀⣠⣾⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠊⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⠃⣿⠀⠀⠀⠀⢻⣿⠤
⡉⡛⠉⣿⣿⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⢸⣏⠀⠀⠀⠀⢸⢻⣁
⡟⠁⢀⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⢁⢇⠃⠀⠀⠀⠀⠈⢿⠹
⢷⣄⣾⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠃⣞⠀⠀⠀⠀⠀⠀⠀⢸⠀
⠘⣿⣿⣿⠁⠀⠀⠀⠀⠀4uCkIn'⠀⠀⠀⠀⠀⠀⠀⠘⠀⠀⠀⠀C⠀⠀⠀⠀⠀⠀⠀⢀⠎⣼⠏⠀⠀⠀⠀⠀⠀⠀⢸⠀
⢣⢹⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠎⣸⠃⠀⠀⠀⠀⠀⠀⠀⠀⢸⣖
⡈⣧⢻⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠋⡼⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿
⡄⣻⣾⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⢃⡜⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡿
⡇⠉⢿⡸⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⢀⡜⣡⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⠀
⢿⡆⠈⢻⢳⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣧⠀⠀⠀⠀⠀⠀⣠⢊⡴⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠇⣴
⣿⣿⠀⠈⡎⢧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣆⠀⠀⠀⣠⢞⡴⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣏⣼⢏
⣬⣿⢿⣦⡸⡄⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣿⣷⡶⠚⣡⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡾⢇⡟⢡⡏
⠙⢿⣄⡙⢷⣿⣄⠻⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣴⣿⡿⠛⢁⡴⠊⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⠋⣠⠞⢀⠏⠁
⠉⠉⠉⠉⠉⠙⠻⣦⡙⣿⣦⠀⠀⠀⠀⣀⣀⡠⢴⢒⣻⠿⠛⣉⠤⠾⠙⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠤⠞⠋⠉⠉⠉⠉⠉⠉⠉⠙
⠉⠉⠉⠉⠉⠉⠉⠉⠉⠛⠿⣷⡿⠿⠯⠴⠖⠒⣋⣩⣴⠶⠭⠤⠤⠴⠶⠾⠦⠤⠼⠶⢒⣒⣞⣉⣉⣁⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠙
 */
