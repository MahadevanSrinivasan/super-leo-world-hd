/*
 * Super Leo World HD: Dino Island (Native RetroPie / SDL2 Widescreen 60FPS Edition)
 * 1:1 Gameplay, Physics, Stage Layout & Visual Match with the Web Edition (game.js)
 * Optimized for Locked 60 FPS on Raspberry Pi 1 (Broadcom VideoCore IV OpenGL ES 2.0):
 *  - Pre-baked Font Atlas Texture (1 draw call per glyph instead of per-pixel FillRects)
 *  - Pre-baked Stage Sky + Mountain + Hill Panorama Texture (zero fullscreen alpha overdraw)
 *  - Exact 1920x1080 (TILE=64, GRAVITY=0.68) physics coordinates mapped to 640x360 -> 1280x720 HDMI
 *  - Includes Tropical Overworld & Cave Background Music + Chiptune SFX
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact 1:1 World Coordinate System from game.js */
#define W 1920.0f
#define H 1080.0f
#define TILE 64.0f
#define GRAVITY 0.68f
#define MAP_W 185
#define MAP_H 17

/* Hardware-Accelerated 16:9 Render Target (scaled by VideoCore IV GPU to 1280x720) */
#define REN_W 640
#define REN_H 360
#define SCALE (1.0f / 3.0f)

#define MAX_ENTITIES 96
#define MAX_FIREBALLS 16
#define MAX_PARTICLES 64
#define MAX_SHOCKWAVES 8

enum {
    DINO_NONE   = 0,
    DINO_GREEN  = 1,
    DINO_RED    = 2,
    DINO_BLUE   = 3,
    DINO_YELLOW = 4
};

enum {
    ENT_NONE = 0,
    ENT_WALKER,
    ENT_KOOPA,
    ENT_SHELL,
    ENT_FLYER,
    ENT_POKEY,
    ENT_PLATFORM,
    ENT_DINO_MOUNT,
    ENT_MUSHROOM,
    ENT_FLOWER,
    ENT_BOSS
};

enum {
    SFX_NONE = 0,
    SFX_JUMP,
    SFX_FLUTTER,
    SFX_TONGUE,
    SFX_SWALLOW,
    SFX_SPIT,
    SFX_HATCH,
    SFX_QUAKE,
    SFX_COIN,
    SFX_STOMP,
    SFX_POWERUP,
    SFX_BRICK,
    SFX_BAIL,
    SFX_HURT,
    SFX_CLEAR,
    SFX_CHECKPOINT
};

typedef struct {
    Uint8 r, g, b, a;
} Color;

static Color rgba(int r, int g, int b, int a) {
    Color c = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    return c;
}
static Color rgb(int r, int g, int b) { return rgba(r, g, b, 255); }

typedef struct {
    int id;
    const char *name;
    Color body;
    Color dark;
    Color belly;
    Color crest;
    Color saddle;
    const char *ability;
} DinoDef;

static const DinoDef DINO_DEFS[5] = {
    {0, "NONE",        {0,0,0,0},        {0,0,0,0},       {0,0,0,0},        {0,0,0,0},        {0,0,0,0},       ""},
    {1, "Emerald Rex", {16,185,129,255}, {4,120,87,255},  {209,250,229,255},{239,68,68,255},  {220,38,38,255}, "Tongue-Grab & Flutter-Jump!"},
    {2, "Blaze Rex",   {239,68,68,255},  {185,28,28,255}, {254,240,138,255},{249,115,22,255}, {251,191,36,255},"Spits 3x Giant Fireballs!"},
    {3, "Sky Rex",     {59,130,246,255}, {29,78,216,255}, {224,242,254,255},{168,85,247,255}, {244,63,94,255}, "Winged Multi-Flap Flight!"},
    {4, "Quake Rex",   {245,158,11,255}, {180,83,9,255},  {254,249,195,255},{16,185,129,255}, {37,99,235,255}, "Seismic Shockwave Stomp!"}
};

typedef struct {
    const char *code;
    const char *title;
    int theme; /* 0=tropics, 1=cave, 2=desert, 3=castle, 4=sky, 5=ice, 6=night */
    int sky_top[3];
    int sky_bot[3];
    int hill_far[3];
    int hill_near[3];
    int gnd_top[3];
    int gnd_body[3];
    int default_dino;
    int second_dino;
    int has_boss;
    const char *boss_name;
    int boss_hp;
} StageDef;

static const StageDef STAGES[8] = {
    {"1-1", "Emerald Dino Lagoon",      0, {14,165,233}, {186,230,253}, {13,148,136}, {16,185,129}, {34,197,94},  {133,77,14},  DINO_GREEN,  DINO_RED,    0, "", 0},
    {"1-2", "Amber Crystal Cavern",     1, {15,23,42},   {30,27,75},    {49,46,129},  {67,56,202},  {245,158,11}, {51,65,85},   DINO_YELLOW, DINO_GREEN,  0, "", 0},
    {"2-1", "Sunburst Oasis & Dunes",   2, {249,115,22}, {254,240,138}, {217,119,6},  {245,158,11}, {253,224,71}, {154,52,18},  DINO_RED,    DINO_YELLOW, 0, "", 0},
    {"2-2", "Tricera Magma Keep",       3, {24,24,27},   {69,10,10},    {127,29,29},  {153,27,27},  {148,163,184},{63,63,70},   DINO_RED,    DINO_BLUE,   1, "TRICERA-KING", 6},
    {"3-1", "Skyward Cloud Isles",      4, {37,99,235},  {224,242,254}, {96,165,250}, {147,197,253},{244,63,94},  {248,250,252},DINO_BLUE,   DINO_GREEN,  0, "", 0},
    {"3-2", "Aurora Glacier Peaks",     5, {15,23,42},   {6,182,212},   {2,132,199},  {56,189,248}, {207,250,254},{30,58,138},  DINO_RED,    DINO_BLUE,   0, "", 0},
    {"4-1", "Starlight Neon Canyon",    6, {9,13,22},    {59,7,100},    {88,28,135},  {126,34,206}, {192,132,252},{30,27,75},   DINO_YELLOW, DINO_BLUE,   0, "", 0},
    {"4-2", "King Tyrannor's Citadel",  3, {9,9,11},     {127,29,29},   {69,10,10},   {153,27,27},  {248,113,113},{39,39,42},   DINO_RED,    DINO_YELLOW, 1, "KING TYRANNOR", 10}
};

typedef struct {
    int active;
    int type;
    int dino_type;
    float x, y, vx, vy;
    float start_x, start_y;
    float w, h;
    int dir;
    int timer;
    int hp, max_hp;
    int invuln;
} Entity;

typedef struct {
    int active;
    float x, y, vx, vy, r;
    int from_dino;
    int life;
} Fireball;

typedef struct {
    int active;
    float x, y, vx, vy, size;
    Color color;
    int life, max_life;
    char text[16];
} Particle;

typedef struct {
    int active;
    float x, y, r, max_r;
    Color color;
} Shockwave;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* Fast Pre-Baked Textures (eliminates per-frame overdraw & per-pixel FillRects) */
static SDL_Texture *tex_font = NULL;
static SDL_Texture *tex_stage_bg = NULL;
static SDL_Texture *tex_cloud = NULL;
static SDL_Texture *tex_circle = NULL;
static SDL_Texture *tex_ring = NULL;
static SDL_Texture *tex_brick = NULL;
static SDL_Texture *tex_qblock = NULL;
static SDL_Texture *tex_used = NULL;
static SDL_Texture *tex_egg[5] = {NULL};
static SDL_Texture *tex_pipe = NULL;
static SDL_Texture *tex_berry = NULL;
static SDL_Texture *tex_coin = NULL;
static SDL_Texture *tex_dino[5][2][2] = {{{NULL}}}; /* [dino_id][walk_frame][mouth_full] */
static SDL_Texture *tex_leo[3][2] = {{NULL}};       /* [power][walk_frame] */
static SDL_Texture *tex_enemy[4] = {NULL};          /* walker, koopa, shell, flyer */
static SDL_Texture *tex_boss = NULL;

static char map[MAP_H][MAP_W];
static float bump_off[MAP_H][MAP_W];

static int game_state = 0; /* 0=title, 1=playing, 2=stageclear, 3=victory */
static int current_stage = 0;
static int global_tick = 0;
static float camera_x = 0.0f;
static int score = 0;
static int coins = 0;
static int lives = 5;
static int berries_eaten = 0;
static float checkpoint_x = 0.0f;
static char banner_text[80] = "";
static int banner_timer = 0;
static int state_timer = 0;

static struct {
    float x, y, vx, vy;
    float w, h;
    int dir;
    int power;
    int invuln;
    int star_timer;
    int on_ground;
    int coyote;
    int wall_slide;
    int ground_pounding;
    int gp_timer;
    float anim_frame;
    float squash_x, squash_y;
    float scarf_wave;
    int dino;
    int dino_mouth; /* 0=empty, 1=shell, 2=fire */
    int tongue_state;
    float tongue_len;
    int tongue_reward; /* 0=none, 1=berry/coin, 2=shell, 3=fire */
    int flutter_cd;
    int wing_flaps;
} player;

static Entity entities[MAX_ENTITIES];
static Fireball fireballs[MAX_FIREBALLS];
static Particle particles[MAX_PARTICLES];
static Shockwave shockwaves[MAX_SHOCKWAVES];

/* =========================================================================
 * Audio Synthesizer (Background Melody + Sound Effects)
 * ========================================================================= */
static volatile int cur_sfx = SFX_NONE;
static volatile int sfx_pos = 0;
static volatile int sfx_len = 0;
static volatile float bgm_freq = 0.0f;
static volatile int bgm_note_timer = 0;
static int music_tick = 0;
static int music_step = 0;
static float sfx_phase = 0.0f;
static float bgm_phase = 0.0f;

static const float MELODY_OVERWORLD[32] = {
    523.25f, 659.25f, 783.99f, 659.25f, 880.0f, 783.99f, 659.25f, 523.25f,
    587.33f, 698.46f, 880.0f, 698.46f, 783.99f, 659.25f, 587.33f, 493.88f,
    523.25f, 659.25f, 783.99f, 1046.5f, 987.77f, 880.0f, 783.99f, 659.25f,
    587.33f, 659.25f, 698.46f, 783.99f, 880.0f, 987.77f, 1046.5f, 0.0f
};

static const float MELODY_CAVE[16] = {
    329.63f, 392.0f, 493.88f, 440.0f, 392.0f, 329.63f, 293.66f, 329.63f,
    349.23f, 440.0f, 523.25f, 493.88f, 440.0f, 392.0f, 329.63f, 0.0f
};

