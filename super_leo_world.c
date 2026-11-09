/*
 * Super Leo World HD: Dino Island (Native RetroPie / SDL2 Hardware-Accelerated 720p HD Edition)
 * True Fullscreen 1280x720 HD (60 FPS OpenGL ES 2.0 / VideoCore IV GPU on Raspberry Pi)
 * Features:
 *  - High-Definition 1280x720 Hardware-Accelerated Rendering with Anti-Aliased Shaded Textures
 *  - 4 Rideable Dino Companions hatched from Spotted Egg Blocks:
 *      1. Emerald Rex (Green): Chameleon Tongue-Grab, Swallow & Spit Shells, Flutter-Jump Hover
 *      2. Blaze Rex (Red): Spits any swallowed enemy/shell as a 3-Way Spread of Giant Fireballs
 *      3. Sky Rex (Blue): Feathered Wings — up to 4 mid-air Wing Flaps
 *      4. Quake Rex (Yellow): Expanding Golden Seismic Shockwave on landings & Ground-Pounds
 *  - Modern Mario Moveset: Wall-Slide & Wall-Jump, Ground-Pound, Dino Bail-Out Save Jump
 *  - 8 Stages across 4 Worlds (1-1 through 4-2) with Tricera-King & King Tyrannor Bosses
 *  - Built-in SDL2 Chiptune Sound Effects over HDMI + Keyboard & Gamepad Support
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Logical World Units (16:9) */
#define W 480
#define H 270
#define TILE 16
#define MAP_W 185
#define MAP_H 17
#define GRAVITY 0.17f

/* Native HD Output Resolution (720p 16:9) */
#define HD_W 1280
#define HD_H 720
#define HD_SCALE ((float)HD_W / (float)W)

#define MAX_ENTITIES 96
#define MAX_FIREBALLS 16
#define MAX_PARTICLES 96
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
    {"1-1", "EMERALD DINO LAGOON",   0, {14,165,233}, {186,230,253}, {13,148,136}, {16,185,129}, {34,197,94},  {133,77,14},  DINO_GREEN,  DINO_RED,    0, "", 0},
    {"1-2", "AMBER CRYSTAL CAVERN",  1, {15,23,42},   {30,27,75},    {49,46,129},  {67,56,202},  {245,158,11}, {51,65,85},   DINO_YELLOW, DINO_GREEN,  0, "", 0},
    {"2-1", "SUNBURST OASIS DUNES",  2, {249,115,22}, {254,240,138}, {217,119,6},  {245,158,11}, {253,224,71}, {154,52,18},  DINO_RED,    DINO_YELLOW, 0, "", 0},
    {"2-2", "TRICERA MAGMA KEEP",    3, {24,24,27},   {69,10,10},    {127,29,29},  {153,27,27},  {148,163,184},{63,63,70},   DINO_RED,    DINO_BLUE,   1, "TRICERA-KING", 6},
    {"3-1", "SKYWARD CLOUD ISLES",   4, {37,99,235},  {224,242,254}, {96,165,250}, {147,197,253},{244,63,94},  {248,250,252},DINO_BLUE,   DINO_GREEN,  0, "", 0},
    {"3-2", "AURORA GLACIER PEAKS",  5, {15,23,42},   {6,182,212},   {2,132,199},  {56,189,248}, {207,250,254},{30,58,138},  DINO_RED,    DINO_BLUE,   0, "", 0},
    {"4-1", "STARLIGHT NEON CANYON", 6, {9,13,22},    {59,7,100},    {88,28,135},  {126,34,206}, {192,132,252},{30,27,75},   DINO_YELLOW, DINO_BLUE,   0, "", 0},
    {"4-2", "KING TYRANNOR CITADEL", 3, {9,9,11},     {127,29,29},   {69,10,10},   {153,27,27},  {248,113,113},{39,39,42},   DINO_RED,    DINO_YELLOW, 1, "KING TYRANNOR", 10}
};

typedef struct {
    Uint8 r, g, b, a;
} Color;

static Color rgba(int r, int g, int b, int a) {
    Color c = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    return c;
}

static Color rgb(int r, int g, int b) {
    return rgba(r, g, b, 255);
}

typedef struct {
    int active;
    int type;
    int dino_type;
    float x, y, vx, vy;
    float start_x, start_y;
    int w, h;
    int dir;
    int timer;
    int hp, max_hp;
    int invuln;
} Entity;

typedef struct {
    int active;
    float x, y, vx, vy;
    int r;
    int from_dino;
    int life;
} Fireball;

typedef struct {
    int active;
    float x, y, vx, vy;
    Color color;
    int life, max_life;
} Particle;

typedef struct {
    int active;
    float x, y, r, max_r;
    Color color;
} Shockwave;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* Pre-baked HD Procedural Textures */
static SDL_Texture *tex_circle = NULL;
static SDL_Texture *tex_glow = NULL;
static SDL_Texture *tex_ring = NULL;
static SDL_Texture *tex_brick = NULL;
static SDL_Texture *tex_qblock = NULL;
static SDL_Texture *tex_used = NULL;
static SDL_Texture *tex_egg[5] = {NULL};
static SDL_Texture *tex_pipe = NULL;
static SDL_Texture *tex_coin = NULL;
static SDL_Texture *tex_berry = NULL;
static SDL_Texture *tex_dino[5][2] = {{NULL}};
static SDL_Texture *tex_leo[3][2] = {{NULL}};
static SDL_Texture *tex_walker = NULL;
static SDL_Texture *tex_koopa = NULL;
static SDL_Texture *tex_shell = NULL;
static SDL_Texture *tex_flyer = NULL;
static SDL_Texture *tex_boss = NULL;

static char map[MAP_H][MAP_W];
static int bump_off[MAP_H][MAP_W];

static int game_state = 0; /* 0=title, 1=playing, 2=stageclear, 3=victory */
static int current_stage = 0;
static int global_tick = 0;
static float camera_x = 0.0f;
static int score = 0;
static int coins = 0;
static int lives = 5;
static int berries_eaten = 0;
static float checkpoint_x = 0.0f;
static char banner_text[64] = "";
static int banner_timer = 0;
static int state_timer = 0;

static struct {
    float x, y, vx, vy;
    int w, h;
    int dir;
    int power; /* 0=small, 1=super, 2=fire */
    int invuln;
    int star_timer;
    int on_ground;
    int coyote;
    int wall_slide;
    int ground_pounding;
    int gp_timer;
    float anim_frame;
    int dino; /* DINO_NONE..DINO_YELLOW */
    int dino_mouth; /* 0=empty, 1=shell, 2=fire */
    int tongue_state; /* 0=idle, 1=ext, 2=ret */
    float tongue_len;
    int tongue_reward; /* 0=none, 1=berry, 2=shell, 3=fire_shell */
    int flutter_cd;
    int wing_flaps;
} player;

static Entity entities[MAX_ENTITIES];
static Fireball fireballs[MAX_FIREBALLS];
static Particle particles[MAX_PARTICLES];
static Shockwave shockwaves[MAX_SHOCKWAVES];

/* Audio Synth */
static volatile int cur_sfx = SFX_NONE;
static volatile int sfx_pos = 0;
static volatile int sfx_len = 0;

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

static void audio_cb(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Sint16 *buf = (Sint16 *)stream;
    int samples = len / 2;
    for (int i = 0; i < samples; i++) {
        Sint16 out = 0;
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
                out = (Sint16)(((rand() % 6400) - 3200) * (1.0f - prog));
            } else {
                int period = (int)(22050.0f / (freq > 40.0f ? freq : 40.0f));
                if (period < 2) period = 2;
                int sq = ((sfx_pos % period) < (period / 2)) ? 1 : -1;
                out = (Sint16)(sq * 2800 * (1.0f - 0.3f * prog));
            }
            sfx_pos++;
            if (sfx_pos >= sfx_len) cur_sfx = SFX_NONE;
        }
        buf[i] = out;
    }
}

static void get_dino_rgb(int d, int *r, int *g, int *b) {
    switch (d) {
        case DINO_RED:    *r = 239; *g = 68;  *b = 68;  break;
        case DINO_BLUE:   *r = 59;  *g = 130; *b = 246; break;
        case DINO_YELLOW: *r = 245; *g = 158; *b = 11;  break;
        default:          *r = 16;  *g = 185; *b = 129; break;
    }
}

static const char *dino_name(int d) {
    switch (d) {
        case DINO_GREEN:  return "EMERALD REX";
        case DINO_RED:    return "BLAZE REX";
        case DINO_BLUE:   return "SKY REX";
        case DINO_YELLOW: return "QUAKE REX";
        default:          return "NONE";
    }
}

