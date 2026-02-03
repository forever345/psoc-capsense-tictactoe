#pragma once
#include <project.h>
#include <stdint.h>

#define LED_COUNT 9  // 9 pól gry

typedef struct { uint8_t g, r, b; } color_grb_t; // WS2812 = GRB

void ws2812_init(void);
void ws2812_fill(color_grb_t c);
void ws2812_show(void);

void ws2812_set_led(uint8_t idx, color_grb_t c);                    // ustaw 1 LED (GRB)
void ws2812_set_rgb(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);  // ustaw 1 LED (RGB)
void ws2812_clear(void);                                            // wyczyść bufor na czarno