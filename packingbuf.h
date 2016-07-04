#ifndef LIB_PACKINGBUF_H
#define LIB_PACKINGBUF_H

#include <stdint.h>

#define ENABLE_INT64_SUPPORT  1
#define ENABLE_DOUBLE_SUPPORT 1
#if defined(__IAR_SYSTEMS_ICC__) || defined(__C51__)
    //8 bits SOC can't support 64 bits data
    //disable int64 & double support
    #undef  ENABLE_INT64_SUPPORT
    #undef  ENABLE_DOUBLE_SUPPORT
    #define ENABLE_INT64_SUPPORT  0
    #define ENABLE_DOUBLE_SUPPORT 0
#endif

#ifndef PACKINGBUFAPI
    #define PACKINGBUFAPI
#endif

//PACKINGBUF(A String-Tagged Data Interchange Format)
typedef struct PACKINGBUF        PACKINGBUF;
typedef struct PACKINGBUF_VALUE  PACKINGBUF_VALUE;
typedef struct PACKINGBUF_STRING PACKINGBUF_STRING;
typedef struct PACKINGBUF_BINARY PACKINGBUF_BINARY;
typedef struct PACKINGBUF_VECTOR PACKINGBUF_VECTOR;
typedef struct PACKINGBUF_VALUE_LIST PACKINGBUF_VALUE_LIST;

enum //PACKINGBUF_TYPE
{
    PACKINGBUF_TYPE_MIN           = 0,
    PACKINGBUF_TYPE_UINT          = 0,
    PACKINGBUF_TYPE_SINT          = 1,
    PACKINGBUF_TYPE_FLOAT         = 2,
    PACKINGBUF_TYPE_STRING        = 3,
    PACKINGBUF_TYPE_BINARY        = 4,
    PACKINGBUF_TYPE_UINT_VECTOR   = 5,
    PACKINGBUF_TYPE_SINT_VECTOR   = 6,
    PACKINGBUF_TYPE_FLOAT_VECTOR  = 7,
    PACKINGBUF_TYPE_MAX           = 7,
};

enum PACKINGBUF_VECTOR_TYPE
{
    PACKINGBUF_VECTOR_TYPE_UINT  = PACKINGBUF_TYPE_UINT_VECTOR,
    PACKINGBUF_VECTOR_TYPE_SINT  = PACKINGBUF_TYPE_SINT_VECTOR,
    PACKINGBUF_VECTOR_TYPE_FLOAT = PACKINGBUF_TYPE_FLOAT_VECTOR,
};

//#include "packpush8.h"
struct PACKINGBUF
{
	unsigned char  *buffer; //buffer[size]
	unsigned int    size;   //sizeof(buffer)
	unsigned int    position;//current position
};

struct PACKINGBUF_VALUE
{
    const char    *tag;  //field tag
    unsigned char *data; //data[size]
    unsigned int   size; //sizeof(*data)
    unsigned int   type; //PACKINGBUF_TYPE_*
};

struct PACKINGBUF_STRING
{
    char         *data;  //data[length+1]
    unsigned int  length;//strlen(length)
};

struct PACKINGBUF_BINARY
{
    unsigned char  *data;//data[size]
    unsigned int    size;//sizeof(value)
};

struct PACKINGBUF_VECTOR
{
	unsigned char  *buffer; //buffer[size]
	unsigned int    size;   //sizeof(buffer)
	unsigned int    position;//current position
    unsigned int    type;   //PACKINGBUF_VECTOR_TYPE_*
};

struct PACKINGBUF_VALUE_LIST
{
    unsigned int      count;
    PACKINGBUF_VALUE *list;
    int              *hash;
};
//#include "packpop.h"

