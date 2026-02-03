#include "tictactoe.h"
#include <stdbool.h>

/* Kolory i gracze */
typedef enum { P_NONE=0, P_O=1, P_X=2 } player_t;
static const color_grb_t COL_EMPTY = (color_grb_t){0, 0, 0};
static const color_grb_t COL_O     = (color_grb_t){0, 0, TTT_BRIGHT_O};  // niebieski (B)
static const color_grb_t COL_X     = (color_grb_t){0, TTT_BRIGHT_X, 0};  // czerwony (R)
static const color_grb_t COL_G     = (color_grb_t){TTT_BRIGHT_G, 0, 0};  // zielony  (G)

/* Stany gry */
typedef enum { GS_SELECT, GS_WIN_ANIM, GS_DRAW_ANIM, GS_IDLE } game_state_t;

/* Idle – fazy */
typedef enum {
    IP_SNAKE_R, IP_SNAKE_G, IP_SNAKE_B,     // wężyk 2-LED: czerwony, zielony, niebieski
    IP_RING_R,  IP_X_R,                      // kółko R, krzyżyk R
    IP_RING_G,  IP_X_G,                      // kółko G, krzyżyk G
    IP_RING_B,  IP_X_B                       // kółko B, krzyżyk B
} idle_phase_t;

static idle_phase_t idle_phase = IP_SNAKE_R;
static uint8_t  idle_idx = 0;     // pozycja głowy wężyka (0..8)
static uint16_t idle_cnt = 0;     // licznik kroków
enum { IDLE_SNAKE_STEP = 8, IDLE_HOLD = 60 };  // dostrój pod swoją pętlę

/* Plansza i stan */
static player_t  board[9];
static uint8_t   cursor_idx = 4;
static player_t  current_player = P_O;
static game_state_t gstate = GS_SELECT;

/* Wizualizacja długiego resetu */
static uint8_t reset_fb_mode  = 0;   // 0=off, 1=hold, 2=flash (jedna klatka)
static uint8_t reset_fb_level = 0;   // 0..100

/* Indeksy 0..8 (3x3) */
static inline uint8_t idx_right(uint8_t i) {
    uint8_t r = i / 3, c = i % 3;
    c = (uint8_t)((c + 1) % 3);
    return (uint8_t)(r * 3 + c);
}
static inline uint8_t idx_down(uint8_t i) {
    uint8_t r = i / 3, c = i % 3;
    r = (uint8_t)((r + 1) % 3);
    return (uint8_t)(r * 3 + c);
}

static bool board_full(void) {
    for (uint8_t i=0;i<9;i++) if (board[i]==P_NONE) return false;
    return true;
}

static inline uint8_t idx_next_col_major(uint8_t i) {
    uint8_t r = i / 3, c = i % 3;
    r++;
    if (r >= 3) { r = 0; c = (uint8_t)((c + 1) % 3); }
    return (uint8_t)(r * 3 + c);
}

/* Kolejny indeks w porządku wierszowym i skok na najbliższe puste */
static inline uint8_t idx_next_row_major(uint8_t i) {
    uint8_t r = i / 3, c = i % 3;
    if (++c >= 3) { c = 0; r = (uint8_t)((r + 1) % 3); }
    return (uint8_t)(r * 3 + c);
}

// poprzedni indeks w porządku wierszowym (0..8)
static inline uint8_t idx_prev_row_major(uint8_t i) {
    uint8_t r = i / 3, c = i % 3;
    if (c == 0) { c = 2; r = (r == 0) ? 2 : (uint8_t)(r - 1); }
    else c--;
    return (uint8_t)(r * 3 + c);
}

// skocz do najbliższego PUSTEGO pola wstecz
static void cursor_prev_free_row_major(void) {
    for (uint8_t n = 0; n < 9; n++) {
        cursor_idx = idx_prev_row_major(cursor_idx);
        if (board[cursor_idx] == P_NONE) break;
    }
}

// pierwszy wolny indeks 0..8; jeśli exclude!=255, pomija ten indeks
static int8_t first_free_index_excl(uint8_t exclude)
{
    for (uint8_t i = 0; i < 9; i++) {
        if (board[i] == P_NONE && i != exclude) return (int8_t)i;
    }
    return -1; // brak
}

static void cursor_move_horiz(void)
{
    uint8_t start = cursor_idx;
    for (uint8_t n = 0; n < 9; n++) {
        // zamiast zawijać w tym samym wierszu -> idź row-major
        cursor_idx = idx_next_row_major(cursor_idx);
        if (board[cursor_idx] == P_NONE) return;
        if (cursor_idx == start) break;  // wróciliśmy do początku
    }
}

