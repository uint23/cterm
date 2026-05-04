#include <stdio.h>
#include <stdlib.h>

#include "util.h"

void die(int ec, const char* msg)
{
	perror(msg);
	exit(ec);
}

