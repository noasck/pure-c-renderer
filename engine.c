#include "engine.h"
#define DBG_CALLCNT( func_name ) \
    static int u_calls = 0;      \
    printf ( "%s called: %d times\n", func_name, ++u_calls );

DBufferPool *
createDBufferPool ( uint32_t size )
{
    DBufferPool * p;
    U_ALLOC ( p, DBufferPool, 1 );

    U_ALLOC ( p->nodes, DBuffer, size );
    p->current = p->nodes;
    p->size    = size;
    p->used    = 0;
    p->prev    = NULL;

    for ( uint32_t i = 0; i < size; i++ )
    {
        glm_vec4_zero ( p->nodes[ i ].color );
        p->nodes[ i ].z    = -FLT_MAX;
        p->nodes[ i ].next = NULL;
    }

    return p;
}

DBuffer *
getAuxDBuffer ( Framebuffer * f )
{
    if ( f->pool->used >= f->pool->size )
    {
        DBufferPool * p;
        p = createDBufferPool ( f->pool->size * 2 );
        if ( p == NULL ) return NULL;
        p->prev = f->pool;
        f->pool = p;
    }

    f->pool->used++;
    return f->pool->current++;
}

void
destroyDBufferPool ( DBufferPool * p )
{
    while ( p )
    {
        DBufferPool * prev = p->prev;
        free ( p->nodes );
        free ( p );
        p = prev;
    }
}

void
cleanFramebuffer ( Framebuffer * f )
{
    vec4 *         c      = f->opaque_c;
    float *        z      = f->opaque_z;
    DBuffer **     t      = f->transparent;
    uint32_t *     s_px   = f->surface->pixels;
    const uint32_t px_cnt = f->surface->h * f->surface->w;

    memset ( s_px, 0, px_cnt * sizeof ( uint32_t ) );
    memset ( t, 0, px_cnt * sizeof ( DBuffer ** ) );

    for ( uint32_t i = 0; i < px_cnt; i = i + 1 )
    {
        glm_vec4_zero ( *( c++ ) );
        *( z++ ) = -FLT_MAX;
    }

    if ( f->pool )
    {
        f->pool->used    = 0;
        f->pool->current = f->pool->nodes;

        destroyDBufferPool ( f->pool->prev );
        f->pool->prev = NULL;
    }
}

Framebuffer *
createFramebuffer ( uint32_t h, uint32_t w )
{
    Framebuffer * f;
    U_ALLOC ( f, Framebuffer, 1 );

    f->surface = SDL_CreateRGBSurfaceWithFormat (
        0, w, h, 32, SDL_PIXELFORMAT_RGBA8888 );
    if ( ! f->surface )
    {
        printf ( "SDL Error: %s\n", SDL_GetError () );
        return NULL;
    }
    typedef DBuffer * dbuffer_ptr_t;
    U_ALLOC ( f->transparent, dbuffer_ptr_t, h * w );
    U_ALLOC ( f->opaque_c, vec4, h * w );
    U_ALLOC ( f->opaque_z, float, h * w );
    f->pool = NULL;
    cleanFramebuffer ( f );

    U_ALLOC ( f->min_x, int, h );
    U_ALLOC ( f->max_x, int, h );

    f->pool = createDBufferPool ( h * w );
    if ( ! f->pool )
    {
        destroyFramebuffer ( f );
        return NULL;
    }

    return f;
}

void
destroyFramebuffer ( Framebuffer * f )
{
    if ( f )
    {
        if ( f->transparent ) free ( f->transparent );
        if ( f->opaque_z ) free ( f->opaque_z );
        if ( f->opaque_c ) free ( f->opaque_c );
        if ( f->min_x ) free ( f->min_x );
        if ( f->max_x ) free ( f->max_x );

        destroyDBufferPool ( f->pool );
        if ( f->surface )
        {
            SDL_FreeSurface ( f->surface );
            f->surface = NULL;
        }
        free ( f );
    }
}

