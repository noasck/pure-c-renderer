
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_mouse.h>
#include <cglm/cglm.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef MIN2
#define MIN2( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#endif
#ifndef MAX2
#define MAX2( a, b ) ( ( a ) < ( b ) ? ( b ) : ( a ) )
#endif

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_SOFTWARE_FONT
#include "nuklear.h"

#define NK_RAWFB_IMPLEMENTATION
#include "nk_raw_fb.h"

/* +----------------------------------------------+
 * | OCHEN' HOROSHO, A TEPER' NAZAD V RENDER SUKA |
 * +----------------------------------------------+
.............................:;+X$&&&&&&&&&&&$x;::................
..........................:X$+;;::;:::::;::;::;;+X&$+::...........
.........................:x$;;:::::::;+++;;::::::::;;X$+..........
........................:+$;::::;;;:;X&&&&+::::::::::;;x&;........
........................;$+;:::;;&&&&&&&&x:;:::::::::::;+&;.......
.......................:XX;;::::x&&&&&&&&+;;;:::::::::;:+$;.......
.......................x$;::::::;X&&&&&&&&&&$;::::::::;;X$:.......
......................+&;::::::::;X&&&&&&&&&X:::::::::;;&X........
.....................:Xx;;;;;;;;;+&&&&&x:;X&x;:::::::::+&+........
....................:X$xxxxxxXXX$$$$&&X;;;;;::::::::::;x&:........
...................:$x;;;;xX$&&&&&$x;;x&&X;;;;::::::::;&X.........
..................:Xx;;;;;;;;;;;;;;;+;;;;;X&X+;;;::::;+&;.........
..................:$+;;;;;;;xX$$Xx;;;;;;;;;;+$$x;:::;;X$:.........
.................:x&&$x;;;;;;;;;;;;;;;;;;;;;;;;x&x;:;;$X..........
................:xx;;;x&&x;;;;;;;;+$&&&&&&&&&x;;;x&+;+$;..........
...............:xX;;$CCX+Xx;;;;;;X$x+X&&&X;;;;;;;;+$&xX:..........
..............:$X;;X&C&&xxX;;;;;X$;;&CCCC&X;;;;;;;;;+&X:..........
.............:$X;;;+&CC$;xX;;;;+&x;;$CC&&&X;;;;;;$&X&&;...........
............:xX;;;;;;;;;;$x;;;;+&;;;;CCCC+;;;;;;x&X&&&:...........
............x$;;;;;;;;;;XX;;;;;x$;;;;;;;;;;;;;;;+&&&&x............
...........;&+;;;;;;;;;$X;;;;;;+&+;;;;;;;;;;;;;;;X&x:.............
..........:&&X+;;;;++;+&+++;;xX+x$+;;;;;;;;;;;;;;x$:..............
.........:x&X;;;;;XX;;;xx++XXx;+x+;Xx;;;;;;;;+X;;x$:..............
.........;$xx;;;+$x;;;;;;;;;;;;;;;;;$+;;;;;;;;Xx;x$:..............
.........+$;;;;x$+;;;;;;;;;;;;;;;;;;+$+;;;;;;;xx;XX:..............
.........+X;;;x&+;;;;;;;;;;;;;;;;;;;;XX;;;;;;;x++&+...............
.........;X;;+$x;;;;++++xxxxxXxxx+;;;+$+;;;;;;;+&x:...............
.........:x&+;$x;;;;;;;;;;;;;;;;;;;;;X$;;;;;;;+&+.................
...........;&X+$X;;;;;;;;;;;;;;;;;;xX+;;;;;;;X&$;:................
............;&&+;;;;;;;;;;;;;;;;;;;;;;;;;;;x&&X;x$X:..............
.........::$$;;&&+;;;;;;;;;;;;;;;;;;;;;;;;$&;+$;:;;X&X:...........
:;+xXX$$Xx$X;;+x$&&;;;;;;;;;;;;;;;;;++xX&&x;;x&$$&$x+;x&&$Xx+;:...
x+;;:::;;X&&$X+;;;X&&$$$$$$X$$&&&&&&&&$+;;;;$&$X::;;x&XX&$;;+x$$:.
;;;:::::;+&X;:::::;x&+;+&X;x&&&&&&&&&$x&+;x&X;Xx:::;;;X&+;;:::;x&X
$;::::::;+X;:;:::::;$&X&x;;;+&&&&&&&&+;X&$&;;+$;::::::;X$;::::;;&X
&x;:::::::;X$;;:::::;$&+;;;;;+&&&&&&$;;;xx;;;XX;:::::;x&;;:::::;+$
 */
#include "engine.h"
#include "pipeline.h"

#define CPI 3.14159265358979323846f

int an_x = 20;
int an_z = 20;

