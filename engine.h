#pragma once
#ifndef CUSTOM_RENDER_ENGINE_H
#define CUSTOM_RENDER_ENGINE_H

#undef TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj.h"

#include <SDL2/SDL.h>
#include <cglm/cglm.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>

#undef NK_IMPLEMENTATION
#undef NK_RAWFB_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT
#include "nuklear.h"
//
#include "nk_raw_fb.h"

#define PX_VAL( R, G, B, A ) \
    ( ( uint32_t ) ( A ) << 24 ) | ( ( B ) << 16 ) | ( ( G ) << 8 ) | ( R )

#define U_ALLOC( var, type, len )                                \
    do {                                                         \
        ( var ) = malloc ( ( len ) * sizeof ( type ) );          \
        if ( ! ( var ) )                                         \
        {                                                        \
            printf ( "error: ualloc of %s of size %zu",          \
                     #var,                                       \
                     ( size_t ) ( ( len ) * sizeof ( type ) ) ); \
            exit ( EXIT_FAILURE );                               \
        }                                                        \
    } while ( 0 )

typedef struct
{
    struct rawfb_context * context;
    struct nk_color        clear;
    struct nk_rect         bounds;
    struct rawfb_pl        pl;
    unsigned char          tex_scratch[ 512 * 512 ];

} NuklearUI;

typedef struct DBuffer
{
    vec4  color;
    float z;

    struct DBuffer * next;
} DBuffer;

typedef struct DBufferPool
{
    DBuffer * nodes;
    DBuffer * current;
    uint32_t  size;
    uint32_t  used;

    struct DBufferPool * prev;
} DBufferPool;

typedef struct Framebuffer
{
    SDL_Surface * surface;

    // left bound of current triangle scanline
    int * min_x;
    // right bound
    int * max_x;

    /* 22.02.25 ::: Converted opaque DBuffer AOS -> SOA */
    DBuffer ** transparent;
    vec4 *     opaque_c;
    float *    opaque_z;

    DBufferPool * pool;
} Framebuffer;

/*
        /////|
       ///// |
      /////  |
     |~~~| | |
     |===| |/|
     | B |/| |
     | I | | |
     | B | | |
     | L |  /
     | E | /
     |===|/
*/

#define POSITION_FIELDS \
    versor quaternion;  \
    vec3   position;    \
    vec3   center;      \
    vec3   scale;

typedef struct Camera
{
    POSITION_FIELDS
    vec3  default_at;
    float speed;
    float yaw;
    float pitch;
} Camera;

typedef struct Config
{
    float fovy_rad;
    float nearClipPlane;
    float faarClipPlane;

    float mouse_sensitivity;

} Config;

typedef struct Engine
{

    uint8_t       godmod;
    Framebuffer * framebuffer;

    uint32_t       height;
    uint32_t       width;
    uint8_t        running;
    NuklearUI      nk_ui;
    SDL_Window *   window;
    SDL_Renderer * renderer;
    SDL_Texture *  texture;

    Config conf;

    uint32_t key_states;
    Camera   camera;

} Engine;

typedef struct
{
    POSITION_FIELDS

    tinyobj_attrib_t     attrib;
    tinyobj_shape_t *    shapes;
    size_t               num_shapes;
    tinyobj_material_t * materials;
    size_t               num_materials;

    /* 0 - obj file ptr;
     * 1 - mtl file ptr
     * */
    char ** pTempFileBuf;
} RObject;

void
swapDBuffer ( DBuffer * a, DBuffer * b );

DBuffer *
getAuxDBuffer ( Framebuffer * f );

DBufferPool *
createDBufferPool ( uint32_t size );

void
destroyDBufferPool ( DBufferPool * p );

Framebuffer *
createFramebuffer ( uint32_t h, uint32_t w );

void
destroyFramebuffer ( Framebuffer * f );

void
cleanFramebuffer ( Framebuffer * f );

int
initEngine ( Engine * e, uint32_t h, uint32_t w );

int
destroyEngine ( Engine * e );

RObject *
loadRObject ( const char * objPath );

void
destroyRObject ( RObject * o );

#endif /* CUSTOM_RENDER_ENGINE_H */