static void cursor_move_vert(void)
{
    uint8_t start = cursor_idx;
    for (uint8_t n = 0; n < 9; n++) {
        cursor_idx = idx_next_col_major(cursor_idx);
        if (board[cursor_idx] == P_NONE) return;
        if (cursor_idx == start) break;
    }
}

static void cursor_next_free_row_major(void) {
    for (uint8_t n = 0; n < 9; n++) {
        cursor_idx = idx_next_row_major(cursor_idx);
        if (board[cursor_idx] == P_NONE) break;
    }
}

/* Dyskretne poziomy zieleni (GRB: G) do feedbacku resetu */
static inline uint8_t green_step_from_level(uint8_t level_0_100)
{
    static const uint8_t lut[] = { 0, 2, 3, 8, 16, 32, 64, 128 };
    enum { N = sizeof(lut) / sizeof(lut[0]) };
    uint16_t idx = (uint16_t)level_0_100 * (N - 1) / 100u;
    if (idx >= N) idx = N - 1;
    return lut[idx];
}

/* Linie wygrywające */
static const uint8_t LINES[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
};

/* Mruganie kursora */
static uint16_t blink_counter = 0;
static inline bool blink_on(void) {
    if (++blink_counter >= TTT_CURSOR_PERIOD) blink_counter = 0;
    return blink_counter < TTT_CURSOR_DUTY;
}

/* Animacje: zwycięstwo */
static uint16_t win_mask = 0;
static player_t winner   = P_NONE;
static uint8_t  win_blinks_left = 0;
static uint16_t win_cnt = 0;
static uint8_t  win_on  = 0;
static uint8_t  win_skip_first = 0;

/* Animacje: remis */
static uint8_t  draw_step = 0;
static uint16_t draw_cnt  = 0;
static uint8_t  draw_on   = 0;

/* Pomocnicze */
static inline color_grb_t color_of(player_t p) {
    return (p==P_O) ? COL_O : (p==P_X) ? COL_X : COL_EMPTY;
}
static inline color_grb_t draw_color_for(uint8_t step) {
    uint8_t k = step % 3; // 0=G, 1=B, 2=R
    return (k==0) ? COL_G : (k==1) ? COL_O : COL_X;
}
static uint16_t win_mask_for(player_t p) {
    for (uint8_t k=0;k<8;k++) {
        uint8_t a=LINES[k][0], b=LINES[k][1], c=LINES[k][2];
        if (board[a]==p && board[b]==p && board[c]==p) return (uint16_t)((1u<<a)|(1u<<b)|(1u<<c));
    }
    return 0;
}

/* API */
void game_reset_feedback(uint8_t mode, uint8_t level)
{
    reset_fb_mode  = mode;
    reset_fb_level = level;
}

void game_reset(void) {
    reset_fb_mode  = 0;
    reset_fb_level = 0;

    for (uint8_t i=0;i<9;i++) board[i]=P_NONE;
    cursor_idx = 4;
    current_player = P_O;
    gstate = GS_SELECT;
    ws2812_clear();
    ws2812_show();
}

void game_update(uint8_t press_rst, uint8_t press_ok, uint8_t press_h, uint8_t press_v)
{
    // --- Tryb IDLE: animujemy i nie reagujemy na przyciski (wybudzanie robi main) ---
    if (gstate == GS_IDLE) {
        // taktowanie: wężyk krok co IDLE_SNAKE_STEP; kółko/X trzymane IDLE_HOLD
        uint16_t limit = (idle_phase <= IP_SNAKE_B) ? IDLE_SNAKE_STEP : IDLE_HOLD;
        if (++idle_cnt >= limit) {
            idle_cnt = 0;
            if (idle_phase <= IP_SNAKE_B) {
                // wężyk 2-LED: przesuwaj głowę
                idle_idx = (uint8_t)((idle_idx + 1) % 9);
                if (idle_idx == 0) idle_phase = (idle_phase + 1);  // po pełnym kółku zmień kolor
            } else {
                // kółko / X: po czasie skacz do następnej fazy
                idle_phase = (idle_phase + 1);
                if (idle_phase > IP_X_B) {
                    idle_phase = IP_SNAKE_R;  // restart całej sekwencji
                    idle_idx = 0;
                }
            }
        }
        return; // w idle nie reagujemy na przyciski (wybudza main)
    }

    // --- reszta Twojej dotychczasowej logiki (RESET/SELECT/WIN/DRAW) ---
    if (press_rst) { game_reset(); return; }

    if (gstate == GS_SELECT) {
        if (press_h) cursor_next_free_row_major();   // H = następny
        if (press_v) cursor_prev_free_row_major();   // V = poprzedni

        if (press_ok && board[cursor_idx] == P_NONE) {
            board[cursor_idx] = current_player;
            uint16_t wm = win_mask_for(current_player);
            if (wm) {
                winner = current_player;
                win_mask = wm;
                win_blinks_left = TTT_WIN_BLINKS;
                win_cnt = 0; win_on = 0; win_skip_first = 1;
                gstate = GS_WIN_ANIM;
            } else if (board_full()) {
                draw_step = 0; draw_cnt = 0; draw_on = 0;
                gstate = GS_DRAW_ANIM;
            } else {
                current_player = (current_player==P_O) ? P_X : P_O;
                cursor_next_free_row_major();
            }
        }
    }
    else if (gstate == GS_WIN_ANIM) {
        if (++win_cnt >= TTT_WIN_PERIOD) win_cnt = 0;
        uint8_t new_on = (win_cnt < TTT_WIN_DUTY) ? 1u : 0u;
        static uint8_t prev_on = 0u;
        if (new_on && !prev_on) {
            if (win_skip_first) win_skip_first = 0;
            else if (win_blinks_left && --win_blinks_left == 0) { game_reset(); return; }
        }
        prev_on = new_on; win_on = new_on;
    }
    else if (gstate == GS_DRAW_ANIM) {
        if (++draw_cnt >= TTT_DRAW_PERIOD) draw_cnt = 0;
        uint8_t new_on = (draw_cnt < TTT_DRAW_DUTY) ? 1u : 0u;
        static uint8_t prev_on_draw = 0u;
        if (!new_on && prev_on_draw) {
            if (++draw_step >= TTT_DRAW_STEPS) { game_reset(); return; }
        }
        prev_on_draw = new_on; draw_on = new_on;
    }
}

