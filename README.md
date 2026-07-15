![](logo.png)  
Is a tiny, cross-platform terminal emulator (written in C).

#### Note
macOS and Wayland won't build as libmaus doesn't have implementations for
those. If you would be interested in making one for your own platform, it would
be amazing if you could contribute!

### Building:
``` sh
./configure
make
cp cterm /usr/local/bin # or where you want it
```

### About
cterm is a tiny terminal emulator I built from frustrations of not being able
to use [st](https://st.suckless.org/) across different platforms. Much like
_st_, it is very, very small--even smaller than _st_. For this reason,
however, cterm does not have the "standard" terminal emulator features such as
mouse selection, scrollback, fancy graphics, configuration files, the like;
this is because, I find all of them useless. If you want to achieve that, use a
terminal multiplexer or even VIM (it can function very well as a modal
multiplexer).

cterm also doesn't rely on any extra dependancies like
[FreeType](https://freetype.org/). Well it sort of does, but I chose to vendor
libmaus as that is a really simple windowing library I created so that it can
be cross platform.
> !! This is a lie, cterm as of now depends on libgrapheme !!

### Info
Only BDF fonts are supported as for now. If requested (or contributed), I have
no problem with adding other formats but no vector font support! If you want to
convert your vector font, you can use something like `otf2bdf` (thats what I
do!).

Reloading the font can be done with `AltR + r`

There is also a ![font scaling program](scripts/fontscale) provided which uses
`otf2ttf` to scale any font in a libary if needed.

