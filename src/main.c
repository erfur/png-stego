#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "stego.h"

void usage()
{
	printf("png-stego: Read stdin and inject the data into png file.\n");
	printf("Usage: ./png-stego\n\n");
	printf("	-x              Extract mode\n");
	printf("	-i inputFile    Png file to embed data\n");
	printf("	-e dataFile     Data file to embed, not active in extract mode\n");
	printf("	-b bitDepth     Bit depth\n");
	printf("	-o outputFile   If extract mode is on, the extracted data, otherwise the embedded png file\n");
}

int main(int argc, char **argv)
{
	int bitDepth=0, extract=0;
	char *inFile=NULL, *outFile=NULL, *dataFile=NULL;
	char opt;

	while((opt = getopt(argc, argv, "i:o:e:xb:")) != -1)
		switch(opt){
			case 'i':
				inFile = optarg;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'x':
				extract = 1;
				break;
			case 'e':
				dataFile = optarg;
				break;
			case 'b':
				bitDepth = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
			default:
				break;
		}

	if(!inFile)
	{
		printf("Missing arguments.\n");
		exit(EXIT_FAILURE);
	}

	pngStruct f;
	pngInit(&f);
	pngOpen(&f, inFile);

	if(!extract && !bitDepth){
	} else if(extract && bitDepth && outFile){
		stegoStruct s;
		stegoExtract(&s, bitDepth, &f, outFile);
	} else if(!extract && dataFile && bitDepth && outFile){
		stegoStruct s;
		stegoEmbed(&s, bitDepth, &f, dataFile);
		pngWrite(&f, outFile);
	} else{
		printf("Invalid arguments.\n");
		usage();
	}

	pngTerm(&f);
	return EXIT_SUCCESS;
}
