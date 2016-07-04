#ifndef LIB_PACKBUF_H
#define LIB_PACKBUF_H

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

#ifndef PACKBUFAPI
    #define PACKBUFAPI
#endif

#define SIZEOF_PACKBUF_TAG 1
typedef uint8_t PACKBUF_TAG;

//PACKBUF(Lightwight/Tagged Data Interchange Format)
typedef struct PACKBUF        PACKBUF;
typedef struct PACKBUF_VALUE  PACKBUF_VALUE;
typedef struct PACKBUF_STRING PACKBUF_STRING;
typedef struct PACKBUF_BINARY PACKBUF_BINARY;
typedef struct PACKBUF_VECTOR PACKBUF_VECTOR;
typedef struct PACKBUF_VALUE_LIST PACKBUF_VALUE_LIST;

enum //PACKBUF_TYPE
{
    PACKBUF_TYPE_MIN           = 0,
    PACKBUF_TYPE_UINT          = 0,
    PACKBUF_TYPE_SINT          = 1,
    PACKBUF_TYPE_FIXED         = 2,
    PACKBUF_TYPE_STRING        = 3,
    PACKBUF_TYPE_BINARY        = 4,
    PACKBUF_TYPE_UINT_VECTOR   = 5,
    PACKBUF_TYPE_SINT_VECTOR   = 6,
    PACKBUF_TYPE_FLOAT_VECTOR  = 7,
    PACKBUF_TYPE_MAX           = 7,
};

enum PACKBUF_VECTOR_TYPE
{
    PACKBUF_VECTOR_TYPE_UINT  = PACKBUF_TYPE_UINT_VECTOR,
    PACKBUF_VECTOR_TYPE_SINT  = PACKBUF_TYPE_SINT_VECTOR,
    PACKBUF_VECTOR_TYPE_FLOAT = PACKBUF_TYPE_FLOAT_VECTOR,
};

//#include "packpush8.h"
struct PACKBUF
{
	unsigned char  *buffer; //buffer[size]
	unsigned int    size;   //sizeof(buffer)
	unsigned int    position;//current position
};

struct PACKBUF_VALUE
{
    unsigned char *data; //data[size]
    unsigned int   size; //sizeof(*data)
    PACKBUF_TAG    tag;  //field tag
    uint8_t        type; //PACKBUF_TYPE_*
};

struct PACKBUF_STRING
{
    char         *data;  //data[length+1]
    unsigned int  length;//strlen(length)
};

struct PACKBUF_BINARY
{
    unsigned char  *data;//data[size]
    unsigned int    size;//sizeof(value)
};

struct PACKBUF_VECTOR
{
	unsigned char  *buffer; //buffer[size]
	unsigned int    size;   //sizeof(buffer)
	unsigned int    position;//current position
    unsigned int    type;   //PACKBUF_VECTOR_TYPE_*
};

struct PACKBUF_VALUE_LIST
{
    unsigned int    count;
    PACKBUF_VALUE  *list;
};
//#include "packpop.h"

