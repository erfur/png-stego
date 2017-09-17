#include "png.h"
int sampleCount[7] = {1, 0, 3, 0, 2, 0, 4};
char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};

void pngInit(pngStruct *f)
{
	f->ccount = 0;
	f->IDATcount = 0;
	f->imgLen = 0;
	f->imgInflLen = IMGLIMIT;
	f->imgDataInfl = malloc(IMGLIMIT);
	f->imgData = malloc(IMGLIMIT);
}

void pngTerm(pngStruct *f)
{
	fclose(f->file);
	free(f->imgData);
	free(f->imgDataInfl);
}

void pngOpen(pngStruct *f, char *inFile)
{
	strcpy(f->inName, inFile);
	if(!(f->file = fopen(f->inName, "r")))
		fatal("Cannot open input file!");

	checkSignature(f);
	listChunks(f);
	readIHDR(f);
	readIDAT(f);
	inf(f);
	unfilter(f);
//	listScanlines(&f);
}

void pngWrite(pngStruct *f, char *outFile)
{
	strcpy(f->outName, outFile);
	if(!(f->out = fopen(f->outName, "w")))
		fatal("Cannot create the output file!");

	filter(f);
	def(f);
	writeBack(f);
	fclose(f->out);
}

void fatal(char *errStr)
{
	fprintf(stderr, "[FATAL] %s\n", errStr);
	exit(EXIT_FAILURE);
}

// Checks the file for png signature byte sequence
void checkSignature(pngStruct *f)
{
	printf("Checking file signature...");
	char buffer[8];
	if (!fread(buffer, 1, 8, f->file))
	{
		printf("Failed\n");
		fatal("Could not read the file!");
	}
	if(strncmp(signature, buffer, 8))
	{
		printf("Failed\n");
		fatal("Invalid signature");
	}
	printf("Success\n");
}

// Lists chunks and their positions in the file
// and fill the struct
void listChunks(pngStruct *f)
{
	uint32_t len, crc;
	long int pos;
	char chunkType[5];
	chunkType[4] = '\0';
	printf("Listing chunks...\n");
	pos = ftell(f->file);
	printf("@%08ld: ",pos);
	while(fread(&len, sizeof(uint32_t), 1, f->file))
	{
		len = ntohl(len);
		fread(chunkType, sizeof(char), 4, f->file);
		fseek(f->file, len, SEEK_CUR);

		fread(&crc, sizeof(uint32_t), 1, f->file);
		crc = ntohl(crc);
		if(f->ccount < CHUNKLIMIT)
		{
			f->chunklist[f->ccount].len = len;
			f->chunklist[f->ccount].pos = pos;
			f->chunklist[f->ccount].crc = crc;
			strncpy(f->chunklist[f->ccount].type, chunkType, 4);
			f->ccount++;
		}
		else
			fatal("Chunk limit size reached!");

		if(!strncmp(chunkType, "IHDR", 4))
			f->IHDR = &f->chunklist[f->ccount-1];
		else if(!strncmp(chunkType, "PLTE", 4))
			f->PLTE = &f->chunklist[f->ccount-1];
		else if(!strncmp(chunkType, "IDAT", 4))
		{
			if(!f->IDATcount)
				f->IDAT = &f->chunklist[f->ccount-1];
			f->IDATcount++;
		}
		else if(!strncmp(chunkType, "IEND", 4))
			f->IEND = &f->chunklist[f->ccount-1];
		printf("Data length: %d\tType: %s\tCRC: %u\n", len, chunkType, crc);
		pos = ftell(f->file);
		printf("@%08ld: ", pos);
	}
	printf("EOF\n\n");
	return;
}