#ifdef __cplusplus
extern "C"
{
#endif

void         PACKINGBUFAPI PackingBuf_Init(PACKINGBUF *packingbuf, void *pBuffer, unsigned int nBufferSize);
unsigned int PACKINGBUFAPI PackingBuf_Finish(PACKINGBUF *packingbuf);
void         PACKINGBUFAPI PackingBuf_Reset(PACKINGBUF *packingbuf);

unsigned int PACKINGBUFAPI PackingBuf_PutNull(PACKINGBUF *packingbuf, const char *tag);
unsigned int PACKINGBUFAPI PackingBuf_PutUint8(PACKINGBUF *packingbuf, const char *tag, uint8_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutInt8(PACKINGBUF *packingbuf, const char *tag, int8_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutUint16(PACKINGBUF *packingbuf, const char *tag, uint16_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutInt16(PACKINGBUF *packingbuf, const char *tag, int16_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutUint32(PACKINGBUF *packingbuf, const char *tag, uint32_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutInt32(PACKINGBUF *packingbuf, const char *tag, int32_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutFloat(PACKINGBUF *packingbuf, const char *tag, float value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKINGBUFAPI PackingBuf_PutUint64(PACKINGBUF *packingbuf, const char *tag, uint64_t value);
unsigned int PACKINGBUFAPI PackingBuf_PutInt64(PACKINGBUF *packingbuf, const char *tag, int64_t value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKINGBUFAPI PackingBuf_PutDouble(PACKINGBUF *packingbuf, const char *tag, double value);
#endif
#endif

unsigned int PACKINGBUFAPI PackingBuf_PutBinary(PACKINGBUF *packingbuf, const char *tag, const void *buffer, unsigned int size);
unsigned int PACKINGBUFAPI PackingBuf_PutString(PACKINGBUF *packingbuf, const char *tag, const char *s);
unsigned int PACKINGBUFAPI PackingBuf_PutStringEx(PACKINGBUF *packingbuf, const char *tag, const char *s, unsigned int length);

unsigned int PACKINGBUFAPI PackingBuf_PutBinaryBegin(PACKINGBUF *packingbuf, void **buffer, unsigned int *size);
unsigned int PACKINGBUFAPI PackingBuf_PutBinaryEnd(PACKINGBUF *packingbuf, unsigned int size, const char *tag);
unsigned int PACKINGBUFAPI PackingBuf_PutStringBegin(PACKINGBUF *packingbuf, void **buffer, unsigned int *size);
unsigned int PACKINGBUFAPI PackingBuf_PutStringEnd(PACKINGBUF *packingbuf, unsigned int length, const char *tag);
unsigned int PACKINGBUFAPI PackingBuf_PutVectorBegin(PACKINGBUF *packingbuf, PACKINGBUF_VECTOR *vector, enum PACKINGBUF_VECTOR_TYPE vectorType);
unsigned int PACKINGBUFAPI PackingBuf_PutVectorEnd(PACKINGBUF *packingbuf, PACKINGBUF_VECTOR *vector, const char *tag);

unsigned int PACKINGBUFAPI PackingBufVector_PutUint8(PACKINGBUF_VECTOR *vector,  uint8_t value);
unsigned int PACKINGBUFAPI PackingBufVector_PutUint16(PACKINGBUF_VECTOR *vector, uint16_t value);
unsigned int PACKINGBUFAPI PackingBufVector_PutUint32(PACKINGBUF_VECTOR *vector, uint32_t value);

unsigned int PACKINGBUFAPI PackingBufVector_PutInt8(PACKINGBUF_VECTOR *vector,  int8_t value);
unsigned int PACKINGBUFAPI PackingBufVector_PutInt16(PACKINGBUF_VECTOR *vector, int16_t value);
unsigned int PACKINGBUFAPI PackingBufVector_PutInt32(PACKINGBUF_VECTOR *vector, int32_t value);

unsigned int PACKINGBUFAPI PackingBufVector_PutInt(PACKINGBUF_VECTOR *vector,  int value);
unsigned int PACKINGBUFAPI PackingBufVector_PutUint(PACKINGBUF_VECTOR *vector, unsigned int value);
unsigned int PACKINGBUFAPI PackingBufVector_PutFloat(PACKINGBUF_VECTOR *vector, float value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKINGBUFAPI PackingBufVector_PutInt64(PACKINGBUF_VECTOR *vector,  int64_t value);
unsigned int PACKINGBUFAPI PackingBufVector_PutUint64(PACKINGBUF_VECTOR *vector, uint64_t value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKINGBUFAPI PackingBufVector_PutDouble(PACKINGBUF_VECTOR *vector, double value);
#endif
#endif

unsigned int PACKINGBUFAPI PackingBuf_Count(PACKINGBUF *packingbuf, int origin);
unsigned int PACKINGBUFAPI PackingBuf_Skip(PACKINGBUF *packingbuf);
unsigned int PACKINGBUFAPI PackingBuf_Get(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBuf_Peek(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBuf_Find(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value, const char *tag, uint8_t casecmp);

unsigned int PACKINGBUFAPI PackingBufValue_GetInt8(PACKINGBUF_VALUE *value,  int8_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetInt16(PACKINGBUF_VALUE *value, int16_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetInt32(PACKINGBUF_VALUE *value, int32_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetUint8(PACKINGBUF_VALUE *value,  uint8_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetUint16(PACKINGBUF_VALUE *value, uint16_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetUint32(PACKINGBUF_VALUE *value, uint32_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetFloat(PACKINGBUF_VALUE *value, float *dst);

unsigned int PACKINGBUFAPI PackingBufValue_IsNull(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsInt(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsUint(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsInteger(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsFloat(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsIntVector(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsUintVector(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsIntegerVector(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsString(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_IsBinary(PACKINGBUF_VALUE *value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKINGBUFAPI PackingBufValue_GetInt64(PACKINGBUF_VALUE *value, int64_t *dst);
unsigned int PACKINGBUFAPI PackingBufValue_GetUint64(PACKINGBUF_VALUE *value, uint64_t *dst);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKINGBUFAPI PackingBufValue_GetDouble(PACKINGBUF_VALUE *value, double *dst);
unsigned int PACKINGBUFAPI PackingBufValue_IsDouble(PACKINGBUF_VALUE *value);
#endif
#endif

unsigned int PACKINGBUFAPI PackingBufValue_GetSize(PACKINGBUF_VALUE *value);
unsigned int PACKINGBUFAPI PackingBufValue_GetIntSize(PACKINGBUF_VALUE *value);

unsigned int PACKINGBUFAPI PackingBufValue_GetString(PACKINGBUF_VALUE *value, PACKINGBUF_STRING *to);
unsigned int PACKINGBUFAPI PackingBufValue_GetBinary(PACKINGBUF_VALUE *value, PACKINGBUF_BINARY *to);
unsigned int PACKINGBUFAPI PackingBufValue_GetVector(PACKINGBUF_VALUE *value, PACKINGBUF_VECTOR *vector);

unsigned int PACKINGBUFAPI PackingBufVector_GetInt8(PACKINGBUF_VECTOR *vector,  int8_t *value);
unsigned int PACKINGBUFAPI PackingBufVector_GetInt16(PACKINGBUF_VECTOR *vector, int16_t *value);
unsigned int PACKINGBUFAPI PackingBufVector_GetInt32(PACKINGBUF_VECTOR *vector, int32_t *value);

unsigned int PACKINGBUFAPI PackingBufVector_GetUint8(PACKINGBUF_VECTOR *vector,  uint8_t *value);
unsigned int PACKINGBUFAPI PackingBufVector_GetUint16(PACKINGBUF_VECTOR *vector, uint16_t *value);
unsigned int PACKINGBUFAPI PackingBufVector_GetUint32(PACKINGBUF_VECTOR *vector, uint32_t *value);

unsigned int PACKINGBUFAPI PackingBufVector_GetFloat(PACKINGBUF_VECTOR *vector,  float *value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKINGBUFAPI PackingBufVector_GetInt64(PACKINGBUF_VECTOR *vector,  int64_t *value);
unsigned int PACKINGBUFAPI PackingBufVector_GetUint64(PACKINGBUF_VECTOR *vector, uint64_t *value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKINGBUFAPI PackingBufVector_GetDouble(PACKINGBUF_VECTOR *vector, double *value);
#endif
#endif

#ifdef __cplusplus
};
#endif

#endif //LIB_IPVODDIF_PACKINGBUF_H

