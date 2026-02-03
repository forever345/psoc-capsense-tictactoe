#pragma once
#include <project.h>
#include "ws2812.h"
#include <stdint.h>

/* === Konfiguracja gry (możesz nadpisywać #define'ami przed #include "tictactoe.h") === */
/* Miganie kursora (liczone w „tikach” pętli – nie w ms) */
#ifndef TTT_CURSOR_PERIOD
#define TTT_CURSOR_PERIOD 10   // pełen cykl ON+OFF (np. 10 tików)
#endif
#ifndef TTT_CURSOR_DUTY
#define TTT_CURSOR_DUTY   5    // przez ile tików jest ON w jednym cyklu
#endif

/* Animacja zwycięstwa */
#ifndef TTT_WIN_PERIOD
#define TTT_WIN_PERIOD    50   // długość cyklu zwycięstwa (tiki)
#endif
#ifndef TTT_WIN_DUTY
#define TTT_WIN_DUTY      25   // ile tików ON w cyklu
#endif
#ifndef TTT_WIN_BLINKS
#define TTT_WIN_BLINKS     3   // ile pełnych „błysków” policzyć
#endif

/* Animacja remisu */
#ifndef TTT_DRAW_PERIOD
#define TTT_DRAW_PERIOD   40   // długość cyklu kroku remisu (tiki)
#endif
#ifndef TTT_DRAW_DUTY
#define TTT_DRAW_DUTY     20   // ile tików ON w kroku
#endif
#ifndef TTT_DRAW_STEPS
#define TTT_DRAW_STEPS     3   // liczba kroków: 3 = G→B→R; 6 = 2 cykle itd.
#endif

/* Jasność kolorów (0..255) */
#ifndef TTT_BRIGHT_O
#define TTT_BRIGHT_O      60   // niebieski (O)
#endif
#ifndef TTT_BRIGHT_X
#define TTT_BRIGHT_X      60   // czerwony (X)
#endif
#ifndef TTT_BRIGHT_G
#define TTT_BRIGHT_G      60   // zielony (remis/feedback itp.)
#endif
/* === Koniec konfiguracji === */

void game_reset(void);
void game_update(uint8_t press_rst, uint8_t press_ok, uint8_t press_h, uint8_t press_v);
void game_draw(void);

// NOWE: wizualizacja długiego resetu (mode: 0=off, 1=hold, 2=flash), level: 0..100
void game_reset_feedback(uint8_t mode, uint8_t level);

void game_idle_start(void);
uint8_t game_is_idle(void);  // 1 gdy gra w trybie idle, 0 gdy normalna