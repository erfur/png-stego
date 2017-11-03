#include "png.h"

/* Sample counts for each colorType:
 * val		type					sampleSize
 * 0		Greyscale				1
 * 2		TrueColor				3
 * 4		GreyScale with Alpha	2
 * 6		TrueColor with Alpha	4
 */
int sampleCount[7] = {1, 0, 3, 0, 2, 0, 4};

/* Signature byte sequence for .png files */
char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};


/* Initialize pngStruct */
void pngInit(pngStruct *f)
{
	f->ccount = 0;
	f->IDATcount = 0;

	f->imgCompressedLen = 0;
	f->imgDecompressedLen = 0;
	f->imgFilteredLen = 0;

	/* dynamically allocate image buffers */
	f->imgDataCompressed = NULL;
	f->imgDataDecompressed = NULL;
	f->imgDataFiltered = NULL;
}


/* Terminate pngStruct */
void pngTerm(pngStruct *f)
{
	fclose(f->file);
	free(f->imgDataCompressed);
	free(f->imgDataDecompressed);
	free(f->imgDataFiltered);
}


/* Open .png file */
void pngOpen(pngStruct *f, char *inFile)
{
	/* get the input file name from inFile */
	strcpy(f->inName, inFile);

	/* open the file */
	if(!(f->file = fopen(f->inName, "r")))
		fatal("Cannot open input file!");

	checkSignature(f);
	listChunks(f);
	readIHDR(f);

	/* calculate pixel and image size */
	f->pixelSize = sampleCount[f->colorType]*f->bitDepth/8;
	f->totalSize = f->height * (f->width * f->pixelSize + 1);

	/* allocate buffers */
	f->imgDataCompressed = calloc(f->totalSize, 1);
	f->imgDataDecompressed = calloc(f->totalSize, 1);
	f->imgDataFiltered = calloc(f->totalSize, 1);

	/* set new sizes */
	f->imgDecompressedLen = f->totalSize;
	f->imgFilteredLen = f->totalSize;

	readIDAT(f);

	/* decompress imgDataCompressed into imgDataDecompressed */
	decompressBuffer(f);

	/* unfilter data in imgDataDecompressed */
	unfilter(f);

//	listScanlines(&f);
}


/* Write the image to outFile */
void pngWrite(pngStruct *f, char *outFile)
{
	/* get output file name from outFile */
	strcpy(f->outName, outFile);

	/* open file */
	if(!(f->out = fopen(f->outName, "w")))
		fatal("Cannot create the output file!");

	/* filter data in imgDataDecompressed and into imgDataFiltered */
	filter(f);

	/* compress data in imgDataFiltered into imgDataCompressed */
	compressBuffer(f);

	/* write final data in imgDataCompressed to output file */
	writeBack(f);

	/* close output file */
	fclose(f->out);
}


/* Print a fatal error and terminate the program */
void fatal(const char *errStr)
{
	fprintf(stderr, "[FATAL] %s\n", errStr);
	exit(EXIT_FAILURE);
}


/* Check the file for png signature byte sequence */
void checkSignature(pngStruct *f)
{
	printf("Checking file signature...");
	char buffer[8];

	/* Terminate if the file cannot be read */
	if (!fread(buffer, 1, 8, f->file))
	{
		printf("Failed\n");
		fatal("Could not read the file!");
	}

	/* Terminate if the signature byte sequence is incorrect */
	if(strncmp(signature, buffer, 8))
	{
		printf("Failed\n");
		fatal("Invalid signature");
	}

	printf("Success\n");
}


/* Lists chunks and their positions in the file and fill the struct */
void listChunks(pngStruct *f)
{
	uint32_t len, crc;
	long int pos;
	char chunkType[5];

	/* setting the last byte to \x00 for string operations */
	chunkType[4] = '\0';

	printf("Listing chunks...\n");

	/* print the position of the chunk in the file */
	pos = ftell(f->file);
	printf("@%08ld: ",pos);

	/* read the chunk, start with data length */
	while(fread(&len, sizeof(uint32_t), 1, f->file))
	{
		/* four byte variables are in network byte order */
		len = ntohl(len);

		/* chunkType is a four byte char array */
		fread(chunkType, sizeof(char), 4, f->file);

		/* skip the data part */
		fseek(f->file, len, SEEK_CUR);

		/* read crc value */
		fread(&crc, sizeof(uint32_t), 1, f->file);
		crc = ntohl(crc);

		/* add chunk properties to the chunk list */
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

		/* set critical chunks' pointers */
		if(!strncmp(chunkType, "IHDR", 4))
			f->IHDR = &f->chunklist[f->ccount-1];
		else if(!strncmp(chunkType, "PLTE", 4))
			f->PLTE = &f->chunklist[f->ccount-1];
		else if(!strncmp(chunkType, "IDAT", 4))
		{
			/* set IDAT pointer to the first IDAT chunk */
			if(!f->IDATcount)
				f->IDAT = &f->chunklist[f->ccount-1];
			f->IDATcount++;
		}
		else if(!strncmp(chunkType, "IEND", 4))
			f->IEND = &f->chunklist[f->ccount-1];


		printf("Data length: %d\tType: %s\tCRC: %u\n", len, chunkType, crc);

		/* print next chunks' position */
		pos = ftell(f->file);
		printf("@%08ld: ", pos);
	}

	printf("EOF\n\n");
	return;
}