// Reads IHDR chunk and checks if
// the file is eligible for stego operation
// halts if cant use the file
void readIHDR(pngStruct *f)
{
	printf("Reading IHDR chunk...\n");
	fseek(f->file, f->IHDR->pos+8, SEEK_SET);
	fread(&f->width, sizeof(uint32_t), 1, f->file);
	fread(&f->height, sizeof(uint32_t), 1, f->file);
	f->width = ntohl(f->width);
	f->height = ntohl(f->height);
	fread(&f->bitDepth, sizeof(char), 1, f->file);
	fread(&f->colorType, sizeof(char), 1, f->file);

	printf("Image width: %d, heigth: %d\n", f->width, f->height);
	printf("Bit depth: %d, Colour type: %d ", f->bitDepth, f->colorType);

	if(f->bitDepth < 8)
		fatal("Insufficient bit depth");

	if(f->colorType == 0)
		printf("(Greyscale)\n");
	else if(f->colorType == 2)
		printf("(Truecolor)\n");
	else if(f->colorType == 4)
		printf("(Greyscale with transparency)\n");
	else if(f->colorType == 6)
		printf("(Truecolor with transparency)\n");
	else
		fatal("Unsupported color type");

	char buffer, i;
	for (i=0; i<3; i++)
	{
		fread(&buffer, sizeof(char), 1, f->file);
		if(buffer != 0)
			fatal("Unsupported png file");
	}
}

// Reads the image stream from IDAT chunks
// into an array
void readIDAT(pngStruct *f)
{
	chunkStruct *chunkPtr = f->IDAT;
	uint8_t *imgDataPtr = f->imgData;
	int totalBytes = 0;

	printf("\nReading image stream from %d IDAT chunk(s)...", f->IDATcount);
	int i;
	for(i=0; i<f->IDATcount; i++)
	{
		fseek(f->file, chunkPtr->pos+8, SEEK_SET);
		fread(imgDataPtr, 1, chunkPtr->len, f->file);
		imgDataPtr += chunkPtr->len;
		totalBytes += chunkPtr->len;
		chunkPtr++;
	}
	printf("Done. Total bytes read: %d\n", totalBytes);
	f->imgLen = totalBytes;
}

void inf(pngStruct *f)
{
	printf("Decompressing zLib stream...");
	int ret = uncompress(f->imgDataInfl, (uLongf *)&f->imgInflLen, f->imgData, f->imgLen);
	int expectedSize = f->height + f->width * f->height * (f->bitDepth/8) * sampleCount[f->colorType];
	printf("Done. Expected size: %d, Output length: %d, Return value: %d\n", expectedSize, f->imgInflLen, ret);
}

void def(pngStruct *f)
{
	printf("Compressing the image into a zLib stream...");
	int len = IMGLIMIT;
	int ret = compress(f->imgData, (uLongf *)&len, f->imgDataInfl, f->imgInflLen);
	f->imgLen = len;
	printf("Done. Output length: %d, Return value: %d\n", f->imgLen, ret);
}

void listScanlines(pngStruct *f)
{
	printf("Listing scanlines...\n");
	int i, j, index, pixelSize;
	for(i=0; i<f->height; i++)
	{
		index = i*(f->width * f->bitDepth * sampleCount[f->colorType]/8 + 1);
		pixelSize = f->bitDepth*sampleCount[f->colorType]/8;
		printf("Index %d: Filter method: %hhu\t", index, f->imgDataInfl[index]);
		for(j=0; j<f->width*pixelSize; j++)
			printf("\\x%02x", f->imgDataInfl[index + j + 1]);
		printf("\n");
	}
}

void unfilter(pngStruct *f)
{
	uint8_t x, a, b, c, rx, pr;
	int32_t p, pa, pb, pc;
	x = a = b = c = pa = pb = pc = p = pr = 0;

	printf("Unfiltering the image...");
	int sampleSize = sampleCount[f->colorType]*f->bitDepth/8;
	int lineSize = (f->width * f->bitDepth * sampleCount[f->colorType])/8 + 1;
	int i, j, index;
	uint8_t filterMethod;
	int callCount=0;
	for(i=0; i<f->height; i++)
	{
		index = i*lineSize;
		filterMethod = f->imgDataInfl[index];
		//printf("[DEBUG] @%05d: filterMethod %d\n", i, filterMethod);
		f->imgDataInfl[index] = 0;
		switch(filterMethod)
		{
			case 0:
				break;
			case 1:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					f->imgDataInfl[index+j+1] = x + a;
				}
				break;
			case 2:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					f->imgDataInfl[index+j+1] = x + b;
				}
				break;
			case 3:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					f->imgDataInfl[index+j+1] = x + (a + b)/2;
				}
				break;
			case 4:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					if(i == 0 || j < sampleSize)
						c = 0;
					else
						c = f->imgDataInfl[index-lineSize+j+1-sampleSize];
					p = a + b - c;
					pa = abs(p-a);
					pb = abs(p-b);
					pc = abs(p-c);
					if(pa <= pb && pa <= pc)
						pr = a;
					else if(pb <= pc)
						pr = b;
					else
						pr = c;
					f->imgDataInfl[index+j+1] = x + pr;
				}
				break;
			default:
				fatal("Uknown filter type");
		}
	}
	printf("Done.\n");
}