static void play_sfx(int id) {
    SDL_LockAudio();
    cur_sfx = id;
    sfx_pos = 0;
    switch (id) {
        case SFX_JUMP:       sfx_len = 3200; break;
        case SFX_FLUTTER:    sfx_len = 4200; break;
        case SFX_TONGUE:     sfx_len = 2600; break;
        case SFX_SWALLOW:    sfx_len = 3800; break;
        case SFX_SPIT:       sfx_len = 3200; break;
        case SFX_HATCH:      sfx_len = 7500; break;
        case SFX_QUAKE:      sfx_len = 6500; break;
        case SFX_COIN:       sfx_len = 3600; break;
        case SFX_STOMP:      sfx_len = 2400; break;
        case SFX_POWERUP:    sfx_len = 8000; break;
        case SFX_BRICK:      sfx_len = 3200; break;
        case SFX_BAIL:       sfx_len = 4000; break;
        case SFX_HURT:       sfx_len = 5200; break;
        case SFX_CLEAR:      sfx_len = 13000; break;
        case SFX_CHECKPOINT: sfx_len = 5000; break;
        default:             sfx_len = 0; break;
    }
    SDL_UnlockAudio();
}

static void update_music(void) {
    if (game_state != 1) {
        bgm_freq = 0.0f;
        return;
    }
    music_tick++;
    if (music_tick >= 13) {
        music_tick = 0;
        float note = (current_stage % 2 == 1)
                         ? MELODY_CAVE[music_step % 16]
                         : MELODY_OVERWORLD[music_step % 32];
        music_step++;
        SDL_LockAudio();
        bgm_freq = note;
        bgm_note_timer = 2400;
        SDL_UnlockAudio();
    }
}

static void audio_cb(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Sint16 *buf = (Sint16 *)stream;
    int samples = len / 2;
    for (int i = 0; i < samples; i++) {
        int mix = 0;
        if (bgm_note_timer > 0 && bgm_freq > 20.0f) {
            bgm_phase += bgm_freq / 22050.0f;
            if (bgm_phase >= 1.0f) bgm_phase -= 1.0f;
            /* Soft triangle wave for background melody */
            float tri = (bgm_phase < 0.5f) ? (bgm_phase * 4.0f - 1.0f) : (3.0f - bgm_phase * 4.0f);
            mix += (int)(tri * 650.0f);
            bgm_note_timer--;
        }
        if (cur_sfx != SFX_NONE && sfx_pos < sfx_len) {
            float prog = (float)sfx_pos / (float)sfx_len;
            float freq = 440.0f;
            int noise = 0;
            switch (cur_sfx) {
                case SFX_JUMP:    freq = 240.0f + 460.0f * prog; break;
                case SFX_FLUTTER: freq = 300.0f + 520.0f * sinf(prog * 3.14159f); break;
                case SFX_TONGUE:  freq = 320.0f + 600.0f * prog; break;
                case SFX_SWALLOW: freq = (prog < 0.5f) ? 320.0f : 640.0f; break;
                case SFX_SPIT:    freq = 820.0f - 620.0f * prog; break;
                case SFX_HATCH: {
                    static const float n[6] = {523, 659, 784, 1046, 987, 1175};
                    int idx = (int)(prog * 6.0f);
                    if (idx > 5) idx = 5;
                    freq = n[idx];
                    break;
                }
                case SFX_QUAKE:   noise = 1; break;
                case SFX_COIN:    freq = (prog < 0.3f) ? 988.0f : 1319.0f; break;
                case SFX_STOMP:   freq = 460.0f - 280.0f * prog; break;
                case SFX_POWERUP: {
                    static const float n[8] = {330, 392, 659, 523, 587, 784, 659, 880};
                    int idx = (int)(prog * 8.0f);
                    if (idx > 7) idx = 7;
                    freq = n[idx];
                    break;
                }
                case SFX_BRICK:   noise = 1; break;
                case SFX_BAIL:    freq = 440.0f + 700.0f * prog; break;
                case SFX_HURT:    freq = 580.0f - 400.0f * prog; break;
                case SFX_CLEAR: {
                    static const float n[7] = {262, 330, 392, 523, 659, 784, 1047};
                    int idx = (int)(prog * 7.0f);
                    if (idx > 6) idx = 6;
                    freq = n[idx];
                    break;
                }
                case SFX_CHECKPOINT: freq = (prog < 0.5f) ? 523.0f : 784.0f; break;
            }
            if (noise) {
                mix += (int)(((rand() % 5600) - 2800) * (1.0f - prog));
            } else {
                sfx_phase += freq / 22050.0f;
                if (sfx_phase >= 1.0f) sfx_phase -= 1.0f;
                int sq = (sfx_phase < 0.5f) ? 1 : -1;
                mix += (int)(sq * 2400 * (1.0f - 0.3f * prog));
            }
            sfx_pos++;
            if (sfx_pos >= sfx_len) cur_sfx = SFX_NONE;
        }
        if (mix > 30000) mix = 30000;
        if (mix < -30000) mix = -30000;
        buf[i] = (Sint16)mix;
    }
}

/* =========================================================================
 * Fast Procedural Texture Baker (Matches game.js Canvas Shapes!)
 * ========================================================================= */
static void put_px(Uint32 *px, int w, int h, int x, int y, Color c) {
    if (x < 0 || x >= w || y < 0 || y >= h || c.a == 0) return;
    px[y * w + x] = ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | (Uint32)c.a;
}

static void fill_elli(Uint32 *px, int w, int h, float cx, float cy, float rx, float ry, Color c) {
    int x0 = (int)(cx - rx - 1), x1 = (int)(cx + rx + 1);
    int y0 = (int)(cy - ry - 1), y1 = (int)(cy + ry + 1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f - cx) / rx;
            float dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) put_px(px, w, h, x, y, c);
        }
    }
}

static void fill_rrect(Uint32 *px, int w, int h, int x0, int y0, int rw, int rh, int rad, Color c) {
    for (int y = y0; y < y0 + rh; y++) {
        for (int x = x0; x < x0 + rw; x++) {
            int dx = 0, dy = 0;
            if (x < x0 + rad) dx = (x0 + rad) - x;
            else if (x >= x0 + rw - rad) dx = x - (x0 + rw - rad - 1);
            if (y < y0 + rad) dy = (y0 + rad) - y;
            else if (y >= y0 + rh - rad) dy = y - (y0 + rh - rad - 1);
            if (dx * dx + dy * dy <= rad * rad) put_px(px, w, h, x, y, c);
        }
    }
}

