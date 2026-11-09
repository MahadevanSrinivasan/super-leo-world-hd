/*
 * Super Leo World: Dino Island (Native RetroPie / SDL 1.2 Widescreen Edition)
 * 16:9 Widescreen (480x270 -> hardware scaled to 1280x720 HDMI at 60 FPS on Raspberry Pi 1)
 * Features:
 *  - 4 Rideable Dino Companions hatched from Spotted Egg Blocks:
 *      1. Emerald Rex (Green): Chameleon Tongue-Grab, Swallow & Spit Shells, Flutter-Jump Hover
 *      2. Blaze Rex (Red): Spits any swallowed enemy/shell as a 3-Way Spread of Giant Fireballs
 *      3. Sky Rex (Blue): Feathered Wings — up to 4 mid-air Wing Flaps
 *      4. Quake Rex (Yellow): Expanding Golden Seismic Shockwave on landings & Ground-Pounds
 *  - Modern Mario Moveset: Wall-Slide & Wall-Jump, Ground-Pound, Dino Bail-Out Save Jump
 *  - 8 Stages across 4 Worlds (1-1 through 4-2) with Tricera-King & King Tyrannor Bosses
 *  - Built-in SDL 1.2 Chiptune Sound Effects over HDMI
 */

#include <SDL/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 480
#define H 270
#define TILE 16
#define MAP_W 185
#define MAP_H 17
#define GRAVITY 0.17f

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
    Uint32 color;
    int life, max_life;
} Particle;

typedef struct {
    int active;
    float x, y, r, max_r;
    Uint32 color;
} Shockwave;

static SDL_Surface *screen = NULL;
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

/* Drawing helpers */
static Uint32 rgb(int r, int g, int b) {
    return SDL_MapRGB(screen->format, (Uint8)r, (Uint8)g, (Uint8)b);
}

static void fill_rect(int x, int y, int w, int h, Uint32 col) {
    if (x >= W || y >= H || x + w <= 0 || y + h <= 0) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w > W) ? W : (x + w);
    int y2 = (y + h > H) ? H : (y + h);
    SDL_Rect r = {(Sint16)x1, (Sint16)y1, (Uint16)(x2 - x1), (Uint16)(y2 - y1)};
    SDL_FillRect(screen, &r, col);
}

static void fill_circle(int cx, int cy, int rad, Uint32 col) {
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrtf((float)(rad * rad - dy * dy));
        fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, col);
    }
}

/* 5x7 Pixel Font */
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

static void draw_text(int x, int y, const char *s, Uint32 col, int scale) {
    int cx = x;
    while (*s) {
        char c = *s++;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 32 && c < 128) {
            const unsigned char *g = font5x7[c - 32];
            for (int r = 0; r < 7; r++) {
                for (int col_i = 0; col_i < 5; col_i++) {
                    if (g[r] & (1 << (4 - col_i))) {
                        fill_rect(cx + col_i * scale, y + r * scale, scale, scale, col);
                    }
                }
            }
        }
        cx += 6 * scale;
    }
}