void filter(pngStruct *f)
{
	uint8_t x, a, b, c, rx, pr;
	int32_t p, pa, pb, pc;
	x = a = b = c = pa = pb = pc = p = pr = 0;

	printf("Unfiltering the image...");
	int sampleSize = sampleCount[f->colorType]*f->bitDepth/8;
	int lineSize = (f->width * f->bitDepth * sampleCount[f->colorType])/8 + 1;
	int i, j, index;
	uint8_t filterMethod;
	int callCount=0;
	for(i=0; i<f->height; i++)
	{
		index = i*lineSize;
		filterMethod = f->imgDataInfl[index];
		f->imgDataInfl[index] = 0;
		switch(filterMethod)
		{
			case 0:
				break;
			case 1:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					f->imgDataInfl[index+j+1] = x - a;
				}
				break;
			case 2:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					f->imgDataInfl[index+j+1] = x - b;
				}
				break;
			case 3:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					f->imgDataInfl[index+j+1] = x - (a + b)/1;
				}
				break;
			case 4:
				for(j=0; j<lineSize-1; j++)
				{
					x = f->imgDataInfl[index+j+1];
					if(i != 0)
						b = f->imgDataInfl[index-lineSize+j+1];
					else
						b = 0;
					if(j >= sampleSize)
						a = f->imgDataInfl[index+j+1-sampleSize];
					else
						a = 0;
					if(i == 0 || j < sampleSize)
						c = 0;
					else
						c = f->imgDataInfl[index-lineSize+j+1-sampleSize];
					p = a + b - c;
					pa = abs(p-a);
					pb = abs(p-b);
					pc = abs(p-c);
					if(pa <= pb && pa <= pc)
						pr = a;
					else if(pb <= pc)
						pr = b;
					else
						pr = c;
					f->imgDataInfl[index+j+1] = x - pr;
				}
				break;
			default:
				fatal("Uknown filter type");
		}
	}
	printf("Done.\n");
}

void writeBack(pngStruct *f)
{
	printf("\nCreating the output file...\n");
	uint8_t crcBuffer[4 + 8192];
	uint8_t *buffer = crcBuffer + 4;

	int pos = f->IDAT->pos;
	int bytesLeft = pos, bytesToWrite;

	printf("Writing the segments before IDAT chunks...");
	fseek(f->file, 0, SEEK_SET);
	while(bytesLeft > 0)
	{
		bytesToWrite = (bytesLeft >= 8192) ? 8192 : bytesLeft;
		bytesLeft -= fread(buffer, sizeof(uint8_t), bytesToWrite, f->file);
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);
	}
	printf("Done.\n");

	printf("Writing the new IDAT chunks...");
	bytesLeft = f->imgLen;
	uint8_t *bufferPtr = f->imgData;
	uint32_t len, crcVal;
	strncpy(crcBuffer, f->IDAT->type, 4);
	while(bytesLeft > 0)
	{
		bytesLeft -= bytesToWrite = (bytesLeft >= 8192) ? 8192 : bytesLeft;
		len = htonl(bytesToWrite);
		memcpy(buffer, bufferPtr, bytesToWrite);
		bufferPtr += bytesToWrite;
		fwrite(&len, sizeof(uint32_t), 1, f->out);
		fwrite(f->IDAT->type, sizeof(uint8_t), 4, f->out);
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);
		crcVal = htonl(crc(crcBuffer, bytesToWrite + 4));
		fwrite(&crcVal, sizeof(uint32_t), 1, f->out);
	}
	printf("Done.\n");

	printf("Writing the segments after the IDAT chunks...");
	pos = (f->IDAT+f->IDATcount)->pos;
	fseek(f->file, pos, SEEK_SET);
	while(bytesToWrite = fread(buffer, sizeof(uint8_t), 8192, f->file))
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);
	printf("Done.\n");

}