static SDL_Texture *create_tex(Uint32 *px, int w, int h, int blend) {
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(px, w, h, 32, w * 4,
                                              0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_SetTextureBlendMode(t, blend ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_FreeSurface(s);
    free(px);
    return t;
}

/* 5x7 Pixel Font Baked into a Single Texture Atlas (96 glyphs x 6x8 pixels = 576x8) */
static const unsigned char font5x7[96][7] = {
    [' ' - 32] = {0,0,0,0,0,0,0},
    ['!' - 32] = {4,4,4,4,4,0,4},
    ['+' - 32] = {0,4,4,31,4,4,0},
    ['-' - 32] = {0,0,0,31,0,0,0},
    ['.' - 32] = {0,0,0,0,0,0,4},
    ['/' - 32] = {1,1,2,4,8,16,16},
    ['0' - 32] = {14,17,19,21,25,17,14},
    ['1' - 32] = {4,12,4,4,4,4,14},
    ['2' - 32] = {14,17,1,6,8,16,31},
    ['3' - 32] = {31,2,4,6,1,17,14},
    ['4' - 32] = {2,6,10,18,31,2,2},
    ['5' - 32] = {31,16,30,1,1,17,14},
    ['6' - 32] = {6,8,16,30,17,17,14},
    ['7' - 32] = {31,1,2,4,8,8,8},
    ['8' - 32] = {14,17,17,14,17,17,14},
    ['9' - 32] = {14,17,17,15,1,2,12},
    [':' - 32] = {0,4,4,0,4,4,0},
    ['<' - 32] = {2,4,8,16,8,4,2},
    ['=' - 32] = {0,0,31,0,31,0,0},
    ['>' - 32] = {8,4,2,1,2,4,8},
    ['?' - 32] = {14,17,1,2,4,0,4},
    ['[' - 32] = {14,8,8,8,8,8,14},
    [']' - 32] = {14,2,2,2,2,2,14},
    ['A' - 32] = {14,17,17,31,17,17,17},
    ['B' - 32] = {30,17,17,30,17,17,30},
    ['C' - 32] = {14,17,16,16,16,17,14},
    ['D' - 32] = {28,18,17,17,17,18,28},
    ['E' - 32] = {31,16,16,30,16,16,31},
    ['F' - 32] = {31,16,16,30,16,16,16},
    ['G' - 32] = {14,17,16,23,17,17,15},
    ['H' - 32] = {17,17,17,31,17,17,17},
    ['I' - 32] = {14,4,4,4,4,4,14},
    ['J' - 32] = {7,2,2,2,2,18,12},
    ['K' - 32] = {17,18,20,24,20,18,17},
    ['L' - 32] = {16,16,16,16,16,16,31},
    ['M' - 32] = {17,27,21,21,17,17,17},
    ['N' - 32] = {17,17,25,21,19,17,17},
    ['O' - 32] = {14,17,17,17,17,17,14},
    ['P' - 32] = {30,17,17,30,16,16,16},
    ['Q' - 32] = {14,17,17,17,21,18,13},
    ['R' - 32] = {30,17,17,30,20,18,17},
    ['S' - 32] = {15,16,16,14,1,1,30},
    ['T' - 32] = {31,4,4,4,4,4,4},
    ['U' - 32] = {17,17,17,17,17,17,14},
    ['V' - 32] = {17,17,17,17,17,10,4},
    ['W' - 32] = {17,17,17,21,21,21,10},
    ['X' - 32] = {17,17,10,4,10,17,17},
    ['Y' - 32] = {17,17,17,10,4,4,4},
    ['Z' - 32] = {31,1,2,4,8,16,31}
};

/* Bake stage sky + sun/stars + mountain range + rolling hills into a single fast texture! */
static void bake_stage_background(int st_idx) {
    if (tex_stage_bg) {
        SDL_DestroyTexture(tex_stage_bg);
        tex_stage_bg = NULL;
    }
    const StageDef *st = &STAGES[st_idx];
    int BW = 640, BH = 360;
    Uint32 *px = (Uint32 *)calloc(BW * BH, 4);

    /* 1. Smooth Sky Gradient */
    for (int y = 0; y < BH; y++) {
        float t = (float)y / (float)BH;
        Color c = rgb(
            (int)(st->sky_top[0] + (st->sky_bot[0] - st->sky_top[0]) * t),
            (int)(st->sky_top[1] + (st->sky_bot[1] - st->sky_top[1]) * t),
            (int)(st->sky_top[2] + (st->sky_bot[2] - st->sky_top[2]) * t)
        );
        Uint32 val = ((Uint32)c.r << 24) | ((Uint32)c.g << 16) | ((Uint32)c.b << 8) | 0xFF;
        for (int x = 0; x < BW; x++) px[y * BW + x] = val;
    }

    /* 2. Sun or Twinkling Stars */
    if (st->theme == 1 || st->theme == 3 || st->theme == 6) {
        for (int i = 0; i < 45; i++) {
            int sx = (i * 73) % BW;
            int sy = (i * 31) % (BH / 2);
            fill_elli(px, BW, BH, (float)sx, (float)sy, 1.5f, 1.5f, rgb(255, 255, 255));
        }
    } else {
        fill_elli(px, BW, BH, BW * 0.82f, 56.0f, 38.0f, 38.0f, rgb(253, 224, 71));
        fill_elli(px, BW, BH, BW * 0.82f, 56.0f, 24.0f, 24.0f, rgb(254, 249, 195));
    }

    /* 3. Mountain Range (Far) & Rolling Hills (Near) matching game.js curves */
    Color hf = rgb(st->hill_far[0], st->hill_far[1], st->hill_far[2]);
    Color hn = rgb(st->hill_near[0], st->hill_near[1], st->hill_near[2]);
    for (int x = 0; x < BW; x++) {
        float u_far = fmodf((float)x, 128.0f) / 128.0f;
        int y_far = BH - 115 - (int)(sinf(u_far * 3.14159f) * 75.0f);
        for (int y = y_far; y < BH; y++) put_px(px, BW, BH, x, y, hf);

        float u_near = fmodf((float)(x + 40), 160.0f) / 160.0f;
        int y_near = BH - 65 - (int)(sinf(u_near * 3.14159f) * 58.0f);
        for (int y = y_near; y < BH; y++) put_px(px, BW, BH, x, y, hn);
    }

    tex_stage_bg = create_tex(px, BW, BH, 0);
}

static void bake_all_textures(void) {
    /* 1. Font Atlas (576x8) with built-in dark drop-shadow */
    {
        int FW = 96 * 6, FH = 8;
        Uint32 *pf = (Uint32 *)calloc(FW * FH, 4);
        for (int ch = 0; ch < 96; ch++) {
            const unsigned char *g = font5x7[ch];
            int bx = ch * 6;
            for (int r = 0; r < 7; r++) {
                for (int c = 0; c < 5; c++) {
                    if (g[r] & (1 << (4 - c))) {
                        put_px(pf, FW, FH, bx + c + 1, r + 1, rgba(10, 15, 28, 220));
                    }
                }
            }
            for (int r = 0; r < 7; r++) {
                for (int c = 0; c < 5; c++) {
                    if (g[r] & (1 << (4 - c))) {
                        put_px(pf, FW, FH, bx + c, r, rgb(255, 255, 255));
                    }
                }
            }
        }
        tex_font = create_tex(pf, FW, FH, 1);
    }

    /* 2. Circle & Shockwave Ring (32x32) */
    {
        int S = 32;
        Uint32 *pc = (Uint32 *)calloc(S * S, 4);
        Uint32 *pr = (Uint32 *)calloc(S * S, 4);
        fill_elli(pc, S, S, 16, 16, 15, 15, rgb(255, 255, 255));
        for (int y = 0; y < S; y++) {
            for (int x = 0; x < S; x++) {
                float d = hypotf(x + 0.5f - 16.0f, y + 0.5f - 16.0f);
                if (d >= 11.5f && d <= 15.0f) put_px(pr, S, S, x, y, rgb(255, 255, 255));
            }
        }
        tex_circle = create_tex(pc, S, S, 1);
        tex_ring   = create_tex(pr, S, S, 1);
    }

    /* 3. Fluffy 3-Puff Cloud (64x32) */
    {
        int CW = 64, CH = 32;
        Uint32 *pc = (Uint32 *)calloc(CW * CH, 4);
        Color wc = rgba(255, 255, 255, 225);
        fill_elli(pc, CW, CH, 18, 19, 12, 11, wc);
        fill_elli(pc, CW, CH, 31, 15, 15, 14, wc);
        fill_elli(pc, CW, CH, 46, 19, 12, 11, wc);
        tex_cloud = create_tex(pc, CW, CH, 1);
    }

    /* 4. World Tiles (24x24 matching game.js renderTiles!) */
    int TS = 24;
    {
        /* Brick 'B' */
        Uint32 *pb = (Uint32 *)calloc(TS * TS, 4);
        fill_rrect(pb, TS, TS, 1, 1, TS - 2, TS - 2, 4, rgb(234, 88, 12));
        fill_rrect(pb, TS, TS, 3, 3, TS - 6, TS - 6, 3, rgb(154, 52, 18));
        fill_rrect(pb, TS, TS, 5, 5, TS - 10, TS - 10, 2, rgb(217, 119, 6));
        tex_brick = create_tex(pb, TS, TS, 1);

        /* Question '?' */
        Uint32 *pq = (Uint32 *)calloc(TS * TS, 4);
        fill_rrect(pq, TS, TS, 1, 1, TS - 2, TS - 2, 4, rgb(202, 138, 4));
        fill_rrect(pq, TS, TS, 2, 2, TS - 4, TS - 4, 4, rgb(253, 224, 71));
        /* '?' glyph inside block */
        fill_rrect(pq, TS, TS, 8, 5, 8, 3, 1, rgb(113, 63, 18));
        fill_rrect(pq, TS, TS, 13, 7, 3, 5, 1, rgb(113, 63, 18));
        fill_rrect(pq, TS, TS, 10, 10, 5, 4, 1, rgb(113, 63, 18));
        fill_rrect(pq, TS, TS, 10, 16, 3, 3, 1, rgb(113, 63, 18));
        tex_qblock = create_tex(pq, TS, TS, 1);

        /* Used 'U' */
        Uint32 *pu = (Uint32 *)calloc(TS * TS, 4);
        fill_rrect(pu, TS, TS, 1, 1, TS - 2, TS - 2, 4, rgb(71, 85, 105));
        tex_used = create_tex(pu, TS, TS, 1);

        /* Spotted Dino Egg Blocks 'E' / 'D' (4 species) */
        for (int d = 1; d <= 4; d++) {
            Uint32 *pe = (Uint32 *)calloc(TS * TS, 4);
            Color dc = DINO_DEFS[d].body;
            fill_rrect(pe, TS, TS, 1, 1, TS - 2, TS - 2, 5, dc);
            fill_rrect(pe, TS, TS, 3, 3, TS - 6, TS - 6, 4, rgb(248, 250, 252));
            fill_elli(pe, TS, TS, 8, 9, 3.2f, 3.2f, dc);
            fill_elli(pe, TS, TS, 16, 11, 3.8f, 3.8f, dc);
            fill_elli(pe, TS, TS, 10, 17, 2.8f, 2.8f, dc);
            tex_egg[d] = create_tex(pe, TS, TS, 1);
        }

        /* Pipe 'P' */
        Uint32 *pp = (Uint32 *)calloc(TS * TS, 4);
        fill_rrect(pp, TS, TS, 0, 0, TS, TS, 3, rgb(5, 150, 105));
        fill_rrect(pp, TS, TS, 5, 0, 6, TS, 0, rgb(52, 211, 153));
        fill_rrect(pp, TS, TS, TS - 5, 0, 4, TS, 0, rgb(6, 95, 70));
        tex_pipe = create_tex(pp, TS, TS, 1);

        /* Berry Bush 'F' & Coin 'C' */
        Uint32 *pf = (Uint32 *)calloc(TS * TS, 4);
        fill_elli(pf, TS, TS, 12, 14, 9, 9, rgb(21, 128, 61));
        fill_elli(pf, TS, TS, 9, 11, 3.5f, 3.5f, rgb(244, 63, 94));
        fill_elli(pf, TS, TS, 16, 13, 3.5f, 3.5f, rgb(244, 63, 94));
        tex_berry = create_tex(pf, TS, TS, 1);

        Uint32 *pc = (Uint32 *)calloc(TS * TS, 4);
        fill_elli(pc, TS, TS, 12, 12, 8, 8, rgb(202, 138, 4));
        fill_elli(pc, TS, TS, 12, 12, 6, 6, rgb(253, 224, 71));
        tex_coin = create_tex(pc, TS, TS, 1);
    }

    /* 5. Rideable Dino Sprites (48x40 matching game.js renderDino!) */
    for (int d = 1; d <= 4; d++) {
        const DinoDef *dd = &DINO_DEFS[d];
        for (int f = 0; f < 2; f++) {
            for (int mf = 0; mf < 2; mf++) {
                int DW = 48, DH = 40;
                Uint32 *pd = (Uint32 *)calloc(DW * DH, 4);
                int leg = (f == 0) ? -2 : 2;
                int cx = 20, by = 37;
                /* Feathered wing for Blue Sky Rex */
                if (d == DINO_BLUE) {
                    fill_elli(pd, DW, DH, cx - 9, by - 19 + (f ? -2 : 1), 9, 5, rgb(248, 250, 252));
                }
                /* Tail */
                fill_elli(pd, DW, DH, cx - 10, by - 9, 8, 4, dd->body);
                /* Back leg */
                fill_rrect(pd, DW, DH, cx - 6 - leg, by - 7, 6, 7, 2, dd->dark);
                /* Plump Body & Belly */
                fill_elli(pd, DW, DH, cx, by - 10, 11, 8, dd->body);
                fill_elli(pd, DW, DH, cx + 3, by - 9, 7, 5, dd->belly);
                /* Saddle */
                fill_elli(pd, DW, DH, cx - 2, by - 16, 5, 4, dd->saddle);
                /* Front leg */
                fill_rrect(pd, DW, DH, cx + 2 + leg, by - 7, 6, 7, 2, dd->body);
                /* Dorsal Crest Spines */
                fill_elli(pd, DW, DH, cx + 2, by - 24, 3, 3, dd->crest);
                fill_elli(pd, DW, DH, cx - 1, by - 20, 2.5f, 2.5f, dd->crest);
                /* Head & Snout */
                fill_elli(pd, DW, DH, cx + 6, by - 21, 7.5f, 7.5f, dd->body);
                fill_elli(pd, DW, DH, cx + 13, by - 19, mf ? 9.0f : 7.0f, mf ? 7.5f : 5.5f, dd->body);
                if (mf) {
                    fill_elli(pd, DW, DH, cx + 13, by - 19, 5.5f, 5.5f, rgba(251, 191, 36, 160));
                }
                /* Expressive White Eye & Pupil */
                fill_elli(pd, DW, DH, cx + 7, by - 25, 3.2f, 4.2f, rgb(255, 255, 255));
                fill_elli(pd, DW, DH, cx + 8, by - 25, 1.8f, 2.0f, rgb(15, 23, 42));
                tex_dino[d][f][mf] = create_tex(pd, DW, DH, 1);
            }
        }
    }

    /* 6. Hero Leo Sprites with Flowing Red Scarf (36x36 matching game.js renderLeoAndDino!) */
    for (int p = 0; p < 3; p++) {
        Color tunic = (p == 2) ? rgb(248, 250, 252) : rgb(239, 68, 68);
        for (int f = 0; f < 2; f++) {
            int LW = 36, LH = 36;
            Uint32 *pl = (Uint32 *)calloc(LW * LH, 4);
            int cx = 20, by = 34;
            int step = (f == 0) ? -2 : 2;
            /* Flowing Hero Scarf */
            fill_rrect(pl, LW, LH, cx - 16, by - 17 + (f ? 1 : -1), 14, 4, 2, rgb(239, 68, 68));
            /* Blue Legs */
            fill_rrect(pl, LW, LH, cx - 5 - step, by - 8, 5, 8, 2, rgb(29, 78, 216));
            fill_rrect(pl, LW, LH, cx + 1 + step, by - 8, 5, 8, 2, rgb(29, 78, 216));
            /* Tunic */
            fill_rrect(pl, LW, LH, cx - 6, by - 18, 12, 11, 4, tunic);
            /* Peach Head */
            fill_elli(pl, LW, LH, cx, by - 23, 6.5f, 6.5f, rgb(254, 215, 170));
            /* Cap & Visor */
            fill_elli(pl, LW, LH, cx, by - 25, 7.0f, 4.0f, tunic);
            fill_rrect(pl, LW, LH, cx, by - 26, 10, 3, 1, tunic);
            /* Eye */
            fill_elli(pl, LW, LH, cx + 3, by - 22, 1.5f, 2.2f, rgb(15, 23, 42));
            tex_leo[p][f] = create_tex(pl, LW, LH, 1);
        }
    }

    /* 7. Enemies & Boss matching game.js renderEntities */
    {
        int ES = 24;
        Color ecols[4] = {rgb(168,85,247), rgb(34,197,94), rgb(16,185,129), rgb(236,72,153)};
        for (int k = 0; k < 4; k++) {
            Uint32 *pe = (Uint32 *)calloc(ES * ES, 4);
            if (k == 2) {
                /* Shell */
                fill_elli(pe, ES, ES, 12, 13, 10, 8, rgb(254, 240, 138));
                fill_elli(pe, ES, ES, 12, 13, 8, 6, ecols[k]);
            } else {
                fill_rrect(pe, ES, ES, 2, 2, ES - 4, ES - 4, 7, ecols[k]);
                if (k == 3) fill_elli(pe, ES, ES, 12, 4, 9, 4, rgb(248, 250, 252));
                fill_elli(pe, ES, ES, 8, 9, 2.8f, 2.8f, rgb(255, 255, 255));
                fill_elli(pe, ES, ES, 16, 9, 2.8f, 2.8f, rgb(255, 255, 255));
                fill_elli(pe, ES, ES, 8, 9, 1.4f, 1.4f, rgb(15, 23, 42));
                fill_elli(pe, ES, ES, 16, 9, 1.4f, 1.4f, rgb(15, 23, 42));
            }
            tex_enemy[k] = create_tex(pe, ES, ES, 1);
        }

        int BS = 48;
        Uint32 *pb = (Uint32 *)calloc(BS * BS, 4);
        fill_rrect(pb, BS, BS, 2, 10, BS - 4, BS - 12, 9, rgb(220, 38, 38));
        /* Golden Crown */
        fill_rrect(pb, BS, BS, 8, 2, BS - 16, 10, 2, rgb(251, 191, 36));
        fill_elli(pb, BS, BS, 16, 22, 4, 4, rgb(255, 255, 255));
        fill_elli(pb, BS, BS, 32, 22, 4, 4, rgb(255, 255, 255));
        fill_elli(pb, BS, BS, 16, 22, 2, 2, rgb(15, 23, 42));
        fill_elli(pb, BS, BS, 32, 22, 2, 2, rgb(15, 23, 42));
        tex_boss = create_tex(pb, BS, BS, 1);
    }
}

/* =========================================================================
 * Fast Batched Drawing Helpers (in 640x360 Render Space)
 * ========================================================================= */
static void fill_rect_ren(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

static void fill_rect_world(float wx, float wy, float ww, float wh, Color c) {
    fill_rect_ren((int)(wx * SCALE + 0.5f), (int)(wy * SCALE + 0.5f),
                  (int)(ww * SCALE + 0.5f), (int)(wh * SCALE + 0.5f), c);
}

static void draw_tex_world(SDL_Texture *t, float wx, float wy, float ww, float wh, int flip_h, Color tint) {
    if (!t) return;
    SDL_SetTextureColorMod(t, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(t, tint.a);
    SDL_Rect dst = {
        (int)(wx * SCALE + 0.5f),
        (int)(wy * SCALE + 0.5f),
        (int)(ww * SCALE + 0.5f),
        (int)(wh * SCALE + 0.5f)
    };
    SDL_RenderCopyEx(renderer, t, NULL, &dst, 0.0, NULL, flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

/* Fast Hardware Font Atlas Text (1 SDL_RenderCopy per character!) */
static void draw_text_ren(int x, int y, const char *s, Color col, int scale) {
    SDL_SetTextureColorMod(tex_font, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(tex_font, col.a);
    int cx = x;
    while (*s) {
        char c = *s++;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c > 32 && c < 128) {
            SDL_Rect src = {(c - 32) * 6, 0, 6, 8};
            SDL_Rect dst = {cx, y, 6 * scale, 8 * scale};
            SDL_RenderCopy(renderer, tex_font, &src, &dst);
        }
        cx += 6 * scale;
    }
}

static void draw_text_centered(int cx, int y, const char *s, Color col, int scale) {
    int w = (int)strlen(s) * 6 * scale;
    draw_text_ren(cx - w / 2, y, s, col, scale);
}

/* =========================================================================
 * Stage Builder & Physics (Exact 1:1 Match with game.js!)
 * ========================================================================= */
static void show_banner(const char *msg, int dur) {
    snprintf(banner_text, sizeof(banner_text), "%s", msg);
    banner_timer = dur;
}

static void spawn_particle(float x, float y, float vx, float vy, Color col, float size, int life, const char *txt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].active = 1;
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].size = size;
            particles[i].color = col;
            particles[i].life = life;
            particles[i].max_life = life;
            if (txt) snprintf(particles[i].text, sizeof(particles[i].text), "%s", txt);
            else particles[i].text[0] = '\0';
            return;
        }
    }
}

static void spawn_entity(int type, float x, float y, int dino_type) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) {
            memset(&entities[i], 0, sizeof(Entity));
            entities[i].active = 1;
            entities[i].type = type;
            entities[i].dino_type = dino_type;
            entities[i].x = x;
            entities[i].y = y;
            entities[i].start_x = x;
            entities[i].start_y = y;
            entities[i].w = 50;
            entities[i].h = 54;
            entities[i].dir = -1;
            entities[i].vx = -1.7f;
            if (type == ENT_PLATFORM) {
                entities[i].w = 160;
                entities[i].h = 26;
                entities[i].vx = 2.2f;
                entities[i].dir = 1;
            } else if (type == ENT_DINO_MOUNT) {
                entities[i].w = 62;
                entities[i].h = 64;
                entities[i].vx = 0.0f;
                entities[i].dir = 1;
            } else if (type == ENT_FLYER) {
                entities[i].w = 54;
                entities[i].h = 54;
                entities[i].vx = -1.4f;
            } else if (type == ENT_POKEY) {
                entities[i].w = 48;
                entities[i].h = 86;
                entities[i].vx = -1.0f;
            } else if (type == ENT_MUSHROOM || type == ENT_FLOWER) {
                entities[i].w = 48;
                entities[i].h = 48;
                entities[i].vx = 2.2f;
                entities[i].vy = -5.0f;
                entities[i].dir = 1;
            } else if (type == ENT_BOSS) {
                entities[i].w = 118;
                entities[i].h = 128;
                entities[i].hp = STAGES[current_stage].boss_hp;
                entities[i].max_hp = entities[i].hp;
                entities[i].vx = -2.2f;
            }
            return;
        }
    }
}

