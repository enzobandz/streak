#pragma once
typedef struct { signed char x, y; unsigned char w, h, adv; int off; } glyph_t;
typedef struct { int height, ascent; const glyph_t *g; const unsigned char *bits; } font_t;
extern const font_t font_ui, font_bold, font_small, font_title, font_big, font_mono;