/* ============================================================================
 * Procedural Anti-Aliased HD Texture Baker (runs once at startup)
 * ============================================================================ */
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void blend_px(Uint32 *px, int w, int h, int x, int y, int r, int g, int b, float a) {
    if (x < 0 || x >= w || y < 0 || y >= h || a <= 0.0f) return;
    a = clampf(a, 0.0f, 1.0f);
    Uint32 dst = px[y * w + x];
    int dr = (dst >> 24) & 0xFF;
    int dg = (dst >> 16) & 0xFF;
    int db = (dst >> 8)  & 0xFF;
    int da = dst & 0xFF;
    float fa = da / 255.0f;
    float out_a = a + fa * (1.0f - a);
    if (out_a <= 0.001f) return;
    int nr = (int)((r * a + dr * fa * (1.0f - a)) / out_a);
    int ng = (int)((g * a + dg * fa * (1.0f - a)) / out_a);
    int nb = (int)((b * a + db * fa * (1.0f - a)) / out_a);
    int na = (int)(out_a * 255.0f);
    px[y * w + x] = ((Uint32)(nr & 0xFF) << 24) | ((Uint32)(ng & 0xFF) << 16) |
                    ((Uint32)(nb & 0xFF) << 8)  | (Uint32)(na & 0xFF);
}

static void sdf_ellipse(Uint32 *px, int w, int h, float cx, float cy, float rx, float ry,
                        int r, int g, int b, int outline_px, int shade_3d) {
    int x0 = (int)(cx - rx - outline_px - 2);
    int x1 = (int)(cx + rx + outline_px + 2);
    int y0 = (int)(cy - ry - outline_px - 2);
    int y1 = (int)(cy + ry + outline_px + 2);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f - cx) / rx;
            float dy = (y + 0.5f - cy) / ry;
            float dist = sqrtf(dx * dx + dy * dy);
            float avg_r = 0.5f * (rx + ry);
            float px_dist = (dist - 1.0f) * avg_r;
            if (outline_px > 0) {
                float oa = clampf(1.0f - (px_dist - outline_px), 0.0f, 1.0f);
                if (oa > 0.0f) blend_px(px, w, h, x, y, 15, 23, 42, oa);
            }
            float ia = clampf(0.5f - px_dist, 0.0f, 1.0f);
            if (ia > 0.0f) {
                int cr = r, cg = g, cb = b;
                if (shade_3d) {
                    float light = clampf(1.18f - 0.38f * (dx * 0.45f + dy * 0.85f), 0.68f, 1.32f);
                    cr = (int)clampf(r * light, 0, 255);
                    cg = (int)clampf(g * light, 0, 255);
                    cb = (int)clampf(b * light, 0, 255);
                }
                blend_px(px, w, h, x, y, cr, cg, cb, ia);
            }
        }
    }
}

static void sdf_round_rect(Uint32 *px, int w, int h, float x0, float y0, float rw, float rh, float rad,
                           int r, int g, int b, int outline_px, int shade_3d) {
    float cx = x0 + rw * 0.5f;
    float cy = y0 + rh * 0.5f;
    float hx = rw * 0.5f - rad;
    float hy = rh * 0.5f - rad;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = fabsf(x + 0.5f - cx) - hx;
            float dy = fabsf(y + 0.5f - cy) - hy;
            float ox = dx > 0.0f ? dx : 0.0f;
            float oy = dy > 0.0f ? dy : 0.0f;
            float dist = sqrtf(ox * ox + oy * oy) + (dx > dy ? (dx < 0 ? dx : 0) : (dy < 0 ? dy : 0)) - rad;
            if (outline_px > 0) {
                float oa = clampf(0.5f - (dist - outline_px), 0.0f, 1.0f);
                if (oa > 0.0f) blend_px(px, w, h, x, y, 15, 23, 42, oa);
            }
            float ia = clampf(0.5f - dist, 0.0f, 1.0f);
            if (ia > 0.0f) {
                int cr = r, cg = g, cb = b;
                if (shade_3d) {
                    float ny = (y - y0) / rh;
                    float light = 1.16f - 0.32f * ny;
                    cr = (int)clampf(r * light, 0, 255);
                    cg = (int)clampf(g * light, 0, 255);
                    cb = (int)clampf(b * light, 0, 255);
                }
                blend_px(px, w, h, x, y, cr, cg, cb, ia);
            }
        }
    }
}

