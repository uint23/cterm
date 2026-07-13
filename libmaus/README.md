![logo](assets/logo_256x112.png)  
is a small, cross-platform, software windowing library

### Building
Unix:
```sh
./configure --backend=[x11|wayland|mac]
make # That's it!
```

Windows:
```
.\build.bat [msvc|gcc|clang]
```

After building, you have a library which you link _your_ sources with. It is
located in `build/`.

### Usage
#### libmaus
Here is an annoted example for a basic window program which counts the number of
frames that have elapsed, draw the text and displays if you've pressed a key
(specifically 'A').

For the full list of functions and what they do, look at
[the header](include/maus.h).

```c
#include "maus.h"
#include "maus_font.h"

int main(void)
{
	int x=50, y=50, width=640, height=480;
	Maus* ctx = maus_init("example!", x, y, width, height);
	maus_create_window(ctx);
	MausFont* font = maus_font_load("assets/fonts/font1.bdf");

	int running = true;
	long long frames = 0;
	int a_pressed = false;

	while (running) {
		MausEvent ev;
		while (maus_event_poll(ctx, &ev)) {
			// to see the full list of event types, look at
			// the enum `MausEventType` in `maus.h`
			if (ev.type == MAUS_EV_CLOSE)  {
				running = false;
				continue;
			}

			if (ev.type == MAUS_EV_KEY && ev.key.pressed)  {
				if (ctx->key_syms[MAUS_KEY_A])
					a_pressed = 1;
			}
		}

		// clear framebuffer with color white
		// colors are in format ARGB
		maus_clear(ctx, (MausColor){255, 255, 255, 255});

		if (a_pressed) {
			const char* msg = "how dare you press a!!";
			maus_draw_text(ctx, font, 10, 50, msg, (MausColor){255, 255, 0, 0});
		}

		char frames_text[256];
		snprintf(frames_text, sizeof(frames_text), "%lld", frames++);
		maus_draw_text(ctx, font, 10, 20, frames_text, (MausColor){255, 255, 0, 0});

		maus_target_fps(ctx, 512);
		maus_present(ctx);
	}

	// free resources
	maus_font_free(font);
	maus_close(ctx);
}
```

