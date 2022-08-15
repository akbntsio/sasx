#ifndef DATATYPE_H
#define DATATYPE_H

/*
	data type
*/

typedef enum {RAW,WAV,AU,ERR} DATATYPE;

typedef struct {
	DATATYPE       type;
	int            headerSize;     /* bytes */
	int            dataSize;       /* bytes */
	int            chan;
	int            freq;           /* Hz */
	int            unit;           /* 1,2,4,8 */
	int            endian;         /* 'B' 'L' */
} DATAINFO;

extern int get_datainfo(char *fname, DATAINFO *info);

#endif /* DATATYPE_H */
