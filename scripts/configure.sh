#!/bin/sh

PLATFORM=$(uname)

LINUX_LDFLAGS="-lX11 -lXrandr"
FREEBSD_LDFLAGS="-lX11 -lXrandr"
OPENBSD_LDFLAGS="-I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -lXrandr"
MACOS_LDFLAGS="-framework Cocoa -framework CoreVideo -framework IOKit -framework CoreGraphics -framework CoreFoundation -framework Carbon"

{
	case "$PLATFORM" in
		Linux)
			echo "# Linux\n"
			echo "LDFLAGS += $LINUX_LDFLAGS\n"
			;;
		FreeBSD)
			echo "# FreeBSD\n"
			echo "LDFLAGS += $FREEBSD_LDFLAGS\n"
			;;
		OpenBSD)
			echo "# OpenBSD\n"
			echo "LDFLAGS += $OPENBSD_LDFLAGS\n"
			;;
		Darwin)
			echo "# Darwin\n"
			echo "LDFLAGS += $MACOS_LDFLAGS\n"
			;;
		*) # TODO: some better default option
			echo "Unsupported platform: $PLATFORM\n" >&2
			exit 1
			;;
	esac
} > platform.mk