static int is_solid(char c) {
    return (c == '#' || c == 'B' || c == '?' || c == 'M' || c == 'E' || c == 'D' || c == 'U' || c == 'P' || c == 'I');
}

static char get_tile(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W) return '#';
    if (ty < 0 || ty >= MAP_H) return '.';
    return map[ty][tx];
}

static void set_tile(int tx, int ty, char c) {
    if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H) map[ty][tx] = c;
}

static int aabb(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

static void spawn_shockwave(float x, float y) {
    play_sfx(SFX_QUAKE);
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (!shockwaves[i].active) {
            shockwaves[i].active = 1;
            shockwaves[i].x = x;
            shockwaves[i].y = y;
            shockwaves[i].r = 16.0f;
            shockwaves[i].max_r = 290.0f;
            shockwaves[i].color = rgb(251, 191, 36);
            break;
        }
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        float d = hypotf((e->x + e->w * 0.5f) - x, (e->y + e->h * 0.5f) - y);
        if (d < 300.0f && (e->type == ENT_WALKER || e->type == ENT_KOOPA || e->type == ENT_SHELL || e->type == ENT_POKEY)) {
            if (e->type == ENT_WALKER || e->type == ENT_POKEY) {
                e->active = 0;
                score += 200;
                spawn_particle(e->x, e->y, 0, -3.0f, rgb(253, 224, 71), 10, 40, "+200");
            } else if (e->type == ENT_KOOPA) {
                e->type = ENT_SHELL;
                e->vx = 0;
            }
        }
    }
}

static void load_stage(int idx, int from_cp) {
    current_stage = ((idx % 8) + 8) % 8;
    const StageDef *st = &STAGES[current_stage];
    bake_stage_background(current_stage);

    memset(map, '.', sizeof(map));
    memset(bump_off, 0, sizeof(bump_off));
    memset(entities, 0, sizeof(entities));
    memset(fireballs, 0, sizeof(fireballs));
    memset(particles, 0, sizeof(particles));
    memset(shockwaves, 0, sizeof(shockwaves));

    char gnd = (st->theme == 5) ? 'I' : '#';
    for (int x = 0; x < MAP_W; x++) {
        int pit = ((x >= 36 && x <= 38) || (x >= 68 && x <= 71 && st->theme != 3) ||
                   (x >= 104 && x <= 107) || (x >= 138 && x <= 141 && !st->has_boss));
        if (!pit) {
            for (int y = 14; y < MAP_H; y++) set_tile(x, y, gnd);
        }
    }

    set_tile(7, 10, '?');
    set_tile(8, 10, 'E');
    set_tile(9, 10, 'B');
    set_tile(10, 10, '?');
    set_tile(13, 13, 'F');
    set_tile(14, 12, 'C');
    set_tile(15, 12, 'C');
    set_tile(16, 13, 'F');

    /* Spawn friendly waiting Dino at x=11 on Stage 1-1 just like the Web version! */
    if (current_stage == 0 && !from_cp && player.dino == DINO_NONE) {
        spawn_entity(ENT_DINO_MOUNT, 11 * TILE, 12 * TILE, st->default_dino);
    }

    for (int sec = 0; sec < 7; sec++) {
        int bx = 20 + sec * 20;
        if (sec % 2 == 0) {
            for (int dx = 0; dx < 5; dx++) {
                set_tile(bx + dx, 10, (dx == 2) ? (sec == 2 ? 'D' : 'M') : (dx % 2 == 0 ? 'B' : '?'));
                set_tile(bx + dx, 6, 'C');
            }
            for (int dx = 1; dx <= 3; dx++) {
                set_tile(bx + dx, 6, (dx == 2) ? 'E' : 'B');
            }
        } else {
            set_tile(bx, 13, '#');
            set_tile(bx + 1, 13, '#');
            set_tile(bx + 1, 12, '#');
            set_tile(bx + 2, 13, 'P');
            set_tile(bx + 2, 12, 'P');
            set_tile(bx + 5, 13, 'F');
            set_tile(bx + 8, 13, 'F');
            set_tile(bx + 6, 9, '?');
            set_tile(bx + 7, 9, 'D');
            set_tile(bx + 8, 9, '?');
        }
        if (bx == 60 || bx == 100 || bx == 120) set_tile(bx + 3, 13, 'J');
        if (sec == 1 || sec == 4) spawn_entity(ENT_PLATFORM, (bx + 15) * TILE, 10 * TILE, 0);
        spawn_entity((sec % 2 == 0) ? ENT_KOOPA : ENT_WALKER, (bx + 4) * TILE, 12 * TILE, 0);
        spawn_entity(ENT_FLYER, (bx + 11) * TILE, 8 * TILE, 0);
        if (st->theme == 2 || st->theme == 6) spawn_entity(ENT_POKEY, (bx + 9) * TILE, 11 * TILE, 0);
    }

    set_tile(88, 13, 'K');
    if (st->has_boss) {
        for (int x = 148; x < 170; x++) {
            set_tile(x, 14, '#');
            if (x % 5 == 0) set_tile(x, 9, 'E');
        }
        spawn_entity(ENT_BOSS, 162 * TILE, 10 * TILE, 0);
    }
    for (int y = 4; y <= 13; y++) set_tile(174, y, 'G');
    set_tile(174, 13, '#');

    player.x = (from_cp && checkpoint_x > 0) ? checkpoint_x : 3 * TILE;
    player.y = 11 * TILE;
    player.vx = 0;
    player.vy = 0;
    player.w = 46;
    player.h = player.dino ? 84 : 64;
    player.dir = 1;
    player.squash_x = 1.0f;
    player.squash_y = 1.0f;
    player.ground_pounding = 0;
    player.tongue_state = 0;
    player.tongue_len = 0;
    camera_x = player.x - W * 0.28f;
    if (camera_x < 0) camera_x = 0;

    char buf[80];
    snprintf(buf, sizeof(buf), "STAGE %s: %s", st->code, st->title);
    show_banner(buf, 140);
}

