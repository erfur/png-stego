#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <zlib.h>
#include "crc.h"

#define CHUNKLIMIT 1024 // Read no more than 1024 chunks
#define IMGLIMIT 104857600L // 100MB

// Data structure to hold the
// chunk info and position in the file
typedef struct chunkStruct
{
	uint32_t len;
	uint8_t type[5];
	uint32_t crc;
	long int pos;
}chunkStruct;

// Data structure to hold the file info
typedef struct pngStruct
{
	//Chunks/
	int ccount;
	chunkStruct chunklist[CHUNKLIMIT];
	chunkStruct *IHDR, *PLTE, *IDAT, *IEND;
	int IDATcount;
	//File
	FILE *file;
	FILE *out;
	char inName[1024];
	char outName[1024];
	//Image Properties
	int width, height;
	uint8_t bitDepth, colorType;
	//Image
	uint8_t *imgDataCompressed;
	uint8_t *imgDataDecompressed;
	uint8_t *imgDataFiltered;
	unsigned long int imgCompressedLen;
	unsigned long int imgDecompressedLen;
	unsigned long int imgFilteredLen;
}pngStruct;

void pngInit(pngStruct *);
void pngTerm(pngStruct *);

void pngOpen(pngStruct *, char *);
void pngWrite(pngStruct *, char *);

void fatal(char *err_str);
void checkSignature(pngStruct *);
void listChunks(pngStruct *);
void readIHDR(pngStruct *);
void readIDAT(pngStruct *);
void decompressBuffer(pngStruct *);
void compressBuffer(pngStruct *);
void listScanlines(pngStruct *);
void unfilter(pngStruct *);
void filter(pngStruct *);
void writeBack(pngStruct *);