static SDL_Texture *make_tex(Uint32 *px, int w, int h) {
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(px, w, h, 32, w * 4,
                                              0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
    free(px);
    return t;
}

static void bake_hd_textures(void) {
    /* 1. Anti-aliased Circle, Glow, and Ring (96x96) */
    {
        int S = 96;
        Uint32 *p_circ = (Uint32 *)calloc(S * S, 4);
        Uint32 *p_glow = (Uint32 *)calloc(S * S, 4);
        Uint32 *p_ring = (Uint32 *)calloc(S * S, 4);
        float c = S * 0.5f, R = S * 0.46f;
        for (int y = 0; y < S; y++) {
            for (int x = 0; x < S; x++) {
                float d = hypotf(x + 0.5f - c, y + 0.5f - c);
                float a_circ = clampf(R - d + 0.5f, 0.0f, 1.0f);
                blend_px(p_circ, S, S, x, y, 255, 255, 255, a_circ);
                float nd = clampf(1.0f - d / R, 0.0f, 1.0f);
                blend_px(p_glow, S, S, x, y, 255, 255, 255, nd * nd);
                float a_ring = clampf(1.0f - fabsf(d - (R - 5.0f)) / 3.5f, 0.0f, 1.0f);
                blend_px(p_ring, S, S, x, y, 255, 255, 255, a_ring);
            }
        }
        tex_circle = make_tex(p_circ, S, S);
        tex_glow   = make_tex(p_glow, S, S);
        tex_ring   = make_tex(p_ring, S, S);
    }

    /* 2. HD Brick Tile (64x64) */
    {
        int S = 64;
        Uint32 *px = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(px, S, S, 2, 2, 60, 60, 6, 180, 83, 9, 2, 1);
        /* Brick sub-blocks */
        sdf_round_rect(px, S, S, 5, 5, 25, 24, 3, 217, 119, 6, 0, 1);
        sdf_round_rect(px, S, S, 34, 5, 25, 24, 3, 217, 119, 6, 0, 1);
        sdf_round_rect(px, S, S, 5, 34, 14, 24, 3, 194, 65, 12, 0, 1);
        sdf_round_rect(px, S, S, 23, 34, 22, 24, 3, 217, 119, 6, 0, 1);
        sdf_round_rect(px, S, S, 49, 34, 10, 24, 3, 194, 65, 12, 0, 1);
        tex_brick = make_tex(px, S, S);
    }

    /* 3. HD Question Block (64x64) */
    {
        int S = 64;
        Uint32 *px = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(px, S, S, 2, 2, 60, 60, 8, 245, 158, 11, 2, 1);
        sdf_round_rect(px, S, S, 6, 6, 52, 52, 6, 250, 204, 21, 0, 1);
        /* Corner rivets */
        sdf_ellipse(px, S, S, 11, 11, 2.5f, 2.5f, 146, 64, 14, 0, 0);
        sdf_ellipse(px, S, S, 53, 11, 2.5f, 2.5f, 146, 64, 14, 0, 0);
        sdf_ellipse(px, S, S, 11, 53, 2.5f, 2.5f, 146, 64, 14, 0, 0);
        sdf_ellipse(px, S, S, 53, 53, 2.5f, 2.5f, 146, 64, 14, 0, 0);
        /* Glossy '?' emblem */
        sdf_round_rect(px, S, S, 22, 14, 20, 8, 4, 255, 255, 255, 2, 0);
        sdf_round_rect(px, S, S, 34, 18, 8, 14, 4, 255, 255, 255, 2, 0);
        sdf_round_rect(px, S, S, 27, 26, 14, 12, 4, 255, 255, 255, 2, 0);
        sdf_ellipse(px, S, S, 31, 47, 4.5f, 4.5f, 255, 255, 255, 2, 0);
        tex_qblock = make_tex(px, S, S);
    }

    /* 4. HD Used Steel Block (64x64) */
    {
        int S = 64;
        Uint32 *px = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(px, S, S, 2, 2, 60, 60, 7, 71, 85, 105, 2, 1);
        sdf_round_rect(px, S, S, 7, 7, 50, 50, 5, 100, 116, 139, 0, 1);
        sdf_ellipse(px, S, S, 12, 12, 2.5f, 2.5f, 30, 41, 59, 0, 0);
        sdf_ellipse(px, S, S, 52, 12, 2.5f, 2.5f, 30, 41, 59, 0, 0);
        sdf_ellipse(px, S, S, 12, 52, 2.5f, 2.5f, 30, 41, 59, 0, 0);
        sdf_ellipse(px, S, S, 52, 52, 2.5f, 2.5f, 30, 41, 59, 0, 0);
        tex_used = make_tex(px, S, S);
    }

    /* 5. HD Spotted Dino Egg Blocks (64x64 x 4 species) */
    for (int d = 1; d <= 4; d++) {
        int S = 64, r, g, b;
        get_dino_rgb(d, &r, &g, &b);
        Uint32 *px = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(px, S, S, 2, 2, 60, 60, 8, 226, 232, 240, 2, 1);
        /* 3D Egg inside block */
        sdf_ellipse(px, S, S, 32, 34, 19, 23, 255, 255, 255, 2, 1);
        sdf_ellipse(px, S, S, 25, 26, 5.5f, 6.5f, r, g, b, 0, 1);
        sdf_ellipse(px, S, S, 39, 32, 6.0f, 5.0f, r, g, b, 0, 1);
        sdf_ellipse(px, S, S, 28, 43, 5.0f, 4.5f, r, g, b, 0, 1);
        sdf_ellipse(px, S, S, 24, 21, 3.5f, 4.5f, 255, 255, 255, 0, 0);
        tex_egg[d] = make_tex(px, S, S);
    }

    /* 6. HD Cylindrical Warp Pipe Tile (64x64) */
    {
        int S = 64;
        Uint32 *px = (Uint32 *)calloc(S * S, 4);
        for (int y = 0; y < S; y++) {
            for (int x = 2; x < S - 2; x++) {
                float nx = (float)(x - 2) / (float)(S - 4);
                float spec = expf(-((nx - 0.28f) * (nx - 0.28f)) * 28.0f);
                float shade = 0.55f + 0.45f * sinf(nx * 3.14159f) + 0.45f * spec;
                int r = (int)clampf(16 * shade + 80 * spec, 0, 255);
                int g = (int)clampf(185 * shade + 70 * spec, 0, 255);
                int b = (int)clampf(129 * shade + 70 * spec, 0, 255);
                if (x <= 4 || x >= S - 5) { r = 6; g = 78; b = 59; }
                blend_px(px, S, S, x, y, r, g, b, 1.0f);
            }
        }
        tex_pipe = make_tex(px, S, S);
    }

    /* 7. HD Coin & Tropical Berry Bush (64x64) */
    {
        int S = 64;
        Uint32 *pc = (Uint32 *)calloc(S * S, 4);
        sdf_ellipse(pc, S, S, 32, 32, 18, 22, 250, 204, 21, 2, 1);
        sdf_ellipse(pc, S, S, 32, 32, 12, 16, 234, 179, 8, 1, 1);
        sdf_round_rect(pc, S, S, 29, 21, 6, 22, 3, 254, 249, 195, 0, 0);
        tex_coin = make_tex(pc, S, S);

        Uint32 *pb = (Uint32 *)calloc(S * S, 4);
        sdf_ellipse(pb, S, S, 22, 38, 15, 14, 21, 128, 61, 2, 1);
        sdf_ellipse(pb, S, S, 42, 38, 15, 14, 21, 128, 61, 2, 1);
        sdf_ellipse(pb, S, S, 32, 28, 17, 16, 34, 197, 94, 2, 1);
        /* Glossy crimson berries */
        sdf_ellipse(pb, S, S, 24, 28, 6.5f, 6.5f, 244, 63, 94, 1, 1);
        sdf_ellipse(pb, S, S, 22, 26, 2.0f, 2.0f, 255, 255, 255, 0, 0);
        sdf_ellipse(pb, S, S, 39, 33, 6.5f, 6.5f, 244, 63, 94, 1, 1);
        sdf_ellipse(pb, S, S, 37, 31, 2.0f, 2.0f, 255, 255, 255, 0, 0);
        tex_berry = make_tex(pb, S, S);
    }

    /* 8. High-Res Rideable Dino Companions (96x96 x 4 species x 2 walk frames) */
    for (int d = 1; d <= 4; d++) {
        int r, g, b;
        get_dino_rgb(d, &r, &g, &b);
        for (int f = 0; f < 2; f++) {
            int S = 96;
            Uint32 *px = (Uint32 *)calloc(S * S, 4);
            int leg_off = (f == 0) ? -5 : 5;
            /* Dorsal spines */
            sdf_ellipse(px, S, S, 34, 26, 5, 5, 251, 191, 36, 2, 1);
            sdf_ellipse(px, S, S, 30, 38, 5, 5, 251, 191, 36, 2, 1);
            sdf_ellipse(px, S, S, 26, 50, 4, 4, 251, 191, 36, 2, 1);
            /* Feathered wings for Sky Rex */
            if (d == DINO_BLUE) {
                sdf_ellipse(px, S, S, 26, 44 + (f ? -4 : 2), 16, 10, 248, 250, 252, 2, 1);
                sdf_ellipse(px, S, S, 22, 38 + (f ? -4 : 2), 12, 7, 224, 242, 254, 1, 1);
            }
            /* Tail */
            sdf_ellipse(px, S, S, 21, 64, 14, 7, r, g, b, 2, 1);
            /* Back leg */
            sdf_round_rect(px, S, S, 33 - leg_off, 68, 12, 19, 5, (int)(r * 0.8f), (int)(g * 0.8f), (int)(b * 0.8f), 2, 1);
            sdf_round_rect(px, S, S, 33 - leg_off, 81, 16, 8, 4, 245, 158, 11, 2, 1);
            /* Torso & Cream Belly */
            sdf_ellipse(px, S, S, 42, 62, 19, 16, r, g, b, 2, 1);
            sdf_ellipse(px, S, S, 48, 64, 13, 12, 254, 249, 195, 0, 1);
            /* Red Saddle */
            sdf_ellipse(px, S, S, 38, 51, 11, 6, 220, 38, 38, 2, 1);
            /* Front leg */
            sdf_round_rect(px, S, S, 45 + leg_off, 69, 12, 19, 5, r, g, b, 2, 1);
            sdf_round_rect(px, S, S, 45 + leg_off, 82, 16, 8, 4, 251, 191, 36, 2, 1);
            /* Neck, Head & Big Round Snout */
            sdf_ellipse(px, S, S, 52, 42, 13, 15, r, g, b, 2, 1);
            sdf_ellipse(px, S, S, 68, 40, 16, 13, r, g, b, 2, 1);
            /* Expressive Eye + Pupil + Catchlight */
            sdf_ellipse(px, S, S, 55, 29, 7, 8, 255, 255, 255, 2, 0);
            sdf_ellipse(px, S, S, 57, 29, 3.5f, 4.5f, 15, 23, 42, 0, 0);
            sdf_ellipse(px, S, S, 56, 27, 1.8f, 1.8f, 255, 255, 255, 0, 0);
            /* Nostril & Smile */
            sdf_ellipse(px, S, S, 75, 35, 2.2f, 2.2f, 15, 23, 42, 0, 0);
            tex_dino[d][f] = make_tex(px, S, S);
        }
    }

    /* 9. High-Res Hero Leo Sprites (64x64 x 3 power states x 2 walk frames) */
    for (int p = 0; p < 3; p++) {
        int tr = (p == 2) ? 248 : 239;
        int tg = (p == 2) ? 250 : 68;
        int tb = (p == 2) ? 252 : 68;
        int or_c = (p == 2) ? 220 : 37;
        int og_c = (p == 2) ? 38  : 99;
        int ob_c = (p == 2) ? 38  : 235;
        for (int f = 0; f < 2; f++) {
            int S = 64;
            Uint32 *px = (Uint32 *)calloc(S * S, 4);
            int step = (f == 0) ? -4 : 4;
            /* Boots */
            sdf_round_rect(px, S, S, 18 - step, 48, 12, 10, 4, 120, 53, 15, 2, 1);
            sdf_round_rect(px, S, S, 32 + step, 48, 12, 10, 4, 146, 64, 14, 2, 1);
            /* Tunic & Overalls */
            sdf_round_rect(px, S, S, 18, 29, 28, 22, 8, tr, tg, tb, 2, 1);
            sdf_round_rect(px, S, S, 21, 34, 22, 17, 6, or_c, og_c, ob_c, 1, 1);
            sdf_ellipse(px, S, S, 26, 39, 2.5f, 2.5f, 250, 204, 21, 0, 0);
            sdf_ellipse(px, S, S, 38, 39, 2.5f, 2.5f, 250, 204, 21, 0, 0);
            /* Head & Face */
            sdf_ellipse(px, S, S, 32, 21, 13, 12, 254, 215, 170, 2, 1);
            sdf_ellipse(px, S, S, 42, 22, 4.5f, 3.8f, 253, 186, 116, 1, 1); /* Nose */
            sdf_ellipse(px, S, S, 37, 19, 2.5f, 3.8f, 15, 23, 42, 0, 0);   /* Eye */
            sdf_ellipse(px, S, S, 36, 17, 1.2f, 1.2f, 255, 255, 255, 0, 0);
            /* Cap + Visor */
            sdf_ellipse(px, S, S, 31, 12, 14, 7, tr, tg, tb, 2, 1);
            sdf_round_rect(px, S, S, 28, 11, 21, 5, 2.5f, tr, tg, tb, 2, 1);
            sdf_ellipse(px, S, S, 35, 11, 3.5f, 3.5f, 250, 204, 21, 0, 0);
            tex_leo[p][f] = make_tex(px, S, S);
        }
    }

    /* 10. High-Res Enemies & Bosses */
    {
        int S = 64;
        /* Walker */
        Uint32 *pw = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(pw, S, S, 14, 46, 14, 12, 5, 15, 23, 42, 1, 1);
        sdf_round_rect(pw, S, S, 36, 46, 14, 12, 5, 15, 23, 42, 1, 1);
        sdf_ellipse(pw, S, S, 32, 30, 22, 20, 147, 51, 234, 2, 1);
        sdf_ellipse(pw, S, S, 24, 26, 4.5f, 6.0f, 255, 255, 255, 1, 0);
        sdf_ellipse(pw, S, S, 40, 26, 4.5f, 6.0f, 255, 255, 255, 1, 0);
        sdf_ellipse(pw, S, S, 24, 27, 2.2f, 3.0f, 15, 23, 42, 0, 0);
        sdf_ellipse(pw, S, S, 40, 27, 2.2f, 3.0f, 15, 23, 42, 0, 0);
        tex_walker = make_tex(pw, S, S);

        /* Koopa */
        Uint32 *pk = (Uint32 *)calloc(S * S, 4);
        sdf_round_rect(pk, S, S, 16, 46, 12, 12, 4, 245, 158, 11, 2, 1);
        sdf_round_rect(pk, S, S, 36, 46, 12, 12, 4, 245, 158, 11, 2, 1);
        sdf_ellipse(pk, S, S, 30, 34, 18, 16, 22, 163, 74, 2, 1);
        sdf_ellipse(pk, S, S, 44, 22, 11, 11, 250, 204, 21, 2, 1);
        sdf_ellipse(pk, S, S, 47, 20, 3.0f, 4.0f, 15, 23, 42, 0, 0);
        tex_koopa = make_tex(pk, S, S);

        /* Shell */
        Uint32 *ps = (Uint32 *)calloc(S * S, 4);
        sdf_ellipse(ps, S, S, 32, 36, 22, 18, 22, 163, 74, 2, 1);
        sdf_ellipse(ps, S, S, 32, 32, 14, 10, 74, 222, 128, 1, 1);
        tex_shell = make_tex(ps, S, S);

        /* Winged Flyer */
        Uint32 *pf = (Uint32 *)calloc(S * S, 4);
        sdf_ellipse(pf, S, S, 24, 20, 14, 9, 255, 255, 255, 2, 1);
        sdf_ellipse(pf, S, S, 34, 34, 19, 15, 236, 72, 153, 2, 1);
        sdf_ellipse(pf, S, S, 44, 30, 4.5f, 5.5f, 255, 255, 255, 1, 0);
        sdf_ellipse(pf, S, S, 45, 30, 2.2f, 2.8f, 15, 23, 42, 0, 0);
        tex_flyer = make_tex(pf, S, S);

        /* Boss (96x96) */
        int BS = 96;
        Uint32 *pb = (Uint32 *)calloc(BS * BS, 4);
        sdf_ellipse(pb, BS, BS, 48, 56, 36, 32, 185, 28, 28, 3, 1);
        sdf_ellipse(pb, BS, BS, 40, 60, 24, 22, 253, 224, 71, 1, 1);
        /* Golden Crown & Horns */
        sdf_round_rect(pb, BS, BS, 28, 14, 40, 14, 4, 250, 204, 21, 2, 1);
        sdf_ellipse(pb, BS, BS, 20, 38, 12, 6, 254, 249, 195, 2, 1);
        sdf_ellipse(pb, BS, BS, 34, 42, 7, 8, 254, 240, 138, 2, 0);
        sdf_ellipse(pb, BS, BS, 33, 42, 3.5f, 4.5f, 220, 38, 38, 0, 0);
        tex_boss = make_tex(pb, BS, BS);
    }
}

/* ============================================================================
 * HD Drawing Helpers (maps 480x270 logical coords to 1280x720 HD pixels)
 * ============================================================================ */
static void fill_rect_hd(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {
        (int)(x * HD_SCALE + 0.5f),
        (int)(y * HD_SCALE + 0.5f),
        (int)(w * HD_SCALE + 0.5f),
        (int)(h * HD_SCALE + 0.5f)
    };
    if (r.w < 1) r.w = 1;
    if (r.h < 1) r.h = 1;
    SDL_RenderFillRect(renderer, &r);
}

static void draw_tex_world(SDL_Texture *tex, float x, float y, float w, float h, int flip_h, Color tint) {
    if (!tex) return;
    SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex, tint.a);
    SDL_Rect dst = {
        (int)(x * HD_SCALE + 0.5f),
        (int)(y * HD_SCALE + 0.5f),
        (int)(w * HD_SCALE + 0.5f),
        (int)(h * HD_SCALE + 0.5f)
    };
    SDL_RenderCopyEx(renderer, tex, NULL, &dst, 0.0, NULL, flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

static void fill_circle_hd(float cx, float cy, float rad, Color c) {
    draw_tex_world(tex_circle, cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 0, c);
}

static void draw_glow_hd(float cx, float cy, float rad, Color c) {
    draw_tex_world(tex_glow, cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 0, c);
}

/* 5x7 Pixel Font rendered crisp in HD with drop shadow */
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

static void draw_text_pass(float x, float y, const char *s, Color col, float scale) {
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    float cx = x;
    while (*s) {
        char c = *s++;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 32 && c < 128) {
            const unsigned char *g = font5x7[c - 32];
            for (int r = 0; r < 7; r++) {
                for (int col_i = 0; col_i < 5; col_i++) {
                    if (g[r] & (1 << (4 - col_i))) {
                        SDL_Rect rc = {
                            (int)((cx + col_i * scale) * HD_SCALE + 0.5f),
                            (int)((y + r * scale) * HD_SCALE + 0.5f),
                            (int)(scale * HD_SCALE + 0.8f),
                            (int)(scale * HD_SCALE + 0.8f)
                        };
                        SDL_RenderFillRect(renderer, &rc);
                    }
                }
            }
        }
        cx += 6.0f * scale;
    }
}

static void draw_text(float x, float y, const char *s, Color col, float scale) {
    draw_text_pass(x + 0.45f * scale, y + 0.45f * scale, s, rgba(2, 6, 23, 220), scale);
    draw_text_pass(x, y, s, col, scale);
}

static void show_banner(const char *msg, int dur) {
    snprintf(banner_text, sizeof(banner_text), "%s", msg);
    banner_timer = dur;
}

static void spawn_particle(float x, float y, float vx, float vy, Color col, int life) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].active = 1;
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].color = col;
            particles[i].life = life;
            particles[i].max_life = life;
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
            entities[i].w = 14;
            entities[i].h = 15;
            entities[i].dir = -1;
            entities[i].vx = -0.5f;
            if (type == ENT_PLATFORM) {
                entities[i].w = 40;
                entities[i].h = 7;
                entities[i].vx = 0.6f;
            } else if (type == ENT_DINO_MOUNT) {
                entities[i].w = 16;
                entities[i].h = 16;
                entities[i].vx = 0.0f;
                entities[i].dir = 1;
            } else if (type == ENT_POKEY) {
                entities[i].h = 22;
            } else if (type == ENT_BOSS) {
                entities[i].w = 30;
                entities[i].h = 32;
                entities[i].hp = STAGES[current_stage].boss_hp;
                entities[i].max_hp = entities[i].hp;
                entities[i].vx = -0.6f;
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
    if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H) {
        map[ty][tx] = c;
    }
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
            shockwaves[i].r = 6.0f;
            shockwaves[i].max_r = 76.0f;
            shockwaves[i].color = rgb(251, 191, 36);
            break;
        }
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        float d = hypotf((e->x + e->w / 2) - x, (e->y + e->h / 2) - y);
        if (d < 80.0f && (e->type == ENT_WALKER || e->type == ENT_KOOPA || e->type == ENT_POKEY)) {
            e->active = 0;
            score += 200;
            spawn_particle(e->x, e->y, 0, -1.5f, rgb(253, 224, 71), 25);
        }
    }
}