/* Read IHDR chunk and check if the file is eligible for stego
 * operation, halt the file cannot be used
 */
void readIHDR(pngStruct *f)
{
	printf("Reading IHDR chunk...\n");

	/* set the chunk position */
	fseek(f->file, f->IHDR->pos+8, SEEK_SET);

	/* read image properties from IHDR data */
	fread(&f->width, sizeof(uint32_t), 1, f->file);
	fread(&f->height, sizeof(uint32_t), 1, f->file);
	fread(&f->bitDepth, sizeof(char), 1, f->file);
	fread(&f->colorType, sizeof(char), 1, f->file);

	/* convert byte order */
	f->width = ntohl(f->width);
	f->height = ntohl(f->height);

	/* print properties */
	printf("Image width: %d, heigth: %d\n", f->width, f->height);
	printf("Bit depth: %d, Colour type: %d ", f->bitDepth, f->colorType);

	/* dont bother with bitDepth less than 8 */
	if(f->bitDepth < 8)
		fatal("Insufficient bit depth");

	/* print colorType */
	if(f->colorType == 0)
		printf("(Greyscale)\n");
	else if(f->colorType == 2)
		printf("(Truecolor)\n");
	else if(f->colorType == 4)
		printf("(Greyscale with transparency)\n");
	else if(f->colorType == 6)
		printf("(Truecolor with transparency)\n");

	/* other colorTypes are not supported */
	else
		fatal("Unsupported color type");

	/* compression method, filter method and interlace method values
	 * should be equal to zero
	 */
	char buffer, i;
	for (i=0; i<3; i++)
	{
		fread(&buffer, sizeof(char), 1, f->file);
		if(buffer != 0)
			fatal("Unsupported png file");
	}
}


/* Read the image stream from IDAT chunks into an array */
void readIDAT(pngStruct *f)
{
	/* get first IDAT chunk */
	chunkStruct *chunkPtr = f->IDAT;
	uint8_t *imgDataPtr = f->imgDataCompressed;
	/* total size of data stream */
	int totalBytes = 0;

	printf("\nReading image stream from %d IDAT chunk(s)...", f->IDATcount);

	/* read each chunk and write into imgDataCompressed buffer */
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

	/* set data length */
	f->imgCompressedLen = totalBytes;
}


/* uncompress the compressed data into imgDataDecompressed buffer using
 * zLib's uncompress() function
 */
void decompressBuffer(pngStruct *f)
{
	printf("Decompressing zLib stream...");

	/* decompress imgData into imgDataDecompressed */
	int ret = uncompress(f->imgDataDecompressed, (uLongf *)&f->imgDecompressedLen, f->imgDataCompressed, f->imgCompressedLen);

	/* expected output size can be calculated with image properties */
	int expectedSize = f->height + f->width * f->height * (f->bitDepth/8) * sampleCount[f->colorType];

	printf("Done. Expected size: %d, Output length: %lu, Return value: %d\n", expectedSize, f->imgDecompressedLen, ret);
}


/* compress the uncompressed data in imgDataDecompressed buffer and write
 * into imgData buffer
 */
void compressBuffer(pngStruct *f)
{
	printf("Compressing the image into a zLib stream...");

	/* create a variable for available space in the target buffer */
	int len = f->totalSize;

	/* compress imgDataDecompressed into imgData */
	int ret = compress(f->imgDataCompressed, (uLongf *)&len, f->imgDataFiltered, f->imgFilteredLen);

	/* get compressed data size */
	f->imgCompressedLen = len;

	printf("Done. Output length: %lu, Return value: %d\n", f->imgCompressedLen, ret);
}


/* list actual bytes of the original image
 **************************************
 * WARNING: This is a DEBUG function! *
 **************************************
 */