static int
sdl_button_to_nk ( int button )
{
    switch ( button )
    {
    default:
    /* ft */
    case SDL_BUTTON_LEFT: return NK_BUTTON_LEFT; break;
    case SDL_BUTTON_MIDDLE: return NK_BUTTON_MIDDLE; break;
    case SDL_BUTTON_RIGHT: return NK_BUTTON_RIGHT; break;
    }
}

void
defaultShader ( RObject * o, Engine * e )
{
    vec4 c[ 3 ] = { { 1.0f, 1.0f, 0.0f, 0.0f },
                    { 1.0f, 0.0f, 0.5f, 0.5f },
                    { 1.0f, 0.0f, 1.0f, 0.0f } };

    mat4 view_proj, cam_proj, cam_rot, world_proj;

    /* World projection: */
    vec3 Ncenter;
    glm_vec3_negate_to ( o->center, Ncenter );
    glm_translate_make ( world_proj, o->position );
    glm_quat_rotate ( world_proj, o->quaternion, world_proj );
    glm_scale ( world_proj, o->scale );
    glm_translate ( world_proj, Ncenter );

    /* Camera Space projection: I - EYE */

    vec3 Neye;

    glm_vec3_negate_to ( e->camera.position, Neye );
    glm_translate_make ( cam_proj, Neye );
    glm_euler ( ( vec3 ) { glm_rad ( e->camera.pitch ),
                           glm_rad ( e->camera.yaw ),
                           0 },
                cam_rot );

    /* Perspective projection */
    float fovy    = e->conf.fovy_rad;
    float aspect  = ( float ) e->width / ( float ) e->height;
    float nearVal = e->conf.nearClipPlane;
    float farVal  = e->conf.faarClipPlane;
    glm_perspective ( fovy, aspect, nearVal, farVal, view_proj );

    /* Viewport */

    mat4 viewport_proj;
    glm_translate_make (
        viewport_proj, ( vec3 ) { e->width / 2.0f, e->height / 2.0f, 0.0f } );
    glm_scale ( viewport_proj,
                ( vec3 ) { e->width / 2.0f, e->height / 2.0f, 1.0f } );

    float face_offset = 0;
    vec4  v4;
    vec3  v[ 3 ];
    // TODO : Handle SHAPES
    for ( uint32_t nfc = 0; nfc < o->attrib.num_face_num_verts; nfc++ )
    {
        int face_cnt   = o->attrib.face_num_verts[ nfc ];
        int face_1_idx = face_offset;
        for ( int face_idx = face_offset + 1;
              face_idx < face_offset + face_cnt - 1;
              face_idx++ )
        {
            for ( int j = 0; j < 3; j++ )
            {
                v[ 0 ][ j ] = o->attrib.vertices[ //
                    o->attrib.faces[ face_1_idx ].v_idx * 3 + j ];
                v[ 1 ][ j ] = o->attrib.vertices[ //
                    o->attrib.faces[ face_idx ].v_idx * 3 + j ];
                v[ 2 ][ j ] = o->attrib.vertices[ //
                    o->attrib.faces[ face_idx + 1 ].v_idx * 3 + j ];
            }

            /* ========= Transforms & rotations ========= */
            for ( int j = 0; j < 3; j++ )
            {
                glm_vec4 ( v[ j ], 1.0, v4 );
                glm_mat4_mulv ( world_proj, v4, v4 );
                v4[ 3 ] = 1.0f;
                glm_mat4_mulv ( cam_proj, v4, v4 );
                glm_mat4_mulv ( cam_rot, v4, v4 );

                if ( v4[ 2 ] < e->conf.faarClipPlane ||
                     v4[ 2 ] > e->conf.nearClipPlane )
                {
                    goto next_face;
                }

                v4[ 3 ] = 1.0f;
                glm_mat4_mulv ( view_proj, v4, v4 );
                glm_vec4_scale ( v4, 1 / v4[ 3 ], v4 );
                v4[ 3 ] = 1.0f;

                glm_mat4_mulv ( viewport_proj, v4, v4 );
                glm_vec4_scale ( v4, 1 / v4[ 3 ], v4 );
                glm_vec3 ( v4, v[ j ] );
            }

            /* ========= Backface culling ========= */
            vec3 normal, v1, v2, view_dir = { 0.0f, 0.0f, 1.0f };

            glm_vec3_sub ( v[ 1 ], v[ 0 ], v1 );
            glm_vec3_sub ( v[ 2 ], v[ 0 ], v2 );
            glm_vec3_cross ( v1, v2, normal );
            float dot_product = glm_vec3_dot ( normal, view_dir );

            if ( dot_product > 0 ) { rasterize ( e->framebuffer, v, c ); }
        }
    next_face:
        face_offset += face_cnt;
    }
}

