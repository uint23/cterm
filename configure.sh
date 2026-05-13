#!/bin/sh

PLATFORM=$(uname)

LINUX_LDFLAGS="-lX11 -lXrandr"
FREEBSD_LDFLAGS="-lX11 -lXrandr"
OPENBSD_LDFLAGS="-I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -lXrandr"
MACOS_LDFLAGS="-framework Cocoa -framework CoreVideo -framework IOKit -framework CoreGraphics -framework CoreFoundation -framework Carbon"

{
	case "$PLATFORM" in
		Linux)
			echo "LDFLAGS += $LINUX_LDFLAGS"
			;;
		FreeBSD)
			echo "LDFLAGS += $FREEBSD_LDFLAGS"
			;;
		OpenBSD)
			echo "LDFLAGS += $OPENBSD_LDFLAGS"
			;;
		Darwin)
			echo "LDFLAGS += $MACOS_LDFLAGS"
			;;
		*) # some better default option
			echo "Unsupported platform: $PLATFORM" >&2
			exit 1
			;;
	esac
} > platform.mk