void listScanlines(pngStruct *f)
{
	printf("Listing scanlines...\n");

	/* iterate through rows with i */
	int i, j, index, pixelSize;
	for(i=0; i<f->height; i++)
	{
		/* calculate sample size */
		pixelSize = f->bitDepth*sampleCount[f->colorType]/8;

		/* first byte of every row is the filterMethod value */
		index = i*(f->width * pixelSize + 1);

		printf("Index %d: Filter method: %hhu\t", index, f->imgDataDecompressed[index]);

		/* iterate through pixels with j */
		for(j=0; j<f->width*pixelSize; j++)
			printf("\\x%02x", f->imgDataDecompressed[index + j + 1]);

		printf("\n");
	}
}


/* unfilter image lines */
void unfilter(pngStruct *f)
{
	/* pixel positions:
	 * -------
	 *| c | b |
	 * -------
	 *| a | x |
	 * -------
	 * x is the pixel being processed. if a, b or c is out of the image,
	 * they are set to zero.
	 */
	uint8_t x, a, b, c;

	/* variables for paethPredictor() function */
	uint8_t pr;
	int32_t p, pa, pb, pc;

	/* set everything to zero */
	x = a = b = c = pa = pb = pc = p = pr = 0;

	printf("Unfiltering the image...");

	/* calculate sampleSize and lineSize */
	int sampleSize = sampleCount[f->colorType]*f->bitDepth/8;
	int lineSize = (f->width * f->bitDepth * sampleCount[f->colorType])/8 + 1;

	/* iterate through rows with i and pixels with j */
	int i, j, index;
	uint8_t filterMethod;
	for(i=0; i<f->height; i++)
	{
		/* calculate line index */
		index = i*lineSize;

		/* get filter method of the line */
		filterMethod = f->imgDataDecompressed[index];

		/* set filter method to zero and dont bother with filtering */
		//f->imgDataDecompressed[index] = 0;

		/* unfilter pixels */
		switch(filterMethod)
		{
			/* Method 0: no filtering */
			case 0:
				break;


			/* Method 1: orig(x) = filt(x) + orig(a) */
			case 1:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if a is out of the image, set it to zero */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* calculate orig(x) */
					f->imgDataDecompressed[index+j+1] = x + a;
				}
				break;


			/* Method 2: orig(x) = filt(x) + orig(b) */
			case 2:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* calculate orig(x) */
					f->imgDataDecompressed[index+j+1] = x + b;
				}
				break;


			/* Method 3: orig(x) = filt(x) + floor((orig(a)+orig(b))/2)
			 */
			case 3:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* if a is out of the image, set it to zero */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* calculate orig(x) */
					f->imgDataDecompressed[index+j+1] = x + (a + b)/2;
				}
				break;


			/* Method 4: PaethPrediction */
			case 4:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* if a is out of the image, set it to zeri */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* if c is out of the image, set it to zero */
					if(i == 0 || j < sampleSize)
						c = 0;
					else
						c = f->imgDataDecompressed[index-lineSize+j+1-sampleSize];

					/* calculate paethPredictor() variables */
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

					/* calculate orig(x) */
					f->imgDataDecompressed[index+j+1] = x + pr;
				}
				break;

			/* No other filter method is available */
			default:
				fatal("Uknown filter type");

		}
	}
	printf("Done.\n");
}


