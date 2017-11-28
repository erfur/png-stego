#include "stego.h"

void checkBitLen(int len)
{
	if(len != 1 && len != 2 && len != 4 && len != 8)
	{
		printf("Invalid bit length! Possible values are: {1, 2, 4, 8}\n");
		exit(EXIT_FAILURE);
	}
}

void stegoInit(stegoStruct *s)
{
	s->data = NULL;
	s->dataLen = 0;
	s->maskData = NULL;
	s->maskLen = 0;
}

void stegoTerm(stegoStruct *s)
{
	if(s->data)
		free(s->data);
	if(s->maskData)
		free(s->maskData);
}

void stegoEmbed(stegoStruct *s, int bitD, pngStruct *p, const char *inFile)
{
	stegoInit(s);
	s->p = p;
	checkBitLen(bitD);
	s->bitDepth = bitD;
	FILE *f = fopen(inFile, "rb");
	if(!f)
	{
		printf("[stegoExtract] no such file!\n");
		exit(EXIT_FAILURE);
	}

	int len = getData(s, f);
	if(len <= 0)
		printf("[stegoEmbed] error!\n");

	int req = checkReqSize(s);
	if(req == -1)
		printf("[stegoEmbed] not enough memory\n");

	printf("[stegoEmbed] bitDepth= %d\n", bitD);
	s->maskData = (char *)malloc(req);
	s->maskLen = req;
	convertToMask(s);

	len = injectData(s);
	printf("[stegoEmbed] injected %d bytes\n", len);

	fclose(f);
	stegoTerm(s);
}

int getData(stegoStruct *s, FILE *f)
{
	int total = CHUNKSIZE, ret = 0, pos = sizeof(int);
	s->data = (char *)malloc(total);

	while(1)
	{
		ret = fread((s->data)+pos, 1, CHUNKSIZE, f);
		pos += ret;
//		printf("[getData] total= %d, pos= %d, ret= %d\n", total, pos, ret);

		/* error */
		if(ret == -1)
			return -EIO;

		/* stream is ended */
		else if(ret < CHUNKSIZE || ret == 0)
			break;

		/* need more memory for data */
		else if(ret == CHUNKSIZE)
		{
			char *temp = s->data;
			s->data = (char *)malloc(total+CHUNKSIZE);
			//printf("[getData] %p\n", s->data);
			memcpy(s->data, temp, pos);
			free(temp);
			total += CHUNKSIZE;
		}
	}
	memcpy(s->data, &pos,  sizeof(int));

	s->dataLen = pos;
	printf("[getData] %d bytes read from stdin\n", pos);

	return pos;
}

int checkReqSize(stegoStruct *s)
{
	int req = (s->dataLen+4) * 8 / s->bitDepth;

//	printf("[checkReqSize] need %d bytes in total\n", req);

	if(s->p->totalSize < req)
		return -ENOMEM;
	else
		return req;
}

void convertToMask(stegoStruct *s)
{
	unsigned char shiftedBits, upperMask, mask;
	int i, pos=0;

	for(i=0; i<s->dataLen; i++)
	{
//		printf("%c -> %hhx\n", s->data[i], s->data[i]);
		int j;
		for(j=0; j<8/s->bitDepth; j++,pos++)
		{
			shiftedBits = s->data[i] >> j*s->bitDepth;
			upperMask = (unsigned char)pow(2, s->bitDepth)-1;
			mask = shiftedBits & upperMask;
			s->maskData[pos] = mask;
//			printf("[convertToMask]%c ", s->data[i]);
//			printf("0x%hhx \n", mask);
		}
//		printf("\n");
	}

	s->maskLen = pos;
//	printf("[convertToMask] mask is %d bytes long\n", pos);
}

int injectData(stegoStruct *s)
{
	int i, pos=0;
	for(i=0; i<s->maskLen; i++, pos++)
	{
		if(pos%s->p->lineSize == 0)
			pos++;
		s->p->imgDataDecompressed[pos] &= 0xff - (int)pow(2, s->bitDepth) + 1;
//		printf("0x%hhx x ", s->p->imgDataDecompressed[pos]);
//		printf("maskData 0x%hhx ->", s->maskData[i]);
		s->p->imgDataDecompressed[pos] |= s->maskData[i];
//		printf("[injectData]0x%hhx\n", s->p->imgDataDecompressed[pos]);
	}
//	printf("[injectData] %d bytes injected\n", i);
	return i;
}

void stegoExtract(stegoStruct *s, int bitD, pngStruct *p, const char *outFile)
{
	stegoInit(s);
	s->p = p;
	checkBitLen(bitD);
	s->bitDepth = bitD;
	FILE *f = fopen(outFile, "wb");
	if(!f)
	{
		printf("[stegoExtract] no such file!\n");
		exit(EXIT_FAILURE);
	}

	s->dataLen = readLen(s);
	s->maskLen = (int)(s->dataLen*8/s->bitDepth);

	s->maskData = (char *)malloc(s->maskLen);
	s->data = (char *)malloc(s->dataLen);

	readData(s, f);
	fclose(f);
	stegoTerm(s);
}

int readLen(stegoStruct *s)
{
	int i, j, len=0, pos=0;
	unsigned char temp;

	unsigned char *charPtr = (unsigned char *)&len;
	for(i=0; i<4; i++)
	{
		for(j=0; j<8/s->bitDepth; j++, pos++)
		{
			if(pos%s->p->lineSize == 0)
				pos++;
			temp = s->p->imgDataDecompressed[pos];
//			printf("[readLen] temp= %hhx\n", temp);
			temp &= (unsigned char)pow(2, s->bitDepth)-1;
			temp = temp << j*s->bitDepth;
			*charPtr |= temp;
		}
		charPtr++;
	}
	s->pos = pos;
//	printf("[readLen] len= %d\n", len);
	return len-sizeof(int);
}

void readData(stegoStruct *s, FILE *f)
{
	int i, j, pos=s->pos;
//	printf("[readData] pos %d\n", pos);
	unsigned char temp, byte;

	for(i=0; i<s->dataLen; i++)
	{
		byte=0;
		for(j=0; j<8/s->bitDepth; j++, pos++)
		{
			if(pos%s->p->lineSize == 0)
				pos++;
			temp = s->p->imgDataDecompressed[pos];
//			printf("[readData] temp= %hhx\n", temp);
			temp &= (unsigned char)pow(2, s->bitDepth)-1;
			temp = temp << j*s->bitDepth;
			byte |= temp;
		}
//		printf("[readData] %c\n", byte);
		fwrite(&byte, 1, 1, f);
	}
}