#ifdef __cplusplus
extern "C"
{
#endif

void         PACKBUFAPI PackBuf_Init(PACKBUF *packbuf, void *pBuffer, unsigned int nBufferSize);
unsigned int PACKBUFAPI PackBuf_Finish(PACKBUF *packbuf);
void         PACKBUFAPI PackBuf_Reset(PACKBUF *packbuf);

unsigned int PACKBUFAPI PackBuf_PutNull(PACKBUF *packbuf, PACKBUF_TAG tag);
unsigned int PACKBUFAPI PackBuf_PutUint8(PACKBUF *packbuf, PACKBUF_TAG tag, uint8_t value);
unsigned int PACKBUFAPI PackBuf_PutInt8(PACKBUF *packbuf, PACKBUF_TAG tag, int8_t value);
unsigned int PACKBUFAPI PackBuf_PutUint16(PACKBUF *packbuf, PACKBUF_TAG tag, uint16_t value);
unsigned int PACKBUFAPI PackBuf_PutInt16(PACKBUF *packbuf, PACKBUF_TAG tag, int16_t value);
unsigned int PACKBUFAPI PackBuf_PutUint32(PACKBUF *packbuf, PACKBUF_TAG tag, uint32_t value);
unsigned int PACKBUFAPI PackBuf_PutInt32(PACKBUF *packbuf, PACKBUF_TAG tag, int32_t value);
unsigned int PACKBUFAPI PackBuf_PutFloat(PACKBUF *packbuf, PACKBUF_TAG tag, float value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKBUFAPI PackBuf_PutUint64(PACKBUF *packbuf, PACKBUF_TAG tag, uint64_t value);
unsigned int PACKBUFAPI PackBuf_PutInt64(PACKBUF *packbuf, PACKBUF_TAG tag, int64_t value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBuf_PutDouble(PACKBUF *packbuf, PACKBUF_TAG tag, double value);
#endif
#endif

unsigned int PACKBUFAPI PackBuf_PutBinary(PACKBUF *packbuf, PACKBUF_TAG tag, const void *buffer, unsigned int size);
unsigned int PACKBUFAPI PackBuf_PutString(PACKBUF *packbuf, PACKBUF_TAG tag, const char *s);
unsigned int PACKBUFAPI PackBuf_PutStringEx(PACKBUF *packbuf, PACKBUF_TAG tag, const char *s, unsigned int length);
unsigned int PACKBUFAPI PackBuf_PutBinaryBegin(PACKBUF *packbuf, void **buffer, unsigned int *size);
unsigned int PACKBUFAPI PackBuf_PutBinaryEnd(PACKBUF *packbuf, unsigned int size, PACKBUF_TAG tag);
unsigned int PACKBUFAPI PackBuf_PutStringBegin(PACKBUF *packbuf, void **buffer, unsigned int *size);
unsigned int PACKBUFAPI PackBuf_PutStringEnd(PACKBUF *packbuf, unsigned int length, PACKBUF_TAG tag);

unsigned int PACKBUFAPI PackBuf_PutVectorBegin(PACKBUF *packbuf, PACKBUF_VECTOR *vector, enum PACKBUF_VECTOR_TYPE vectorType);
unsigned int PACKBUFAPI PackBuf_PutVectorEnd(PACKBUF *packbuf, PACKBUF_VECTOR *vector, PACKBUF_TAG tag);

unsigned int PACKBUFAPI PackBufVector_PutUint8(PACKBUF_VECTOR *vector,  uint8_t value);
unsigned int PACKBUFAPI PackBufVector_PutUint16(PACKBUF_VECTOR *vector, uint16_t value);
unsigned int PACKBUFAPI PackBufVector_PutUint32(PACKBUF_VECTOR *vector, uint32_t value);

unsigned int PACKBUFAPI PackBufVector_PutInt8(PACKBUF_VECTOR *vector,  int8_t value);
unsigned int PACKBUFAPI PackBufVector_PutInt16(PACKBUF_VECTOR *vector, int16_t value);
unsigned int PACKBUFAPI PackBufVector_PutInt32(PACKBUF_VECTOR *vector, int32_t value);

unsigned int PACKBUFAPI PackBufVector_PutInt(PACKBUF_VECTOR *vector,  int value);
unsigned int PACKBUFAPI PackBufVector_PutUint(PACKBUF_VECTOR *vector, unsigned int value);
unsigned int PACKBUFAPI PackBufVector_PutFloat(PACKBUF_VECTOR *vector, float value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKBUFAPI PackBufVector_PutInt64(PACKBUF_VECTOR *vector,  int64_t value);
unsigned int PACKBUFAPI PackBufVector_PutUint64(PACKBUF_VECTOR *vector, uint64_t value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufVector_PutDouble(PACKBUF_VECTOR *vector, double value);
#endif
#endif

unsigned int PACKBUFAPI PackBuf_Count(PACKBUF *packbuf, int origin);
unsigned int PACKBUFAPI PackBuf_Skip(PACKBUF *packbuf);
unsigned int PACKBUFAPI PackBuf_Get(PACKBUF *packbuf, PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBuf_Peek(PACKBUF *packbuf, PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBuf_Find(PACKBUF *packbuf, PACKBUF_VALUE *value, PACKBUF_TAG tag);

unsigned int PACKBUFAPI PackBufValue_GetInt8(PACKBUF_VALUE *value,  int8_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetInt16(PACKBUF_VALUE *value, int16_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetInt32(PACKBUF_VALUE *value, int32_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetUint8(PACKBUF_VALUE *value,  uint8_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetUint16(PACKBUF_VALUE *value, uint16_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetUint32(PACKBUF_VALUE *value, uint32_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetFloat(PACKBUF_VALUE *value, float *dst);

#if ENABLE_INT64_SUPPORT
unsigned int PACKBUFAPI PackBufValue_GetInt64(PACKBUF_VALUE *value, int64_t *dst);
unsigned int PACKBUFAPI PackBufValue_GetUint64(PACKBUF_VALUE *value, uint64_t *dst);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufValue_GetDouble(PACKBUF_VALUE *value, double *dst);
#endif
#endif

unsigned int PACKBUFAPI PackBufValue_IsNull(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsInt(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsUint(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsInteger(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsFloat(PACKBUF_VALUE *value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufValue_IsDouble(PACKBUF_VALUE *value);
#endif

unsigned int PACKBUFAPI PackBufValue_IsIntVector(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsUintVector(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsIntegerVector(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsString(PACKBUF_VALUE *value);
unsigned int PACKBUFAPI PackBufValue_IsBinary(PACKBUF_VALUE *value);

unsigned int PACKBUFAPI PackBufValue_GetIntSize(PACKBUF_VALUE *value);

unsigned int PACKBUFAPI PackBufValue_GetString(PACKBUF_VALUE *value, PACKBUF_STRING *to);
unsigned int PACKBUFAPI PackBufValue_GetBinary(PACKBUF_VALUE *value, PACKBUF_BINARY *to);
unsigned int PACKBUFAPI PackBufValue_GetVector(PACKBUF_VALUE *value, PACKBUF_VECTOR *vector);

unsigned int PACKBUFAPI PackBufVector_GetInt8(PACKBUF_VECTOR *vector,  int8_t *value);
unsigned int PACKBUFAPI PackBufVector_GetInt16(PACKBUF_VECTOR *vector, int16_t *value);
unsigned int PACKBUFAPI PackBufVector_GetInt32(PACKBUF_VECTOR *vector, int32_t *value);

unsigned int PACKBUFAPI PackBufVector_GetUint8(PACKBUF_VECTOR *vector,  uint8_t *value);
unsigned int PACKBUFAPI PackBufVector_GetUint16(PACKBUF_VECTOR *vector, uint16_t *value);
unsigned int PACKBUFAPI PackBufVector_GetUint32(PACKBUF_VECTOR *vector, uint32_t *value);

unsigned int PACKBUFAPI PackBufVector_GetFloat(PACKBUF_VECTOR *vector,  float *value);

#if ENABLE_INT64_SUPPORT
unsigned int PACKBUFAPI PackBufVector_GetInt64(PACKBUF_VECTOR *vector,  int64_t *value);
unsigned int PACKBUFAPI PackBufVector_GetUint64(PACKBUF_VECTOR *vector, uint64_t *value);
#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufVector_GetDouble(PACKBUF_VECTOR *vector, double *value);
#endif
#endif

#ifdef __cplusplus
};
#endif

#endif //LIB_PACKBUF_H