/* IMPORTANT: (re)create Framebuffer and fb->Surface first */
int
createNKUI ( Engine * e )
{
    /* Nuklear UI */
    e->nk_ui.clear  = ( struct nk_color ) { 0, 0, 0, 0 };
    e->nk_ui.bounds = ( struct nk_rect ) { 40, 40, 0, 0 };

    e->nk_ui.bounds.w = 400;
    e->nk_ui.bounds.h = 800;

    e->nk_ui.pl.bytesPerPixel =
        e->framebuffer->surface->format->BytesPerPixel;
    e->nk_ui.pl.rshift = e->framebuffer->surface->format->Rshift;
    e->nk_ui.pl.gshift = e->framebuffer->surface->format->Gshift;
    e->nk_ui.pl.bshift = e->framebuffer->surface->format->Bshift;
    e->nk_ui.pl.ashift = e->framebuffer->surface->format->Ashift;
    e->nk_ui.pl.rloss  = e->framebuffer->surface->format->Rloss;
    e->nk_ui.pl.gloss  = e->framebuffer->surface->format->Gloss;
    e->nk_ui.pl.bloss  = e->framebuffer->surface->format->Bloss;
    e->nk_ui.pl.aloss  = e->framebuffer->surface->format->Aloss;

    e->nk_ui.context = nk_rawfb_init ( e->framebuffer->surface->pixels,
                                       e->nk_ui.tex_scratch,
                                       e->framebuffer->surface->w,
                                       e->framebuffer->surface->h,
                                       e->framebuffer->surface->pitch,
                                       e->nk_ui.pl );
    if ( ! e->nk_ui.context )
    {
        printf ( "Nuklear init nk_rawfb_init failed\n" );
        return -1;
    }
    return 0;
}

int
initEngine ( Engine * e, uint32_t h, uint32_t w )
{
    if ( SDL_Init ( SDL_INIT_VIDEO ) != 0 )
    {
        printf ( "SDL Error: %s\n", SDL_GetError () );
        return -1;
    };

    e->window = SDL_CreateWindow ( "CPU Rendering",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   w,
                                   h,
                                   SDL_WINDOW_FULLSCREEN );

    if ( ! e->window )
    {
        printf ( "SDL Error: %s\n", SDL_GetError () );
        return -1;
    }

    SDL_ShowCursor ( SDL_DISABLE );

    e->renderer = SDL_CreateRenderer ( e->window, -1, SDL_RENDERER_SOFTWARE );
    if ( ! e->renderer )
    {
        printf ( "SDL Error: %s\n", SDL_GetError () );
        return -1;
    }

    SDL_SetRelativeMouseMode ( SDL_TRUE );

    e->texture = SDL_CreateTexture ( e->renderer,
                                     SDL_PIXELFORMAT_RGBA8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     w,
                                     h );
    if ( ! e->texture )
    {
        printf ( "SDL Error: %s\n", SDL_GetError () );
        return -1;
    }

    e->height = h;
    e->width  = w;

    e->framebuffer = createFramebuffer ( h, w );
    if ( ! e->framebuffer ) return -1;

    if ( createNKUI ( e ) ) return -1;

    /* ========== Game objects init ========== */

    e->camera.default_at[ 2 ] = 1.0f;
    glm_vec3_zero ( e->camera.position );
    glm_quat_identity ( e->camera.quaternion );
    e->camera.speed = 1.0f;

    e->key_states = 0;

    e->conf.fovy_rad      = glm_rad ( 60.0f );
    e->conf.nearClipPlane = -0.5f;
    e->conf.faarClipPlane = -200.0f;

    e->conf.mouse_sensitivity = 0.1f;

    e->running = 1;
    e->godmod  = 0;
    return 0;
}

