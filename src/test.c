#include <stdio.h>
#include <stdlib.h>
#include "png.h"

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("png-read - Read png files and write them back intact.\n");
		printf("Usage: ./png_read $inputFile $outputFile\n");
		return EXIT_SUCCESS;
	}

	pngStruct f;
	pngInit(&f);

	pngOpen(&f, argv[1]);

	if(argc == 3)
		pngWrite(&f, argv[2]);

	pngTerm(&f);
	return EXIT_SUCCESS;
}