static void load_stage(int idx, int from_cp) {
    current_stage = ((idx % 8) + 8) % 8;
    const StageDef *st = &STAGES[current_stage];
    memset(map, '.', sizeof(map));
    memset(bump_off, 0, sizeof(bump_off));
    memset(entities, 0, sizeof(entities));
    memset(fireballs, 0, sizeof(fireballs));
    memset(particles, 0, sizeof(particles));
    memset(shockwaves, 0, sizeof(shockwaves));

    char gnd = (st->theme == 5) ? 'I' : '#';
    for (int x = 0; x < MAP_W; x++) {
        int pit = ((x >= 36 && x <= 38) || (x >= 68 && x <= 71 && !st->has_boss) ||
                   (x >= 104 && x <= 107) || (x >= 138 && x <= 141 && !st->has_boss));
        if (!pit) {
            for (int y = 14; y < MAP_H; y++) set_tile(x, y, gnd);
        }
    }

    /* Starter Dino Egg Block & Berries near spawn */
    set_tile(7, 10, '?');
    set_tile(8, 10, 'E');
    set_tile(9, 10, 'B');
    set_tile(10, 10, '?');
    set_tile(13, 13, 'F');
    set_tile(14, 12, 'C');
    set_tile(15, 12, 'C');
    set_tile(16, 13, 'F');

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
    player.w = 12;
    player.h = player.dino ? 21 : 16;
    player.ground_pounding = 0;
    player.tongue_state = 0;
    player.tongue_len = 0;
    camera_x = player.x - W * 0.28f;
    if (camera_x < 0) camera_x = 0;

    char buf[64];
    snprintf(buf, sizeof(buf), "STAGE %s: %s", st->code, st->title);
    show_banner(buf, 130);
}