/*
DONT SCROLL !!!
⡐⢆⠦⡱⢌⠦⡑⢎⡔⢢⠒⡄⡀⠀⠀⠑⢪⠔⡤⡑⢄⡐⠌⠂⠀⠀⠀⢠⠐⡴⣲⠿⡏⡑⠢⠐⠀⢠⢊⡴⢪⡔⠴⡲⡙⠊⠁⠠⠐⡀⢂⠔⡈⠢⢉⣿⡿⢏⡱⢂⡒⡐⠢⡜⡸⡹⢆⠳⣘⢦⡛⣼⢻⣜⡣⡟⣽⢫⣟⣿⣻⠻⣷⢭⣛⢸⣿⣶⣦⣍⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠿⠿⠿⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠘⣌⠲⡑⢎⡒⢍⠲⣌⢣⡙⠴⣑⢢⢀⠀⠀⠘⠰⣉⠦⡘⢢⡀⠀⠠⢈⠦⣹⣾⢏⡒⠡⠐⠁⠀⢠⢃⡎⢖⣣⠚⡵⠃⠓⢁⠋⠐⠲⠤⡄⠂⠄⡑⠈⠍⡐⠠⢁⠣⢒⠡⡑⢌⠡⡱⠉⠢⠑⢎⠱⢣⢏⠶⡹⣝⡮⣝⣞⣶⣻⠷⡉⣧⠹⣧⡹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠱⣌⠲⣉⠖⡸⢌⠣⡜⢢⠜⡱⠌⢦⡉⢖⣀⠀⠀⠈⠒⡍⢦⡙⢦⣀⠡⢚⡽⢃⠆⠈⠄⠁⠀⡠⢎⠦⣙⠲⡌⡝⣀⠴⠚⠯⠿⢷⢦⡐⢀⠁⠂⠄⡁⠂⢄⡡⢆⡴⢈⠆⡱⢈⠆⡑⢈⠡⢉⠢⣉⠗⣪⠝⣳⡍⠾⡽⣞⡶⣏⡿⢧⣜⣃⣽⠃⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣩⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠒⡬⠱⡌⢎⡑⢎⡱⢌⠣⢎⡅⣋⠖⡘⢦⡘⡌⢆⡀⠀⠈⠂⢙⠦⣌⡑⢄⡀⠁⠀⠂⠀⢠⢲⣡⢠⣀⣈⣁⠈⠑⠀⠀⠀⠠⣀⠠⢀⠘⠤⡈⢐⠠⡐⠈⡶⣙⢎⡔⣃⠒⡄⠡⡘⢀⠃⠜⡠⢃⢼⡣⢇⠎⣱⢚⡵⣙⠺⣽⣳⣹⣛⣾⣽⣾⡐⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⣮⣻⣯⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡘⡔⢳⡘⠦⡙⠦⡘⣌⠳⢌⠲⡡⢎⡱⢢⠱⡘⠦⡑⢦⢀⠀⠀⠘⠤⡙⢆⡑⢦⡀⠀⡔⡠⢄⠠⡉⠙⠳⢯⣿⣄⠂⡉⢃⠁⣠⣈⡬⣜⡄⡑⠌⡐⠠⠑⡀⠁⠀⠈⠀⠁⢀⠐⠤⡁⢊⠐⡀⠃⢮⡝⣌⠲⣡⢏⡶⠉⡓⠢⢹⣖⡃⢉⡹⢷⣷⡘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣰⣾⣿⣿⣿⣿⣿⣎⢻⣯⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡘⢬⢡⠚⡴⢉⠖⡱⢌⡚⢬⠱⣑⠪⢔⡃⠧⣙⢢⡙⢦⡉⢖⡠⠀⠀⠈⠊⡕⢦⣉⠲⢄⠑⢌⡒⡁⠉⠀⠀⠀⠉⠛⢓⣦⠜⡡⢀⠀⠀⠈⠐⣇⠀⠂⠡⠀⠀⠀⠈⠀⠠⠀⡈⠔⡡⢊⡐⠀⠡⢀⠒⡄⢣⠅⣻⢞⠀⣿⣷⡄⠸⣹⠈⣷⣌⠹⣧⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣴⢿⣯⣟⣿⣿⣿⣿⣿⣿⠷⠉⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠘⡆⢣⢍⡒⢭⡘⡔⢣⠜⣢⠓⢬⡑⠎⡬⡑⢆⠣⡜⢢⡙⢢⠱⣉⢆⡀⠀⠈⠰⢌⡒⢆⡑⢄⠐⠁⠈⢠⡔⣞⣾⣿⣻⡽⡘⢄⠃⡐⢀⠀⠀⡐⠈⠀⠠⠁⠀⠀⠀⠀⠀⠀⠀⢎⠱⣀⠂⠈⢀⠂⠐⡈⢆⠂⢽⡊⠀⣿⣿⣿⠀⣧⠀⣾⣿⣷⠘⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢄⡼⢻⣼⢻⡾⣽⣻⣾⠿⠛⠋⠀⠀⠀⢹⣯⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠱⣌⢣⠒⡜⢢⠱⣌⢣⠚⡤⣙⠢⡜⡱⢢⡙⢬⠱⡌⢣⠜⣡⠓⣌⠲⡘⢤⡀⠀⠀⠘⢌⠲⣌⡑⠆⢹⣷⣿⣿⣿⣿⣿⣻⣿⣷⣎⣐⢂⠐⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢉⠢⠑⠀⠀⢀⠀⡀⢁⠀⠂⠆⠸⣿⡀⢸⣿⣿⡄⣿⠂⢻⣿⣿⠃⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⢀⢙⡯⣞⢯⡛⠷⠋⠁⠀⠀⠀⠀⠀⠀⠀⣿⡞⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡑⢆⢎⡱⢌⢣⡑⢆⣃⠳⡰⢡⢓⡸⣐⠣⢜⢢⠓⣌⢣⠚⣄⠫⣄⢣⡙⠦⣙⠒⠄⠀⣀⠳⢤⠙⠀⠈⠁⠉⠉⠙⠛⠛⠛⠛⠉⠉⢀⠊⡄⠀⠈⠠⠀⠄⠀⠀⠀⠀⠀⠄⠀⠀⠀⠈⠀⢀⠂⠀⠀⠠⠈⡄⠈⡀⠹⣃⢸⣿⣿⣿⣿⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠌⠐⠉⠀⠀⠀⠀⠀⠀⠀⠀⠄⠀⠀⠀⢹⡷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡘⣌⠲⢌⠎⡆⣍⠲⢌⡱⢡⢃⠎⣔⢢⡙⡌⢦⡙⢤⠣⡙⢤⠓⡌⢦⢑⠣⡌⠁⠀⡴⣈⠳⠀⠤⣉⠀⠀⠀⠄⡀⠀⠀⠀⠀⠀⠀⣠⡟⡴⠁⠀⠀⠈⠠⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠈⠄⠀⢂⠄⠀⠁⠀⠐⢤⣀⡈⠝⡻⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⡀⠀⢸⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠰⣌⠳⢌⡚⠴⡨⡑⢎⡔⠣⢎⡜⢤⠣⡜⢸⠰⡘⢆⢣⡙⠦⡙⡜⢢⠍⠂⠀⣠⠓⡴⠁⢠⠘⡰⠀⠉⠀⠀⠀⣀⠄⠠⠀⠐⠀⠀⢼⡻⠴⠁⠀⠀⠀⠀⠂⠌⡀⠄⠀⠀⠀⠀⠀⠀⠀⠠⢁⠀⠐⡈⠄⠣⢐⠀⡀⢀⠀⠑⠣⣌⡐⡩⢛⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⡏⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⣼⡏⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠱⡌⢣⠎⡔⢣⠱⣉⠖⡌⠳⣌⢢⢃⠳⢌⡃⠧⡙⡌⢦⡑⢣⠱⡌⠃⠀⠠⡜⣄⠋⢀⡐⠢⠁⠀⡀⠀⢀⣀⣬⣥⣬⣥⣦⣦⣤⣤⣬⣉⣋⠀⠀⠀⠀⠀⠁⠂⠄⠐⠈⠀⠀⠀⠀⠀⠀⠀⠠⠐⠠⠐⡈⡐⠡⠘⡐⢂⠆⡠⢀⠀⠁⠓⢦⣉⡄⠙⣛⠿⣿⣿⣿⣿⠟⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⡟⣣⡇⠀⠀⠀⠠⠠⠀⠀⡀⠀⠀⢀⣠⠀⠀⠀⢀⣿⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠱⣌⠣⡜⢌⢣⠓⣌⠲⣉⠕⣢⠑⡎⡱⢊⠬⡱⢱⡘⠦⣉⢆⡓⠀⠀⡔⢣⠜⠀⡐⠢⠌⣁⣬⣴⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣾⣯⣿⣷⢦⣤⣀⠀⠀⠈⡀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠠⠁⢂⠐⠠⠁⢂⠌⡰⢈⡑⢌⠸⠄⠁⡄⢀⠙⠳⢢⣅⢪⠛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡸⣿⣣⡟⠀⠀⠀⡠⣽⣷⠀⠀⠠⣶⣶⡮⠁⠀⠀⠀⡰⢯⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡑⢆⡓⡸⢌⠦⡙⢤⠓⣌⠚⡤⢋⠴⣡⠋⡖⣡⠣⡜⠱⡌⠂⠀⣀⠳⢌⠃⡀⢂⣡⡶⣟⣭⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣽⣻⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠂⠠⠁⡈⠄⢂⠐⠠⠐⡌⠘⠀⡜⠰⡃⡔⡠⠄⠈⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠓⠀⣿⠁⠀⠀⢀⠃⣽⡿⠀⢀⠀⢿⠋⠀⠀⠀⠀⢈⣦⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡘⠦⢱⡑⢎⡒⢍⠦⡙⢤⢋⠴⣉⠖⡤⢋⠴⣡⠒⡭⠑⠀⢀⠲⠌⢑⣂⣠⣶⣻⣵⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣯⣻⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠀⠌⠀⠠⢁⠂⠌⢀⠱⢈⠅⡓⢌⠱⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣐⠉⠀⢰⠏⠀⠀⠀⠸⢰⣿⣿⣤⣤⠖⠁⡀⠀⠀⠀⣰⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⡘⢥⠣⢜⠢⢍⡊⢖⠩⢆⡍⢲⢡⢚⡰⣉⠖⠡⠙⣀⣀⣤⣥⣶⠾⢟⣫⣵⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣽⢷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠄⠈⠀⠀⠠⠁⠄⠀⡀⠢⢈⠢⢘⠰⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠠⣀⣽⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣴⣬⣀⠀⠀⠀⡅⣸⣿⣿⣿⣿⢱⢻⣿⣭⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣘⠠⢋⠦⣙⢢⡙⣌⠓⡬⠘⠅⢊⣂⣥⣴⠶⠿⣛⣋⣭⣷⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣯⣳⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠁⠀⠀⠀⠀⠁⢂⠰⠈⠐⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠐⣈⣤⣾⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣽⣷⡀⠀⣆⠻⣿⣿⣿⠿⢎⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣷⣦⣘⠠⠣⠜⣀⣭⣴⣾⣿⣯⣿⣶⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣵⣶⣿⣿⣿⣿⣿⣿⣷⣶⣦⣄⣀⠀⠀⠠⠐⠀⠀⢀⣠⡶⠃⠀⠀⠀⠀⠀⠀⠀⠀⣠⡴⢊⣿⣿⣿⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⡿⠀⣠⣭⣵⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣷⣦⡚⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⡠⢶⣾⣿⣿⣇⠀⠀⠀⠀⢀⠀⠀⣠⡾⢋⣴⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢞⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⢷⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣧⣌⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡶⣍⠻⣿⣿⣤⢀⣶⣰⡏⣰⡷⢋⣴⣿⣿⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠁⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⡙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡳⣌⠻⣿⣿⣿⠿⢋⣡⣶⣿⣿⣿⣿⣿⣿⣿⡅⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⢿⣿⡿⠁⠀⢨⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡵⣆⡉⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⢿⣿⡟⣡⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡳⣌⢩⣤⣞⣯⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⢠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣿⣿⣿⣿⣿⣿⡿⣳⣿⠟⠀⠀⢀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿*/
