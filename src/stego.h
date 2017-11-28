#include "png.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>

#define CHUNKSIZE 8192

typedef struct stegoStruct
{
	/* input data */
	char *data;
	int dataLen;

	/* stego data */
	int bitDepth;
	int maskLen;
//	int offset;
	char *maskData;

	/* image data */
	pngStruct *p;
	int pos;
}stegoStruct;

void stegoInit(stegoStruct *);
void stegoTerm(stegoStruct *);

void stegoEmbed(stegoStruct *, int, pngStruct *, const char *);
void stegoExtract(stegoStruct *, int, pngStruct *, const char *);

/* embed operations */
int getData(stegoStruct *, FILE *);
int checkReqSize(stegoStruct *);
void convertToMask(stegoStruct *);
int injectData(stegoStruct *);

/* extract operations */
int readLen(stegoStruct *);
void readData(stegoStruct *, FILE *);

void checkBitLen(int);