static void show_banner(const char *msg, int dur) {
    snprintf(banner_text, sizeof(banner_text), "%s", msg);
    banner_timer = dur;
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

static void get_dino_rgb(int d, int *r, int *g, int *b) {
    switch (d) {
        case DINO_RED:    *r = 239; *g = 68;  *b = 68;  break;
        case DINO_BLUE:   *r = 59;  *g = 130; *b = 246; break;
        case DINO_YELLOW: *r = 245; *g = 158; *b = 11;  break;
        default:          *r = 16;  *g = 185; *b = 129; break;
    }
}

static void spawn_particle(float x, float y, float vx, float vy, Uint32 col, int life) {
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
            shockwaves[i].r = 4.0f;
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

static Uint8 keys_held[SDLK_LAST];
static Uint8 prev_held[SDLK_LAST];

static void update_game(void) {
    global_tick++;
    if (banner_timer > 0) banner_timer--;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (bump_off[y][x] > 0) bump_off[y][x]--;

    int k_left   = keys_held[SDLK_LEFT]  || keys_held[SDLK_a];
    int k_right  = keys_held[SDLK_RIGHT] || keys_held[SDLK_d];
    int k_down   = keys_held[SDLK_DOWN]  || keys_held[SDLK_s];
    int k_jump   = keys_held[SDLK_SPACE] || keys_held[SDLK_UP] || keys_held[SDLK_w];
    int p_jump   = prev_held[SDLK_SPACE] || prev_held[SDLK_UP] || prev_held[SDLK_w];
    int k_tongue = keys_held[SDLK_z] || keys_held[SDLK_LSHIFT] || keys_held[SDLK_RSHIFT] || keys_held[SDLK_j];
    int p_tongue = prev_held[SDLK_z] || prev_held[SDLK_LSHIFT] || prev_held[SDLK_RSHIFT] || prev_held[SDLK_j];
    int k_bail   = keys_held[SDLK_x] || keys_held[SDLK_e] || keys_held[SDLK_LCTRL];
    int p_bail   = prev_held[SDLK_x] || prev_held[SDLK_e] || prev_held[SDLK_LCTRL];

    if (game_state == 0 || game_state == 3) {
        if ((keys_held[SDLK_LEFT] && !prev_held[SDLK_LEFT])) {
            current_stage = (current_stage + 7) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if ((keys_held[SDLK_RIGHT] && !prev_held[SDLK_RIGHT])) {
            current_stage = (current_stage + 1) % 8;
            load_stage(current_stage, 0);
            play_sfx(SFX_COIN);
        }
        if ((keys_held[SDLK_RETURN] && !prev_held[SDLK_RETURN]) || (k_jump && !p_jump)) {
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

    if (!player.on_ground && k_down && !prev_held[SDLK_DOWN] && !player.ground_pounding) {
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

static void render_dino_sprite(int cx, int by, int dir, int dino_id, float anim, int mouth_full, float tongue_len) {
    int r, g, b;
    get_dino_rgb(dino_id, &r, &g, &b);
    Uint32 body = rgb(r, g, b);
    Uint32 belly = rgb(220, 252, 231);
    int s = (dir >= 0) ? 1 : -1;

    if (tongue_len > 0) {
        int tx = (s > 0) ? (cx + 6) : (cx - 6 - (int)tongue_len);
        fill_rect(tx, by - 10, (int)tongue_len, 3, rgb(251, 113, 133));
        fill_circle(cx + s * (6 + (int)tongue_len), by - 9, 3, rgb(244, 63, 94));
    }
    if (dino_id == DINO_BLUE) {
        fill_circle(cx - s * 4, by - 11, 5, rgb(248, 250, 252));
    }
    int leg = (int)(sinf(anim * 2.2f) * 2.0f);
    fill_rect(cx - s * 7, by - 6, 5, 3, body); /* tail */
    fill_circle(cx, by - 6, 6, body);
    fill_circle(cx + s * 2, by - 5, 4, belly);
    fill_rect(cx - 3 + leg, by - 3, 3, 3, body);
    fill_rect(cx + 1 - leg, by - 3, 3, 3, body);
    fill_circle(cx + s * 4, by - 12, 4, body);
    fill_circle(cx + s * 8, by - 11, mouth_full ? 5 : 4, body);
    fill_rect(cx + s * 4, by - 14, 2, 2, rgb(255, 255, 255));
}

static void render_frame(void) {
    const StageDef *st = &STAGES[current_stage];
    int cam_x = (int)(camera_x + 0.5f);
    /* Gradient sky */
    for (int y = 0; y < H; y += 2) {
        float t = (float)y / (float)H;
        int r = (int)(st->sky_top[0] + (st->sky_bot[0] - st->sky_top[0]) * t);
        int g = (int)(st->sky_top[1] + (st->sky_bot[1] - st->sky_top[1]) * t);
        int b = (int)(st->sky_top[2] + (st->sky_bot[2] - st->sky_top[2]) * t);
        fill_rect(0, y, W, 2, rgb(r, g, b));
    }

    /* Parallax hills */
    Uint32 hf = rgb(st->hill_far[0], st->hill_far[1], st->hill_far[2]);
    Uint32 hn = rgb(st->hill_near[0], st->hill_near[1], st->hill_near[2]);
    for (int i = -1; i < 6; i++) {
        int hx = i * 110 - ((int)(cam_x * 0.2f) % 110);
        fill_circle(hx + 55, H - 30, 58, hf);
        int nx = i * 90 - ((int)(cam_x * 0.35f) % 90);
        fill_circle(nx + 45, H - 15, 42, hn);
    }

    /* Tiles */
    int c1 = (cam_x / TILE) - 1;
    int c2 = ((cam_x + W) / TILE) + 1;
    if (c1 < 0) c1 = 0;
    if (c2 >= MAP_W) c2 = MAP_W - 1;

    Uint32 g_top = rgb(st->gnd_top[0], st->gnd_top[1], st->gnd_top[2]);
    Uint32 g_bod = rgb(st->gnd_body[0], st->gnd_body[1], st->gnd_body[2]);

    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = c1; tx <= c2; tx++) {
            char c = map[ty][tx];
            if (c == '.') continue;
            int sx = tx * TILE - cam_x;
            int sy = ty * TILE - bump_off[ty][tx];
            if (c == '#' || c == 'I') {
                fill_rect(sx, sy, TILE, TILE, (c == 'I') ? rgb(2, 132, 199) : g_bod);
                if (ty == 0 || !is_solid(get_tile(tx, ty - 1))) {
                    fill_rect(sx, sy, TILE, 4, (c == 'I') ? rgb(224, 242, 254) : g_top);
                }
            } else if (c == 'B') {
                fill_rect(sx + 1, sy + 1, TILE - 2, TILE - 2, rgb(217, 119, 6));
            } else if (c == '?' || c == 'M') {
                fill_rect(sx + 1, sy + 1, TILE - 2, TILE - 2, rgb(250, 204, 21));
                draw_text(sx + 5, sy + 4, "?", rgb(113, 63, 18), 1);
            } else if (c == 'E' || c == 'D') {
                int r, g, b;
                get_dino_rgb((c == 'E') ? st->default_dino : st->second_dino, &r, &g, &b);
                fill_rect(sx + 1, sy + 1, TILE - 2, TILE - 2, rgb(248, 250, 252));
                fill_circle(sx + 5, sy + 6, 2, rgb(r, g, b));
                fill_circle(sx + 11, sy + 10, 2, rgb(r, g, b));
            } else if (c == 'U') {
                fill_rect(sx + 1, sy + 1, TILE - 2, TILE - 2, rgb(71, 85, 105));
            } else if (c == 'P') {
                fill_rect(sx, sy, TILE, TILE, rgb(16, 185, 129));
            } else if (c == 'F') {
                fill_circle(sx + 8, sy + 9, 6, rgb(21, 128, 61));
                fill_circle(sx + 6, sy + 7, 2, rgb(244, 63, 94));
                fill_circle(sx + 10, sy + 9, 2, rgb(244, 63, 94));
            } else if (c == 'C') {
                fill_circle(sx + 8, sy + 8, 4, rgb(253, 224, 71));
            } else if (c == 'J') {
                fill_rect(sx + 2, sy + 6, TILE - 4, 4, rgb(239, 68, 68));
                fill_rect(sx + 4, sy + 10, TILE - 8, 6, rgb(253, 224, 71));
            } else if (c == 'K') {
                fill_rect(sx + 7, sy - 8, 2, 24, rgb(248, 250, 252));
                fill_rect(sx + 9, sy - 8, 8, 6, rgb(59, 130, 246));
            } else if (c == 'G') {
                fill_rect(sx + 7, sy, 2, TILE, rgb(248, 250, 252));
                if (ty == 4) fill_rect(sx + 9, sy + 2, 14, 8, rgb(16, 185, 129));
            }
        }
    }

    /* Entities */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active) continue;
        int sx = (int)(e->x + 0.5f) - cam_x;
        int sy = (int)(e->y + 0.5f);
        if (e->type == ENT_PLATFORM) {
            fill_rect(sx, sy, e->w, e->h, rgb(251, 191, 36));
        } else if (e->type == ENT_DINO_MOUNT) {
            render_dino_sprite(sx + 8, sy + e->h, e->dir, e->dino_type, global_tick * 0.1f, 0, 0);
        } else if (e->type == ENT_BOSS) {
            fill_rect(sx, sy, e->w, e->h, (e->invuln % 4 > 1) ? rgb(255, 255, 255) : rgb(220, 38, 38));
            fill_rect(sx + 4, sy - 6, e->w - 8, 4, rgb(251, 191, 36));
        } else {
            Uint32 ec = (e->type == ENT_KOOPA || e->type == ENT_SHELL) ? rgb(34, 197, 94) :
                        (e->type == ENT_FLYER) ? rgb(236, 72, 153) : rgb(168, 85, 247);
            fill_rect(sx, sy, e->w, e->h, ec);
        }
    }

    /* Fireballs & Shockwaves */
    for (int i = 0; i < MAX_FIREBALLS; i++) {
        if (fireballs[i].active) {
            fill_circle((int)(fireballs[i].x + 0.5f) - cam_x, (int)(fireballs[i].y + 0.5f), fireballs[i].r, rgb(239, 68, 68));
        }
    }
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (shockwaves[i].active) {
            int r = (int)shockwaves[i].r;
            int sx = (int)(shockwaves[i].x + 0.5f) - cam_x;
            int sy = (int)(shockwaves[i].y + 0.5f);
            fill_rect(sx - r, sy - 2, 4, 4, shockwaves[i].color);
            fill_rect(sx + r, sy - 2, 4, 4, shockwaves[i].color);
        }
    }

    /* Player & Rideable Dino */
    if (!(player.invuln > 0 && (player.invuln / 4) % 2 == 1)) {
        int px = (int)(player.x + player.w / 2 + 0.5f) - cam_x;
        int py = (int)(player.y + player.h + 0.5f);
        int leo_y = player.dino ? (py - 8) : py;
        Uint32 tunic = (player.star_timer > 0 && (global_tick % 4 < 2)) ? rgb(253, 224, 71) :
                       (player.power == 2) ? rgb(248, 250, 252) : rgb(239, 68, 68);
        fill_rect(px - 4, leo_y - 10, 8, 7, tunic);
        fill_circle(px, leo_y - 12, 3, rgb(254, 215, 170));
        fill_rect(px - 4, leo_y - 15, 9, 2, tunic);
        if (player.dino) {
            render_dino_sprite(px, py, player.dir, player.dino, player.anim_frame, player.dino_mouth > 0, player.tongue_len);
        }
    }

    /* HUD */
    fill_rect(6, 4, W - 12, 16, rgb(15, 23, 42));
    char hud[128];
    snprintf(hud, sizeof(hud), "LEO X%d  COINS:%02d  SCORE:%06d  %s  DINO:%s",
             lives, coins, score, st->code, dino_name(player.dino));
    draw_text(12, 8, hud, rgb(253, 224, 71), 1);

    if (banner_timer > 0) {
        fill_rect(W / 2 - 140, 24, 280, 14, rgb(15, 23, 42));
        draw_text(W / 2 - 130, 28, banner_text, rgb(52, 211, 153), 1);
    }

    if (game_state == 0 || game_state == 3) {
        fill_rect(W / 2 - 170, H / 2 - 65, 340, 130, rgb(15, 23, 42));
        draw_text(W / 2 - 140, H / 2 - 50, "SUPER LEO WORLD: DINO ISLAND", rgb(253, 224, 71), 2);
        draw_text(W / 2 - 125, H / 2 - 20, "4 RIDEABLE DINOS - 8 STAGES (1-1 TO 4-2)", rgb(52, 211, 153), 1);
        draw_text(W / 2 - 135, H / 2 + 0,  "GREEN:TONGUE/FLUTTER  RED:3X FIREBALLS", rgb(248, 250, 252), 1);
        draw_text(W / 2 - 135, H / 2 + 14, "BLUE:WINGED FLIGHT    YELLOW:QUAKE STOMP", rgb(248, 250, 252), 1);
        char sel[64];
        snprintf(sel, sizeof(sel), "< STAGE %s: %s >", st->code, st->title);
        draw_text(W / 2 - 115, H / 2 + 32, sel, rgb(56, 189, 248), 1);
        draw_text(W / 2 - 110, H / 2 + 48, "PRESS ENTER OR SPACE TO START!", rgb(251, 191, 36), 1);
    }

    SDL_Flip(screen);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    SDL_ShowCursor(SDL_DISABLE);
    screen = SDL_SetVideoMode(W, H, 32, SDL_SWSURFACE | SDL_FULLSCREEN);
    if (!screen) screen = SDL_SetVideoMode(W, H, 32, SDL_SWSURFACE);

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
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    if (game_state == 0) running = 0;
                    else { game_state = 0; load_stage(current_stage, 0); }
                }
                if (ev.key.keysym.sym < SDLK_LAST) keys_held[ev.key.keysym.sym] = 1;
            } else if (ev.type == SDL_KEYUP) {
                if (ev.key.keysym.sym < SDLK_LAST) keys_held[ev.key.keysym.sym] = 0;
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
