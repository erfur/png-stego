#include <stdio.h>
#include <stdlib.h>
#include "png.h"

int main(int argc, char **argv)
{
	pngStruct f;
	pngInit(&f);

	pngOpen(&f, argv[1]);

	if(argc == 3)
		pngWrite(&f, argv[2]);

	pngTerm(&f);
	return EXIT_SUCCESS;
}