static void hit_block(int tx, int ty) {
    char c = get_tile(tx, ty);
    if (!is_solid(c)) return;
    bump_off[ty][tx] = 14.0f;
    float wx = tx * TILE + TILE * 0.5f;
    float wy = ty * TILE;
    const StageDef *st = &STAGES[current_stage];

    if (c == '?') {
        set_tile(tx, ty, 'U');
        coins++;
        score += 100;
        play_sfx(SFX_COIN);
        spawn_particle(wx, wy - 16, 0, -4.0f, rgb(253, 224, 71), 12, 35, "+100");
    } else if (c == 'M') {
        set_tile(tx, ty, 'U');
        play_sfx(SFX_POWERUP);
        spawn_entity(player.power >= 1 ? ENT_FLOWER : ENT_MUSHROOM, tx * TILE + 8, (ty - 1) * TILE, 0);
    } else if (c == 'E' || c == 'D') {
        set_tile(tx, ty, 'U');
        int dt = (c == 'E') ? st->default_dino : st->second_dino;
        play_sfx(SFX_HATCH);
        spawn_entity(ENT_DINO_MOUNT, tx * TILE, (ty - 1) * TILE - 6, dt);
        char msg[80];
        snprintf(msg, sizeof(msg), "%s HATCHED! %s", DINO_DEFS[dt].name, DINO_DEFS[dt].ability);
        show_banner(msg, 140);
    } else if (c == 'B') {
        if (player.power > 0 || player.dino || player.ground_pounding) {
            set_tile(tx, ty, '.');
            play_sfx(SFX_BRICK);
            score += 50;
            for (int i = 0; i < 6; i++) {
                spawn_particle(wx, wy + 24, (rand() % 11) - 5.0f, -(rand() % 8) - 2.0f, rgb(217, 119, 6), 12, 30, NULL);
            }
        } else {
            play_sfx(SFX_STOMP);
        }
    }
}

static void use_tongue_or_fire(void) {
    if (player.dino) {
        if (player.dino_mouth > 0) {
            play_sfx(SFX_SPIT);
            int is_fire = (player.dino == DINO_RED || player.dino_mouth == 2);
            float sx = (player.dir > 0) ? (player.x + player.w + 18) : (player.x - 32);
            float sy = player.y + 24;
            if (is_fire) {
                float vys[3] = {-3.2f, 0.0f, 3.2f};
                for (int k = 0; k < 3; k++) {
                    for (int i = 0; i < MAX_FIREBALLS; i++) {
                        if (!fireballs[i].active) {
                            fireballs[i].active = 1;
                            fireballs[i].x = sx;
                            fireballs[i].y = sy;
                            fireballs[i].vx = player.dir * 13.5f;
                            fireballs[i].vy = vys[k];
                            fireballs[i].r = 18.0f;
                            fireballs[i].from_dino = 1;
                            fireballs[i].life = 110;
                            break;
                        }
                    }
                }
            } else {
                for (int i = 0; i < MAX_ENTITIES; i++) {
                    if (!entities[i].active) {
                        memset(&entities[i], 0, sizeof(Entity));
                        entities[i].active = 1;
                        entities[i].type = ENT_SHELL;
                        entities[i].x = sx;
                        entities[i].y = player.y + 10;
                        entities[i].w = 50;
                        entities[i].h = 48;
                        entities[i].vx = player.dir * 14.0f;
                        entities[i].dir = player.dir;
                        break;
                    }
                }
            }
            player.dino_mouth = 0;
            return;
        }
        if (player.tongue_state == 0) {
            player.tongue_state = 1;
            player.tongue_len = 20.0f;
            player.tongue_reward = 0;
            play_sfx(SFX_TONGUE);
        }
    } else if (player.power == 2) {
        play_sfx(SFX_SPIT);
        for (int i = 0; i < MAX_FIREBALLS; i++) {
            if (!fireballs[i].active) {
                fireballs[i].active = 1;
                fireballs[i].x = (player.dir > 0) ? (player.x + player.w + 6) : (player.x - 18);
                fireballs[i].y = player.y + 22;
                fireballs[i].vx = player.dir * 12.0f;
                fireballs[i].vy = 3.5f;
                fireballs[i].r = 12.0f;
                fireballs[i].from_dino = 0;
                fireballs[i].life = 95;
                break;
            }
        }
    }
}

static void bail_out_dino(void) {
    if (!player.dino) return;
    int old = player.dino;
    player.dino = DINO_NONE;
    player.dino_mouth = 0;
    player.tongue_state = 0;
    player.vy = -17.5f;
    player.squash_x = 0.72f;
    player.squash_y = 1.35f;
    play_sfx(SFX_BAIL);
    spawn_entity(ENT_DINO_MOUNT, player.x - player.dir * 20, player.y + 24, old);
}

static void hurt_player(void) {
    if (player.invuln > 0 || player.star_timer > 0) return;
    if (player.dino) {
        bail_out_dino();
        player.vy = -12.0f;
        player.invuln = 90;
        show_banner("DINO PROTECTED YOU! HOP BACK ON!", 95);
        return;
    }
    if (player.power > 0) {
        player.power--;
        player.invuln = 90;
        play_sfx(SFX_HURT);
        return;
    }
    play_sfx(SFX_HURT);
    if (lives > 1) lives--;
    player.power = 1;
    load_stage(current_stage, 1);
}

static Uint8 keys_held[SDL_NUM_SCANCODES];
static Uint8 prev_held[SDL_NUM_SCANCODES];