static void hit_block(int tx, int ty) {
    char c = get_tile(tx, ty);
    if (!is_solid(c)) return;
    bump_off[ty][tx] = 4;
    const StageDef *st = &STAGES[current_stage];
    if (c == '?') {
        set_tile(tx, ty, 'U');
        coins++;
        score += 100;
        play_sfx(SFX_COIN);
    } else if (c == 'M') {
        set_tile(tx, ty, 'U');
        play_sfx(SFX_POWERUP);
        spawn_entity(player.power >= 1 ? ENT_FLOWER : ENT_MUSHROOM, tx * TILE, (ty - 1) * TILE, 0);
    } else if (c == 'E' || c == 'D') {
        set_tile(tx, ty, 'U');
        int dt = (c == 'E') ? st->default_dino : st->second_dino;
        play_sfx(SFX_HATCH);
        spawn_entity(ENT_DINO_MOUNT, tx * TILE, (ty - 1) * TILE - 2, dt);
        char msg[64];
        snprintf(msg, sizeof(msg), "%s HATCHED! HOP ON!", dino_name(dt));
        show_banner(msg, 120);
    } else if (c == 'B') {
        if (player.power > 0 || player.dino || player.ground_pounding) {
            set_tile(tx, ty, '.');
            play_sfx(SFX_BRICK);
            score += 50;
            for (int i = 0; i < 6; i++) {
                spawn_particle(tx * TILE + 8, ty * TILE + 8, (rand() % 5) - 2.0f, -2.5f, rgb(217, 119, 6), 25);
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
            float sx = (player.dir > 0) ? (player.x + player.w + 4) : (player.x - 10);
            float sy = player.y + 6;
            if (is_fire) {
                float vys[3] = {-0.8f, 0.0f, 0.8f};
                for (int k = 0; k < 3; k++) {
                    for (int i = 0; i < MAX_FIREBALLS; i++) {
                        if (!fireballs[i].active) {
                            fireballs[i].active = 1;
                            fireballs[i].x = sx;
                            fireballs[i].y = sy;
                            fireballs[i].vx = player.dir * 3.5f;
                            fireballs[i].vy = vys[k];
                            fireballs[i].r = 5;
                            fireballs[i].from_dino = 1;
                            fireballs[i].life = 100;
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
                        entities[i].y = player.y + 3;
                        entities[i].w = 13;
                        entities[i].h = 12;
                        entities[i].vx = player.dir * 3.8f;
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
            player.tongue_len = 4.0f;
            player.tongue_reward = 0;
            play_sfx(SFX_TONGUE);
        }
    } else if (player.power == 2) {
        play_sfx(SFX_SPIT);
        for (int i = 0; i < MAX_FIREBALLS; i++) {
            if (!fireballs[i].active) {
                fireballs[i].active = 1;
                fireballs[i].x = (player.dir > 0) ? (player.x + player.w + 2) : (player.x - 6);
                fireballs[i].y = player.y + 6;
                fireballs[i].vx = player.dir * 3.2f;
                fireballs[i].vy = 0.9f;
                fireballs[i].r = 3;
                fireballs[i].from_dino = 0;
                fireballs[i].life = 85;
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
    player.vy = -4.5f;
    play_sfx(SFX_BAIL);
    spawn_entity(ENT_DINO_MOUNT, player.x - player.dir * 6, player.y + 4, old);
}

static void hurt_player(void) {
    if (player.invuln > 0 || player.star_timer > 0) return;
    if (player.dino) {
        bail_out_dino();
        player.vy = -3.2f;
        player.invuln = 90;
        show_banner("DINO PROTECTED YOU! HOP BACK ON!", 90);
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

/* Input State (Scancodes + Gamepad) */
static Uint8 keys_held[SDL_NUM_SCANCODES];
static Uint8 prev_held[SDL_NUM_SCANCODES];

static void update_game(void) {
    global_tick++;
    if (banner_timer > 0) banner_timer--;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (bump_off[y][x] > 0) bump_off[y][x]--;

    int k_left   = keys_held[SDL_SCANCODE_LEFT]  || keys_held[SDL_SCANCODE_A];
    int k_right  = keys_held[SDL_SCANCODE_RIGHT] || keys_held[SDL_SCANCODE_D];
    int k_down   = keys_held[SDL_SCANCODE_DOWN]  || keys_held[SDL_SCANCODE_S];
    int p_down   = prev_held[SDL_SCANCODE_DOWN]  || prev_held[SDL_SCANCODE_S];
    int k_jump   = keys_held[SDL_SCANCODE_SPACE] || keys_held[SDL_SCANCODE_UP] || keys_held[SDL_SCANCODE_W];
    int p_jump   = prev_held[SDL_SCANCODE_SPACE] || prev_held[SDL_SCANCODE_UP] || prev_held[SDL_SCANCODE_W];
    int k_tongue = keys_held[SDL_SCANCODE_Z] || keys_held[SDL_SCANCODE_LSHIFT] || keys_held[SDL_SCANCODE_RSHIFT] || keys_held[SDL_SCANCODE_J];
    int p_tongue = prev_held[SDL_SCANCODE_Z] || prev_held[SDL_SCANCODE_LSHIFT] || prev_held[SDL_SCANCODE_RSHIFT] || prev_held[SDL_SCANCODE_J];
    int k_bail   = keys_held[SDL_SCANCODE_X] || keys_held[SDL_SCANCODE_E] || keys_held[SDL_SCANCODE_LCTRL];
    int p_bail   = prev_held[SDL_SCANCODE_X] || prev_held[SDL_SCANCODE_E] || prev_held[SDL_SCANCODE_LCTRL];

    if (game_state == 0 || game_state == 3) {
        if ((keys_held[SDL_SCANCODE_LEFT] && !prev_held[SDL_SCANCODE_LEFT])) {
            current_stage = (current_stage + 7) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if ((keys_held[SDL_SCANCODE_RIGHT] && !prev_held[SDL_SCANCODE_RIGHT])) {
            current_stage = (current_stage + 1) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if ((keys_held[SDL_SCANCODE_RETURN] && !prev_held[SDL_SCANCODE_RETURN]) || (k_jump && !p_jump)) {
            game_state = 1;
            load_stage(current_stage, 0);
        }
        player.anim_frame += 0.16f;
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
    player.h = player.dino ? 21 : (player.power > 0 ? 16 : 12);

    if (k_tongue && !p_tongue) use_tongue_or_fire();
    if (k_bail && !p_bail && player.dino) bail_out_dino();

    /* Tongue update */
    if (player.tongue_state > 0 && player.dino) {
        if (player.tongue_state == 1) {
            player.tongue_len += 6.5f;
            if (player.tongue_len >= 62.0f) player.tongue_state = 2;
        } else {
            player.tongue_len -= 6.0f;
            if (player.tongue_len <= 0.0f) {
                player.tongue_len = 0.0f;
                player.tongue_state = 0;
                if (player.tongue_reward == 1) {
                    coins += 3;
                    score += 500;
                    berries_eaten++;
                    play_sfx(SFX_SWALLOW);
                    if (berries_eaten % 3 == 0) {
                        player.star_timer = 360;
                        play_sfx(SFX_POWERUP);
                        show_banner("3 BERRIES EATEN - STARMAN POWER!", 100);
                    }
                } else if (player.tongue_reward >= 2) {
                    play_sfx(SFX_SWALLOW);
                    player.dino_mouth = (player.tongue_reward == 3) ? 2 : 1;
                }
                player.tongue_reward = 0;
            }
        }
        float tip_x = (player.dir > 0) ? (player.x + player.w + player.tongue_len) : (player.x - player.tongue_len);
        float tip_y = player.y + 7.0f;
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
                if (aabb(tip_x - 6, tip_y - 6, 12, 12, e->x, e->y, e->w, e->h)) {
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
            player.vy = 5.0f;
        }
    } else {
        float max_s = (k_tongue || player.dino) ? 2.1f : 1.65f;
        float acc = player.on_ground ? 0.16f : 0.11f;
        if (k_left) {
            player.vx -= acc;
            if (player.vx < -max_s) player.vx = -max_s;
            player.dir = -1;
        } else if (k_right) {
            player.vx += acc;
            if (player.vx > max_s) player.vx = max_s;
            player.dir = 1;
        } else {
            player.vx *= 0.82f;
            if (fabsf(player.vx) < 0.03f) player.vx = 0;
        }
        player.vy += GRAVITY;
        if (player.vy > 4.2f) player.vy = 4.2f;
    }

    if (player.on_ground) {
        player.coyote = 7;
        player.wing_flaps = 4;
    } else if (player.coyote > 0) {
        player.coyote--;
    }

    if (k_jump && !p_jump) {
        if (player.coyote > 0) {
            player.vy = player.dino ? -3.85f : -3.65f;
            player.on_ground = 0;
            player.coyote = 0;
            play_sfx(SFX_JUMP);
        } else if (player.wall_slide != 0 && !player.dino) {
            player.vy = -3.55f;
            player.vx = -player.wall_slide * 2.1f;
            player.dir = -player.wall_slide;
            player.wall_slide = 0;
            play_sfx(SFX_JUMP);
        } else if (player.dino == DINO_BLUE && player.wing_flaps > 0) {
            player.wing_flaps--;
            player.vy = -3.35f;
            play_sfx(SFX_FLUTTER);
        }
    }

    if (!k_jump && player.vy < -1.3f && !player.ground_pounding) {
        player.vy *= 0.65f;
    }

    if (player.dino && k_jump && player.vy > 0.4f && player.flutter_cd == 0 && !player.ground_pounding) {
        player.vy = -2.45f;
        player.flutter_cd = 26;
        play_sfx(SFX_FLUTTER);
    }

    /* Horizontal collision */
    player.x += player.vx;
    if (player.x < 0) { player.x = 0; player.vx = 0; }
    player.wall_slide = 0;
    int tx1 = (int)(player.x / TILE);
    int tx2 = (int)((player.x + player.w - 0.01f) / TILE);
    int ty1 = (int)((player.y + 1) / TILE);
    int ty2 = (int)((player.y + player.h - 1) / TILE);
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
    if (player.wall_slide != 0 && player.vy > 0.7f) player.vy = 0.7f;

    /* Vertical collision */
    float prev_bot = player.y + player.h;
    float orig_vy = player.vy;
    player.y += player.vy;
    player.on_ground = 0;
    int vtx1 = (int)((player.x + 1) / TILE);
    int vtx2 = (int)((player.x + player.w - 1) / TILE);
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
                show_banner("CHECKPOINT REACHED!", 90);
                continue;
            }
            if (c == 'G') {
                play_sfx(SFX_CLEAR);
                score += 2000;
                game_state = 2;
                state_timer = 100;
                show_banner("STAGE CLEAR!", 100);
                return;
            }
            if (c == 'J' && orig_vy >= 0) {
                player.y = ty * TILE - player.h;
                player.vy = -4.9f;
                player.ground_pounding = 0;
                play_sfx(SFX_JUMP);
                res_y = 1;
                break;
            }
            if (!res_y && is_solid(c)) {
                if (orig_vy < 0) {
                    player.y = (ty + 1) * TILE;
                    player.vy = 0.3f;
                    hit_block(tx, ty);
                    res_y = 1;
                    break;
                } else if (orig_vy >= 0 && prev_bot <= ty * TILE + 6.0f) {
                    player.y = ty * TILE - player.h;
                    player.vy = 0;
                    player.on_ground = 1;
                    if (player.ground_pounding) {
                        player.ground_pounding = 0;
                        hit_block(tx, ty);
                        if (player.dino == DINO_YELLOW) spawn_shockwave(player.x + 6, player.y + player.h);
                        else play_sfx(SFX_STOMP);
                    } else if (player.dino == DINO_YELLOW && orig_vy > 1.8f) {
                        spawn_shockwave(player.x + 6, player.y + player.h);
                    }
                    res_y = 1;
                    break;
                }
            }
        }
        if (res_y) break;
    }

    if (player.y > H + 32) {
        if (player.dino) {
            bail_out_dino();
            player.y = H - 36;
        } else {
            hurt_player();
        }
        return;
    }

    player.anim_frame += fabsf(player.vx) * 0.15f + 0.04f;

    /* Fireballs */
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        Fireball *f = &fireballs[i];
        if (!f->active) continue;
        f->x += f->vx;
        f->y += f->vy;
        if (!f->from_dino) {
            f->vy += 0.12f;
            if (is_solid(get_tile((int)(f->x / TILE), (int)((f->y + f->r) / TILE)))) f->vy = -1.6f;
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
                        e->invuln = 30;
                        play_sfx(SFX_STOMP);
                        if (e->hp <= 0) { e->active = 0; score += 5000; play_sfx(SFX_CLEAR); }
                    }
                } else {
                    e->active = 0;
                    score += 200;
                    play_sfx(SFX_STOMP);
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
            if (fabsf(e->x - e->start_x) > 48.0f) e->vx = -e->vx;
            if (player.vy >= 0 && player.x + player.w > e->x && player.x < e->x + e->w &&
                player.y + player.h >= e->y - 3 && player.y + player.h <= e->y + 6) {
                player.y = e->y - player.h;
                player.vy = 0;
                player.on_ground = 1;
                player.x += e->vx;
            }
            continue;
        }
        if (e->type == ENT_DINO_MOUNT) {
            e->vy += GRAVITY * 0.8f;
            e->y += e->vy;
            int dty = (int)((e->y + e->h) / TILE);
            int dtx = (int)((e->x + e->w / 2) / TILE);
            if (is_solid(get_tile(dtx, dty))) {
                e->y = dty * TILE - e->h;
                e->vy = 0;
            }
            if (e->timer > 18 && aabb(player.x, player.y, player.w, player.h, e->x, e->y, e->w, e->h)) {
                if (!player.dino) {
                    player.dino = e->dino_type;
                    e->active = 0;
                    play_sfx(SFX_HATCH);
                    char msg[64];
                    snprintf(msg, sizeof(msg), "RIDING %s! Z=TONGUE, SPACE=FLUTTER", dino_name(player.dino));
                    show_banner(msg, 120);
                } else {
                    e->active = 0;
                    score += 1000;
                    play_sfx(SFX_POWERUP);
                }
            }
            continue;
        }
        if (e->type == ENT_FLYER) {
            e->x += e->vx;
            e->y = e->start_y + sinf(e->timer * 0.06f) * 14.0f;
            if (e->timer % 180 == 0) e->vx = -e->vx;
        } else if (e->type == ENT_BOSS) {
            if (e->invuln > 0) e->invuln--;
            e->vy += GRAVITY;
            e->x += e->vx;
            e->y += e->vy;
            int bty = (int)((e->y + e->h) / TILE);
            int btx = (int)((e->x + e->w / 2) / TILE);
            if (is_solid(get_tile(btx, bty))) {
                e->y = bty * TILE - e->h;
                e->vy = 0;
                if (e->timer % 95 == 0) e->vy = -3.6f;
            }
            if (e->x < 149 * TILE) e->vx = fabsf(e->vx);
            if (e->x > 168 * TILE) e->vx = -fabsf(e->vx);
            if (e->timer % 110 == 0) {
                spawn_entity(ENT_SHELL, e->x - 10, e->y + 12, 0);
            }
        } else {
            e->vy += GRAVITY;
            e->x += e->vx;
            int etx = (int)((e->vx > 0 ? e->x + e->w : e->x) / TILE);
            int ety = (int)((e->y + e->h / 2) / TILE);
            if (is_solid(get_tile(etx, ety))) { e->vx = -e->vx; e->dir = -e->dir; }
            e->y += e->vy;
            int gty = (int)((e->y + e->h) / TILE);
            int gtx = (int)((e->x + e->w / 2) / TILE);
            if (is_solid(get_tile(gtx, gty))) { e->y = gty * TILE - e->h; e->vy = 0; }
        }

        if (aabb(player.x, player.y, player.w, player.h, e->x, e->y, e->w, e->h)) {
            if (e->type == ENT_MUSHROOM || e->type == ENT_FLOWER) {
                e->active = 0;
                player.power = (e->type == ENT_FLOWER) ? 2 : 1;
                score += 1000;
                play_sfx(SFX_POWERUP);
                continue;
            }
            if (player.star_timer > 0 && e->type != ENT_BOSS) {
                e->active = 0;
                score += 200;
                play_sfx(SFX_STOMP);
                continue;
            }
            if (player.vy > 0 && player.y + player.h - player.vy <= e->y + 8) {
                player.vy = -3.2f;
                play_sfx(SFX_STOMP);
                if (player.dino == DINO_YELLOW) spawn_shockwave(player.x + 6, e->y + e->h);
                if (e->type == ENT_FLYER) e->type = ENT_KOOPA;
                else if (e->type == ENT_KOOPA) { e->type = ENT_SHELL; e->vx = 0; }
                else if (e->type == ENT_SHELL) e->vx = (fabsf(e->vx) > 0.2f) ? 0 : player.dir * 3.2f;
                else if (e->type == ENT_BOSS) {
                    if (e->invuln <= 0) {
                        e->hp--;
                        e->invuln = 35;
                        if (e->hp <= 0) { e->active = 0; score += 5000; play_sfx(SFX_CLEAR); }
                    }
                } else { e->active = 0; score += 200; }
            } else if (e->type == ENT_SHELL && fabsf(e->vx) < 0.2f) {
                e->vx = player.dir * 3.2f;
                e->x += player.dir * 5;
                play_sfx(SFX_STOMP);
            } else {
                hurt_player();
            }
        }
    }

    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            shockwaves[i].r += 3.5f;
            if (shockwaves[i].r >= shockwaves[i].max_r) shockwaves[i].active = 0;
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.1f;
            if (--particles[i].life <= 0) particles[i].active = 0;
        }
    }

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

/* ============================================================================
 * Hardware-Accelerated 720p HD Renderer
 * ============================================================================ */
static void render_dino_hd(float cx, float by, int dir, int dino_id, float anim, int mouth_full, float tongue_len) {
    int s = (dir >= 0) ? 1 : -1;
    int frame = ((int)(anim * 1.4f)) & 1;
    if (dino_id < 1 || dino_id > 4) dino_id = 1;

    if (tongue_len > 0.0f) {
        float tx = (s > 0) ? (cx + 6.0f) : (cx - 6.0f - tongue_len);
        fill_rect_hd(tx, by - 11.0f, tongue_len, 2.5f, rgb(251, 113, 133));
        fill_circle_hd(cx + s * (6.0f + tongue_len), by - 9.8f, 3.2f, rgb(244, 63, 94));
    }

    /* Draw High-Res Pre-Baked Shaded Dino Companion Texture */
    float dw = 26.0f, dh = 26.0f;
    draw_tex_world(tex_dino[dino_id][frame], cx - dw * 0.5f, by - dh + 2.0f, dw, dh, s < 0, rgb(255, 255, 255));
    if (mouth_full) {
        fill_circle_hd(cx + s * 7.5f, by - 11.5f, 3.2f, (dino_id == DINO_RED) ? rgb(251, 146, 60) : rgb(134, 239, 172));
    }
}

static void render_frame(void) {
    const StageDef *st = &STAGES[current_stage];
    float cam_x = camera_x;

    /* 1. Smooth HD Sky Gradient + Sun/Moon Glow */
    for (int y = 0; y < 72; y++) {
        float t = (float)y / 72.0f;
        int r = (int)(st->sky_top[0] + (st->sky_bot[0] - st->sky_top[0]) * t);
        int g = (int)(st->sky_top[1] + (st->sky_bot[1] - st->sky_top[1]) * t);
        int b = (int)(st->sky_top[2] + (st->sky_bot[2] - st->sky_top[2]) * t);
        SDL_SetRenderDrawColor(renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
        SDL_Rect band = {0, y * 10, HD_W, 10};
        SDL_RenderFillRect(renderer, &band);
    }
    draw_glow_hd(390.0f, 54.0f, 52.0f, rgba(254, 249, 195, 95));
    fill_circle_hd(390.0f, 54.0f, 20.0f, rgba(254, 240, 138, 235));

    /* 2. Soft Anti-Aliased Parallax Clouds & Layered Mountains */
    for (int i = -1; i < 6; i++) {
        float clx = i * 115.0f - fmodf(cam_x * 0.12f, 115.0f);
        float cly = 36.0f + (i % 2) * 16.0f;
        fill_circle_hd(clx, cly, 14.0f, rgba(255, 255, 255, 190));
        fill_circle_hd(clx + 11.0f, cly - 4.0f, 16.0f, rgba(255, 255, 255, 210));
        fill_circle_hd(clx + 22.0f, cly, 13.0f, rgba(255, 255, 255, 190));
    }

    Color hf = rgb(st->hill_far[0], st->hill_far[1], st->hill_far[2]);
    Color hn = rgb(st->hill_near[0], st->hill_near[1], st->hill_near[2]);
    for (int i = -1; i < 6; i++) {
        float hx = i * 110.0f - fmodf(cam_x * 0.2f, 110.0f);
        fill_circle_hd(hx + 55.0f, H - 22.0f, 62.0f, hf);
        float nx = i * 90.0f - fmodf(cam_x * 0.35f, 90.0f);
        fill_circle_hd(nx + 45.0f, H - 10.0f, 44.0f, hn);
    }

    /* 3. High-Resolution Shaded Tiles */
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
                fill_rect_hd(sx, sy, TILE + 0.4f, TILE + 0.4f, (c == 'I') ? rgb(2, 132, 199) : g_bod);
                fill_rect_hd(sx + 2, sy + 5, TILE - 4, TILE - 7,
                             (c == 'I') ? rgba(56, 189, 248, 90) : rgba(0, 0, 0, 45));
                if (ty == 0 || !is_solid(get_tile(tx, ty - 1))) {
                    fill_rect_hd(sx, sy, TILE + 0.4f, 4.5f, (c == 'I') ? rgb(224, 242, 254) : g_top);
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
                draw_tex_world(tex_pipe, sx, sy, TILE, TILE, 0, rgb(255, 255, 255));
                if (get_tile(tx, ty - 1) != 'P') {
                    draw_tex_world(tex_pipe, sx - 1.5f, sy, TILE + 3.0f, 6.0f, 0, rgb(52, 211, 153));
                }
            } else if (c == 'F') {
                draw_tex_world(tex_berry, sx - 1.0f, sy - 1.0f, TILE + 2.0f, TILE + 2.0f, 0, rgb(255, 255, 255));
            } else if (c == 'C') {
                float pulse = 0.75f + 0.25f * fabsf(sinf(global_tick * 0.1f + tx));
                float cw = TILE * pulse;
                draw_tex_world(tex_coin, sx + (TILE - cw) * 0.5f, sy, cw, TILE, 0, rgb(255, 255, 255));
            } else if (c == 'J') {
                fill_rect_hd(sx + 1, sy + 5, TILE - 2, 4, rgb(239, 68, 68));
                fill_rect_hd(sx + 3, sy + 9, TILE - 6, 7, rgb(250, 204, 21));
            } else if (c == 'K') {
                fill_rect_hd(sx + 7, sy - 10, 2.2f, 26, rgb(248, 250, 252));
                fill_circle_hd(sx + 8.1f, sy - 11, 2.5f, rgb(250, 204, 21));
                fill_rect_hd(sx + 9, sy - 10, 11, 7, (checkpoint_x > 0) ? rgb(34, 197, 94) : rgb(59, 130, 246));
            } else if (c == 'G') {
                fill_rect_hd(sx + 7, sy, 2.5f, TILE + 0.5f, rgb(248, 250, 252));
                if (ty == 4) {
                    fill_circle_hd(sx + 8.2f, sy, 4.0f, rgb(250, 204, 21));
                    fill_rect_hd(sx + 9.5f, sy + 2, 16, 9, rgb(16, 185, 129));
                }
            }
        }
    }

    /* 4. High-Resolution Entities */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        float sx = e->x - cam_x;
        float sy = e->y;
        if (e->type == ENT_PLATFORM) {
            fill_rect_hd(sx, sy, e->w, e->h, rgb(245, 158, 11));
            fill_rect_hd(sx + 1, sy + 1, e->w - 2, 2.5f, rgb(254, 240, 138));
        } else if (e->type == ENT_DINO_MOUNT) {
            render_dino_hd(sx + 8.0f, sy + e->h, e->dir, e->dino_type, global_tick * 0.1f, 0, 0);
        } else if (e->type == ENT_BOSS) {
            Color tint = (e->invuln % 4 > 1) ? rgb(255, 180, 180) : rgb(255, 255, 255);
            draw_tex_world(tex_boss, sx - 3, sy - 4, e->w + 6, e->h + 6, e->vx > 0, tint);
            /* Boss HP bar */
            fill_rect_hd(sx - 2, sy - 9, e->w + 4, 4, rgb(15, 23, 42));
            fill_rect_hd(sx - 1, sy - 8, (e->w + 2) * ((float)e->hp / (float)e->max_hp), 2.2f, rgb(239, 68, 68));
        } else if (e->type == ENT_WALKER) {
            draw_tex_world(tex_walker, sx - 2, sy - 2, e->w + 4, e->h + 3, e->vx > 0, rgb(255, 255, 255));
        } else if (e->type == ENT_KOOPA) {
            draw_tex_world(tex_koopa, sx - 2, sy - 3, e->w + 4, e->h + 4, e->vx < 0, rgb(255, 255, 255));
        } else if (e->type == ENT_SHELL) {
            draw_tex_world(tex_shell, sx - 1, sy - 2, e->w + 2, e->h + 3, 0, rgb(255, 255, 255));
        } else if (e->type == ENT_FLYER) {
            draw_tex_world(tex_flyer, sx - 2, sy - 2, e->w + 4, e->h + 4, e->vx < 0, rgb(255, 255, 255));
        } else if (e->type == ENT_POKEY) {
            for (int seg = 0; seg < 3; seg++) {
                fill_circle_hd(sx + 7, sy + e->h - 4 - seg * 7, 6.5f, rgb(234, 179, 8));
            }
        } else if (e->type == ENT_MUSHROOM || e->type == ENT_FLOWER) {
            fill_circle_hd(sx + 7, sy + 7, 7.0f, (e->type == ENT_FLOWER) ? rgb(249, 115, 22) : rgb(239, 68, 68));
            fill_circle_hd(sx + 5, sy + 5, 2.5f, rgb(255, 255, 255));
        }
    }

    /* 5. Glowing Fireballs, Shockwave Rings & Particles */
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        if (fireballs[i].active) {
            float fx = fireballs[i].x - cam_x;
            float fy = fireballs[i].y;
            draw_glow_hd(fx, fy, fireballs[i].r * 2.4f, rgba(251, 146, 60, 160));
            fill_circle_hd(fx, fy, fireballs[i].r, rgb(239, 68, 68));
            fill_circle_hd(fx, fy, fireballs[i].r * 0.55f, rgb(254, 240, 138));
        }
    }
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            float r = shockwaves[i].r;
            float sx = shockwaves[i].x - cam_x;
            float sy = shockwaves[i].y;
            draw_tex_world(tex_ring, sx - r, sy - r * 0.45f, r * 2.0f, r * 0.9f, 0, shockwaves[i].color);
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            fill_circle_hd(particles[i].x - cam_x, particles[i].y, 2.5f, particles[i].color);
        }
    }

    /* 6. High-Res Hero Leo & Rideable Dino Mount */
    if (!(player.invuln > 0 && (player.invuln / 4) % 2 == 1)) {
        float px = player.x + player.w * 0.5f - cam_x;
        float py = player.y + player.h;
        float leo_y = player.dino ? (py - 8.5f) : py;
        int p_idx = (player.power >= 2) ? 2 : (player.power == 1 ? 1 : 0);
        int l_frame = ((int)(player.anim_frame * 1.5f)) & 1;
        Color l_tint = (player.star_timer > 0 && (global_tick % 4 < 2)) ? rgb(254, 240, 138) : rgb(255, 255, 255);
        float lw = 18.0f, lh = (player.power == 0 && !player.dino) ? 15.0f : 19.0f;
        draw_tex_world(tex_leo[p_idx][l_frame], px - lw * 0.5f, leo_y - lh + 1.0f, lw, lh, player.dir < 0, l_tint);

        if (player.dino) {
            render_dino_hd(px, py, player.dir, player.dino, player.anim_frame, player.dino_mouth > 0, player.tongue_len);
        }
    }

    /* 7. Crisp 720p HD HUD Bar */
    fill_rect_hd(5, 3, W - 10, 17, rgba(15, 23, 42, 220));
    fill_rect_hd(5, 19.5f, W - 10, 1.0f, rgba(56, 189, 248, 160));
    char hud[128];
    snprintf(hud, sizeof(hud), "LEO X%d   COINS:%02d   SCORE:%06d   STAGE %s   DINO: %s",
             lives, coins, score, st->code, dino_name(player.dino));
    draw_text(12, 8, hud, rgb(253, 224, 71), 1.05f);

    if (banner_timer > 0) {
        fill_rect_hd(W * 0.5f - 148, 24, 296, 15, rgba(15, 23, 42, 230));
        draw_text(W * 0.5f - 138, 28, banner_text, rgb(52, 211, 153), 1.0f);
    }

    if (game_state == 0 || game_state == 3) {
        fill_rect_hd(W * 0.5f - 178, H * 0.5f - 70, 356, 142, rgba(15, 23, 42, 238));
        fill_rect_hd(W * 0.5f - 178, H * 0.5f - 70, 356, 2.0f, rgb(250, 204, 21));
        fill_rect_hd(W * 0.5f - 178, H * 0.5f + 70, 356, 2.0f, rgb(56, 189, 248));
        draw_text(W * 0.5f - 152, H * 0.5f - 54, "SUPER LEO WORLD: DINO ISLAND", rgb(253, 224, 71), 1.85f);
        draw_text(W * 0.5f - 134, H * 0.5f - 28, "720P HD EDITION - 4 RIDEABLE DINOS - 8 STAGES", rgb(52, 211, 153), 1.0f);
        draw_text(W * 0.5f - 145, H * 0.5f - 8,  "EMERALD:TONGUE/FLUTTER   BLAZE:3X FIREBALLS", rgb(248, 250, 252), 1.0f);
        draw_text(W * 0.5f - 145, H * 0.5f + 6,  "SKY:4-FLAP FLIGHT        QUAKE:SHOCKWAVE STOMP", rgb(248, 250, 252), 1.0f);
        char sel[64];
        snprintf(sel, sizeof(sel), "< STAGE %s: %s >", st->code, st->title);
        draw_text(W * 0.5f - 122, H * 0.5f + 27, sel, rgb(56, 189, 248), 1.08f);
        draw_text(W * 0.5f - 118, H * 0.5f + 46, "PRESS ENTER OR SPACE TO START!", rgb(251, 191, 36), 1.1f);
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_ShowCursor(SDL_DISABLE);

    if (SDL_NumJoysticks() > 0) {
        SDL_JoystickOpen(0);
    }

    window = SDL_CreateWindow("Super Leo World HD: Dino Island",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              HD_W, HD_H,
                              SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    if (!window) {
        window = SDL_CreateWindow("Super Leo World HD: Dino Island",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  HD_W, HD_H, SDL_WINDOW_FULLSCREEN);
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    SDL_RenderSetLogicalSize(renderer, HD_W, HD_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    bake_hd_textures();

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;
    if (SDL_OpenAudio(&want, NULL) >= 0) SDL_PauseAudio(0);

    player.power = 1;
    player.dino = DINO_GREEN;
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
