#include <project.h>
#include "ws2812.h"
#include "tictactoe.h"

static uint8_t autorepeat(uint8_t raw, uint8_t *st, uint8_t *cnt, uint16_t *hold);
enum { DEB = 3, RST_HOLD_TICKS = 60 };
static uint16_t hold_h = 0, hold_v = 0;

enum { IDLE_TIMEOUT_TICKS = 2400 }; // ~2 min (stroić)
static uint32_t idle_ticks = 0;

int main(void)
{
    CyGlobalIntEnable;

    CapSense_CSD_TunerStart();
    CapSense_CSD_EnableWidget(CapSense_CSD_SENSOR_PROXIMITYSENSOR0_0__PROX); // Reset
    CapSense_CSD_EnableWidget(CapSense_CSD_SENSOR_PROXIMITYSENSOR1_0__PROX); // Confirm
    CapSense_CSD_EnableWidget(CapSense_CSD_SENSOR_PROXIMITYSENSOR2_0__PROX); // Move prev
    CapSense_CSD_EnableWidget(CapSense_CSD_SENSOR_PROXIMITYSENSOR3_0__PROX); // Move next
    
    //while(1u){CapSense_CSD_TunerComm();} SENSOR DEBUG Comment all below
    
    ws2812_init();
    CapSense_CSD_InitializeAllBaselines();
    game_reset();
    game_idle_start();                  // start od razu w IDLE
    CapSense_CSD_ScanEnabledWidgets();
    
    static uint8_t  st_h=0, st_v=0, st_ok=0, st_rst=0;
    static uint8_t  cnt_h=0, cnt_v=0, cnt_ok=0, cnt_rst=0;
    static uint16_t rst_hold=0;
    static uint8_t  rst_latched=0;
    static uint8_t input_quiet = 0;

    while(1u) {
        if (CapSense_CSD_IsBusy()) continue;

        CapSense_CSD_UpdateEnabledBaselines();
        
        uint8_t raw_rst = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_SENSOR_PROXIMITYSENSOR0_0__PROX) ? 1u : 0u;
        uint8_t raw_ok  = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_SENSOR_PROXIMITYSENSOR1_0__PROX) ? 1u : 0u;
        uint8_t raw_v   = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_SENSOR_PROXIMITYSENSOR2_0__PROX) ? 1u : 0u;
        uint8_t raw_h   = CapSense_CSD_CheckIsWidgetActive(CapSense_CSD_SENSOR_PROXIMITYSENSOR3_0__PROX) ? 1u : 0u;

        uint8_t raw_any = (raw_rst | raw_ok | raw_h | raw_v);

        // --- Wybudzanie z IDLE: pierwsze dotknięcie ignorujemy, tylko start gry ---
        if (game_is_idle() && raw_any) {
            game_reset();                   // wyjście z idle i czysta plansza
            game_draw();
            idle_ticks = 0;
            CapSense_CSD_ScanEnabledWidgets();
            continue;
        }

        // --- Zliczanie bezczynności do wejścia w IDLE ---
        if (!game_is_idle()) {
            if (raw_any) idle_ticks = 0;
            else if (idle_ticks < 0x7FFFFFFF) idle_ticks++;

            if (idle_ticks >= IDLE_TIMEOUT_TICKS) {
                game_idle_start();
                game_draw();
                CapSense_CSD_ScanEnabledWidgets();
                continue;
            }
        }
        
        if (input_quiet) {
            input_quiet--;

            // podtrzymaj brak wejść i czyść liczniki, żeby nic się nie „zatrzaskiwało”
            st_h = st_v = st_ok = st_rst = 0;
            cnt_h = cnt_v = cnt_ok = cnt_rst = 0;
            hold_h = hold_v = 0;
            rst_hold = 0;

            game_update(0, 0, 0, 0);
            game_draw();
            CapSense_CSD_ScanEnabledWidgets();
            continue;
        }

        // debounce
        if (raw_rst != st_rst) { cnt_rst = 0; st_rst = raw_rst; } else if (cnt_rst < 255) cnt_rst++;
        if (raw_ok  != st_ok ) { cnt_ok  = 0; st_ok  = raw_ok;  } else if (cnt_ok  < 255) cnt_ok++;
        if (raw_h   != st_h  ) { cnt_h   = 0; st_h   = raw_h;   } else if (cnt_h   < 255) cnt_h++;
        if (raw_v   != st_v  ) { cnt_v   = 0; st_v   = raw_v;   } else if (cnt_v   < 255) cnt_v++;

        uint8_t press_ok = (st_ok == 1u && cnt_ok == DEB);
        uint8_t press_h  = autorepeat(raw_h, &st_h, &cnt_h, &hold_h);
        uint8_t press_v  = autorepeat(raw_v, &st_v, &cnt_v, &hold_v);

        // długi RESET (tylko tu wyzwalamy reset)
        if (st_rst == 1u && cnt_rst >= DEB) {
            if (rst_hold < 1000) rst_hold++;

            if (!rst_latched && rst_hold >= RST_HOLD_TICKS) {
                // flash pełną zielenią i właściwy reset
                game_reset_feedback(2, 100);
                game_draw();

                game_update(1, 0, 0, 0);   // reset=1
                game_draw();
                
                // >>> DODANE:  krótka blokada wejść i wyczyszczenie stanów
                input_quiet = 8;           // ~kilka obiegów pętli; dostrój jeśli trzeba
                st_h = st_v = st_ok = st_rst = 0;
                cnt_h = cnt_v = cnt_ok = cnt_rst = 0;
                hold_h = hold_v = 0;
                rst_hold = 0;
                
                rst_latched = 1;
                CapSense_CSD_ScanEnabledWidgets();
                continue;                  // <<< ważne
            } else {
                // progres: rosnąca zieleń 0..100
                uint8_t lvl = (uint8_t)((uint32_t)rst_hold * 100u / RST_HOLD_TICKS);
                if (lvl > 100u) lvl = 100u;
                game_reset_feedback(1, lvl);
                game_draw();

                CapSense_CSD_ScanEnabledWidgets();
                continue;                  // <<< ważne: nie rysuj zwykłej planszy w tym cyklu
            }
        } else {
            rst_hold = 0;
            rst_latched = 0;
            game_reset_feedback(0, 0);     // feedback off
        }

        // normalna logika (bez krótkiego resetu)
        game_update(0, press_ok, press_h, press_v);
        game_draw();
        CapSense_CSD_ScanEnabledWidgets();
        // CapSense_CSD_TunerComm();
    }
}

// Auto-repeat dla przycisków H/V: pierwsze kliknięcie po DEB,
// potem po przytrzymaniu FIRST_DELAY co NEXT_DELAY generuje kolejny "klik".
static uint8_t autorepeat(uint8_t raw, uint8_t *st, uint8_t *cnt, uint16_t *hold)
{
    enum { FIRST_DELAY = 25, NEXT_DELAY = 20 };  // dopasuj do tempa pętli

    // debounce + detekcja zmiany
    if (raw != *st) { *st = raw; *cnt = 0; *hold = 0; }
    else if (*cnt < 255) (*cnt)++;

    // pierwszy impuls po debouncie
    if (*st == 1u && *cnt == DEB) { *hold = 0; return 1u; }

    // powtarzanie podczas trzymania
    if (*st == 1u && *cnt > DEB) {
        (*hold)++;
        if (*hold == FIRST_DELAY) return 1u;
        if (*hold > FIRST_DELAY && ((*hold - FIRST_DELAY) % NEXT_DELAY) == 0) return 1u;
    }
    return 0u;
}