int
main ( void )
{
    //  RObject * seahawk_ro = loadRObject (
    //      "treeDecorated.obj" );

    RObject * seahawk_ro    = loadRObject ( "models/Seahawk.obj" );
    seahawk_ro->center[ 0 ] = 0.0119630;
    seahawk_ro->center[ 1 ] = 31.758559;
    seahawk_ro->center[ 2 ] = 0.9221725;

    uint64_t last_time = SDL_GetPerformanceCounter ();
    int      frames    = 0;

    Engine         E;
    const uint16_t WIDTH = 1920, HEIGHT = 1080;

    int err;
    err = initEngine ( &E, HEIGHT, WIDTH );
    if ( err ) { goto exit_routine; }

    struct nk_context * pNK_CTX = &( E.nk_ui.context->ctx );

    SDL_Event      event;
    struct nk_vec2 scrollvec;

    char fps_str[ 16 ];
    while ( E.running )
    {
        const Uint8 * state = SDL_GetKeyboardState ( NULL );
        /* FPS count */
        uint64_t current_time = SDL_GetPerformanceCounter ();
        frames++;
        if ( ( ( double ) ( current_time - last_time ) /
               SDL_GetPerformanceFrequency () ) >= 1.0 )
        {
            snprintf ( fps_str, sizeof ( fps_str ), "%u", frames );
            frames    = 0;
            last_time = current_time;
        }

        while ( SDL_PollEvent ( &event ) )
        {
            nk_input_begin ( pNK_CTX );
            switch ( event.type )
            {
            case SDL_QUIT: goto exit_routine;
            case SDL_MOUSEMOTION:
                if ( ! E.godmod )
                {
                    E.camera.yaw +=
                        event.motion.xrel * E.conf.mouse_sensitivity;
                    E.camera.pitch -=
                        event.motion.yrel * E.conf.mouse_sensitivity;
                    E.camera.pitch =
                        fmaxf ( -89.0f, fminf ( 89.0f, E.camera.pitch ) );
                    E.camera.yaw = fmodf ( E.camera.yaw, 360.0f );
                }
                nk_input_motion ( pNK_CTX, event.motion.x, event.motion.y );
                break;

            case SDL_KEYDOWN:
                if ( event.key.keysym.sym == SDLK_LCTRL )
                {
                    SDL_SetRelativeMouseMode ( SDL_FALSE );
                    E.godmod = 1;
                }
                break;

            case SDL_KEYUP:
                if ( event.key.keysym.sym == SDLK_LCTRL )
                {
                    SDL_SetRelativeMouseMode ( SDL_TRUE );
                    E.godmod = 0;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                nk_input_button ( pNK_CTX,
                                  sdl_button_to_nk ( event.button.button ),
                                  event.button.x,
                                  event.button.y,
                                  1 );
                break;
            case SDL_MOUSEBUTTONUP:
                nk_input_button ( pNK_CTX,
                                  sdl_button_to_nk ( event.button.button ),
                                  event.button.x,
                                  event.button.y,
                                  0 );
                break;
            case SDL_MOUSEWHEEL:
                scrollvec.x = event.wheel.x;
                scrollvec.y = event.wheel.y;
                nk_input_scroll ( pNK_CTX, scrollvec );

                break;

            default: break;
            }
        }

        float c = cos ( glm_rad ( E.camera.yaw ) ) * E.camera.speed,
              s = sin ( glm_rad ( E.camera.yaw ) ) * E.camera.speed;

        if ( state[ SDL_SCANCODE_W ] )
        {
            E.camera.position[ 0 ] += s;
            E.camera.position[ 2 ] -= c;
        }
        if ( state[ SDL_SCANCODE_S ] )
        {
            E.camera.position[ 0 ] -= s;
            E.camera.position[ 2 ] += c;
        }
        if ( state[ SDL_SCANCODE_A ] )
        {
            E.camera.position[ 0 ] -= c;
            E.camera.position[ 2 ] -= s;
        }
        if ( state[ SDL_SCANCODE_D ] )
        {
            E.camera.position[ 0 ] += c;
            E.camera.position[ 2 ] += s;
        }
        if ( state[ SDL_SCANCODE_X ] )
        {
            E.camera.position[ 1 ] += E.camera.speed;
        }
        if ( state[ SDL_SCANCODE_Z ] )
        {
            E.camera.position[ 1 ] -= E.camera.speed;
        }

        cleanFramebuffer ( E.framebuffer );

        /* ========= Rendering pipeline ========= */
        defaultShader ( seahawk_ro, &E );
        // defaultShader ( seahawk_ro2, &E );
        merge ( E.framebuffer );

        /* Nuklear UI devfinition */
        nk_input_end ( pNK_CTX );
        if ( E.godmod )
            if ( nk_begin ( pNK_CTX,
                            "Debug",
                            E.nk_ui.bounds,
                            NK_WINDOW_MOVABLE | NK_WINDOW_TITLE ) )
            {
                nk_layout_row_static ( pNK_CTX, 30, 80, 1 );
                if ( nk_button_label ( pNK_CTX, "button" ) )
                {
                    printf ( "button pressed\n" );
                }
                nk_layout_row_dynamic ( pNK_CTX, 45, 2 );
                nk_label ( pNK_CTX, "fps:", NK_TEXT_LEFT );
                nk_label ( pNK_CTX, fps_str, NK_TEXT_RIGHT );
                nk_layout_row_dynamic ( pNK_CTX, 45, 1 );
                nk_label ( pNK_CTX, "scale:", NK_TEXT_LEFT );
                nk_layout_row_dynamic ( pNK_CTX, 45, 1 );
                nk_slider_float (
                    pNK_CTX, 0.01, &seahawk_ro->scale[ 0 ], 4.0f, 0.01f );
                seahawk_ro->scale[ 1 ] = seahawk_ro->scale[ 0 ];
                seahawk_ro->scale[ 2 ] = seahawk_ro->scale[ 0 ];

                nk_end ( pNK_CTX );
                nk_rawfb_render ( E.nk_ui.context, E.nk_ui.clear, 0 );
            }

        SDL_UpdateTexture ( E.texture,
                            NULL,
                            E.framebuffer->surface->pixels,
                            E.framebuffer->surface->pitch );
        SDL_RenderClear ( E.renderer );
        SDL_RenderCopy ( E.renderer, E.texture, NULL, NULL );
        SDL_RenderPresent ( E.renderer );
    }

exit_routine:
    destroyEngine ( &E );
    if ( seahawk_ro ) destroyRObject ( seahawk_ro );
    return 0;
}

/* DON'T FUCKING SCROLL
⣿⣿⣿⣷⡽⠿⣟⣻⣭⣴⣶⣿⣷⣿⣿⣷⣶⣄⡀⢂⠔⡠⢂⠔⠠⡀⠄⡀⠀⠀⠀⠀⠀⠀⠂⠔⡠⢂⠔⡠⢂⠆⠱⡈⠝⡙⢯⡙⠤⠀⠄⢂⠜⡨⣙⢻⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⡛⢯⠛⡭⢋⡜⠁⣊⣤⣾⣿⣿⣿⡼⣿⣿⣿⣿⣿⣿
⣿⡿⢫⣽⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣔⠠⠂⡘⡐⠰⠐⠠⠁⠄⠀⠀⠀⠀⠈⠠⢁⠊⠤⡑⠢⢌⡑⡈⠆⡑⢂⠌⡐⢉⠰⠈⡄⡑⢄⢊⡱⢢⢛⡽⣻⢿⣿⣿⣿⡿⡿⢯⡙⢦⠙⠢⠙⠀⢁⣤⣾⣿⣿⣿⣿⣿⢟⠇⣟⣯⣿⣿⣿⣿
⠁⡰⣟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡄⠐⠈⡅⠌⡡⠌⡐⠠⠀⠀⠀⠀⠀⠀⢈⠂⢅⠃⢆⠰⡁⢎⠰⠡⡘⢐⠨⣀⠃⠔⡰⢈⠢⡐⢡⠊⠴⡑⢎⠞⡱⢋⡜⡑⢢⠑⠢⠉⣀⣤⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣝⢽⣿⣿⣿⣿
⠰⣹⢿⣿⣿⣿⣿⢿⣿⣿⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣄⠀⠣⡐⠰⢀⡁⠂⠄⠀⠀⠀⠀⠀⠈⠀⡘⠠⠃⡜⠠⢃⠱⡈⠆⢡⠐⠌⡒⢠⠃⡔⢡⠂⡍⢢⠑⡌⢌⠡⢃⠔⠡⠂⢉⣠⣾⡿⠿⢛⣭⣷⣾⣿⣿⣿⣿⣿⣿⣿⣿⣷⣝⢿⣿⢻
⢸⣹⢿⣻⢿⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣄⠡⡑⠂⡄⠃⠌⡐⢀⠀⠀⠀⠀⠀⠀⠁⠐⠠⠑⡈⢂⠡⠘⠠⠌⢂⠑⠢⢘⠐⠢⡑⠌⡂⠥⠘⡀⠃⢈⠀⣠⣴⣿⡿⢛⢤⡺⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡻⣿
⢲⡭⢿⣽⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⡑⠌⡘⠄⠢⠄⠂⠄⡀⠀⠀⠄⠠⠀⠀⠀⠀⠀⠀⠁⠀⠀⠂⠈⠁⠂⠉⠐⠀⠂⢀⠀⡄⢐⣨⣶⣿⡿⠟⠉⣰⣷⢣⢻⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡼
⣚⡜⡳⣞⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣬⣈⠡⠘⢠⠒⠠⢀⠀⠀⠀⠁⠂⠄⡀⠀⠀⠀⠀⠀⠀⠀⠀⡀⢀⠂⠤⢑⣨⢴⣾⣿⠿⠟⠁⢀⢠⣾⣿⣯⢇⡻⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠦⠱⡑⢎⢷⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣌⡠⠌⡐⠄⠂⠄⠀⠀⠁⠠⠐⠀⡀⠀⠀⠀⠀⠀⢀⣀⣂⠐⢊⠱⡈⢎⠴⡈⠂⢀⡔⣫⣿⣿⣿⡗⣎⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠈⡁⠐⡈⢎⠳⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣔⡠⠘⠠⠌⡀⠀⠀⠀⠀⠀⠀⢀⠒⣠⢯⣙⣒⣲⠦⣝⠢⡀⠌⠠⠂⠀⢄⠣⡜⣽⣿⣿⣿⠵⣪⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⠀⠐⠀⠌⡱⢣⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣧⣄⣀⠁⠀⣀⣀⣀⢖⢿⣮⣾⡵⣞⣻⣿⣷⣶⣡⢀⠀⠂⠀⡐⢌⠲⣹⣿⣿⣿⣿⢣⠷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⠀⠀⡁⢢⠱⡍⢾⡹⡟⡿⢿⢿⡿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢋⣴⣪⣷⣿⣿⣿⣢⣵⣻⣿⣾⣿⣿⣿⣿⣿⣿⣾⣄⠄⠰⣈⢣⢳⣿⣿⣿⣿⢣⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⠀⡐⠀⠄⢂⠘⢀⠂⠑⡈⠌⢂⠑⠌⢊⠔⠣⡙⢌⠫⢝⡫⣟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢟⣵⣽⣿⢿⣯⣿⣿⡪⣋⡿⣻⢖⣿⣿⣷⣿⣽⡻⣿⣿⣿⣿⣆⠀⢊⠵⣻⣿⣿⣿⣏⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⠀⠄⠐⡀⢂⠀⠂⠌⠠⠐⢀⠂⢈⠐⠠⢈⠐⣀⣎⣴⣧⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⡛⣱⣿⢯⣻⣿⣿⣽⣾⢷⢯⣺⢾⣮⢭⣭⣽⣻⢿⣿⣿⣷⣝⢿⣿⡿⣷⡄⠘⠽⣿⣿⣿⣾⣫⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⢀⠂⠄⠀⠂⠌⠒⡈⠤⢁⠂⠌⠠⢈⠐⡠⠙⢬⠛⡽⣻⢿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⢁⡐⣰⣿⣏⠯⢛⣾⣿⣿⣿⢯⡳⢸⣯⣾⣿⣿⣿⣿⣿⣯⡻⣿⣿⣷⡽⣿⣾⢯⢆⢩⢻⣿⣿⣯⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
⠀⠄⡀⠂⠄⡀⢀⠀⡀⠀⠁⠊⢌⠁⠆⢂⠄⠡⢂⠉⡐⠡⢊⠵⡉⢯⠹⢻⢟⡿⣿⠿⠋⠠⠈⠄⣰⣿⣟⢎⢮⣿⣿⣿⢿⣿⡫⣿⠞⣿⡗⣪⡝⢿⣿⣿⣿⣿⣮⣿⣿⣿⣞⣿⣯⢧⠄⢳⢻⡿⢎⡿⣿⣿⣿⣿⣿⣿⣿⣯⢿⣻⣿⣿⣿⣿⣿⣿⣿⣿
⣌⡰⢠⢁⢂⠡⠌⠒⣀⠂⡀⠀⠀⠘⠨⡐⠌⠒⡠⠌⢠⠁⣂⠐⡈⠄⡉⠢⠘⡐⠁⠠⠐⠠⠁⠀⣿⣿⡎⣮⣿⣿⣫⣳⣫⣾⣿⣿⣘⡭⣿⣷⣿⡻⣜⢿⣿⣿⣿⣷⡽⣟⢿⣿⣿⡞⡇⠈⢇⡻⣑⠺⣽⣿⣿⣿⣿⣿⣿⢏⢯⣻⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣶⣬⡒⡌⠰⡁⠄⠒⠠⢁⠂⡀⠀⠈⠌⡑⢠⠘⣀⠒⡀⠆⡐⠠⢀⠁⠂⠄⠈⢀⠂⠁⠠⢈⣿⣿⢸⡿⣟⢱⡳⢽⣿⣿⣯⣺⢸⢮⢟⣿⣿⣿⣿⣷⡹⣿⣿⣿⣿⣝⣎⣿⣿⣿⣹⡀⠌⠴⣃⠱⢎⡿⣿⣿⣿⣿⣿⠘⣎⢷⣿⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣌⠳⣄⠃⡌⢁⠂⡐⢀⠂⠄⠀⠀⠀⠂⠄⢂⠐⠠⢀⠁⠂⠈⢀⠠⠈⠀⠀⠀⠠⢸⣿⡇⣿⣗⡾⡿⣹⣿⣟⣳⡟⣵⠸⣸⣯⢾⡿⣿⣿⣿⣝⠜⡻⣿⣿⣿⡽⡶⣻⣿⢇⢷⠈⡐⢢⠑⣊⠼⣻⣿⣿⣿⡧⢉⠬⣛⣾⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣦⣫⠔⡂⠆⡐⠠⠈⢀⠂⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠒⣼⣿⠇⣿⣿⣧⠙⣾⡟⢴⣿⢳⣿⡾⣱⣻⣏⣿⣿⣿⣿⣝⣷⡙⡝⣿⣷⣻⣽⣥⢿⡟⠚⣆⠠⢁⠎⡰⢈⠕⡻⣿⣿⠂⠌⡒⢭⢞⣿⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣮⣴⣀⡁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⢸⣿⣿⡟⣺⣿⣧⣺⡟⣿⣻⡅⢧⢷⣻⡜⣿⣿⣿⣿⣮⢿⣞⣌⢿⣧⢯⣿⡘⣧⡁⢻⡄⡀⠎⡰⢁⠎⠱⣉⠶⡁⢂⠱⢌⡞⣿⣿⣿⣿⣿⣿⣿
⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣶⣶⡒⡆⢤⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⠹⣿⡜⡟⣿⣇⣿⣿⣿⣿⣇⡟⣿⣶⢞⣍⣷⣻⡹⣿⣿⣿⣿⣯⣻⣞⢎⢿⣏⢹⣧⠽⡅⢨⣷⠠⠀⠱⣈⠌⡡⢂⠜⠠⠀⢌⠢⢜⡳⣿⣿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⡻⣷⡘⡡⢫⣱⠈⠆⡀⠀⠀⠀⠀⠠⠀⣟⢀⣿⡆⡏⣿⢸⣿⣿⣿⣿⣿⡆⡇⡟⢘⡼⣞⣷⣳⣻⣝⣿⣿⣿⣷⣻⣯⢎⣿⡜⣿⣇⠇⢸⡼⣧⠀⠁⢆⠚⡐⢢⠈⠄⡁⢂⠘⠤⣛⡼⣻⢿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⢿⣮⢿⡔⠠⡑⢎⠀⠅⢂⠀⠀⠀⠀⢠⡜⢸⣸⣧⠁⡏⢸⣿⣿⣿⣿⣿⣿⢡⠳⠀⠞⡽⣯⡳⣳⡸⡮⣿⣮⢻⣷⡽⣏⣞⣇⣿⣿⡆⣋⣷⣹⠀⡀⠈⢆⡉⢆⠘⡀⠐⡠⢁⠒⠤⣙⢳⣛⠾⡽⣧
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡙⢷⡙⠄⡁⠂⠌⡈⠄⡀⠀⠀⠠⠀⣼⠸⣧⢻⡆⠳⣽⣿⣿⣿⣿⣿⡏⣬⢧⠀⠴⢫⡹⣿⣿⣷⡽⡹⣟⢧⢿⣿⣟⣞⡼⢸⢿⣿⣔⡸⣿⠀⠀⠀⠠⠘⡄⠃⠄⠡⠐⠠⡈⠔⡈⠦⢩⡛⡵⢣
⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⢿⣿⣦⡘⠆⠀⡐⠀⠄⢂⠐⠀⠀⠀⢰⣿⢠⢿⡎⢿⠀⣿⢹⣿⣿⣱⢳⢳⣿⡾⢠⠰⢋⣷⡝⣿⣿⣟⣔⠽⣷⡫⡪⣝⢮⢧⢸⡏⣿⡟⣦⣍⠀⠀⠠⠀⠀⠌⡱⠈⢀⠡⠒⡀⠂⠄⡑⢂⠱⢉⠆
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣯⣗⣯⠞⣹⢛⡄⠀⠀⢈⠀⠂⠀⠀⢀⢮⣿⡃⣿⡞⣿⠘⡇⣽⣿⣿⢃⣏⢏⣿⣿⣫⠇⡄⣫⠽⣿⣮⢯⣻⣿⣷⣌⠻⣿⣗⠱⣯⠊⣿⠲⣻⡽⣎⢦⠀⠀⠀⠀⠀⠰⢡⠀⢂⠡⢀⠁⠂⠄⠂⠄⠃⠄
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠵⡊⢀⠀⠄⠀⠄⠂⢡⢢⢣⣿⢣⠃⣿⣿⠻⠆⢸⠹⣿⢏⠜⢌⡾⢟⣵⢫⠃⠁⡵⣷⡻⣿⣷⡻⣝⢿⣿⣷⢬⣻⢯⡘⠆⢣⢼⣡⡟⢹⡌⢷⡀⠀⠀⠀⠀⠀⢃⠀⠢⢀⠌⡐⠠⠈⠀⠂⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣵⡞⣵⣿⣾⣷⡄⠀⠀⠠⣠⠇⠆⣾⡗⠸⡀⢊⠛⡇⠀⢸⡜⢁⠅⣡⠞⣡⢟⠵⠃⢸⡎⡝⣞⢿⣮⡻⢿⣾⣿⣿⢷⣿⣲⢯⣶⣢⣽⣚⠿⣧⢿⣿⢸⣷⠀⠀⠀⠀⠁⠀⠀⢁⠂⡐⠠⢀⠁⠐⠀⠁
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⢿⡸⣝⢹⣿⣿⣿⣿⣿⣶⣄⠘⠸⠁⠃⠻⠀⠡⠁⠂⢑⡀⢠⠛⠀⠐⠌⠂⠉⣂⠥⢂⠀⢈⢷⡼⡌⣣⠘⠿⢷⣭⣛⠮⡳⢭⣛⠛⣟⣒⣋⣹⡻⢮⣾⣇⠚⣿⢢⠀⠀⠀⠀⠐⢀⠀⠂⠄⠡⠀⠂⡀⠂⠀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡵⣝⡷⣝⣻⠿⠿⠟⢛⣋⢥⢒⢡⢢⠧⣙⢲⡹⣌⠧⣍⢇⡒⢲⣉⡒⠔⢅⢂⠠⠡⢹⠀⢎⢿⣮⡓⡭⣔⣂⣀⡤⡄⠠⣥⣼⣿⠶⣖⡲⣮⢏⡨⢉⠞⣇⠫⡂⣸⢢⣀⠀⡀⠀⢀⠠⠈⠄⠡⠐⢀⠠⠁
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢟⣻⠭⠲⢛⡷⠦⢤⠻⠏⠛⡈⡱⢤⢏⡖⠨⡡⢇⡳⣌⢷⢪⡝⢶⣑⡑⢺⡝⠢⣒⡫⡙⠸⢰⠘⣧⠛⢿⣞⡯⠭⢽⠶⢿⡟⢛⣍⣮⣭⡷⠋⣱⢃⣴⡶⣺⣶⣶⣶⣯⣤⣙⠓⠢⠄⠐⠸⠭⣦⡀⢁⠂⠠⢀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢫⢚⣭⣶⣾⣿⣷⡽⣿⣿⡻⠝⠋⡴⣙⠮⣝⣾⣷⣌⡵⠒⠬⢜⣊⣭⣕⢇⢛⡬⣗⢯⣳⡽⡞⡃⡏⠣⣈⢷⣆⠺⢭⣭⠭⣭⣵⣾⠿⡿⠛⢋⢡⣾⣳⣿⡟⣵⣿⣿⣿⣿⣿⣿⣿⣷⣔⠺⣝⢯⣻⣴⣿⣦⣌⣐⣀
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⢔⡵⣿⠛⠋⣙⣩⣭⢭⡻⠁⣀⣠⢰⡳⢍⡞⣹⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣞⡽⠶⠶⣏⣧⡽⢜⣌⢦⣕⡩⣛⣆⣈⣉⣙⢻⡭⢕⣠⣴⠟⡾⣳⡛⣏⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⠙⢚⡯⠿⠿⣟⡡⣸⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣕⡴⣻⣽⢾⣟⣿⡷⣫⢗⡽⢡⣚⠦⠃⣹⢜⢣⡚⣵⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣮⡻⡛⢟⢽⡲⣼⠲⣒⣪⣵⣦⣽⣛⠉⡙⡹⣵⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⢣⣬⣍⡡⢸⢾⣿⣿
⣿⣿⣿⣿⣿⣿⣿⣿⡟⡾⢪⡾⣫⣷⣿⣿⣯⣾⣳⠏⠐⢃⣠⣤⣨⣟⡎⢧⡙⢦⣛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣝⢦⡑⡔⢿⣿⣿⣿⣿⡿⣟⠥⡪⣪⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⢈⠛⠿⡷⣮⣌⢿
⣿⣿⣿⣿⣿⣿⣿⡿⢸⡧⣫⣾⣿⣿⡿⣵⣿⡳⠙⢰⣾⣿⣿⢧⣿⢧⡿⡬⡝⢦⡛⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣾⣥⣾⣌⣉⣋⣤⣴⣶⣽⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣏⠿⡖⣶⣿⡻⣿
⣿⣿⣿⣿⣿⣿⣿⢱⣼⣴⣿⣿⣿⣿⢓⣿⠯⡗⣬⠸⣿⡿⠋⣾⣯⠳⡍⡿⣜⢣⡝⣺⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡜⠴⣻⢽⡻⣾
⣿⣿⣿⣿⣿⣿⡏⠸⣸⣧⢿⣿⣿⣇⢸⡏⡇⣮⣛⢓⡼⠊⢸⡿⣜⡳⠄⠹⣯⢖⡱⢣⢳⡻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣷⣹⣵⣾⢌⢻
⣿⣿⣿⣿⣿⣿⡇⡟⣵⣻⣯⢿⣿⡇⣇⣇⠧⢻⢫⡞⡁⠄⣾⡻⣌⢧⢩⠀⢹⡮⣕⡋⠶⣙⢮⢿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣻⣿⣿⣿⣿⣿⣷⣪⣻⣷⣵
⣿⣿⣿⣿⣿⣿⣷⣻⣞⢯⣳⢕⡽⠟⠙⣨⡷⡠⣻⠴⢃⢲⣟⡳⣍⠦⣃⠆⠀⢻⢦⡙⢧⡩⢎⡝⣻⣿⣿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣫⠀⣽⣿⣿⣿⣿⣿⣿⣷⠳⢿⣿
⣿⣿⣿⣿⣿⣿⣿⣷⣝⢔⢝⢿⣶⣶⡮⢛⢀⢱⣟⢠⠂⢸⡞⡵⢪⡱⢌⡘⠀⠈⢳⡝⢦⡙⣎⠼⣱⢻⡟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣽⣿⡜⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣡⠂⡜⣳⣿⣿⣿⣿⣿⣿⣿⣇⠋⣿
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣕⡧⣿⣷⣿⣿⠸⢿⡆⣸⠀⠰⣹⢑⠣⡜⢠⠂⠁⠀⢸⢞⡥⢫⡔⣫⢖⣫⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣜⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡍⠂⢠⢚⣵⣻⣿⣿⣿⣿⣿⣿⣿⣇⢻
⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣯⢿⣿⡆⠻⣳⢙⠀⢠⠑⣊⠱⠈⡄⠀⡰⣈⠐⣯⡜⣣⠞⣡⠞⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠫⣜⢷⣻⣿⣿⣿⣿⣿⣿⣿⡎
⣿⣿⣿⣿⣿⣿⣿⣿⢟⣿⣿⢟⣿⣿⣯⢻⣿⡄⡙⢆⠀⡞⢁⠠⢁⠂⢀⡾⣱⢣⠆⢓⢮⡱⢎⡵⢫⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⢐⣤⢁⠘⣯⠾⣽⢿⣿⣿⣿⣿⣿⣷
⣿⣿⣿⣿⣿⣿⡟⣵⣿⣿⢱⣿⣿⣿⣿⣇⣻⣿⣮⡳⡈⢡⢎⡆⠀⢀⣹⣳⣏⣧⠞⡈⢷⡙⢮⡜⣏⢾⣹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢁⡾⣟⣛⣳⠌⠻⣭⢿⣝⢿⣿⣿⣿⣿
⣿⣿⣿⣿⣿⣿⢁⢷⣿⢣⣫⡻⣿⣿⣿⣿⣞⡿⣟⠥⣜⡫⡞⢀⡌⡐⣿⣾⣵⡵⣞⡡⢸⡹⢦⡹⡜⣎⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⡯⡘⣽⣿⣿⣿⣿⣦⡈⣿⣚⢷⡹⢿⡿⣟
⣿⣿⣿⣿⣿⣿⢨⢷⣝⣷⠷⣽⣾⣯⢭⣭⣗⣚⣾⢻⡜⣣⢴⠋⠀⣾⣷⣿⣿⣿⡼⡱⡀⢟⢦⢳⡹⢬⣛⣿⣿⠿⠛⣨⣿⣿⣿⣿⣿⣿⡇⣟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢃⣽⣿⣿⣿⣿⣿⣿⣷⡆⢛⢮⡱⠃⠞⢠
⣿⣿⣿⣿⣿⣿⣧⢐⠮⠟⠿⣋⣃⣚⣛⣻⣋⢭⣒⢵⡻⠌⠃⠀⠀⣼⣿⣿⣿⣿⣿⡱⠁⢺⢭⡖⡹⢎⡵⢪⠱⣁⣾⣿⣿⣿⣿⣿⣿⣿⡇⣯⣿⣿⣿⣿⣿⣟⢿⣿⢝⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣌⢆⡡⠍⢢⣿
⣿⣿⣿⣿⣿⣿⣿⣷⣕⡯⢭⣭⣭⣅⠭⣴⡺⢿⡛⠍⣀⡤⠚⠀⡀⣿⣻⣿⣿⣿⣿⡷⡡⢘⡾⣸⠱⢣⠌⣡⢾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⣳⢿⣿⣿⣿⣿⣮⣷⣽⢷⣝⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢱⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⡄⣀⡽
*/