void
file_reader ( void *       ctx,
              const char * filename,
              int          is_mtl,
              const char * obj_filename,
              char **      buf,
              size_t *     len )
{
    ( void ) obj_filename; // Mark 'obj_filename' as unused
    RObject * o = ( RObject * ) ctx;

    FILE * file = fopen ( filename, "rb" );
    if ( ! file )
    {
        perror ( "Error opening file" );
        *buf = NULL;
        *len = 0;
        return;
    }

    fseek ( file, 0, SEEK_END );
    *len = ftell ( file );
    fseek ( file, 0, SEEK_SET );

    *buf = malloc ( sizeof ( char ) * ( ( *len ) + 1 ) );
    o->pTempFileBuf[ ( is_mtl ? 1 : 0 ) ] = *buf;

    if ( *buf ) { fread ( *buf, 1, *len, file ); }
    fclose ( file );
}

RObject *
loadRObject ( const char * objPath )
{
    RObject * o;
    U_ALLOC ( o, RObject, 1 );
    U_ALLOC ( o->pTempFileBuf, char *, 3 );

    tinyobj_attrib_init ( &o->attrib );
    int result = tinyobj_parse_obj ( &o->attrib,
                                     &o->shapes,
                                     &o->num_shapes,
                                     &o->materials,
                                     &o->num_materials,
                                     objPath,
                                     file_reader,
                                     o,
                                     0 );

    if ( result != TINYOBJ_SUCCESS )
    {
        printf ( "Error loading %s\n", objPath );
        free ( o );
        return NULL;
    }

    printf ( "Successfully loaded %s\n", objPath );
    for ( int i = 0; i < 2; i++ ) { free ( o->pTempFileBuf[ i ] ); }
    free ( o->pTempFileBuf );

    /* center the object initially */
    float min_x = o->attrib.vertices[ 0 ];
    float max_x = o->attrib.vertices[ 0 ];
    float min_y = o->attrib.vertices[ 1 ];
    float max_y = o->attrib.vertices[ 1 ];
    float min_z = o->attrib.vertices[ 2 ];
    float max_z = o->attrib.vertices[ 2 ];
    float x, y, z;
    for ( unsigned int i = 0; i < o->attrib.num_vertices / 3; i++ )
    {
        x = o->attrib.vertices[ i * 3 + 0 ];
        y = o->attrib.vertices[ i * 3 + 1 ];
        z = o->attrib.vertices[ i * 3 + 2 ];

        if ( min_x > x ) min_x = x;
        if ( max_x < x ) max_x = x;
        if ( min_y > y ) min_y = y;
        if ( max_y < y ) max_y = y;
        if ( min_z > z ) min_z = z;
        if ( max_z < z ) max_z = z;
    }

    printf ( "min_x: %f, max_x: %f\n", min_x, max_x );
    printf ( "min_y: %f, max_y: %f\n", min_y, max_y );
    printf ( "min_z: %f, max_z: %f\n", min_z, max_z );

    glm_vec3_zero ( o->center );
    o->center[ 0 ] = ( min_x + max_x ) / 2;
    o->center[ 1 ] = ( min_y + max_y ) / 2;
    o->center[ 2 ] = ( min_z + max_z ) / 2;

    glm_vec3_one ( o->scale );
    glm_quat_identity ( o->quaternion );
    glm_vec3_zero ( o->position );

    return o;
}

void
destroyRObject ( RObject * o )
{
    if ( o )
    {
        tinyobj_attrib_free ( &o->attrib );
        tinyobj_shapes_free ( o->shapes, o->num_shapes );
        tinyobj_materials_free ( o->materials, o->num_materials );
        free ( o );
    }
}

int
destroyEngine ( Engine * e )
{
    if ( e->window ) { SDL_DestroyWindow ( e->window ); }
    if ( e->renderer ) { SDL_DestroyRenderer ( e->renderer ); }
    if ( e->texture ) { SDL_DestroyTexture ( e->texture ); }
    if ( e->nk_ui.context ) { nk_rawfb_shutdown ( e->nk_ui.context ); }

    destroyFramebuffer ( e->framebuffer );
    SDL_Quit ();
    return 0;
}