void game_draw(void) {
    /* Feedback resetu – absolutny priorytet */
    if (reset_fb_mode) {
        uint8_t gval = green_step_from_level(reset_fb_level);
        ws2812_fill((color_grb_t){ gval, 0, 0 });
        ws2812_show();
        if (reset_fb_mode == 2) reset_fb_mode = 0;
        return;
    }

    // --- Rysowanie trybu IDLE ---
    if (gstate == GS_IDLE) {
        ws2812_clear();

        // wybór koloru wg fazy
        color_grb_t col;
        switch (idle_phase) {
            case IP_SNAKE_R: case IP_RING_R: case IP_X_R: col = COL_X; break; // czerwony
            case IP_SNAKE_G: case IP_RING_G: case IP_X_G: col = COL_G; break; // zielony
            default:                                        col = COL_O; break; // niebieski
        }

        if (idle_phase <= IP_SNAKE_B) {
            // WĘŻYK 2-LED: zapal idle_idx oraz kolejny
            ws2812_set_led(idle_idx, col);
            ws2812_set_led((uint8_t)((idle_idx + 1) % 9), col);
        } else if (idle_phase == IP_RING_R || idle_phase == IP_RING_G || idle_phase == IP_RING_B) {
            // KÓŁKO: wszystkie poza środkiem (4)
            for (uint8_t i=0;i<9;i++) if (i != 4) ws2812_set_led(i, col);
        } else {
            // KRZYŻYK: diagonale + środek
            const uint8_t Xmask[5] = {0, 2, 4, 6, 8};
            for (uint8_t k=0;k<5;k++) ws2812_set_led(Xmask[k], col);
        }

        ws2812_show();
        return;
    }
    /* Normalne rysowanie */
    ws2812_clear();

    if (gstate == GS_WIN_ANIM) {
        color_grb_t wcol = color_of(winner);
        for (uint8_t i=0;i<9;i++)
            if (win_mask & (1u<<i))
                ws2812_set_led(i, win_on ? wcol : COL_EMPTY);
    }
    else if (gstate == GS_DRAW_ANIM) {
        if (draw_on) {
            color_grb_t c = draw_color_for(draw_step);
            ws2812_fill(c);
        }
    }
    else { // GS_SELECT
        for (uint8_t i=0;i<9;i++)
            if (board[i]!=P_NONE)
                ws2812_set_led(i, color_of(board[i]));
        if (board[cursor_idx]==P_NONE) {
            ws2812_set_led(cursor_idx, blink_on() ? color_of(current_player) : COL_EMPTY);
        }
    }

    ws2812_show();
}

void game_idle_start(void)
{
    // nie czyścimy planszy na siłę – idle rysuje własne klatki
    gstate = GS_IDLE;
    idle_phase = IP_SNAKE_R;
    idle_idx = 0;
    idle_cnt = 0;
    reset_fb_mode = 0;  // pewność: brak feedbacku resetu w idle
    reset_fb_level = 0;
}

uint8_t game_is_idle(void)
{
    return (gstate == GS_IDLE) ? 1u : 0u;
}