static void update_game(void) {
    global_tick++;
    update_music();
    if (banner_timer > 0) banner_timer--;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (bump_off[y][x] > 0) {
                bump_off[y][x] -= 2.0f;
                if (bump_off[y][x] < 0) bump_off[y][x] = 0;
            }

    int k_left   = keys_held[SDL_SCANCODE_LEFT]  || keys_held[SDL_SCANCODE_A];
    int k_right  = keys_held[SDL_SCANCODE_RIGHT] || keys_held[SDL_SCANCODE_D];
    int k_down   = keys_held[SDL_SCANCODE_DOWN]  || keys_held[SDL_SCANCODE_S];
    int p_down   = prev_held[SDL_SCANCODE_DOWN]  || prev_held[SDL_SCANCODE_S];
    int k_jump   = keys_held[SDL_SCANCODE_SPACE] || keys_held[SDL_SCANCODE_UP] || keys_held[SDL_SCANCODE_W];
    int p_jump   = prev_held[SDL_SCANCODE_SPACE] || prev_held[SDL_SCANCODE_UP] || prev_held[SDL_SCANCODE_W];
    int k_tongue = keys_held[SDL_SCANCODE_Z] || keys_held[SDL_SCANCODE_LSHIFT] || keys_held[SDL_SCANCODE_RSHIFT] || keys_held[SDL_SCANCODE_J];
    int p_tongue = prev_held[SDL_SCANCODE_Z] || prev_held[SDL_SCANCODE_LSHIFT] || prev_held[SDL_SCANCODE_RSHIFT] || prev_held[SDL_SCANCODE_J];
    int k_bail   = keys_held[SDL_SCANCODE_X] || keys_held[SDL_SCANCODE_E] || keys_held[SDL_SCANCODE_K];
    int p_bail   = prev_held[SDL_SCANCODE_X] || prev_held[SDL_SCANCODE_E] || prev_held[SDL_SCANCODE_K];

    if (game_state == 0 || game_state == 3) {
        if (keys_held[SDL_SCANCODE_LEFT] && !prev_held[SDL_SCANCODE_LEFT]) {
            current_stage = (current_stage + 7) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if (keys_held[SDL_SCANCODE_RIGHT] && !prev_held[SDL_SCANCODE_RIGHT]) {
            current_stage = (current_stage + 1) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if ((keys_held[SDL_SCANCODE_RETURN] && !prev_held[SDL_SCANCODE_RETURN]) || (k_jump && !p_jump)) {
            if (game_state == 3) current_stage = 0;
            game_state = 1;
            load_stage(current_stage, 0);
        }
        player.anim_frame += 0.18f;
        player.scarf_wave += 0.22f;
        return;
    }

    if (game_state == 2) {
        state_timer--;
        if (state_timer <= 0) {
            checkpoint_x = 0;
            if (current_stage + 1 < 8) {
                game_state = 1;
                load_stage(current_stage + 1, 0);
            } else {
                game_state = 3;
            }
        }
        return;
    }

    if (player.invuln > 0) player.invuln--;
    if (player.star_timer > 0) player.star_timer--;
    if (player.flutter_cd > 0) player.flutter_cd--;
    player.h = player.dino ? 84.0f : (player.power > 0 ? 64.0f : 48.0f);

    if (k_tongue && !p_tongue) use_tongue_or_fire();
    if (k_bail && !p_bail && player.dino) bail_out_dino();

    if (player.tongue_state > 0 && player.dino) {
        if (player.tongue_state == 1) {
            player.tongue_len += 26.0f;
            if (player.tongue_len >= 245.0f) player.tongue_state = 2;
        } else {
            player.tongue_len -= 24.0f;
            if (player.tongue_len <= 0.0f) {
                player.tongue_len = 0.0f;
                player.tongue_state = 0;
                if (player.tongue_reward == 1) {
                    coins += 3;
                    score += 500;
                    berries_eaten++;
                    play_sfx(SFX_SWALLOW);
                    spawn_particle(player.x, player.y - 10, 0, -3.0f, rgb(251, 191, 36), 12, 40, "YUM! +500");
                    if (berries_eaten % 3 == 0) {
                        player.star_timer = 360;
                        play_sfx(SFX_POWERUP);
                        show_banner("3 BERRIES EATEN - STAR POWER!", 110);
                    }
                } else if (player.tongue_reward >= 2) {
                    play_sfx(SFX_SWALLOW);
                    player.dino_mouth = (player.tongue_reward == 3) ? 2 : 1;
                }
                player.tongue_reward = 0;
            }
        }
        float tip_x = (player.dir > 0) ? (player.x + player.w + player.tongue_len) : (player.x - player.tongue_len);
        float tip_y = player.y + 28.0f;
        int ttx = (int)(tip_x / TILE);
        int tty = (int)(tip_y / TILE);
        char tc = get_tile(ttx, tty);
        if ((tc == 'F' || tc == 'C') && player.tongue_reward == 0) {
            set_tile(ttx, tty, '.');
            player.tongue_reward = 1;
            player.tongue_state = 2;
        }
        if (player.tongue_reward == 0) {
            for (int i = 0; i < MAX_ENTITIES; i++) {
                Entity *e = &entities[i];
                if (!e->active || e->type == ENT_PLATFORM || e->type == ENT_DINO_MOUNT || e->type == ENT_BOSS) continue;
                if (aabb(tip_x - 24, tip_y - 24, 48, 48, e->x, e->y, e->w, e->h)) {
                    e->active = 0;
                    player.tongue_reward = (e->type == ENT_FLYER || e->type == ENT_POKEY || player.dino == DINO_RED) ? 3 : 2;
                    player.tongue_state = 2;
                    break;
                }
            }
        }
    }

    if (!player.on_ground && k_down && !p_down && !player.ground_pounding) {
        player.ground_pounding = 1;
        player.gp_timer = 10;
        player.vx = 0;
        player.vy = 0;
    }

    if (player.ground_pounding) {
        if (player.gp_timer > 0) {
            player.gp_timer--;
            player.vy = 0;
        } else {
            player.vy = 20.0f;
        }
    } else {
        int on_ice = (get_tile((int)((player.x + player.w * 0.5f) / TILE), (int)((player.y + player.h + 2) / TILE)) == 'I');
        float max_s = (k_tongue || player.dino) ? 8.2f : 6.4f;
        float acc = player.on_ground ? (on_ice ? 0.25f : 0.65f) : 0.42f;
        float fric = on_ice ? 0.96f : 0.82f;
        if (k_left) {
            player.vx -= acc;
            if (player.vx < -max_s) player.vx = -max_s;
            player.dir = -1;
        } else if (k_right) {
            player.vx += acc;
            if (player.vx > max_s) player.vx = max_s;
            player.dir = 1;
        } else {
            player.vx *= fric;
            if (fabsf(player.vx) < 0.08f) player.vx = 0;
        }
        player.vy += GRAVITY;
        if (player.vy > 16.0f) player.vy = 16.0f;
    }

    if (player.on_ground) {
        player.coyote = 7;
        player.wing_flaps = 4;
    } else if (player.coyote > 0) {
        player.coyote--;
    }

    if (k_jump && !p_jump) {
        if (player.coyote > 0) {
            player.vy = player.dino ? -15.2f : -14.4f;
            player.on_ground = 0;
            player.coyote = 0;
            player.squash_x = 0.78f;
            player.squash_y = 1.26f;
            play_sfx(SFX_JUMP);
        } else if (player.wall_slide != 0 && !player.dino) {
            player.vy = -14.0f;
            player.vx = -player.wall_slide * 8.5f;
            player.dir = -player.wall_slide;
            player.wall_slide = 0;
            play_sfx(SFX_JUMP);
        } else if (player.dino == DINO_BLUE && player.wing_flaps > 0) {
            player.wing_flaps--;
            player.vy = -13.2f;
            play_sfx(SFX_FLUTTER);
        }
    }

    if (!k_jump && player.vy < -5.0f && !player.ground_pounding) {
        player.vy *= 0.65f;
    }

    if (player.dino && k_jump && player.vy > 1.5f && player.flutter_cd == 0 && !player.ground_pounding) {
        player.vy = -9.6f;
        player.flutter_cd = 26;
        play_sfx(SFX_FLUTTER);
    }

    /* Horizontal Tile Collision */
    player.x += player.vx;
    if (player.x < 0) { player.x = 0; player.vx = 0; }
    player.wall_slide = 0;
    int tx1 = (int)(player.x / TILE);
    int tx2 = (int)((player.x + player.w - 0.01f) / TILE);
    int ty1 = (int)((player.y + 4) / TILE);
    int ty2 = (int)((player.y + player.h - 4) / TILE);
    for (int ty = ty1; ty <= ty2; ty++) {
        for (int tx = tx1; tx <= tx2; tx++) {
            if (is_solid(get_tile(tx, ty))) {
                if (player.vx > 0) {
                    player.x = tx * TILE - player.w;
                    player.vx = 0;
                    if (!player.on_ground && k_right) player.wall_slide = 1;
                } else if (player.vx < 0) {
                    player.x = (tx + 1) * TILE;
                    player.vx = 0;
                    if (!player.on_ground && k_left) player.wall_slide = -1;
                }
            }
        }
    }
    if (player.wall_slide != 0 && player.vy > 2.6f) player.vy = 2.6f;

    /* Vertical Tile Collision */
    float prev_bot = player.y + player.h;
    float orig_vy = player.vy;
    player.y += player.vy;
    player.on_ground = 0;
    int vtx1 = (int)((player.x + 4) / TILE);
    int vtx2 = (int)((player.x + player.w - 4) / TILE);
    int vty1 = (int)(player.y / TILE);
    int vty2 = (int)((player.y + player.h) / TILE);
    int res_y = 0;

    for (int ty = vty1; ty <= vty2; ty++) {
        for (int tx = vtx1; tx <= vtx2; tx++) {
            char c = get_tile(tx, ty);
            if (c == 'C' || c == 'F') {
                set_tile(tx, ty, '.');
                coins += (c == 'F') ? 2 : 1;
                score += (c == 'F') ? 300 : 100;
                play_sfx(SFX_COIN);
                continue;
            }
            if (c == 'K') {
                set_tile(tx, ty, '.');
                checkpoint_x = tx * TILE;
                play_sfx(SFX_CHECKPOINT);
                show_banner("CHECKPOINT REACHED!", 100);
                continue;
            }
            if (c == 'G') {
                play_sfx(SFX_CLEAR);
                score += 2000;
                game_state = 2;
                state_timer = 110;
                show_banner("STAGE CLEAR!", 110);
                return;
            }
            if (c == 'J' && orig_vy >= 0) {
                player.y = ty * TILE - player.h;
                player.vy = -19.5f;
                player.ground_pounding = 0;
                play_sfx(SFX_JUMP);
                res_y = 1;
                break;
            }
            if (!res_y && is_solid(c)) {
                if (orig_vy < 0) {
                    player.y = (ty + 1) * TILE;
                    player.vy = 1.0f;
                    hit_block(tx, ty);
                    res_y = 1;
                    break;
                } else if (orig_vy >= 0 && prev_bot <= ty * TILE + 24.0f) {
                    player.y = ty * TILE - player.h;
                    player.vy = 0;
                    player.on_ground = 1;
                    if (player.ground_pounding) {
                        player.ground_pounding = 0;
                        hit_block(tx, ty);
                        if (player.dino == DINO_YELLOW) spawn_shockwave(player.x + player.w * 0.5f, player.y + player.h);
                        else play_sfx(SFX_STOMP);
                    } else if (player.dino == DINO_YELLOW && orig_vy > 6.0f) {
                        spawn_shockwave(player.x + player.w * 0.5f, player.y + player.h);
                    }
                    res_y = 1;
                    break;
                }
            }
        }
        if (res_y) break;
    }

    if (player.y > H + 120.0f) {
        if (player.dino) {
            bail_out_dino();
            player.y = H - 140.0f;
        } else {
            hurt_player();
        }
        return;
    }

    player.squash_x += (1.0f - player.squash_x) * 0.22f;
    player.squash_y += (1.0f - player.squash_y) * 0.22f;
    player.anim_frame += fabsf(player.vx) * 0.06f + 0.04f;
    player.scarf_wave += 0.2f;

    /* Fireballs */
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        Fireball *f = &fireballs[i];
        if (!f->active) continue;
        f->x += f->vx;
        f->y += f->vy;
        if (!f->from_dino) {
            f->vy += 0.45f;
            if (is_solid(get_tile((int)(f->x / TILE), (int)((f->y + f->r) / TILE)))) f->vy = -6.5f;
        }
        if (--f->life <= 0) { f->active = 0; continue; }
        for (int j = 0; j < MAX_ENTITIES; j++) {
            Entity *e = &entities[j];
            if (!e->active || e->type == ENT_PLATFORM || e->type == ENT_DINO_MOUNT) continue;
            if (aabb(f->x - f->r, f->y - f->r, f->r * 2, f->r * 2, e->x, e->y, e->w, e->h)) {
                f->active = 0;
                if (e->type == ENT_BOSS) {
                    if (e->invuln <= 0) {
                        e->hp--;
                        e->invuln = 35;
                        play_sfx(SFX_STOMP);
                        if (e->hp <= 0) { e->active = 0; score += 5000; play_sfx(SFX_CLEAR); }
                    }
                } else {
                    e->active = 0;
                    score += 200;
                    play_sfx(SFX_STOMP);
                    spawn_particle(e->x, e->y, 0, -3.0f, rgb(251, 191, 36), 10, 35, "+200");
                }
                break;
            }
        }
    }

    /* Entities */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        e->timer++;

        if (e->type == ENT_PLATFORM) {
            e->x += e->vx;
            if (fabsf(e->x - e->start_x) > 190.0f) e->vx = -e->vx;
            if (player.vy >= 0 && player.x + player.w > e->x && player.x < e->x + e->w &&
                player.y + player.h >= e->y - 10 && player.y + player.h <= e->y + 22) {
                player.y = e->y - player.h;
                player.vy = 0;
                player.on_ground = 1;
                player.x += e->vx;
            }
            continue;
        }

        if (e->type == ENT_DINO_MOUNT) {
            e->vy += GRAVITY * 0.8f;
            if (e->vy > 12.0f) e->vy = 12.0f;
            e->x += e->vx;
            e->y += e->vy;
            e->vx *= 0.92f;
            int dty = (int)((e->y + e->h) / TILE);
            int dtx = (int)((e->x + e->w * 0.5f) / TILE);
            if (is_solid(get_tile(dtx, dty))) {
                e->y = dty * TILE - e->h;
                e->vy = 0;
            }
            if (e->timer > 18 && aabb(player.x, player.y, player.w, player.h, e->x, e->y, e->w, e->h)) {
                if (!player.dino) {
                    player.dino = e->dino_type;
                    e->active = 0;
                    play_sfx(SFX_HATCH);
                    char msg[80];
                    snprintf(msg, sizeof(msg), "RIDING %s - %s", DINO_DEFS[player.dino].name, DINO_DEFS[player.dino].ability);
                    show_banner(msg, 130);
                } else {
                    e->active = 0;
                    score += 1000;
                    play_sfx(SFX_POWERUP);
                    spawn_particle(e->x, e->y, 0, -3.0f, rgb(253, 224, 71), 12, 40, "+1000");
                }
            }
            continue;
        }

        if (e->type == ENT_FLYER) {
            e->x += e->vx;
            e->y = e->start_y + sinf(e->timer * 0.06f) * 58.0f;
            if (e->timer % 180 == 0) e->vx = -e->vx;
        } else if (e->type == ENT_BOSS) {
            if (e->invuln > 0) e->invuln--;
            e->vy += GRAVITY;
            e->x += e->vx;
            e->y += e->vy;
            int bty = (int)((e->y + e->h) / TILE);
            int btx = (int)((e->x + e->w * 0.5f) / TILE);
            if (is_solid(get_tile(btx, bty))) {
                e->y = bty * TILE - e->h;
                e->vy = 0;
                if (e->timer % 95 == 0) e->vy = -14.0f;
            }
            if (e->x < 149 * TILE) e->vx = fabsf(e->vx);
            if (e->x > 168 * TILE) e->vx = -fabsf(e->vx);
            if (e->timer % 110 == 0) {
                spawn_entity(ENT_SHELL, e->x - 36, e->y + 50, 0);
            }
        } else {
            e->vy += GRAVITY;
            if (e->vy > 14.0f) e->vy = 14.0f;
            e->x += e->vx;
            int etx = (int)((e->vx > 0 ? e->x + e->w : e->x) / TILE);
            int ety = (int)((e->y + e->h * 0.5f) / TILE);
            if (is_solid(get_tile(etx, ety))) { e->vx = -e->vx; e->dir = -e->dir; }
            e->y += e->vy;
            int gty = (int)((e->y + e->h) / TILE);
            int gtx = (int)((e->x + e->w * 0.5f) / TILE);
            if (is_solid(get_tile(gtx, gty))) { e->y = gty * TILE - e->h; e->vy = 0; }
        }

        /* Sliding shell bowling */
        if (e->type == ENT_SHELL && fabsf(e->vx) > 4.0f) {
            for (int j = 0; j < MAX_ENTITIES; j++) {
                Entity *o = &entities[j];
                if (!o->active || o == e || o->type == ENT_PLATFORM || o->type == ENT_DINO_MOUNT) continue;
                if (aabb(e->x, e->y, e->w, e->h, o->x, o->y, o->w, o->h)) {
                    if (o->type == ENT_BOSS) {
                        if (o->invuln <= 0) {
                            o->hp -= 2;
                            o->invuln = 40;
                            e->active = 0;
                            play_sfx(SFX_QUAKE);
                            if (o->hp <= 0) { o->active = 0; score += 5000; play_sfx(SFX_CLEAR); }
                        }
                    } else {
                        o->active = 0;
                        score += 300;
                        play_sfx(SFX_STOMP);
                        spawn_particle(o->x, o->y, 0, -3.0f, rgb(253, 224, 71), 11, 35, "+300");
                    }
                }
            }
        }

        if (aabb(player.x, player.y, player.w, player.h, e->x, e->y, e->w, e->h)) {
            if (e->type == ENT_MUSHROOM || e->type == ENT_FLOWER) {
                e->active = 0;
                player.power = (e->type == ENT_FLOWER) ? 2 : (player.power > 1 ? player.power : 1);
                score += 1000;
                play_sfx(SFX_POWERUP);
                continue;
            }
            if (player.star_timer > 0) {
                if (e->type != ENT_BOSS) { e->active = 0; score += 200; play_sfx(SFX_STOMP); }
                continue;
            }
            if (player.vy > 0 && player.y + player.h - player.vy <= e->y + 32.0f) {
                player.vy = -12.5f;
                play_sfx(SFX_STOMP);
                if (player.dino == DINO_YELLOW) spawn_shockwave(player.x + player.w * 0.5f, e->y + e->h);
                if (e->type == ENT_FLYER) { e->type = ENT_KOOPA; e->vx = -2.0f; }
                else if (e->type == ENT_KOOPA) { e->type = ENT_SHELL; e->vx = 0; e->h = 46; }
                else if (e->type == ENT_SHELL) e->vx = (fabsf(e->vx) > 1.0f) ? 0 : player.dir * 12.0f;
                else if (e->type == ENT_BOSS) {
                    if (e->invuln <= 0) {
                        e->hp--;
                        e->invuln = 40;
                        if (e->hp <= 0) { e->active = 0; score += 5000; play_sfx(SFX_CLEAR); }
                    }
                } else {
                    e->active = 0;
                    score += 200;
                    spawn_particle(e->x, e->y, 0, -3.0f, rgb(253, 224, 71), 10, 35, "+200");
                }
            } else if (e->type == ENT_SHELL && fabsf(e->vx) < 1.0f) {
                e->vx = player.dir * 12.0f;
                e->x += player.dir * 18.0f;
                play_sfx(SFX_STOMP);
            } else {
                hurt_player();
            }
        }
    }

    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            shockwaves[i].r += 14.0f;
            if (shockwaves[i].r >= shockwaves[i].max_r) shockwaves[i].active = 0;
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            if (!particles[i].text[0]) particles[i].vy += 0.35f;
            if (--particles[i].life <= 0) particles[i].active = 0;
        }
    }

    /* Smooth Deadzone Camera */
    float screen_px = player.x - camera_x;
    float left_bound = W * 0.33f;
    float right_bound = W * 0.47f;
    float target_cam = camera_x;
    if (screen_px < left_bound) target_cam = player.x - left_bound;
    else if (screen_px > right_bound) target_cam = player.x - right_bound;
    if (target_cam < 0) target_cam = 0;
    if (target_cam > MAP_W * TILE - W) target_cam = MAP_W * TILE - W;
    camera_x += (target_cam - camera_x) * 0.22f;
}

