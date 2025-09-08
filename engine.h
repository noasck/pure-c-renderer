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

typedef uint8_t icolor_t[ 4 ];

typedef struct DBuffer
{
    icolor_t c;
    uint64_t z;

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

typedef struct Bound
{
    int *      x;
    icolor_t * c;
    uint64_t * zi;
} Bound;

typedef struct Framebuffer
{
    SDL_Surface * surface;

    // left bound of current triangle scanline
    Bound l;
    // right bound
    Bound r;

    /* 22.02.25 ::: Converted opaque DBuffer AOS -> SOA */
    DBuffer ** transparent;
    icolor_t * opaque_c;
    uint64_t * opaque_z;

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

    /* SOA for vertices after projection applied */
    vec3 * v;
    int    v_cnt;

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
OdbI~yZPUUMMZMMMZZbddddO660$aPd$$Q8g$$gg88g8QQQEzv?|\vvvv?((cKd8$$KYLLLLYY
IUK}rwMIIqZZMMMMMMM555MZd68zxYv}$BB88$$gQg8QQQQEPx?|\\\v|rr(xuzGOEyYYYY}}}
xL})V]yj3MZMMM5MMMMMMMZO0QQy\v>=q#BQ8ggg$8QQQQgZmx|\v|))rrr(vxxuKMuYYYYYYY
\\v|wKyyhWMMMMqaP5MZdd0QQg0q,:!!9#BQ88Q8g8QQ8$0EOaIkyuYxxvTzIV}VmmY]]]]iii
|?\cammHPmemKa3GMbdb5d$0MxuZ~._*gQBQQQQQ8gg0OZ3a3WMZbdddZZbZd6ZzHIT}}}}}}}
|?xv}ZMKGqWWPWZddOOMKXV):.=j_-',ceTqVukhwyGMayl}}uVywzhK3Gd$gRMIWzcccucccV
((xxVd8d|r?xL}YY}vr<=!_'''_x``''*T<~L~,___:vV}v\\vvvvvvxxL}uVVyXHVuuuulllu
))xvVdOgEr,:="~wa*...''``'=<````,~=!:r!....-_=*r|vv\\|\\v\vv\xVKGozooojXXo
))x|}Md6QZY(::Pd"^_`````'._`````,!:!,,~-.'''''''-_:!=~<~=!!!!!=<*vVKaa3aa3
))vvv396EEyzPylx!_.`````````````!<!!__:-'````````````````````'..--_~u3Kaaa
))\v(zRR6RzcVGWz]-``````````````xV\=_--'```````````````````````'.---_xK3KK
rr)x|xd$EOIyxhWul:`````````````-hU\,-.'`````````````````````` ``'.----=VPm
rrr(x|V0gRUkxyKx\*<:,.`.'``````rU)_-.'``````````'```````````````'.._:.._rX
)))r)xxVDgPIT}}))}uv?((r^^=:,_~z^_..`````````````'.```````````````._=-...-
)))rrrxLX09owx))(xx(??|?|(???v?>-.'```````````````..'`````````````.-!=-...
}YL]xxvvxlMbz}|)???(????(?|(^!_-.```````````` `````'.'````````````'-_*~-.'
uucuccccccuzKezc}YL]xxxv|*=,-.''````HOLY C`````````''.'````````````.-:)~-.
ulluuuulllllluuVyyVcVcL*:_-.'```````````````````````''..```````````'._!v=-
lTTTlllllllllulllulc}<,--.'```````````````````````````'''```````````'-,~x=
IozzwwyyyVVcuulucyzIr,-.'''````````````````````````````'''``````````'._:\y
LY}luyzXhmK35WGW5M5o!_-.''''''''```````````````````````''''`````````'.-_!y
xxxvvvv\|\vxi}TuVoUI^,-..........''''```````````````````''''````''``''.-:^
wyVuuT}}Li]xxxxxvv|)r:_...-_::,_--...'''````````````````'''..'''''''''.-_!
yVwjIUeeUehehjwyVcu}}*,_..-_:*r!,_---..'''`````````````'''........''...--,
v?||\vvvvv}y}cyzoIeIUj~_-.._,!*v!:,__--..''''````````'''...............--_
|?|\||???\ui)??\vvxxxu|:_---_:=*x^:",_---.......''``''.......---.......--_
||\\\|\v\xyx|(?|?\vxLc}^,_--_,:=rcu<!:,__------..''`'........-__-..-.-..--
||\\\\vvv}yv\\\vxxxx}u}x:_--_,:!^xO5i>!:":",__---..''......---__,---------
xxxxxxxLYycvv\vvvvv]Vwzo^,_-_,"!<rm6E3x^~!:",__---....-..-----__,_--------
||vvvvvvLolxxxxiiTVoqGG3Y:_-__,:~*c6$ggz^!::,,____-----------__,""--------
\vvvvvvxckxxx}}YTcVIKwU3m<,_-_,:~*xR8du^!:",,,,,___---------__,":!_----___
\vv\vvv]wwL}luuuVVym3eeUKL"___,:=^x5y>!::::::""",,___________,"::=:_______
vvvvvxiToyuuuuuuVVwKaeeUKm=___"!^xKEO$gg$$E6d5myTx*=::",,,,,,"::!=:____,,,
Yi]LTuuVIVuuuuucVyo3mUUeKGv!!!!:"::=^rTaEQQ8$00$8QB#B$dmcv~!:::!!~:,,,,,,"
uVwkzzzm3yzjojzjwzKPmjuv^!___--------__,:*Tb88EdMM5Z9$QQBBQ0Wyx^~~:,,,,"":
uT}YTucXzucVVyzIU3Pc*:__---...........--__,:regQ09ddObMd69D$8BBQ05x=::""""
luccccVeyucccVVu\~,--......''''''''......---_"rPQ8RRRR9OdO6dddR8###8MY<:::
VVVVVVymVccVVx^"-....'''````````'`''''''....-__!}08$0ERRR96RE6O$8dOE8BQZV*
yVVVVyUmVVcx^_-..'''`````````````````'''''...--_:x8Q0E9RRRER0$0QOdbMWd08BQ
mmmmmeMUwx=_-...'```````````````````````''''...-_,}8B8QQ88g$g8#Q09OddbMMd0
kkzoUHRj*,--..''``````````````````````````'''...-_~XBQBQQQ8$$Q800D6bZddZqG
XjzV]xv!_--..''````````````````````````````'....--,xR#BQQ88gQQ00$$g0dZZdOd
v*=,!~:,_-...''````````````````````````````'....-_,~I##BQ88BB0$g88QQEdddE6
:::=^:,_--..''````````````   ``````````````''...--_!uQ#BQQB#8$$8QQQ8E96OE0
Vyw(!:,_--..'```````````       ```````````''....-_,:T8#BB##Q8QQBBQQQBBQ8$D
#Bb>:"__--..''`````````         ``````````'....--_,:uQ####BBB###BgdIT|*^>~
QQx!:,__-....''````````         ``````````'...---_,!wB######QZur=,_-------
Q0*!",__--.....'''````````     `````````'''..---__,^M@@@#QML<"___---------
Q0*!",_----.....''''```````````````````''...---__,"iQ@@$y*:__-------------
88v!",__---.....''''''```````````````'''..---___,"<b#$l<,__--------.---.--
gQo=:,__-----......''''''````````''''....---___,,:wQz^",_-------........--
$gEx<:,___-----........'''''`''''''....----__,,":*u*:,___----.............
v=:!~*^!,,__-----...................----____,":=||:",__-----..............
,,,,":~r~:",___-------...........-----____,,:!>)^:",__------..............
",,,,:!^)~!::,,____----------------____,,"::=*(~::",__------..............
::::::!~|r<~!::",,,_________________,,,"::=>v\~!::",__-----..........''...
::!!!!=<iy)*^~=!!::"",,,,,,,,,,,"""":::!=~r}v<=!::",__-----.....''''''''..
!!!===~^\HmT?*^^>~==!!!::::::::!!!!!==~^*Lwx^~!!::",___----......'`````'''
!===~~<^rLkwyl]\)r**^^^<<<>><<<<^^^*r)vcdG]*<~=!::",,__----.....''`````''.
===~~<^^r)xul}lVVyVcTYxxvvvvvvvvx}VUb8#@$i)*^~=!!:::,,___----.....'```''..
^~~><^**r))v}u}L]iYlVwzXIemKPGKPd08B###@Kx(*^<~=!!:::",,___-----....'....-
)^<^^^*r)\r*rx}VV}YY}Tuywzkyu}xxYV36gQB#ULvr*^<~==!:::",,,____---__----___
c*^^^**r|vr^^^*|umUozkwyuYixxxxxxx]cM0D$Zcivr*^^~~=!!!:::"",,,,____,,,,,":
Hv*^***r(vr*<><^*vTVcul}YLi]x]xxxx}jZdOD0Klxv)r*^<~~==!!!::::::::::::::!!!
Ic)*****rxvr*^<<^*\TclT}}YLLLiiLcIGdddROZOKuLx|rr*^^<~~~~===!!!!!!!!!===~~

*/