/* filter image lines */
void filter(pngStruct *f)
{
	/* pixel positions:
	 * -------
	 *| c | b |
	 * -------
	 *| a | x |
	 * -------
	 * x is the pixel being processed. if a, b or c is out of the image,
	 * they are set to zero.
	 */
	uint8_t x, a, b, c;

	/* variables for paethPredictor() function */
	uint8_t pr;
	int32_t p, pa, pb, pc;

	/* set everything to zero */
	x = a = b = c = pa = pb = pc = p = pr = 0;

	printf("Filtering the image...");

	/* calculate sampleSize and lineSize */
	int sampleSize = sampleCount[f->colorType]*f->bitDepth/8;
	int lineSize = (f->width * f->bitDepth * sampleCount[f->colorType])/8 + 1;

	/* iterate through rows with i and pixels with j */
	int i, j, index;
	uint8_t filterMethod;
	for(i=0; i<f->height; i++)
	{
		/* calculate line index */
		index = i*lineSize;

		/* get filter method of the line */
		filterMethod = f->imgDataDecompressed[index];

		/* set filter method */
		f->imgDataFiltered[index] = filterMethod;

		/* unfilter pixels */
		switch(filterMethod)
		{
			/* Method 0: no filtering
			 * copy the entire line to imgDataFiltered
			 */
			case 0:
				memcpy(&f->imgDataFiltered[index], &f->imgDataDecompressed[index], lineSize);
				break;


			/* Method 1: orig(x) = filt(x) + orig(a) */
			case 1:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if a is out of the image, set it to zero */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* calculate orig(x) */
					f->imgDataFiltered[index+j+1] = x - a;
				}
				break;


			/* Method 2: orig(x) = filt(x) + orig(b) */
			case 2:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* calculate orig(x) */
					f->imgDataFiltered[index+j+1] = x - b;
				}
				break;


			/* Method 3: orig(x) = filt(x) + floor((orig(a)+orig(b))/2)
			 */
			case 3:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* if a is out of the image, set it to zero */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* calculate orig(x) */
					f->imgDataFiltered[index+j+1] = x - (a + b)/2;
				}
				break;


			/* Method 4: PaethPrediction */
			case 4:
				for(j=0; j<lineSize-1; j++)
				{
					/* get filt(x) */
					x = f->imgDataDecompressed[index+j+1];

					/* if b is out of the image, set it to zero */
					if(i != 0)
						b = f->imgDataDecompressed[index-lineSize+j+1];
					else
						b = 0;

					/* if a is out of the image, set it to zeri */
					if(j >= sampleSize)
						a = f->imgDataDecompressed[index+j+1-sampleSize];
					else
						a = 0;

					/* if c is out of the image, set it to zero */
					if(i == 0 || j < sampleSize)
						c = 0;
					else
						c = f->imgDataDecompressed[index-lineSize+j+1-sampleSize];

					/* calculate paethPredictor() variables */
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

					/* calculate orig(x) */
					f->imgDataFiltered[index+j+1] = x - pr;
				}
				break;

			/* No other filter method is available */
			default:
				fatal("Uknown filter type");

		}
	}
	/* set imgFilteredLen */
	f->imgFilteredLen = f->imgDecompressedLen;

	printf("Done.\n");
}



/* write finished image to file */
void writeBack(pngStruct *f)
{
	printf("\nCreating the output file...\n");

	/* create an array to hold and calculate crc of chunk data */
	uint8_t crcBuffer[4 + 8192] = {0};
	/* first four bytes will be "IDAT" */
	uint8_t *buffer = crcBuffer + 4;

	/* get IDAT chunk position on input file */
	int pos = f->IDAT->pos;
	int bytesLeft = pos, bytesToWrite = 0;

	printf("Writing the segments before IDAT chunks...");

	/* go to the beginning of input file */
	fseek(f->file, 0, SEEK_SET);

	/* write everything up to the IDAT chunks to output file */
	while(bytesLeft > 0)
	{
		bytesToWrite = (bytesLeft >= 8192) ? 8192 : bytesLeft;
		bytesLeft -= fread(buffer, sizeof(uint8_t), bytesToWrite, f->file);
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);
	}
	printf("Done.\n");

	printf("Writing the new IDAT chunks...");

	bytesLeft = f->imgCompressedLen;
	uint8_t *bufferPtr = f->imgDataCompressed;
	uint32_t len = 0, crcVal = 0;

	/* set chunkType */
	strncpy(crcBuffer, f->IDAT->type, 4);

	/* write final data in chunks */
	while(bytesLeft > 0)
	{
		/* get 8192 bytes at a time */
		bytesLeft -= bytesToWrite = (bytesLeft >= 8192) ? 8192 : bytesLeft;

		/* length of the chunk data */
		len = htonl(bytesToWrite);

		/* copy data to internal buffer */
		memcpy(buffer, bufferPtr, bytesToWrite);

		/* set bufferPtr to the next data */
		bufferPtr += bytesToWrite;

		/* write data length, chunk type and chunk data */
		fwrite(&len, sizeof(uint32_t), 1, f->out);
		fwrite(f->IDAT->type, sizeof(uint8_t), 4, f->out);
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);

		/* calculate crc value of the chunk data */
		crcVal = htonl(crc(crcBuffer, bytesToWrite + 4));

		/* write crc value */
		fwrite(&crcVal, sizeof(uint32_t), 1, f->out);
	}
	printf("Done.\n");

	printf("Writing the segments after the IDAT chunks...");

	/* get the position of the segment after IDAT chunks */
	pos = (f->IDAT+f->IDATcount)->pos;

	/* set position in input file */
	fseek(f->file, pos, SEEK_SET);

	/* write 8192 bytes at a time */
	while((bytesToWrite = fread(buffer, sizeof(uint8_t), 8192, f->file)))
		fwrite(buffer, sizeof(uint8_t), bytesToWrite, f->out);

	printf("Done.\n");
}