/* =========================================================================
 * Fast Locked-60FPS Renderer (Matches Web Version Visuals!)
 * ========================================================================= */
static void render_dino_world(float cx, float by, int dir, int dino_id, float anim, int mouth_full, float tongue_len) {
    int s = (dir >= 0) ? 1 : -1;
    int frame = ((int)(anim * 1.4f)) & 1;
    if (dino_id < 1 || dino_id > 4) dino_id = 1;

    if (tongue_len > 0.0f) {
        float tx = (s > 0) ? (cx + 22.0f) : (cx - 22.0f - tongue_len);
        fill_rect_world(tx, by - 36.0f, tongue_len, 10.0f, rgb(251, 113, 133));
        draw_tex_world(tex_circle, cx + s * (28.0f + tongue_len) - 9.0f, by - 41.0f, 18.0f, 18.0f, 0, rgb(244, 63, 94));
    }
    draw_tex_world(tex_dino[dino_id][frame][mouth_full ? 1 : 0],
                   cx - 56.0f, by - 88.0f, 112.0f, 92.0f, s < 0, rgb(255, 255, 255));
}

static void render_frame(void) {
    const StageDef *st = &STAGES[current_stage];
    float cam_x = floorf(camera_x + 0.5f);

    /* 1. Pre-Baked Sky + Sun + Mountain Panorama (2 fast opaque blits!) */
    int bg_scroll = ((int)(cam_x * 0.08f)) % REN_W;
    if (bg_scroll < 0) bg_scroll += REN_W;
    SDL_Rect d1 = {-bg_scroll, 0, REN_W, REN_H};
    SDL_Rect d2 = {REN_W - bg_scroll, 0, REN_W, REN_H};
    SDL_RenderCopy(renderer, tex_stage_bg, NULL, &d1);
    SDL_RenderCopy(renderer, tex_stage_bg, NULL, &d2);

    /* Fluffy Clouds */
    if (st->theme != 1) {
        for (int i = 0; i < 6; i++) {
            float cx = fmodf(i * 360.0f - cam_x * 0.22f - global_tick * 0.3f, W + 300.0f);
            if (cx < 0) cx += W + 300.0f;
            cx -= 150.0f;
            float cy = 95.0f + (i % 3) * 80.0f;
            draw_tex_world(tex_cloud, cx, cy, 160.0f, 80.0f, 0, rgb(255, 255, 255));
        }
    }

    /* 2. World Tiles */
    int c1 = (int)(cam_x / TILE) - 1;
    int c2 = (int)((cam_x + W) / TILE) + 1;
    if (c1 < 0) c1 = 0;
    if (c2 >= MAP_W) c2 = MAP_W - 1;

    Color g_top = rgb(st->gnd_top[0], st->gnd_top[1], st->gnd_top[2]);
    Color g_bod = rgb(st->gnd_body[0], st->gnd_body[1], st->gnd_body[2]);

    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = c1; tx <= c2; tx++) {
            char c = map[ty][tx];
            if (c == '.') continue;
            float sx = tx * TILE - cam_x;
            float sy = ty * TILE - bump_off[ty][tx];

            if (c == '#' || c == 'I') {
                fill_rect_world(sx, sy, TILE + 1.0f, TILE + 1.0f, (c == 'I') ? rgb(2, 132, 199) : g_bod);
                if (ty == 0 || !is_solid(get_tile(tx, ty - 1))) {
                    fill_rect_world(sx, sy, TILE + 1.0f, 18.0f, (c == 'I') ? rgb(224, 242, 254) : g_top);
                }
            } else if (c == 'B') {
                draw_tex_world(tex_brick, sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
            } else if (c == '?' || c == 'M') {
                draw_tex_world(tex_qblock, sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
            } else if (c == 'E' || c == 'D') {
                int dt = (c == 'E') ? st->default_dino : st->second_dino;
                draw_tex_world(tex_egg[dt], sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
            } else if (c == 'U') {
                draw_tex_world(tex_used, sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
            } else if (c == 'P') {
                draw_tex_world(tex_pipe, sx - 2.0f, sy, TILE + 4.0f, TILE + 1.0f, 0, rgb(255, 255, 255));
            } else if (c == 'F') {
                draw_tex_world(tex_berry, sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
            } else if (c == 'C') {
                float spin = 0.35f + 0.65f * fabsf(cosf(global_tick * 0.08f));
                float cw = 44.0f * spin;
                draw_tex_world(tex_coin, sx + (TILE - cw) * 0.5f, sy + 10.0f, cw, 44.0f, 0, rgb(255, 255, 255));
            } else if (c == 'J') {
                fill_rect_world(sx + 6, sy + 22, TILE - 12, 16, rgb(239, 68, 68));
                fill_rect_world(sx + 16, sy + 38, TILE - 32, 24, rgb(253, 224, 71));
            } else if (c == 'K') {
                fill_rect_world(sx + 28, sy - 32, 6, 96, rgb(226, 232, 240));
                fill_rect_world(sx + 34, sy - 32, 42, 28, (checkpoint_x > 0) ? rgb(34, 197, 94) : rgb(59, 130, 246));
            } else if (c == 'G') {
                fill_rect_world(sx + 28, sy, 8, TILE + 2.0f, rgb(248, 250, 252));
                if (ty == 4) {
                    draw_tex_world(tex_circle, sx + 16, sy - 16, 32, 32, 0, rgb(251, 191, 36));
                    fill_rect_world(sx + 36, sy + 8, 68, 42, rgb(16, 185, 129));
                }
            }
        }
    }

    /* 3. Shockwaves & Fireballs */
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            float r = shockwaves[i].r;
            draw_tex_world(tex_ring, shockwaves[i].x - cam_x - r, shockwaves[i].y - r, r * 2.0f, r * 2.0f, 0, shockwaves[i].color);
        }
    }
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        if (fireballs[i].active) {
            float fx = fireballs[i].x - cam_x - fireballs[i].r;
            float fy = fireballs[i].y - fireballs[i].r;
            float d = fireballs[i].r * 2.0f;
            draw_tex_world(tex_circle, fx, fy, d, d, 0, rgb(239, 68, 68));
            draw_tex_world(tex_circle, fx + d * 0.22f, fy + d * 0.22f, d * 0.55f, d * 0.55f, 0, rgb(253, 224, 71));
        }
    }

    /* 4. Entities */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        float sx = e->x - cam_x;
        float sy = e->y;
        if (sx + e->w < -120 || sx > W + 120) continue;

        if (e->type == ENT_PLATFORM) {
            fill_rect_world(sx, sy, e->w, e->h, rgb(251, 191, 36));
        } else if (e->type == ENT_DINO_MOUNT) {
            render_dino_world(sx + e->w * 0.5f, sy + e->h, e->dir, e->dino_type, global_tick * 0.08f, 0, 0);
            int bob = (int)(sinf(global_tick * 0.12f) * 2.0f);
            draw_text_centered((int)((sx + e->w * 0.5f) * SCALE), (int)((sy - 26.0f) * SCALE) + bob, "HOP ON!", rgb(253, 224, 71), 1);
        } else if (e->type == ENT_MUSHROOM || e->type == ENT_FLOWER) {
            draw_tex_world(tex_circle, sx + 4, sy + 4, 40, 40, 0, (e->type == ENT_FLOWER) ? rgb(251, 146, 60) : rgb(239, 68, 68));
            draw_tex_world(tex_circle, sx + 14, sy + 12, 12, 12, 0, rgb(254, 240, 138));
        } else if (e->type == ENT_BOSS) {
            Color tint = (e->invuln > 0 && (e->invuln % 4 < 2)) ? rgb(255, 180, 180) : rgb(255, 255, 255);
            draw_tex_world(tex_boss, sx, sy - 16, e->w, e->h + 16, e->vx > 0, tint);
            fill_rect_world(sx, sy - 46, e->w, 12, rgb(0, 0, 0));
            fill_rect_world(sx + 2, sy - 44, (e->w - 4) * ((float)e->hp / (float)e->max_hp), 8, rgb(34, 197, 94));
        } else if (e->type == ENT_POKEY) {
            for (int seg = 0; seg < 3; seg++) {
                draw_tex_world(tex_circle, sx + 6, sy + e->h - 32 - seg * 26, 34, 30, 0, rgb(234, 179, 8));
            }
        } else {
            int k = (e->type == ENT_KOOPA) ? 1 : (e->type == ENT_SHELL ? 2 : (e->type == ENT_FLYER ? 3 : 0));
            draw_tex_world(tex_enemy[k], sx, sy, e->w, e->h, e->vx > 0, rgb(255, 255, 255));
        }
    }

    /* 5. Leo & Rideable Dino (matching game.js renderLeoAndDino!) */
    if (!(player.invuln > 0 && ((player.invuln / 4) % 2 == 1))) {
        float cx = player.x + player.w * 0.5f - cam_x;
        float by = player.y + player.h;
        float leo_y = player.dino ? (by - 30.0f) : by;
        int p_idx = (player.power >= 2) ? 2 : (player.power == 1 ? 1 : 0);
        int l_frame = ((int)(player.anim_frame * 1.6f)) & 1;
        Color l_tint = (player.star_timer > 0 && (global_tick % 4 < 2)) ? rgb(253, 224, 71) : rgb(255, 255, 255);
        float lw = 78.0f * player.squash_x;
        float lh = 78.0f * player.squash_y;
        float lx = cx + (player.dino ? -4.0f * player.dir : 0.0f) - lw * 0.5f;
        draw_tex_world(tex_leo[p_idx][l_frame], lx, leo_y - lh + 4.0f, lw, lh, player.dir < 0, l_tint);

        if (player.dino) {
            render_dino_world(cx, by, player.dir, player.dino, player.anim_frame, player.dino_mouth > 0, player.tongue_len);
        }
    }

    /* 6. Particles & Score Popups */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &particles[i];
        if (!p->active) continue;
        if (p->text[0]) {
            draw_text_centered((int)((p->x - cam_x) * SCALE), (int)(p->y * SCALE), p->text, p->color, 1);
        } else {
            draw_tex_world(tex_circle, p->x - cam_x - p->size, p->y - p->size, p->size * 2.0f, p->size * 2.0f, 0, p->color);
        }
    }

    /* 7. HUD Bar (Matches game.js renderHUD!) */
    fill_rect_ren(8, 6, REN_W - 16, 22, rgba(8, 12, 20, 200));
    char hud_left[96];
    snprintf(hud_left, sizeof(hud_left), "LEO X%d  COINS:%02d  SCORE:%06d  %s:%s",
             lives, coins, score, st->code, st->title);
    draw_text_ren(16, 13, hud_left, rgb(248, 250, 252), 1);

    if (player.dino) {
        char d_msg[48];
        snprintf(d_msg, sizeof(d_msg), "DINO:%s%s", DINO_DEFS[player.dino].name, player.dino_mouth ? " [SPIT:Z]" : "");
        draw_text_ren(REN_W - (int)strlen(d_msg) * 6 - 14, 13, d_msg, DINO_DEFS[player.dino].body, 1);
    } else {
        draw_text_ren(REN_W - 190, 13, "HIT EGG BLOCK FOR DINO!", rgb(253, 224, 71), 1);
    }

    if (banner_timer > 0 && banner_text[0]) {
        fill_rect_ren(REN_W / 2 - 160, 34, 320, 18, rgba(8, 12, 20, 220));
        draw_text_centered(REN_W / 2, 39, banner_text, rgb(253, 224, 71), 1);
    }

    if (game_state == 0 || game_state == 3) {
        fill_rect_ren(REN_W / 2 - 190, REN_H / 2 - 80, 380, 160, rgba(8, 12, 20, 215));
        fill_rect_ren(REN_W / 2 - 190, REN_H / 2 - 80, 380, 2, rgb(251, 191, 36));
        fill_rect_ren(REN_W / 2 - 190, REN_H / 2 + 78, 380, 2, rgb(251, 191, 36));
        draw_text_centered(REN_W / 2, REN_H / 2 - 62,
                           game_state == 3 ? "DINO ISLAND SAVED!" : "SUPER LEO WORLD HD",
                           rgb(253, 224, 71), 2);
        draw_text_centered(REN_W / 2, REN_H / 2 - 34, "60FPS WIDESCREEN - 4 RIDEABLE DINOS - 8 WORLDS", rgb(52, 211, 153), 1);
        draw_text_centered(REN_W / 2, REN_H / 2 - 10, "EMERALD REX (TONGUE/FLUTTER) - BLAZE REX (3X FIRE)", rgb(226, 232, 240), 1);
        draw_text_centered(REN_W / 2, REN_H / 2 + 6,  "SKY REX (WINGED FLIGHT) - QUAKE REX (SHOCKWAVE)", rgb(226, 232, 240), 1);
        char st_sel[64];
        snprintf(st_sel, sizeof(st_sel), "< STAGE %s: %s >", st->code, st->title);
        draw_text_centered(REN_W / 2, REN_H / 2 + 28, st_sel, rgb(56, 189, 248), 1);
        draw_text_centered(REN_W / 2, REN_H / 2 + 52, "PRESS ENTER OR SPACE TO PLAY!", rgb(251, 191, 36), 1);
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) return 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_ShowCursor(SDL_DISABLE);
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    window = SDL_CreateWindow("Super Leo World HD: Dino Island",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    if (!window) window = SDL_CreateWindow("Super Leo World HD", 0, 0, 1280, 720, SDL_WINDOW_FULLSCREEN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, REN_W, REN_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    bake_all_textures();

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;
    if (SDL_OpenAudio(&want, NULL) >= 0) SDL_PauseAudio(0);

    /* Start on foot with friendly Emerald Rex waiting at x=11 just like the Web version! */
    player.power = 1;
    player.dino = DINO_NONE;
    load_stage(0, 0);

    int running = 1;
    Uint32 last = SDL_GetTicks();
    while (running) {
        memcpy(prev_held, keys_held, sizeof(keys_held));
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            else if (ev.type == SDL_KEYDOWN) {
                SDL_Scancode sc = ev.key.keysym.scancode;
                if (sc == SDL_SCANCODE_ESCAPE) {
                    if (game_state == 0) running = 0;
                    else { game_state = 0; load_stage(current_stage, 0); }
                }
                if (sc < SDL_NUM_SCANCODES) keys_held[sc] = 1;
            } else if (ev.type == SDL_KEYUP) {
                SDL_Scancode sc = ev.key.keysym.scancode;
                if (sc < SDL_NUM_SCANCODES) keys_held[sc] = 0;
            } else if (ev.type == SDL_JOYBUTTONDOWN || ev.type == SDL_JOYBUTTONUP) {
                int down = (ev.type == SDL_JOYBUTTONDOWN) ? 1 : 0;
                if (ev.jbutton.button == 0 || ev.jbutton.button == 1) keys_held[SDL_SCANCODE_SPACE] = down;
                else if (ev.jbutton.button == 2 || ev.jbutton.button == 3) keys_held[SDL_SCANCODE_Z] = down;
                else if (ev.jbutton.button == 4 || ev.jbutton.button == 5) keys_held[SDL_SCANCODE_X] = down;
                else if (ev.jbutton.button == 7 || ev.jbutton.button == 9) keys_held[SDL_SCANCODE_RETURN] = down;
            } else if (ev.type == SDL_JOYHATMOTION) {
                keys_held[SDL_SCANCODE_LEFT]  = (ev.jhat.value & SDL_HAT_LEFT)  ? 1 : 0;
                keys_held[SDL_SCANCODE_RIGHT] = (ev.jhat.value & SDL_HAT_RIGHT) ? 1 : 0;
                keys_held[SDL_SCANCODE_UP]    = (ev.jhat.value & SDL_HAT_UP)    ? 1 : 0;
                keys_held[SDL_SCANCODE_DOWN]  = (ev.jhat.value & SDL_HAT_DOWN)  ? 1 : 0;
            }
        }
        update_game();
        render_frame();
        Uint32 now = SDL_GetTicks();
        int elapsed = (int)(now - last);
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        last = SDL_GetTicks();
    }

    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}
