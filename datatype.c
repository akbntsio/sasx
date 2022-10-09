/*
	data type
	typedef enum {RAW,WAV,AU,ERR} DATATYPE;
	typedef struct {
		DATATYPE       type;
		int            headerSize, dataSize, chan, freq, unit, endian;
	} DATAINFO;

	参考資料 sox source
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "datatype.h"

/* WAV HEADER DEFINITION */
#define WAVE_FMT_UNKNOWN     (0x0000)
#define WAVE_FMT_PCM         (0x0001)
#define WAVE_FMT_ADPCM       (0x0002)
#define WAVE_FMT_ALAW        (0x0003)
#define WAVE_FMT_ULAW        (0x0004)

struct RIFF {
	char              riff[4];
	uint32_t             size;
	char              wave[4];
};

struct RIFF_CHUNK {
	char    id[4];  /* "data" */
	uint32_t   size;
};

struct RIFF_FMT {
	short   enc;
	short   chan;
	uint32_t    freq;
	uint32_t    bytes_sec;
	short   balign;
	short   bit_sample;
};

/* AU HEADER DEFINITION */

struct AU {
	char    snd[4];
	uint32_t    hsize;
	uint32_t    dsize;
	uint32_t    enc;
	uint32_t    freq;
	uint32_t    chan;
};

/* COMMON HEADER DEFINITION */

union HEADER {
	char           type[4];
	struct  RIFF   riff;
	struct  AU     au;
};

/* LOCAL UTILITY */

static int  fpsize(FILE *fp)
{
	struct stat stt;
	fstat(fileno(fp), &stt);
	return stt.st_size;
}

static void reverse(char *p, int len)
{
	int tmp, i;
	for(i=0;i<len/2;i++)
		{ tmp=p[i];p[i]=p[len-1-i];p[len-1-i]=tmp; }
}

/* GET DATAINFO */

/* normally return 0 , or return -1 */
int get_datainfo(char *fname, DATAINFO *info)
{
	FILE                *fp;
	union HEADER        hdr;
	struct RIFF_CHUNK   riff_chunk;
	struct RIFF_FMT     riff_fmt;
	int                 rest;
	union {int i; char c[4];} endian = { ('B'<<24)|'L' };

	/* RESET TO DEFAULT */
	info->type = ERR;
	info->headerSize = 0;
	info->dataSize = 0;
	info->freq = 12000;
	info->chan = 1;
	info->unit = 2;
	info->endian = endian.c[0];

	if( ! (fp=fopen(fname,"rb")) )
		return -1;

	info->dataSize = fpsize(fp);

	if( fread(&hdr,1,4,fp) != 4 ) goto RAW;

	if( ! strncmp(hdr.type,"RIFF",4) ){
		/* RIFF だ  のこりの size, WAVE を読む */
		if( fread(&hdr.riff.size,4,2,fp) != 2 ) goto FAIL;
		if( strncmp(hdr.riff.wave, "WAVE", 4) ) goto FAIL;
		if( endian.c[0] == 'B' )
			reverse((char*)&hdr.riff.size,4);
		info->headerSize += 12;
		/* WAVE だ */
		/* FORMAT を待つ */
		while( fread(&riff_chunk,sizeof(riff_chunk),1,fp) == 1 ){
			if( endian.c[0] == 'B' )
				reverse((char*)&riff_chunk.size,4);
			info->headerSize += sizeof(riff_chunk) + riff_chunk.size;
			if( ! strncmp(riff_chunk.id,"fmt ",4) ){
				/* FORMAT を読む 16byte */
				if( fread(&riff_fmt,sizeof(riff_fmt),1,fp) != 1 ) goto FAIL;
				if( endian.c[0] == 'B' ){
					reverse((char*)&riff_fmt.enc,2);
					reverse((char*)&riff_fmt.chan,2);
					reverse((char*)&riff_fmt.freq,4);
					reverse((char*)&riff_fmt.bytes_sec,4);
					reverse((char*)&riff_fmt.balign,2);
					reverse((char*)&riff_fmt.bit_sample,2);
				}
				/* fmt のpadding を読み飛ばす */
				rest = riff_chunk.size - sizeof(riff_fmt);
				while(rest-->0) getc(fp);
				break;
			}
			else {
				/* FORMAT 以外は読み飛ばす */
				rest = riff_chunk.size;
				while(rest-->0) getc(fp);
			}
		}
		/* データを待つ */
		while( fread(&riff_chunk,sizeof(riff_chunk),1,fp) == 1 ){
			if( endian.c[0] == 'B' )
				reverse((char*)&riff_chunk.size,4);
			if( ! strncmp(riff_chunk.id,"data",4) ){
				info->headerSize += sizeof(riff_chunk);
				info->dataSize = riff_chunk.size;
				info->chan = riff_fmt.chan;
				info->freq = riff_fmt.freq;
				info->unit = riff_fmt.bit_sample/8;
				info->endian = 'L';
				info->type = WAV;
				if( riff_fmt.enc != WAVE_FMT_PCM ){
					fprintf(stderr,"not PCM, but treat as PCM\n");
				}
				break;
			}
			else{
				/* DATA 以外は読み飛ばす 例えば "fact" */
				rest = riff_chunk.size;
				while(rest-->0) getc(fp);
				info->headerSize += sizeof(riff_chunk) + riff_chunk.size;
			}
		}
	} /* end of RIFF */
	else if( ! strncmp(hdr.type,".snd",4) ){
		/* AU だ */
		rest = sizeof(hdr.au)-4;
		if( fread(&hdr.au.hsize,rest,1,fp) == 1 ){
			if( endian.c[0] == 'L' ){
				reverse((char*)&hdr.au.hsize, 4);
				reverse((char*)&hdr.au.dsize, 4);
				reverse((char*)&hdr.au.freq, 4);
				reverse((char*)&hdr.au.chan, 4);
				reverse((char*)&hdr.au.enc, 4);
			}
			info->type = AU;
			info->headerSize = hdr.au.hsize;
			info->dataSize = hdr.au.dsize;
			info->freq = hdr.au.freq;
			info->chan = hdr.au.chan;
			info->unit = 2;
			info->endian = 'B';
		}
	} /* end of AU */
	else {
		goto RAW;
	}

	fclose(fp);
	return 0;

RAW:
	info->type = RAW;
	info->headerSize = 0;
	info->dataSize = fpsize(fp);
	info->endian = endian.c[0];
	info->freq = 12000;
	info->unit = 2;
	info->chan = 1;
	fclose(fp);
	return 0;

FAIL:
	info->type = ERR;
	info->headerSize = 0;
	info->dataSize = fpsize(fp);
	info->endian = endian.c[0];
	info->freq = 12000;
	info->unit = 2;
	info->chan = 1;
	fclose(fp);
	return 0;
}

#ifdef DTMAIN /* FOR TEST ONLY */
int main(int argc, char **argv)
{
	int   i;
	char  *type;
	DATAINFO info;
	i = get_datainfo(argv[1], &info);
	if( info.type == WAV ) type = "WAV";
	if( info.type == AU ) type = "AU";
	if( info.type == RAW ) type = "RAW";
	if( info.type == ERR ) type = "ERR";
	printf("%-4s h:%d d:%d f:%d c:%d u:%d e:%c\n",
				type, info.headerSize, info.dataSize,
				info.freq, info.chan, info.unit, info.endian);
	return 0;
}
#endif

