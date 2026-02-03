#include "ws2812.h"

static uint8_t encode_byte[256][3];
static uint8_t spi_buf[LED_COUNT * 9]; // 3 kolory × 3 bajty po enkodowaniu = 9 B/LED

static inline void encode_color(uint8_t v, uint8_t *dst3) {
    dst3[0] = encode_byte[v][0];
    dst3[1] = encode_byte[v][1];
    dst3[2] = encode_byte[v][2];
}

void ws2818_build_lut(void) {
    for (uint16_t v = 0; v < 256; ++v) {
        uint32_t out = 0;
        for (int i = 7; i >= 0; --i) {
            // bit '0' -> 100, bit '1' -> 110  (spełnia T0/T1 z datasheet)
            uint32_t bits3 = ((v >> i) & 1) ? 0b110 : 0b100;
            out = (out << 3) | bits3;
        }
        encode_byte[v][0] = (out >> 16) & 0xFF;
        encode_byte[v][1] = (out >> 8)  & 0xFF;
        encode_byte[v][2] = (out)       & 0xFF;
    }
}

void ws2812_init(void) {
    ws2818_build_lut();
    SPIM_1_Start();
    ws2812_fill((color_grb_t){0,0,0});
    ws2812_show();
}

void ws2812_fill(color_grb_t c) {
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        uint8_t *p = &spi_buf[i * 9];
        encode_color(c.g, p + 0);  // kolejność GRB
        encode_color(c.r, p + 3);
        encode_color(c.b, p + 6);
    }
}

void ws2812_show(void) {
    SPIM_1_PutArray(spi_buf, sizeof(spi_buf));
    while ((SPIM_1_ReadTxStatus() & SPIM_1_STS_SPI_DONE) == 0u) { }
    CyDelayUs(300);                 // reset latch ≥ ~280 µs;
}

void ws2812_set_led(uint8_t idx, color_grb_t c)
{
    if (idx >= LED_COUNT) return;              // ochrona indeksu
    uint8_t *p = &spi_buf[idx * 9];            // 9 bajtów/LED
    encode_color(c.g, p + 0);                  // G
    encode_color(c.r, p + 3);                  // R
    encode_color(c.b, p + 6);                  // B
}

void ws2812_set_rgb(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_set_led(idx, (color_grb_t){ g, r, b });  // zamiana RGB->GRB
}

void ws2812_clear(void)
{
    // szybkie wyzerowanie całego bufora
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        uint8_t *p = &spi_buf[i * 9];
        encode_color(0, p + 0); // G=0 -> 3 bajty wzorca '100...'
        encode_color(0, p + 3); // R=0
        encode_color(0, p + 6); // B=0
    }
}