#include <stdio.h>
#include <stdlib.h>
#include "stego.h"

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("png-stego: Read stdin and inject the data into png file.\n");
		printf("Usage: ./png_read $inputFile $outputFile\n");
		printf("       ./png_read $inputFile $outputFile $bitDepth\n");
		return EXIT_SUCCESS;
	}

	printf("[DEBUG] %d arguments given\n", argc);

	pngStruct f;
	pngInit(&f);

	pngOpen(&f, argv[1]);

	if(argc == 4)
	{
		stegoStruct s;
		int bitDepth = atoi(argv[3]);
		if(bitDepth != 1 && bitDepth != 2
				&& bitDepth != 4 && bitDepth != 8)
			return EXIT_FAILURE;
		stegoEmbed(&s, bitDepth, &f, "in");
		stegoExtract(&s, bitDepth, "out");
	}

	if(argc == 3 || argc == 4)
	{
		pngWrite(&f, argv[2]);
	}

	pngTerm(&f);
	return EXIT_SUCCESS;
}
