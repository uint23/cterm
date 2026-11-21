#include <stdint.h>
#include <stdlib.h>

#include "sixel.h"

#define MAX_DIM    4096
#define MAX_COLORS 256

enum {
	SIXEL_REPEAT = '!',
	SIXEL_RASTER = '"',
	SIXEL_COLOR  = '#',
	SIXEL_CR     = '$',
	SIXEL_NL     = '-',
	SIXEL_FIRST  = '?',
	SIXEL_LAST   = '~',
};

enum {
	COLOR_PARAM_COLOR_REG,
	COLOR_PARAM_COLOR_FMT,
	COLOR_PARAM_RED_VAL,
	COLOR_PARAM_GREEN_VAL,
	COLOR_PARAM_BLUE_VAL,
	COLOR_PARAMS,
};

enum {
	RASTER_PARAM_ASPECT_X,
	RASTER_PARAM_ASPECT_Y,
	RASTER_PARAM_WIDTH,
	RASTER_PARAM_HEIGHT,
	RASTER_PARAMS, /* at least */
};

#define COLOR_FMT_RGB 2

static void draw_pixels(Sixel* img, int x, int y, int repeat, unsigned char c, uint32_t color);
static int parameters(const char* s, size_t len, size_t* i, int p[COLOR_PARAMS]);
static uint32_t rgb(int r, int g, int b);
static int tonum(const char* s, size_t len, size_t* i);

static void draw_pixels(Sixel* img, int x, int y, int repeat, unsigned char c, uint32_t color)
{
	int bits = c - SIXEL_FIRST;

	for (int dx = 0; dx < repeat && x + dx < img->width; dx++) {
		for (int bit = 0; bit < 6 && y + bit < img->height; bit++) {
			if (bits & (1 << bit))
				img->pixels[(y + bit) * img->width + x + dx] = color;
		}
	}
}

/* store delimited parameters into array */
static int parameters(const char* s, size_t len, size_t* i, int p[COLOR_PARAMS])
{
	int count = 0;

	while (1) {
		int value = tonum(s, len, i);
		if (count < COLOR_PARAMS)
			p[count] = value;
		count++;

		if (*i >= len || s[*i] != ';')
			break;
		(*i)++;
	}

	return count;
}

/* normalise r, g, b channels to 255 */
static uint32_t rgb(int r, int g, int b)
{
	return 0xff000000 |
		((r * 255 / 100) << 16) |
		((g * 255 / 100) << 8) |
		(b * 255 / 100);
}


/* convert string to int strtol but limited by length, not nul termination and
   return integer */
static int tonum(const char* s, size_t len, size_t* i)
{
	int n = 0;

	while (*i < len && s[*i] >= '0' && s[*i] <= '9') {
		int d = s[*i] - '0'; /* convert to integer */
		if (n <= (MAX_DIM - d) / 10)
			n = n * 10 + d;
		else
			n = MAX_DIM + 1;
		(*i)++;
	}

	return n;
}

Sixel sixel_decode(const char* data, size_t size)
{
	Sixel img = { 0 };
	if (!data)
		return img;

	uint32_t palette[MAX_COLORS];
	for (int i = 0; i < MAX_COLORS; i++)
		palette[i] = 0x00000000;

	for (int pass = 0; pass < 2; pass++) {
		int x = 0, y = 0, color = 0;
		int width = 0, height = 0;

		for (size_t i = 0; i < size;) {
			unsigned char c = data[i++];
			int repeat = 1;

			switch (c) {
			case SIXEL_CR:
				x = 0;
				continue;

			case SIXEL_NL:
				x = 0;
				y += 6;
				if (y > MAX_DIM)
					goto fail;
				continue;

			case SIXEL_COLOR:
			case SIXEL_RASTER: {
				int p[COLOR_PARAMS] = { 0 };
				int count = parameters(data, size, &i, p);

				if (c == SIXEL_COLOR) {
					color = p[COLOR_PARAM_COLOR_REG];
					if (color >= MAX_COLORS)
						goto fail;

					if (pass == 1 && count >= COLOR_PARAMS &&
					    p[COLOR_PARAM_COLOR_FMT] == COLOR_FMT_RGB &&
					    p[COLOR_PARAM_RED_VAL]   <= 100 &&
					    p[COLOR_PARAM_GREEN_VAL] <= 100 &&
					    p[COLOR_PARAM_BLUE_VAL]  <= 100) {
						palette[color] = rgb(
							p[COLOR_PARAM_RED_VAL],
							p[COLOR_PARAM_GREEN_VAL],
							p[COLOR_PARAM_BLUE_VAL]);
					}
				}

				/* raster attributes */
				else if (count >= RASTER_PARAMS) {
					width = p[RASTER_PARAM_WIDTH];
					height = p[RASTER_PARAM_HEIGHT];
				}
				continue;
			}

			case SIXEL_REPEAT:
				repeat = tonum(data, size, &i);
				if (!repeat || i >= size)
					goto fail;
				c = data[i++];
				break;

			default:
				break;
			}

			if (c < SIXEL_FIRST || c > SIXEL_LAST)
				continue;
			if (repeat > MAX_DIM - x || y > MAX_DIM - 6)
				goto fail;

			if (pass == 1) { /* pass 2 */
				draw_pixels(&img, x, y, repeat, c, palette[color]);
			}
			else { /* (pass 1) measure image */
				if (x + repeat > img.width)
					img.width = x + repeat;
				if (y + 6 > img.height)
					img.height = y + 6;
			}

			x += repeat;
		}

		if (pass == 0) {
			if (width > img.width)
				img.width = width;
			if (height > img.height)
				img.height = height;

			if (!img.width || !img.height ||
			    img.width > MAX_DIM || img.height > MAX_DIM ||
			    img.width * img.height > SIXEL_MAX_PIXELS)
				goto fail;

			img.pixels = calloc(img.width * img.height, sizeof(*img.pixels));
			if (!img.pixels)
				goto fail;
		}
	}

	return img;

fail:
	sixel_free(&img);
	return (Sixel){ 0 };
}

void sixel_free(Sixel* sixel)
{
	if (sixel && sixel->pixels) {
		free(sixel->pixels);
		sixel->pixels = NULL;
		sixel->width = 0;
		sixel->height = 0;
	}
}

