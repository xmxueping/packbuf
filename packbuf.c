#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "packbuf.h"

#define assert(x) ((void)0)

#if (defined(__BORLANDC__))
    #pragma warn -8008
    #pragma warn -8066
#endif
#if (defined(_MSC_VER))
    #pragma warning( disable : 4200)
#endif
#if (defined(_MSC_VER) || defined(__BORLANDC__))
    #define inline __inline
#endif

//changed carefully
#define PACKBUF_LENGTH_FIELD_MASK      0x1f
#define PACKBUF_LENGTH_FIELD_BITS      5
#define PACKBUF_LONG_LENGTH_OCTETS_MAX 4
#define PACKBUF_SHORT_LENGTH_MAX       ((1<<PACKBUF_LENGTH_FIELD_BITS)-1-PACKBUF_LONG_LENGTH_OCTETS_MAX)
#define PACKBUF_LONG_LENGTH_MIN        (PACKBUF_SHORT_LENGTH_MAX+1)

#define ZIGZAG_SINT8(n)  ((n << 1) ^ (n >> 7))
#define ZIGZAG_SINT16(n) ((n << 1) ^ (n >> 15))
#define ZIGZAG_SINT32(n) ((n << 1) ^ (n >> 31))
#define ZIGZAG_SINT64(n) ((n << 1) ^ (n >> 63))
#define UNZIGZAG(n)      ((n >> 1) ^ (0-(n & 1)))

static inline
uint8_t PackBuf_EncodeTag(void* dst, PACKBUF_TAG tag)
{
    unsigned char* ptr = (unsigned char*)dst;

    #if (SIZEOF_PACKBUF_TAG >= 4)
    *ptr++ = (unsigned char)(tag>>24);
    #endif
    #if (SIZEOF_PACKBUF_TAG >= 3)
    *ptr++ = (unsigned char)(tag>>16);
    #endif
    #if (SIZEOF_PACKBUF_TAG >= 2)
    *ptr++ = (unsigned char)(tag>>8);
    #endif
    *ptr = (unsigned char)tag;

    return (SIZEOF_PACKBUF_TAG);
}

static inline
PACKBUF_TAG PackBuf_DecodeTag(void* src)
{
    unsigned char* ptr = (unsigned char*)src;
    PACKBUF_TAG tag = 0;

    #if (SIZEOF_PACKBUF_TAG >= 4)
    tag |= *ptr++;
    tag <<= 8;
    #endif
    #if (SIZEOF_PACKBUF_TAG >= 3)
    tag |= *ptr++;
    tag <<= 8;
    #endif
    #if (SIZEOF_PACKBUF_TAG >= 2)
    tag |= *ptr++;
    tag <<= 8;
    #endif
    tag |= *ptr;

    return tag;
}

static
double PackBuf_atof(const char *s, unsigned int c)
{
    double val=0.0, power=1.0;
    unsigned char sign = 0;

    while ((c!=0) && isspace(*s))
    {
        s++;c--;
    }

    if (c != 0)
    {
        if (*s == '-')
        {
            s++;c--;
            sign = 1;
        }
        else if (*s == '+')
        {
            s++;c--;
        }

        while ((c!=0) && isdigit(*s))
        {
            val = val * 10 + (*s - '0');
            s++;c--;
        }

        if ((c!=0) && (*s == '.'))
        {
            s++;c--;
            while ((c!=0) && isdigit(*s))
            {
                val = val * 10 + (*s - '0');
                power = power * 10;
                s++;c--;
            }
        }

        if ((c!=0) && ((*s == 'e') || (*s == 'E')))
        {
            s++;c--;
            if (c!=0)
            {
                int exp = atoi(s);
                while (exp < 0)
                {
                    val /= 10;
                    exp++;
                }
                while (exp > 0)
                {
                    val *= 10;
                    exp--;
                }
            }
        }

        val /= power;
    }

    return sign ? -val : val;
}

static
int32_t PackBuf_atoi32(const char *s, unsigned int c)
{
    int32_t val = 0;
    unsigned char sign = 0;

    while ((c>0) && isspace(*s))
    {
        s++;c--;
    }

    if (c != 0)
    {

        if (*s == '-')
        {
            sign = 1;
            s++;c--;
        }
        else if (*s == '+')
        {
            s++;c--;
        }

        while ((c>0) && isdigit(*s))
        {
            val = val * 10 + *s++ - '0';
            c--;
        }
    }

    return sign ? -val : val;
}

#if ENABLE_INT64_SUPPORT
static
int64_t PackBuf_atoi64(const char *s, unsigned int c)
{
    int64_t n = 0;
    unsigned char sign = 0;

    while ((c>0) && isspace(*s))
    {
        s++;c--;
    }

    if (c != 0)
    {

        if (*s == '-')
        {
            sign = 1;
            s++;c--;
        }
        else if (*s == '+')
        {
            s++;c--;
        }

        while ((c>0) && isdigit(*s))
        {
            n = n * 10 + *s++ - '0';
            c--;
        }
    }

    return sign ? -n : n;
}
#endif

static
uint32_t PackBuf_btoi32(const unsigned char *s, unsigned int c)
{
    uint32_t n = 0;
    while (c > 0)
    {
        n = (n<<8) | (*s);
        s++;
        c--;
    }

    return n;
}

#if ENABLE_INT64_SUPPORT
static
uint64_t PackBuf_btoi64(const unsigned char *s, unsigned int c)
{
    uint64_t n = 0;
    while (c > 0)
    {
        n = (n<<8) | (*s);
        s++;
        c--;
    }

    return n;
}
#endif

static inline
uint32_t encode_float(float value)
{
    union {float f; uint32_t i;} temp;
    temp.f=value;
    return temp.i;
}

static inline
float decode_float(uint32_t value)
{
    union {float f; uint32_t i;} temp;
    temp.i=value;
    return temp.f;
}

#if ENABLE_INT64_SUPPORT && ENABLE_DOUBLE_SUPPORT
static inline
uint64_t encode_double(double value)
{
    union {double d; uint64_t i;} temp;
    temp.d=value;
    return temp.i;
}

static inline
double decode_double(uint64_t value)
{
    union {double d; uint64_t i;} temp;
    temp.i=value;
    return temp.d;
}
#endif

static inline
uint8_t encode_uint8(void* dst, uint8_t val)
{
    unsigned char *ptr = (unsigned char *)dst;

    if (val == 0)
    {
        return 0;
    }else
    {
        *ptr = val;
        return 1;
    }
}

static inline
uint8_t get_uint8_encoding_size(uint8_t val)
{
    if (val == 0)
    {
        return 0;
    }else
    {
        return 1;
    }
}

static inline
uint8_t encode_uint16(void* dst, uint16_t val)
{
    unsigned char *ptr = (unsigned char *)dst;

    if (val == 0)
    {
        return 0;
    }else
    if (val < (1<<8))
    {
        *ptr = (unsigned char)val;
        return 1;
    }else
    {
        *ptr++ = (unsigned char)(val>>8);
        *ptr   = (unsigned char)(val   );
        return 2;
    }
}

static inline
uint8_t get_uint16_encoding_size(uint16_t val)
{
    if (val == 0)
    {
        return 0;
    }else
    if (val < (1<<8))
    {
        return 1;
    }else
    {
        return 2;
    }
}

static inline
uint8_t encode_uint32(void* dst, uint32_t val)
{
    unsigned char *ptr = (unsigned char *)dst;

    if (val == 0)
    {
        return 0;
    }else
    if (val < ((uint32_t)1<<8))
    {
        *ptr = (unsigned char)val;
        return 1;
    }else
    if (val < ((uint32_t)1<<16))
    {
        *ptr++ = (unsigned char)(val>>8);
        *ptr   = (unsigned char)(val   );
        return 2;
    }else
    if (val < ((uint32_t)1<<24))
    {
        *ptr++ = (unsigned char)(val>>16);
        *ptr++ = (unsigned char)(val>>8 );
        *ptr   = (unsigned char)(val    );
        return 3;
    }else
    {
        *ptr++ = (unsigned char)(val>>24);
        *ptr++ = (unsigned char)(val>>16);
        *ptr++ = (unsigned char)(val>>8 );
        *ptr   = (unsigned char)(val    );
        return 4;
    }
}

static inline
uint8_t get_uint32_encoding_size(uint32_t val)
{
    if (val == 0)
    {
        return 0;
    }else
    if (val < ((uint32_t)1<<8))
    {
        return 1;
    }else
    if (val < ((uint32_t)1<<16))
    {
        return 2;
    }else
    if (val < ((uint32_t)1<<24))
    {
        return 3;
    }else
    {
        return 4;
    }
}

#if 0
static inline
uint8_t encode_uint32_EX(void* dst, uint64_t val)
{
    unsigned char *ptr = (unsigned char *)dst;
    int8_t shift = 24;

    do
    {
        if (((val>>shift)&0xff) != 0)
        {
            do
            {
                *ptr++ = (val>>shift);
                shift -= 8;
            }while (shift >= 0);
            break;
        }
        shift -= 8;
    }while (shift >= 0);

    return (char *)ptr-(char *)dst;
}

static inline
uint8_t get_uint32_encoding_size_EX(uint32_t val)
{
    uint8_t count = 0;
    int8_t  shift = 24;

    do
    {
        if (((val>>shift)&0xff) != 0)
        {
            do
            {
                count++;
                shift -= 8;
            }while (shift >= 0);
            break;
        }
        shift -= 8;
    }while (shift >= 0);

    return count;
}
#endif

#if ENABLE_INT64_SUPPORT
static inline
uint8_t encode_uint64(void* dst, uint64_t val)
{
    unsigned char *ptr = (unsigned char *)dst;
    int8_t shift = 56;

    do
    {
        if (((val>>shift)&0xff) != 0)
        {
            do
            {
                *ptr++ = (unsigned char)(val>>shift);
                shift -= 8;
            }while (shift >= 0);
            break;
        }
        shift -= 8;
    }while (shift >= 0);

    return (char *)ptr-(char *)dst;
}

static inline
uint8_t get_uint64_encoding_size(uint64_t val)
{
    uint8_t count = 0;
    int8_t  shift = 56;

    do
    {
        if (((val>>shift)&0xff) != 0)
        {
            do
            {
                count++;
                shift -= 8;
            }while (shift >= 0);
            break;
        }
        shift -= 8;
    }while (shift >= 0);

    return count;
}
#endif

static inline
uint8_t encode_varint8(void* dst, uint8_t val)
{
    unsigned char* ptr = (unsigned char*)dst;

    if (val < (1<<7))
    {
        *ptr = (unsigned char)(val);
        return 1;
    }else
    {
        *ptr++ = (unsigned char)((val>>7) | 0x80);
        *ptr   = (unsigned char)((val   ) & 0x7f);
        return 2;
    }
}

static inline
uint8_t encode_varint16(void* dst, uint16_t val)
{
    unsigned char* ptr = (unsigned char*)dst;

    if (val < (1<<7))
    {
        *ptr = (unsigned char)val;
        return 1;
    }else
    if (val < (1<<14))
    {
        *ptr++ = (unsigned char)((val>>7) | 0x80);
        *ptr   = (unsigned char)((val   ) & 0x7f);
        return 2;
    }else
    {
        *ptr++ = (unsigned char)((val>>14) | 0x80);
        *ptr++ = (unsigned char)((val>>7 ) | 0x80);
        *ptr   = (unsigned char)((val    ) & 0x7f);
        return 3;
    }
}

static inline
uint8_t encode_varint32(void* dst, uint32_t val)
{
    unsigned char* ptr = (unsigned char*)dst;

    if (val < ((uint32_t)1<<7))
    {
        *ptr = (unsigned char)val;
        return 1;
    }else
    if (val < ((uint32_t)1<<14))
    {
        *ptr++ = (unsigned char)((val>>7) | 0x80);
        *ptr   = (unsigned char)((val   ) & 0x7f);
        return 2;
    }else
    if (val < ((uint32_t)1<<21))
    {
        *ptr++ = (unsigned char)((val>>14) | 0x80);
        *ptr++ = (unsigned char)((val>>7 ) | 0x80);
        *ptr   = (unsigned char)((val    ) & 0x7f);
        return 3;
    }else
    if (val < ((uint32_t)1<<28))
    {
        *ptr++ = (unsigned char)((val>>21) | 0x80);
        *ptr++ = (unsigned char)((val>>14) | 0x80);
        *ptr++ = (unsigned char)((val>>7 ) | 0x80);
        *ptr   = (unsigned char)((val    ) & 0x7f);
        return 4;
    }else
    {
        *ptr++ = (unsigned char)((val>>28) | 0x80);
        *ptr++ = (unsigned char)((val>>21) | 0x80);
        *ptr++ = (unsigned char)((val>>14) | 0x80);
        *ptr++ = (unsigned char)((val>>7 ) | 0x80);
        *ptr   = (unsigned char)((val    ) & 0x7f);
        return 5;
    }
}

static
void* decode_varint8(const void *src
                    ,const void *limit
                    ,uint8_t*    value)
{
    unsigned char *ptr = (unsigned char *)src;
    
    if (ptr < (unsigned char *)limit)
    {
        if ((*ptr&0x80) == 0)
        {
            *value = *ptr++;
            return ptr;
        }else
        {
            uint8_t result = (*ptr++)&0x7f;
            while (ptr < (unsigned char *)limit)
            {
                result <<= 7;
                result  |= (*ptr)&0x7f;

                if (((*ptr++)&0x80) == 0)
                {
                    *value = result;
                    return ptr;
                }
            }
        }
    }

    return NULL;
}

static
void* decode_varint16(const void *src
                     ,const void *limit
                     ,uint16_t*   value)
{
    unsigned char* ptr = (unsigned char *)src;

    if (ptr < (unsigned char *)limit)
    {
        if ((*ptr&0x80) == 0)
        {
            *value = *ptr++;
            return ptr;
        }else
        {
            uint16_t result = (*ptr++)&0x7f;
            while (ptr < (unsigned char *)limit)
            {
                result <<= 7;
                result  |= (*ptr)&0x7f;

                if (((*ptr++)&0x80) == 0)
                {
                    *value = result;
                    return ptr;
                }
            }
        }
    }

    return NULL;
}

static
void* decode_varint32(const void *src
                     ,const void *limit
                     ,uint32_t*   value)
{
    unsigned char* ptr = (unsigned char *)src;

    if (ptr < (unsigned char *)limit)
    {
        if ((*ptr&0x80) == 0)
        {
            *value = *ptr++;
            return ptr;
        }else
        {
            uint32_t result = (*ptr++)&0x7f;
            while (ptr < (unsigned char *)limit)
            {
                result <<= 7;
                result  |= (*ptr)&0x7f;

                if (((*ptr++)&0x80) == 0)
                {
                    *value = result;
                    return ptr;
                }
            }
        }
    }

    return NULL;
}

#if 0
static
uint8_t encode_varint32_EX(void* dst, uint32_t val)
{
    unsigned char *ptr = (unsigned char *)dst;
    uint8_t shift = 28;

    do
    {
        if (((val>>shift)&0x7f) != 0)
        {
            break;
        }
        shift -= 7;
    }while (shift >= 7);
    while (shift != 0)
    {
        *ptr++ = (unsigned char)((val>>shift)|0x80);
        shift -= 7;
    }
    *ptr++ = val&0x7f;

    return (char *)ptr-(char *)dst;
}
#endif

#if ENABLE_INT64_SUPPORT
static
unsigned int encode_varint64(void* dst, uint64_t val)
{
    unsigned char *ptr = (unsigned char *)dst;
    uint8_t shift = 63;

    do
    {
        if (((val>>shift)&0x7f) != 0)
        {
            break;
        }
        shift -= 7;
    }while (shift >= 7);
    while (shift != 0)
    {
        *ptr++ = (unsigned char)((val>>shift)|0x80);
        shift -= 7;
    }
    *ptr++ = (unsigned char)(val&0x7f);

    return (char *)ptr-(char *)dst;
}

static
const void* decode_varint64(const void *src
                           ,const void *limit
                           ,uint64_t*   value)
{
    unsigned char* ptr = (unsigned char*)src;

    if (ptr < (unsigned char *)limit)
    {
        if ((*ptr&0x80) == 0)
        {
            *value = *ptr++;
            return ptr;
        }else
        {
            uint64_t result = (*ptr++)&0x7f;

            while (ptr < (unsigned char *)limit)
            {
                result <<= 7;
                result  |= (*ptr)&0x7f;

                if (((*ptr++)&0x80) == 0)
                {
                    *value = result;
                    return ptr;
                }
            }
        }
    }

    return NULL;
}

static
unsigned int count_varint_size(const void *src, const void *limit)
{
    unsigned char* ptr = (unsigned char*)src;
    unsigned int   count = 0;

    if (ptr < (unsigned char *)limit)
    {
        count++;
        if ((*ptr&0x80) != 0)
        {
            ptr++;
            while (ptr < (unsigned char *)limit)
            {
                count++;
                if (((*ptr++)&0x80) == 0)
                {
                    break;
                }
            }
        }
    }

    return count;
}
#endif

void PACKBUFAPI PackBuf_Init(PACKBUF *packbuf, void *pBuffer, unsigned int nBufferSize)
{
    packbuf->buffer = (unsigned char*)pBuffer;
    packbuf->size = nBufferSize;
    packbuf->position = 0;
}

unsigned int PACKBUFAPI PackBuf_Finish(PACKBUF *packbuf)
{
    return packbuf->position;
}

void PACKBUFAPI PackBuf_Reset(PACKBUF *packbuf)
{
    if (packbuf->buffer != NULL)
    {
        packbuf->buffer -= packbuf->position;
    }
    packbuf->position = 0;
}

unsigned int PACKBUFAPI PackBuf_PutNull(PACKBUF *packbuf, PACKBUF_TAG tag)
{
    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: SIZEOF_PACKBUF_TAG octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;
        packbuf->position += SIZEOF_PACKBUF_TAG;

        //type+length: 1 octet
        packbuf->buffer[0] = (PACKBUF_TYPE_FIXED<<PACKBUF_LENGTH_FIELD_BITS)|0;
        packbuf->buffer++;
        packbuf->position++;

        //data: 0 octet
    }else
    {
        //header: sizeof(tag)+1 octets
        packbuf->position += SIZEOF_PACKBUF_TAG+1;

        //data: 0 octet
    }

    return SIZEOF_PACKBUF_TAG+1;
}

static inline
unsigned int put_uint8(PACKBUF *packbuf, PACKBUF_TAG tag, uint8_t value, unsigned char type)
{
    unsigned int byteCount = get_uint8_encoding_size(value);

    assert(byteCount <= PACKBUF_SHORT_LENGTH_MAX);
    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: (SIZEOF_PACKBUF_TAG) octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: (1) octet
        packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS)|byteCount;
        packbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint8(packbuf->buffer, value);
        packbuf->buffer += byteCount;
    }

    byteCount += SIZEOF_PACKBUF_TAG+1;//add header length to get total count
    packbuf->position += byteCount;

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutUint8(PACKBUF *packbuf, PACKBUF_TAG tag, uint8_t value)
{
    return put_uint8(packbuf, tag, value, PACKBUF_TYPE_UINT);
}

unsigned int PACKBUFAPI PackBuf_PutInt8(PACKBUF *packbuf, PACKBUF_TAG tag, int8_t value)
{
    return put_uint8(packbuf, tag, (uint8_t)ZIGZAG_SINT8(value) ,PACKBUF_TYPE_SINT);
}

static inline
unsigned int put_uint16(PACKBUF *packbuf, PACKBUF_TAG tag, uint16_t value, unsigned char type)
{
    unsigned int byteCount = get_uint16_encoding_size(value);

    assert(byteCount <= PACKBUF_SHORT_LENGTH_MAX);
    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: (SIZEOF_PACKBUF_TAG) octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: (1) octet
        packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS)|byteCount;
        packbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint16(packbuf->buffer, value);
        packbuf->buffer += byteCount;
    }

    byteCount += SIZEOF_PACKBUF_TAG+1;//add header length to get total count
    packbuf->position += byteCount;

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutUint16(PACKBUF *packbuf, PACKBUF_TAG tag, uint16_t value)
{
    return put_uint16(packbuf, tag, value, PACKBUF_TYPE_UINT);
}

unsigned int PACKBUFAPI PackBuf_PutInt16(PACKBUF *packbuf, PACKBUF_TAG tag, int16_t value)
{
    return put_uint16(packbuf, tag, (uint16_t)ZIGZAG_SINT16(value), PACKBUF_TYPE_SINT);
}

static inline
unsigned int put_fixed32(PACKBUF *packbuf, PACKBUF_TAG tag, uint32_t value, uint8_t size, uint8_t type)
{
    assert(size <= sizeof(value));/*1,2,4*/
    assert(size <= PACKBUF_SHORT_LENGTH_MAX);

    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+size) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+size) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: (SIZEOF_PACKBUF_TAG) octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: 1 octet
        packbuf->buffer[0] = (type << PACKBUF_LENGTH_FIELD_BITS) | (size);
        packbuf->buffer++;

        //data: (size) octets
        {
            int8_t shift = (size-1)*8;
            do
            {
                packbuf->buffer[0] = (unsigned char)(value>>shift);
                packbuf->buffer++;
                shift -= 8;
            }while (shift >= 0);
        }
    }
    packbuf->position += SIZEOF_PACKBUF_TAG+1+size;

    return (SIZEOF_PACKBUF_TAG+1+size);
}

static inline
unsigned int put_uint32(PACKBUF *packbuf, PACKBUF_TAG tag, uint32_t value, unsigned char type)
{
    unsigned int byteCount = get_uint32_encoding_size(value);

    assert(byteCount <= PACKBUF_SHORT_LENGTH_MAX);
    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: (SIZEOF_PACKBUF_TAG) octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: (1) octet
        packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS)|byteCount;
        packbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint32(packbuf->buffer, value);
        packbuf->buffer += byteCount;
    }

    byteCount += SIZEOF_PACKBUF_TAG+1;//add header length to get total count
    packbuf->position += byteCount;

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutUint32(PACKBUF *packbuf, PACKBUF_TAG tag, uint32_t value)
{
    return put_uint32(packbuf, tag, value, PACKBUF_TYPE_UINT);
}

unsigned int PACKBUFAPI PackBuf_PutInt32(PACKBUF *packbuf, PACKBUF_TAG tag, int32_t value)
{
    return put_uint32(packbuf, tag, (uint32_t)ZIGZAG_SINT32(value), PACKBUF_TYPE_SINT);
}

unsigned int PACKBUFAPI PackBuf_PutUint(PACKBUF *packbuf, PACKBUF_TAG tag, unsigned int value)
{
    if (sizeof(int) == 4)
    {
        return put_uint32(packbuf, tag, value, PACKBUF_TYPE_UINT);
    }else
    if (sizeof(int) == 2)
    {
        return put_uint16(packbuf, tag, value, PACKBUF_TYPE_UINT);
    }else
    if (sizeof(int) == 1)
    {
        return put_uint8(packbuf, tag, value, PACKBUF_TYPE_UINT);
    }
    return 0;
}

unsigned int PACKBUFAPI PackBuf_PutInt(PACKBUF *packbuf, PACKBUF_TAG tag, int value)
{
    if (sizeof(int) == 4)
    {
        return put_uint32(packbuf, tag, (uint32_t)ZIGZAG_SINT32(value), PACKBUF_TYPE_SINT);
    }else
    if (sizeof(int) == 2)
    {
        return put_uint16(packbuf, tag, (uint32_t)ZIGZAG_SINT16(value), PACKBUF_TYPE_SINT);
    }else
    if (sizeof(int) == 1)
    {
        return put_uint8(packbuf, tag, (uint32_t)ZIGZAG_SINT8(value), PACKBUF_TYPE_SINT);
    }
    return 0;
}

unsigned int PACKBUFAPI PackBuf_PutFloat(PACKBUF *packbuf, PACKBUF_TAG tag, float value)
{
    return put_fixed32(packbuf, tag, encode_float(value), sizeof(uint32_t), PACKBUF_TYPE_FIXED);
}

#if ENABLE_INT64_SUPPORT
static inline
unsigned int put_fixed64(PACKBUF *packbuf, PACKBUF_TAG tag, uint64_t value, uint8_t size, uint8_t type)
{
    assert(size<=sizeof(value));/*1,2,4,8*/
    assert(size <= PACKBUF_SHORT_LENGTH_MAX);

    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+size) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+size) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: SIZEOF_PACKBUF_TAG octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: 1 octet
        packbuf->buffer[0] = (type << PACKBUF_LENGTH_FIELD_BITS) | (size);
        packbuf->buffer++;

        //data: size octets
        {
            int8_t shift = (size-1)*8;
            do
            {
                packbuf->buffer[0] = (unsigned char)(value>>shift);
                packbuf->buffer++;
                shift -= 8;
            }while (shift >= 0);
        }
    }
    packbuf->position += SIZEOF_PACKBUF_TAG+1+size;

    return (SIZEOF_PACKBUF_TAG+1+size);
}

static inline
unsigned int put_uint64(PACKBUF *packbuf, PACKBUF_TAG tag, uint64_t value, unsigned char type)
{
    unsigned int byteCount = get_uint64_encoding_size(value);

    assert(byteCount <= PACKBUF_SHORT_LENGTH_MAX);
    if (packbuf->buffer != NULL)
    {
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) < packbuf->position)
        {
            return 0;//overflow
        }
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+byteCount) > packbuf->size)
        {
            return 0;//oom
        }

        //tag: (SIZEOF_PACKBUF_TAG) octets
        PackBuf_EncodeTag(packbuf->buffer, tag);
        packbuf->buffer += SIZEOF_PACKBUF_TAG;

        //type+length: (1) octet
        packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS)|byteCount;
        packbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint64(packbuf->buffer, value);
        packbuf->buffer += byteCount;
    }

    byteCount += SIZEOF_PACKBUF_TAG+1;//add header length to get total count
    packbuf->position += byteCount;

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutUint64(PACKBUF *packbuf, PACKBUF_TAG tag, uint64_t value)
{
    return put_uint64(packbuf, tag, value, PACKBUF_TYPE_UINT);
}

unsigned int PACKBUFAPI PackBuf_PutInt64(PACKBUF *packbuf, PACKBUF_TAG tag, int64_t value)
{
    return put_uint64(packbuf, tag, (uint64_t)ZIGZAG_SINT64(value), PACKBUF_TYPE_SINT);
}

#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBuf_PutDouble(PACKBUF *packbuf, PACKBUF_TAG tag, double value)
{
    return put_fixed64(packbuf, tag, encode_double(value), sizeof(uint64_t), PACKBUF_TYPE_FIXED);
}
#endif
#endif

static
unsigned int put_binary(PACKBUF *packbuf, PACKBUF_TAG tag, const void *buffer, unsigned int size, unsigned char type)
{
    unsigned int byteCount = 0;

    if (packbuf->buffer != NULL)
    {
        if (size <= PACKBUF_SHORT_LENGTH_MAX)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+0)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length+data: (1+0+size) octets
                packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS) | (size);
                memcpy(&packbuf->buffer[1], buffer, size);

                size += (1+0);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+1)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+1) octets
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+1));
                packbuf->buffer[1] = (unsigned char)(size);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data:(size) octets
                memcpy(&packbuf->buffer[2], buffer, size);

                size += (1+1);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+2)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+2) octets
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<< PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+2));
                packbuf->buffer[1] = (unsigned char)((size>>8) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size   ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packbuf->buffer[3], buffer, size);

                size += (1+2);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+3)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+3) octets
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+3));
                packbuf->buffer[1] = (unsigned char)((size>>16) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size>>8 ) & 0xff);
                packbuf->buffer[3] = (unsigned char)((size    ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packbuf->buffer[4], buffer, size);

                size += (1+3);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+4)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+4) octets
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+4));
                packbuf->buffer[1] = (unsigned char)((size>>24) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size>>16) & 0xff);
                packbuf->buffer[3] = (unsigned char)((size>>8 ) & 0xff);
                packbuf->buffer[4] = (unsigned char)((size    ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packbuf->buffer[5], buffer, size);

                size += (1+4);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #endif
        #endif
        #endif
        #endif
        }
    }else
    {
        if (size <= PACKBUF_SHORT_LENGTH_MAX)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+0)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+0);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+1)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+1);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+2)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+2);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+3)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+3);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(SIZEOF_PACKBUF_TAG+(1+4)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+4);
        #endif
        #endif
        #endif
        #endif
        }

        assert(packbuf->position+size > packbuf->position);
        packbuf->position += size;
        byteCount += size;
    }

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutBinary(PACKBUF *packbuf, PACKBUF_TAG tag, const void *buffer, unsigned int size)
{
    return put_binary(packbuf, tag, buffer, size, PACKBUF_TYPE_BINARY);
}

unsigned int PACKBUFAPI PackBuf_PutString(PACKBUF *packbuf, PACKBUF_TAG tag, const char *s)
{
    unsigned int l=strlen(s)+1; //include terminated '0'

    return put_binary(packbuf, tag, s, l, PACKBUF_TYPE_STRING);
}

unsigned int PACKBUFAPI PackBuf_PutStringEx(PACKBUF *packbuf, PACKBUF_TAG tag, const char *s, unsigned int length)
{
    unsigned int ir=put_binary(packbuf, tag, s, length+1, PACKBUF_TYPE_STRING);//include terminated '0'

    if ((ir!=0) && (packbuf->buffer!=NULL))
    {
        packbuf->buffer[-1] = 0;//force terminated '0'
    }

    return ir;
}

static inline
unsigned int put_binary_begin(PACKBUF *packbuf, void **buffer, unsigned int *size)
{
    if (packbuf->buffer!=NULL)
    {
        //header: SIZEOF_PACKBUF_TAG+1 octets
        //data: 0 octets
        //return (pointer & size) according this position
        if ((packbuf->position+SIZEOF_PACKBUF_TAG+1+0) > packbuf->size)
        {
            return 0;
        }

        if (buffer!=NULL)
        {
            *buffer=&packbuf->buffer[SIZEOF_PACKBUF_TAG+1+0];
        }
        if (size!=NULL)
        {
            *size=packbuf->size-(packbuf->position+SIZEOF_PACKBUF_TAG+1+0);
        }
    }else
    {
        if (buffer!=NULL)
        {
            *buffer=NULL;
        }
        if (size!=NULL)
        {
            *size=0;
        }
    }

    return SIZEOF_PACKBUF_TAG+1+0;
}

static
unsigned int put_binary_end(PACKBUF *packbuf, unsigned int size, PACKBUF_TAG tag, unsigned char type)
{
    unsigned int byteCount = 0;

    if (packbuf->buffer != NULL)
    {
        if (size <= PACKBUF_SHORT_LENGTH_MAX)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+0)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+0)+size > size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+0) octet
                packbuf->buffer[0] = (type<<PACKBUF_LENGTH_FIELD_BITS) | (size);

                //data: (size) octets
                //memcpy(&packbuf->buffer[1], buffer, size);

                size += (1+0);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+1)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+1)+size > size);

                memmove(&packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+1)], &packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+0)], size);

                //tag: (SIZEOF_PACKBUF_TAG) octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: (1+1) octets
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+1));
                packbuf->buffer[1] = (unsigned char)(size);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                //memcpy(&packbuf->buffer[2],buffer,size);

                size += (1+1);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+2)+size) < packbuf->size))
            {
                assert(SIZEOF_PACKBUF_TAG+(1+2)+size > size);

                memmove(&packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+2)], &packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+0)], size);

                //tag: SIZEOF_PACKBUF_TAG octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: 1+2 octet
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+2));
                packbuf->buffer[1] = (unsigned char)((size>>8) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size   ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packbuf->buffer[3],buffer,size);

                size += (1+2);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size) < packbuf->size))
            {
                assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+3)+size > packbuf->position);
                assert(SIZEOF_PACKBUF_TAG+(1+3)+size > size);

                memmove(&packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+3)], &packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+0)], size);

                //tag: SIZEOF_PACKBUF_TAG octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: 1+3 octet
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+3));
                packbuf->buffer[1] = (unsigned char)((size>>16) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size>>8 ) & 0xff);
                packbuf->buffer[3] = (unsigned char)((size    ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packbuf->buffer[4],buffer,size);

                size += (1+3);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size > packbuf->position); //not overflow
            assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size < packbuf->size); //not oom

            if (((packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size) > packbuf->position) &&
                ((packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size) < packbuf->size))
            {
                assert(packbuf->position+SIZEOF_PACKBUF_TAG+(1+4)+size > packbuf->position);
                assert(SIZEOF_PACKBUF_TAG+(1+4)+size > size);

                memmove(&packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+4)], &packbuf->buffer[SIZEOF_PACKBUF_TAG+(1+0)], size);

                //tag: SIZEOF_PACKBUF_TAG octets
                PackBuf_EncodeTag(packbuf->buffer, tag);
                byteCount += SIZEOF_PACKBUF_TAG;
                packbuf->buffer += SIZEOF_PACKBUF_TAG;
                packbuf->position += SIZEOF_PACKBUF_TAG;

                //type+length: 1+4 octet
                size -= PACKBUF_LONG_LENGTH_MIN;
                packbuf->buffer[0] = (unsigned char)((type<<PACKBUF_LENGTH_FIELD_BITS) | (PACKBUF_SHORT_LENGTH_MAX+4));
                packbuf->buffer[1] = (unsigned char)((size>>24) & 0xff);
                packbuf->buffer[2] = (unsigned char)((size>>16) & 0xff);
                packbuf->buffer[3] = (unsigned char)((size>>8 ) & 0xff);
                packbuf->buffer[4] = (unsigned char)((size    ) & 0xff);
                size += PACKBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packbuf->buffer[5],buffer,size);

                size += (1+4);
                byteCount += size;
                packbuf->buffer += size;
                packbuf->position += size;
            }
        #endif
        #endif
        #endif
        #endif
        }
    }else
    {
        if (size <= PACKBUF_SHORT_LENGTH_MAX)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+0)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+0);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+1)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+1);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+2)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+2);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKBUF_LONG_LENGTH_MIN)
        {
            assert(SIZEOF_PACKBUF_TAG+(1+3)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+3);
        #if (PACKBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(SIZEOF_PACKBUF_TAG+(1+4)+size > size);
            size += SIZEOF_PACKBUF_TAG+(1+4);
        #endif
        #endif
        #endif
        #endif
        }

        assert(packbuf->position+size > packbuf->position);
        packbuf->position += size;
        byteCount += size;
    }

    return byteCount;
}

unsigned int PACKBUFAPI PackBuf_PutBinaryBegin(PACKBUF *packbuf, void **buffer, unsigned int *size)
{
    return put_binary_begin(packbuf, buffer, size);
}

unsigned int PACKBUFAPI PackBuf_PutBinaryEnd(PACKBUF *packbuf, unsigned int size, PACKBUF_TAG tag)
{
    return put_binary_end(packbuf, size, tag, PACKBUF_TYPE_BINARY);
}

unsigned int PACKBUFAPI PackBuf_PutStringBegin(PACKBUF *packbuf, void **buffer, unsigned int *size)
{
    return put_binary_begin(packbuf, buffer, size);
}

unsigned int PACKBUFAPI PackBuf_PutStringEnd(PACKBUF *packbuf, unsigned int length, PACKBUF_TAG tag)
{
    unsigned int ir=put_binary_end(packbuf, length+1, tag, PACKBUF_TYPE_STRING);//include terminated '0'

    if ((ir!=0) && (packbuf->buffer!=NULL))
    {
        packbuf->buffer[-1]=0;//force terminated '0'
    }

    return ir;
}

static
unsigned int vector_put_varint8(PACKBUF_VECTOR *vector, uint8_t value)
{
    unsigned char buffer[2];//the largest 8 bits integer needs 2 bytes
    unsigned char count;
    unsigned char size = encode_varint8(buffer,value);

    if ((vector->position+size) < vector->position)
    {
        return 0;//overflow
    }
    if ((vector->position+size) > vector->size)
    {
        return 0;//oom
    }

    for (count=0; count<size; count++)
    {
        vector->buffer[count] = buffer[count];
    }
    vector->buffer += size;
    vector->position += size;

    return size;
}

static
unsigned int vector_put_varint16(PACKBUF_VECTOR *vector, uint16_t value)
{
    unsigned char buffer[3];//the largest 16 bits integer needs 3 bytes
    unsigned char count;
    unsigned char size = encode_varint16(buffer,value);

    if ((vector->position+size) < vector->position)
    {
        return 0;//overflow
    }
    if ((vector->position+size) > vector->size)
    {
        return 0;//oom
    }

    for (count=0; count<size; count++)
    {
        vector->buffer[count] = buffer[count];
    }
    vector->buffer += size;
    vector->position += size;

    return size;
}

static
unsigned int vector_put_varint32(PACKBUF_VECTOR *vector, uint32_t value)
{
    unsigned char buffer[5];//the largest 32 bits integer needs 5 bytes
    unsigned char count;
    unsigned char size = encode_varint32(buffer,value);

    if ((vector->position+size) < vector->position)
    {
        return 0;//overflow
    }
    if ((vector->position+size) > vector->size)
    {
        return 0;//oom
    }

    for (count=0; count<size; count++)
    {
        vector->buffer[count] = buffer[count];
    }
    vector->buffer += size;
    vector->position += size;

    return size;
}

unsigned int PACKBUFAPI PackBufVector_PutUint8(PACKBUF_VECTOR *vector, uint8_t value)
{
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint8(vector, value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int8_t v=(int8_t)value;
        return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(v));
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutUint16(PACKBUF_VECTOR *vector, uint16_t value)
{
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint16(vector, value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int16_t v=(int16_t)value;
        return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(v));
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutUint32(PACKBUF_VECTOR *vector, uint32_t value)
{
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int32_t v=(int32_t)value;
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutInt8(PACKBUF_VECTOR *vector, int8_t value)
{
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint8(vector, (uint32_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutInt16(PACKBUF_VECTOR *vector, int16_t value)
{
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint16(vector, (uint32_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutInt32(PACKBUF_VECTOR *vector, int32_t value)
{
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutInt(PACKBUF_VECTOR *vector, int value)
{
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(value));
        }else
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(value));
        }else
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(value));
        }
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)value);
        }else
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)value);
        }else
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)value);
        }
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutUint(PACKBUF_VECTOR *vector, unsigned int value)
{
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)value);
        }else
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)value);
        }else
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)value);
        }
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        if (sizeof(int)==4)
        {
            int32_t v=(int32_t)value;
            return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
        }else
        if (sizeof(int)==2)
        {
            int16_t v=(int16_t)value;
            return vector_put_varint32(vector, (uint16_t)ZIGZAG_SINT16(v));
        }else
        if (sizeof(int)==1)
        {
            int8_t v=(int8_t)value;
            return vector_put_varint32(vector, (uint8_t)ZIGZAG_SINT8(v));
        }
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutFloat(PACKBUF_VECTOR *vector, float value)
{
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int32_t v=(int32_t)value;
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
    }else
    {
    }

    return 0;
}

#if ENABLE_INT64_SUPPORT
static
unsigned int vector_put_varint64(PACKBUF_VECTOR *vector, uint64_t value)
{
    unsigned char buffer[10];//the largest 64 bits integer needs 10 bytes
    unsigned int  count;
    unsigned int  size = encode_varint64(buffer,value);

    if (size != count_varint_size(buffer, buffer+size))
    {
        int i=0;
        i++;
    }

    if ((vector->position+size) < vector->position)
    {
        return 0;//overflow
    }
    if ((vector->position+size) > vector->size)
    {
        return 0;//oom
    }

    for (count=0; count<size; count++)
    {
        vector->buffer[count] = buffer[count];
    }
    vector->buffer += size;
    vector->position += size;

    return size;
}

unsigned int PACKBUFAPI PackBufVector_PutUint64(PACKBUF_VECTOR *vector, uint64_t value)
{
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int64_t v=(int64_t)value;
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(v));
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double((double)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_PutInt64(PACKBUF_VECTOR *vector, int64_t value)
{
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double((double)value));
    }else
    {
    }

    return 0;
}

#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufVector_PutDouble(PACKBUF_VECTOR *vector, double value)
{
    if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double(value));
    }else
    if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
    {   /* PACKBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)value);
    }else
    if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
    {   /* PACKBUF_TYPE_SINT_VECTOR */
        int64_t v=(int64_t)value;
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(v));
    }else
    {
    }

    return 0;
}
#endif
#endif

#define DEFINE_PACKBUFVECTOR_GETSINT32(NAME,TYPE)                           \
unsigned int PACKBUFAPI PackBufVector_Get##NAME(PACKBUF_VECTOR *vector,TYPE *value) \
{                                                                           \
    uint32_t temp;                                                          \
    unsigned char *ptr = (unsigned char *)decode_varint32(vector->buffer    \
                                                         ,vector->buffer    \
                                                         +(vector->size-vector->position) \
                                                         ,&temp);   \
    if (ptr != NULL)                                                \
    {                                                               \
        unsigned int size = ptr-vector->buffer;                     \
        vector->buffer += size;                                     \
        vector->position += size;                                   \
                                                                    \
        if (value != NULL)                                          \
        {                                                           \
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)           \
            {   /* PACKBUF_TYPE_SINT_VECTOR */                      \
                *value = (TYPE)((int32_t)UNZIGZAG(temp));           \
            }else                                                   \
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)           \
            {   /* PACKBUF_TYPE_UINT_VECTOR */                      \
                *value = (TYPE)temp;                                \
            }else                                                   \
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)          \
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */                     \
                *value = (TYPE)decode_float(temp);                  \
            }else                                                   \
            {                                                       \
                return 0;                                           \
            }                                                       \
        }                                                           \
        return size;                                                \
    }                                                               \
                                                                    \
    return 0;                                                       \
}
//int8/16/32
DEFINE_PACKBUFVECTOR_GETSINT32(Int8,   int8_t);
DEFINE_PACKBUFVECTOR_GETSINT32(Int16,  int16_t);
DEFINE_PACKBUFVECTOR_GETSINT32(Int32,  int32_t);

#define DEFINE_PACKBUFVECTOR_GETUINT32(NAME,TYPE)                           \
unsigned int PACKBUFAPI PackBufVector_Get##NAME(PACKBUF_VECTOR *vector,TYPE *value) \
{                                                                           \
    uint32_t temp;                                                          \
    unsigned char *ptr = (unsigned char *)decode_varint32(vector->buffer    \
                                                         ,vector->buffer    \
                                                         +(vector->size-vector->position) \
                                                         ,&temp);   \
    if (ptr != NULL)                                                \
    {                                                               \
        unsigned int size = ptr-vector->buffer;                     \
        vector->buffer += size;                                     \
        vector->position += size;                                   \
                                                                    \
        if (value != NULL)                                          \
        {                                                           \
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)           \
            {   /* PACKBUF_TYPE_UINT_VECTOR */                      \
                *value = (TYPE)temp;                                \
            }else                                                   \
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)           \
            {   /* PACKBUF_TYPE_SINT_VECTOR */                      \
                *value = (TYPE)((int32_t)UNZIGZAG(temp));           \
            }else                                                   \
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)          \
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */                     \
                *value = (TYPE)decode_float(temp);                  \
            }else                                                   \
            {                                                       \
                return 0;                                           \
            }                                                       \
        }                                                           \
        return size;                                                \
    }                                                               \
                                                                    \
    return 0;                                                       \
}
//uint8/16/32
DEFINE_PACKBUFVECTOR_GETUINT32(Uint8,  uint8_t);
DEFINE_PACKBUFVECTOR_GETUINT32(Uint16, uint16_t);
DEFINE_PACKBUFVECTOR_GETUINT32(Uint32, uint32_t);

#if 0
unsigned int PACKBUFAPI PackBufVector_GetUint32(PACKBUF_VECTOR *vector, uint32_t *value)
{
    uint32_t temp;
    const char *ptr = decode_varint32(vector->buffer
                                     ,vector->buffer+(vector->size-vector->position)
                                     ,&temp);
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = temp;
            }else
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {   /* PACKBUF_TYPE_SINT_VECTOR */
                *value = (uint32_t)((int32_t)UNZIGZAG(temp));
            }else
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
                *value = (uint32_t)decode_float((uint32_t)temp);
            }else
            {
                return 0;
            }
        }
        return size;
    }

    return 0;
}
#endif

unsigned int PACKBUFAPI PackBufVector_GetFloat(PACKBUF_VECTOR *vector,float *value)
{
#if ENABLE_DOUBLE_SUPPORT
    uint64_t temp;
    unsigned char *ptr = (unsigned char *)decode_varint64(vector->buffer
                                                         ,vector->buffer
                                                         +(vector->size-vector->position)
                                                         ,&temp);
#else
    uint32_t temp;
    unsigned char *ptr = (unsigned char *)decode_varint32(vector->buffer
                                                         ,vector->buffer
                                                         +(vector->size-vector->position)
                                                         ,&temp);
#endif
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
#if ENABLE_DOUBLE_SUPPORT
                if (size > 5)
                {   //Double-precision
                    *value = (float)decode_double(temp);
                }else
#endif
                {   //Single-precision
                    *value = decode_float((uint32_t)temp);
                }
            }else
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = (float)temp;
            }else
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {   /* PACKBUF_TYPE_SINT_VECTOR */
#if ENABLE_DOUBLE_SUPPORT
                *value = (float)((int64_t)UNZIGZAG(temp));
#else
                *value = (float)((int32_t)UNZIGZAG(temp));
#endif
            }else
            {
                return 0;
            }
        }
        return size;
    }
    return 0;
}


#if ENABLE_INT64_SUPPORT
unsigned int PACKBUFAPI PackBufVector_GetInt64(PACKBUF_VECTOR *vector, int64_t *value)
{
    uint64_t temp;
    unsigned char *ptr = (unsigned char *)decode_varint64(vector->buffer
                                                         ,vector->buffer
                                                         +(vector->size-vector->position)
                                                         ,&temp);
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {   /* PACKBUF_TYPE_SINT_VECTOR */
                *value = (int64_t)UNZIGZAG(temp);
            }else
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = (int64_t)temp;
            }else
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
                if (size <= 5)
                {   //Single-precision
                    *value = (int64_t)decode_float((uint32_t)temp);
                }else
                {   //Double-precision 
                    *value = (int64_t)decode_double((uint64_t)temp);
                }
            }else
            {
                return 0;
            }
        }
        return size;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufVector_GetUint64(PACKBUF_VECTOR *vector, uint64_t *value)
{
    uint64_t temp;
    unsigned char *ptr = (unsigned char *)decode_varint64(vector->buffer
                                                         ,vector->buffer
                                                         +(vector->size-vector->position)
                                                         ,&temp);
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = temp;
            }else
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {   /* PACKBUF_TYPE_SINT_VECTOR */
                *value = (uint64_t)((int64_t)UNZIGZAG(temp));
            }else
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
                if (size <= 5)
                {   //Single-precision
                    *value = (uint64_t)decode_float((uint32_t)temp);
                }else
                {   //Double-precision
                    *value = (uint64_t)decode_double((uint64_t)temp);
                }
            }else
            {
                return 0;
            }
        }
        return size;
    }

    return 0;
}

#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufVector_GetDouble(PACKBUF_VECTOR *vector,double *value)
{
    uint64_t temp;
    unsigned char *ptr = (unsigned char *)decode_varint64(vector->buffer
                                                         ,vector->buffer
                                                         +(vector->size-vector->position)
                                                         ,&temp);
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
                if (size > 5)
                {   //Double-precision
                    *value = decode_double(temp);
                }else
                {   //Single-precision
                    *value = (double)decode_float((uint32_t)temp);
                }
            }else
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = (double)temp;
            }else
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {   /* PACKBUF_TYPE_SINT_VECTOR */
                *value = (double)((int64_t)UNZIGZAG(temp));
            }else
            {
                return 0;
            }
        }
        return size;
    }
    return 0;
}
#endif
#endif

#if 0
unsigned int PACKBUFAPI PackBufVector_GetInt8(PACKBUF_VECTOR *vector, int8_t *value)
{
    uint32_t temp;
    const char *ptr = decode_varint32(vector->buffer
                                     ,vector->buffer+(vector->size-vector->position)
                                     ,&temp);
    if (ptr != NULL)
    {
        unsigned int size = ptr-vector->buffer;
        vector->buffer += size;
        vector->position += size;

        if (value != NULL)
        {
            if (vector->type == PACKBUF_TYPE_SINT_VECTOR)
            {
                *value = (int8_t)UNZIGZAG(temp);
            }else
            if (vector->type == PACKBUF_TYPE_UINT_VECTOR)
            {
                *value = (int8_t)temp;
            }else
            {
                //PACKBUF_TYPE_FLOAT_VECTOR
                *value = (int8_t)decode_float(temp);
            }
        }
        return size;
    }

    return 0;
}
#endif

unsigned int PACKBUFAPI PackBuf_PutVectorBegin(PACKBUF *packbuf, PACKBUF_VECTOR *vector, enum PACKBUF_VECTOR_TYPE vectorType)
{
    unsigned int ir = put_binary_begin(packbuf, (void**)&vector->buffer, &vector->size);
    if (ir != 0)
    {
        vector->type = vectorType;
        vector->position = 0;
        return ir;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBuf_PutVectorEnd(PACKBUF *packbuf, PACKBUF_VECTOR *vector, PACKBUF_TAG tag)
{
    return put_binary_end(packbuf, vector->position, tag, vector->type);
}

unsigned int PACKBUFAPI PackBuf_Find(PACKBUF *packbuf, PACKBUF_VALUE *value, PACKBUF_TAG tag)
{
    unsigned char *buffer = packbuf->buffer;
    unsigned int   position = packbuf->position;
    unsigned int   valueSize;

    do
    {
        if ((position+SIZEOF_PACKBUF_TAG+1) < position)
        {
            break;//overflow
        }
        if ((position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            break;//oom
        }

        if (PackBuf_DecodeTag(buffer) == tag)
        {
            value->tag = tag;
            buffer += SIZEOF_PACKBUF_TAG;
            position += SIZEOF_PACKBUF_TAG;

            value->type =(buffer[0]>>PACKBUF_LENGTH_FIELD_BITS)&0xff;

            valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
            if (valueSize <= PACKBUF_SHORT_LENGTH_MAX)
            {
                //1 octet hdr, 0 octet length(extented), valueSize octets data
                if ((position+1+0+valueSize) < position)
                {
                    break;//overflow
                }
                if ((position+1+0+valueSize) > packbuf->size)
                {
                    break;//oom
                }

                value->data = &buffer[1];
                value->size = valueSize;
            }else
            {
                uint8_t index;
                uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

                //1 octet hdr, [count] octets length(extented)
                if ((position+1+0+count) < position)
                {
                    break;//overflow
                }
                if ((position+1+count) > packbuf->size)
                {
                    break;
                }

                valueSize = 0;
                for (index=1; index<=count; index++)
                {
                    valueSize = (valueSize<<8)|buffer[index];
                }

                valueSize += PACKBUF_LONG_LENGTH_MIN;
                //1 octet hdr, [count] octets length(extented), valueSize octets data
                if ((position+1+count+valueSize) < position)
                {
                    break;//overflow
                }
                if ((position+1+count+valueSize) > packbuf->size)
                {
                    break;//oom
                }
                if ((position+1+count+valueSize) < valueSize)
                {
                    break;//overflow
                }

                value->data = &buffer[1+count];
                value->size = valueSize;

                valueSize += count;
            }

            valueSize += SIZEOF_PACKBUF_TAG+1;
            return valueSize;
        }else
        {
            buffer += SIZEOF_PACKBUF_TAG;
            position += SIZEOF_PACKBUF_TAG;

            valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
            if (valueSize > PACKBUF_SHORT_LENGTH_MAX)
            {
                uint8_t index;
                uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

                if ((position+1+count) < position)
                {
                    break;//overflow
                }
                if ((position+1+count) > packbuf->size)
                {
                    break;//oom
                }

                valueSize = 0;
                for (index=1; index<=count; index++)
                {
                    valueSize = (valueSize<<8)|buffer[index];
                }

                valueSize += PACKBUF_LONG_LENGTH_MIN;
                valueSize += count;
            }

            //add size of (value-hdr)
            valueSize += 1;
            if ((position+valueSize) < position)
            {
                break;//overflow
            }
            if ((position+valueSize) < valueSize)
            {
                break;//overflow
            }
            if ((position+valueSize) > packbuf->size)
            {
                break;
            }

            buffer += valueSize;
            position += valueSize;
        }
    }while(1);

    return 0;
}

unsigned int PACKBUFAPI PackBuf_Count(PACKBUF *packbuf, int origin)
{
    unsigned char *buffer = packbuf->buffer;
    unsigned int   position = packbuf->position;
    unsigned int   count=0;
    unsigned int   valueSize;

    if (origin)
    {
        buffer -= position;
        position=0;
    }

    do
    {
        if ((position+SIZEOF_PACKBUF_TAG+1) < position)
        {
            break;//overflow
        }
        if ((position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            break;//oom
        }

        buffer += SIZEOF_PACKBUF_TAG;
        position += SIZEOF_PACKBUF_TAG;

        valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
        if (valueSize > PACKBUF_SHORT_LENGTH_MAX)
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

            if ((position+1+count) < position)
            {
                break;//overflow
            }
            if ((position+1+count) > packbuf->size)
            {
                break;//oom
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKBUF_LONG_LENGTH_MIN;
            valueSize += count;
        }

        //add size of (value-hdr)
        valueSize += 1;
        if ((position+valueSize) < position)
        {
            break;//overflow
        }
        if ((position+valueSize) > packbuf->size)
        {
            break;//oom
        }

        buffer += valueSize;
        position += valueSize;
        count++;
    }while(1);

    return count;
}

unsigned int PACKBUFAPI PackBuf_Skip(PACKBUF *packbuf)
{
    do
    {
        unsigned char *buffer = packbuf->buffer;
        unsigned int   position = packbuf->position;
        unsigned int   valueSize;

        if ((position+SIZEOF_PACKBUF_TAG+1) < position)
        {
            break;//overflow
        }
        if ((position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            break;//oom
        }

        //PACKBUF_TAG valueTag = PackBuf_DecodeTag(buffer);
        buffer += SIZEOF_PACKBUF_TAG;
        position += SIZEOF_PACKBUF_TAG;

        valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
        if (valueSize > PACKBUF_SHORT_LENGTH_MAX)
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

            if ((position+1+count) < position)
            {
                break;
            }
            if ((position+1+count) > packbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKBUF_LONG_LENGTH_MIN;
            valueSize += count;
        }

        valueSize += SIZEOF_PACKBUF_TAG+1;
        if ((packbuf->position+valueSize) < position)
        {
            break;
        }
        if ((packbuf->position+valueSize) > packbuf->size)
        {
            break;
        }

        packbuf->buffer += valueSize;
        packbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKBUFAPI PackBuf_Get(PACKBUF *packbuf, PACKBUF_VALUE *value)
{
    do
    {
        unsigned char *buffer = packbuf->buffer;
        unsigned int   position = packbuf->position;
        unsigned int   valueSize;

        if ((position+SIZEOF_PACKBUF_TAG+1) < position)
        {
            break;
        }
        if ((position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            break;
        }

        value->tag = PackBuf_DecodeTag(buffer);
        buffer += SIZEOF_PACKBUF_TAG;
        position += SIZEOF_PACKBUF_TAG;

        value->type =(buffer[0]>>PACKBUF_LENGTH_FIELD_BITS)&0xff;

        valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
        if (valueSize <= PACKBUF_SHORT_LENGTH_MAX)
        {
            //1 octet hdr, 0 octet length(extented), valueSize octets data
            if ((position+1+0+valueSize) < position)
            {
                break;
            }
            if ((position+1+0+valueSize) > packbuf->size)
            {
                break;
            }

            value->data = &buffer[1];
            value->size = valueSize;
        }else
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

            //1 octet hdr, [count] octets length(extented)
            if ((position+1+count) < position)
            {
                break;
            }
            if ((position+1+count) > packbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKBUF_LONG_LENGTH_MIN;
            //1 octet hdr, [count] octets length(extented), valueSize octets data
            if ((position+1+count+valueSize) < position)
            {
                break;
            }
            if ((position+1+count+valueSize) > packbuf->size)
            {
                break;
            }
            if ((position+1+count+valueSize) < valueSize)
            {
                break;
            }

            value->data = &buffer[1+count];
            value->size = valueSize;

            valueSize += count;
        }

        valueSize += SIZEOF_PACKBUF_TAG+1;
        packbuf->buffer += valueSize;
        packbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKBUFAPI PackBuf_Peek(PACKBUF *packbuf, PACKBUF_VALUE *value)
{
    do
    {
        unsigned char *buffer = packbuf->buffer;
        unsigned int   position = packbuf->position;
        unsigned int   valueSize;

        if ((position+SIZEOF_PACKBUF_TAG+1) < position)
        {
            break;
        }
        if ((position+SIZEOF_PACKBUF_TAG+1) > packbuf->size)
        {
            break;
        }

        value->tag = PackBuf_DecodeTag(buffer);
        buffer += SIZEOF_PACKBUF_TAG;
        position += SIZEOF_PACKBUF_TAG;

        value->type =(buffer[0]>>PACKBUF_LENGTH_FIELD_BITS)&0xff;

        valueSize = (buffer[0]&PACKBUF_LENGTH_FIELD_MASK);
        if (valueSize <= PACKBUF_SHORT_LENGTH_MAX)
        {
            //1 octet hdr, 0 octet length(extented), valueSize octets data
            if ((position+1+0+valueSize) < position)
            {
                break;
            }
            if ((position+1+0+valueSize) > packbuf->size)
            {
                break;
            }

            value->data = &buffer[1];
            value->size = valueSize;
        }else
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKBUF_SHORT_LENGTH_MAX);

            //1 octet hdr, [count] octets length(extented)
            if ((position+1+count) < position)
            {
                break;
            }
            if ((position+1+count) > packbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKBUF_LONG_LENGTH_MIN;
            //1 octet hdr, [count] octets length(extented), valueSize octets data
            if ((position+1+count+valueSize) < position)
            {
                break;
            }
            if ((position+1+count+valueSize) > packbuf->size)
            {
                break;
            }
            if ((position+1+count+valueSize) < valueSize)
            {
                break;
            }

            value->data = &buffer[1+count];
            value->size = valueSize;

            valueSize += count;
        }

        valueSize += SIZEOF_PACKBUF_TAG+1;
        //packbuf->buffer += valueSize;
        //packbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_GetFloat(PACKBUF_VALUE *value, float *dst)
{
    union
    {
        uint8_t  u8;
        uint16_t u16;
        uint32_t u32;
        int32_t  i32;
#if ENABLE_INT64_SUPPORT
        uint64_t u64;
#endif
    }temp;
    unsigned int count;
    unsigned int result = 0;

    switch(value->type)
    {
    case PACKBUF_TYPE_UINT:
        if (value->size <= 1)
        {   /* uint8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (float)(temp.u8);
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            if (dst != NULL)
            {
                temp.u16 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (float)(temp.u16);
            }
            result = 2;
        }else
#if ENABLE_INT64_SUPPORT
        if (value->size <= 4)
#endif
        {   /* uint32 */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (float)(temp.u32);
            }
            result = 4;
#if ENABLE_INT64_SUPPORT
        }else
        {   /* uint64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (float)(temp.u64);
            }
            result = 8;
#endif
        }
        break;

    case PACKBUF_TYPE_SINT:
        if (value->size <= 1)
        {   /* int8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (float)((int8_t)UNZIGZAG(temp.u8));
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* int16 */
            if (dst != NULL)
            {
                temp.u16=0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (float)((int16_t)UNZIGZAG(temp.u16));
            }
            result = 2;
        }else
#if ENABLE_INT64_SUPPORT
        if (value->size <= 4)
#endif
        {   /* int32 */
            if (dst != NULL)
            {
                temp.u32=0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (float)((int32_t)UNZIGZAG(temp.u32));
            }
            result = 4;
#if ENABLE_INT64_SUPPORT
        }else
        {   /* int64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (float)((int64_t)UNZIGZAG(temp.u64));
            }
            result = 8;
#endif
        }
        break;

    case PACKBUF_TYPE_FIXED:
        if (value->size == 4)
        {   /* float */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (float)decode_float(temp.u32);
            }
            result = 4;
        }else
#if ENABLE_INT64_SUPPORT
        if (value->size == 8)
        {   /* double */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (float)decode_double(temp.u64);
            }
            result = 8;
        }else
#endif
        if (value->size <= 1)
        {   /* uint8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (float)(int8_t)(temp.u8);
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            if (dst != NULL)
            {
                temp.u16 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (float)(int16_t)(temp.u16);
            }
            result = 2;
        }else
#if ENABLE_INT64_SUPPORT
        if (value->size <= 4)
#endif
        {   /* uint32 */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (float)(int32_t)(temp.u32);
            }
            result = 4;
#if ENABLE_INT64_SUPPORT
        }else
        {   /* uint64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (float)(int64_t)(temp.u64);
            }
            result = 8;
#endif
        }
        break;

    case PACKBUF_TYPE_STRING:
        if (dst != NULL)
        {
            *dst = (float)PackBuf_atof((char *)value->data,value->size);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_BINARY:
        if (dst != NULL)
        {
            temp.u32 = PackBuf_btoi32(value->data,value->size);
            *dst = (float)decode_float(temp.u32);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_UINT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = (float)(temp.u32);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_SINT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = (float)((int32_t)UNZIGZAG(temp.u32));
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_FLOAT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = decode_float(temp.u32);
        }
        result = sizeof(*dst);
        break;
    }

    return result;
}

#if ENABLE_INT64_SUPPORT && ENABLE_DOUBLE_SUPPORT
unsigned int PACKBUFAPI PackBufValue_GetDouble(PACKBUF_VALUE *value, double *dst)
{
    union
    {
        uint8_t  u8;
        uint16_t u16;
        uint32_t u32;
        int32_t  i32;
        uint64_t u64;
    }temp;
    unsigned int count;
    unsigned int result = 0;

    switch(value->type)
    {
    case PACKBUF_TYPE_UINT:
        if (value->size <= 1)
        {   /* uint8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (double)(temp.u8);
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            if (dst != NULL)
            {
                temp.u16 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (double)(temp.u16);
            }
            result = 2;
        }else
        if (value->size <= 4)
        {   /* uint32 */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (double)(temp.u32);
            }
            result = 4;
        }else
        {   /* uint64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (double)(temp.u64);
            }
            result = 8;
        }
        break;

    case PACKBUF_TYPE_SINT:
        if (value->size <= 1)
        {   /* int8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (double)((int8_t)UNZIGZAG(temp.u8));
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* int16 */
            if (dst != NULL)
            {
                temp.u16=0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (double)((int16_t)UNZIGZAG(temp.u16));
            }
            result = 2;
        }else
        if (value->size <= 4)
        {   /* int32 */
            if (dst != NULL)
            {
                temp.u32=0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (double)((int32_t)UNZIGZAG(temp.u32));
            }
            result = 4;
        }else
        {   /* int64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (double)((int64_t)UNZIGZAG(temp.u64));
            }
            result = 8;
        }
        break;

    case PACKBUF_TYPE_FIXED:
        if (value->size == 4)
        {   /* float */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (double)decode_float(temp.u32);
            }
            result = 4;
        }else
        if (value->size == 8)
        {   /* double */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (double)decode_double(temp.u64);
            }
            result = 8;
        }else
        if (value->size <= 1)
        {   /* uint8 */
            if (dst != NULL)
            {
                temp.u8 = 0;
                if (value->size != 0)
                {
                    temp.u8 = value->data[0];
                }
                *dst = (double)(temp.u8);
            }
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            if (dst != NULL)
            {
                temp.u16 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u16 <<= 8;
                    temp.u16 |= value->data[count];
                }
                *dst = (double)(temp.u16);
            }
            result = 2;
        }else
        if (value->size <= 4)
        {   /* uint32 */
            if (dst != NULL)
            {
                temp.u32 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u32 <<= 8;
                    temp.u32 |= value->data[count];
                }
                *dst = (double)(temp.u32);
            }
            result = 4;
        }else
        {   /* uint64 */
            if (dst != NULL)
            {
                temp.u64 = 0;
                for (count=0; count<value->size; count++)
                {
                    temp.u64 <<= 8;
                    temp.u64 |= value->data[count];
                }
                *dst = (double)(temp.u64);
            }
            result = 8;
        }
        break;

    case PACKBUF_TYPE_STRING:
        if (dst != NULL)
        {
            *dst = (double)PackBuf_atof((char *)value->data,value->size);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_BINARY:
        if (dst != NULL)
        {
            temp.u64 = PackBuf_btoi64(value->data,value->size);
            *dst = (double)decode_double(temp.u64);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_UINT_VECTOR:
        if (dst != NULL)
        {
            temp.u64 = 0;
            decode_varint64(value->data,value->data+value->size,&temp.u64);
            *dst = (double)(temp.u64);
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_SINT_VECTOR:
        if (dst != NULL)
        {
            temp.u64 = 0;
            decode_varint64(value->data,value->data+value->size,&temp.u64);
            *dst = (double)((int64_t)UNZIGZAG(temp.u64));
        }
        result = sizeof(*dst);
        break;

    case PACKBUF_TYPE_FLOAT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = (double)decode_float(temp.u32);
        }
        result = 4;
        break;
    }

    return result;
}
#endif

#if ENABLE_INT64_SUPPORT
#define DEFINE_PACKBUFVALUE_GET(NAME,TYPE)                                      \
unsigned int PACKBUFAPI PackBufValue_Get##NAME(PACKBUF_VALUE *value,TYPE *dst)  \
{                                                                   \
    union                                                           \
    {                                                               \
        uint8_t  u8;                                                \
        uint16_t u16;                                               \
        uint32_t u32;                                               \
        int32_t  i32;                                               \
        uint64_t u64;                                               \
        int64_t  i64;                                               \
    }temp;                                                          \
    unsigned int count;                                             \
    unsigned int result = 0;                                        \
                                                                    \
    switch (value->type)                                            \
    {                                                               \
    case PACKBUF_TYPE_UINT:                                         \
        if (value->size <= 1)                                       \
        {   /* uint8 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)(temp.u8);                             \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* uint16 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u16);                            \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        if (value->size <= 4)                                       \
        {   /* uint32 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u32);                            \
            }                                                       \
            result = 4;                                             \
        }else                                                       \
        {   /* uint64 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u64 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u64 <<= 8;                                 \
                    temp.u64 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u64);                            \
            }                                                       \
            result = 8;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_SINT:                                         \
        if (value->size <= 1)                                       \
        {   /* int8 */                                              \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)((int8_t)UNZIGZAG(temp.u8));           \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* int16 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16=0;                                         \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)((int16_t)UNZIGZAG(temp.u16));         \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        if (value->size <= 4)                                       \
        {   /* int32 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32=0;                                         \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)((int32_t)UNZIGZAG(temp.u32));         \
            }                                                       \
            result = 4;                                             \
        }else                                                       \
        {   /* int64 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u64 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u64 <<= 8;                                 \
                    temp.u64 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)((int64_t)UNZIGZAG(temp.u64));         \
            }                                                       \
            result = 8;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_FIXED:                                        \
        if (value->size == 4)                                       \
        {   /* float */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)decode_float(temp.u32);                \
            }                                                       \
            result = 4;                                             \
        }else                                                       \
        if (value->size == 8)                                       \
        {   /* double */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u64 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u64 <<= 8;                                 \
                    temp.u64 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)decode_double(temp.u64);               \
            }                                                       \
            result = 8;                                             \
        }else                                                       \
        if (value->size <= 1)                                       \
        {   /* uint8 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)(temp.u8);                             \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* uint16 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u16);                            \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        if (value->size <= 4)                                       \
        {   /* uint32 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u32);                            \
            }                                                       \
            result = 4;                                             \
        }else                                                       \
        {   /* uint64 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u64 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u64 <<= 8;                                 \
                    temp.u64 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(int64_t)(temp.u64);                   \
            }                                                       \
            result = 8;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_STRING:                                       \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* int32 */                                         \
                temp.i32 = PackBuf_atoi32((char *)value->data,value->size);    \
                *dst = (TYPE)(temp.i32);                            \
            }else                                                   \
            {   /* int64 */                                         \
                temp.i64 = PackBuf_atoi64((char *)value->data,value->size);    \
                *dst = (TYPE)(temp.i64);                            \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_BINARY:                                       \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* uint32 */                                        \
                temp.u32 = PackBuf_btoi32(value->data,value->size); \
                *dst = (TYPE)(temp.u32);                            \
            }else                                                   \
            {   /* uint64 */                                        \
                temp.u64 = PackBuf_btoi64(value->data,value->size); \
                *dst = (TYPE)(temp.u64);                            \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_UINT_VECTOR:                                  \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* uint32 */                                        \
                temp.u32 = 0;                                       \
                decode_varint32(value->data,value->data+value->size,&temp.u32); \
                *dst = (TYPE)(temp.u32);                            \
            }else                                                   \
            {   /* uint64 */                                        \
                temp.u64 = 0;                                       \
                decode_varint64(value->data,value->data+value->size,&temp.u64); \
                *dst = (TYPE)(temp.u64);                            \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_SINT_VECTOR:                                  \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* uint32 */                                        \
                temp.u32 = 0;                                       \
                decode_varint32(value->data,value->data+value->size,&temp.u32); \
                *dst = (TYPE)((int32_t)UNZIGZAG(temp.u32));         \
            }else                                                   \
            {   /* uint64 */                                        \
                temp.u64 = 0;                                       \
                decode_varint64(value->data,value->data+value->size,&temp.u64); \
                *dst = (TYPE)((int64_t)UNZIGZAG(temp.u64));         \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_FLOAT_VECTOR:                                 \
        if (dst != NULL)                                            \
        {   /* uint32 */                                            \
            temp.u32 = 0;                                           \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)decode_float(temp.u32);                    \
        }                                                           \
        result = 4;                                                 \
        break;                                                      \
    }                                                               \
                                                                    \
    return result;                                                  \
}
#else
#define DEFINE_PACKBUFVALUE_GET(NAME,TYPE)                                 \
unsigned int PACKBUFAPI PackBufValue_Get##NAME(PACKBUF_VALUE *value,TYPE *dst)  \
{                                                                   \
    union                                                           \
    {                                                               \
        uint8_t  u8;                                                \
        uint16_t u16;                                               \
        uint32_t u32;                                               \
        int32_t  i32;                                               \
    }temp;                                                          \
    unsigned int count;                                             \
    unsigned int result = 0;                                        \
                                                                    \
    switch (value->type)                                            \
    {                                                               \
    case PACKBUF_TYPE_UINT:                                         \
        if (value->size <= 1)                                       \
        {   /* uint8 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)(temp.u8);                             \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* uint16 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u16);                            \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        {   /* uint32 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u32);                            \
            }                                                       \
            result = 4;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_SINT:                                         \
        if (value->size <= 1)                                       \
        {   /* int8 */                                              \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)((int8_t)UNZIGZAG(temp.u8));           \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* int16 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16=0;                                         \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)((int16_t)UNZIGZAG(temp.u16));         \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        {   /* int32 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32=0;                                         \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)((int32_t)UNZIGZAG(temp.u32));         \
            }                                                       \
            result = 4;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_FIXED:                                        \
        if (value->size == 4)                                       \
        {   /* float */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)decode_float(temp.u32);                \
            }                                                       \
            result = 4;                                             \
        }else                                                       \
        if (value->size <= 1)                                       \
        {   /* uint8 */                                             \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u8 = 0;                                        \
                if (value->size != 0)                               \
                {                                                   \
                    temp.u8 = value->data[0];                       \
                }                                                   \
                *dst = (TYPE)(temp.u8);                             \
            }                                                       \
            result = 1;                                             \
        }else                                                       \
        if (value->size <= 2)                                       \
        {   /* uint16 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u16 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u16 <<= 8;                                 \
                    temp.u16 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u16);                            \
            }                                                       \
            result = 2;                                             \
        }else                                                       \
        {   /* uint32 */                                            \
            if (dst != NULL)                                        \
            {                                                       \
                temp.u32 = 0;                                       \
                for (count=0; count<value->size; count++)           \
                {                                                   \
                    temp.u32 <<= 8;                                 \
                    temp.u32 |= value->data[count];                 \
                }                                                   \
                *dst = (TYPE)(temp.u32);                            \
            }                                                       \
            result = 4;                                             \
        }                                                           \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_STRING:                                       \
        if (dst != NULL)                                            \
        {   /* int32 */                                             \
            temp.i32 = PackBuf_atoi32((char *)value->data,value->size);    \
            *dst = (TYPE)(temp.i32);                                \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_BINARY:                                       \
        if (dst != NULL)                                            \
        {   /* uint32 */                                            \
            temp.u32 = PackBuf_btoi32(value->data,value->size);     \
            *dst = (TYPE)(temp.u32);                                \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_UINT_VECTOR:                                  \
        if (dst != NULL)                                            \
        {   /* uint32 */                                            \
            temp.u32 = 0;                                           \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)(temp.u32);                                \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_SINT_VECTOR:                                  \
        if (dst != NULL)                                            \
        {   /* uint32 */                                            \
            temp.u32 = 0;                                           \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)((int32_t)UNZIGZAG(temp.u32));             \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKBUF_TYPE_FLOAT_VECTOR:                                    \
        if (dst != NULL)                                            \
        {   /* uint32 */                                            \
            temp.u32 = 0;                                           \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)decode_float(temp.u32);                    \
        }                                                           \
        result = 4;                                                 \
        break;                                                      \
    }                                                               \
                                                                    \
    return result;                                                  \
}
#endif

//int 8,16,32,64
DEFINE_PACKBUFVALUE_GET(Int8,  int8_t);
DEFINE_PACKBUFVALUE_GET(Int16, int16_t);
DEFINE_PACKBUFVALUE_GET(Int32, int32_t);
#if ENABLE_INT64_SUPPORT
DEFINE_PACKBUFVALUE_GET(Int64, int64_t);
#endif

//uint 8,16,32,64
DEFINE_PACKBUFVALUE_GET(Uint8,  uint8_t);
DEFINE_PACKBUFVALUE_GET(Uint16, uint16_t);
DEFINE_PACKBUFVALUE_GET(Uint32, uint32_t);
#if ENABLE_INT64_SUPPORT
DEFINE_PACKBUFVALUE_GET(Uint64, uint64_t);
#endif

//int & uint
DEFINE_PACKBUFVALUE_GET(Int,  int);
DEFINE_PACKBUFVALUE_GET(Uint, unsigned int);

unsigned int PACKBUFAPI PackBufValue_IsNull(PACKBUF_VALUE *value)
{
    return ((value->type==PACKBUF_TYPE_FIXED) && (value->size==0))?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsInt(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_SINT)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsUint(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_UINT)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsInteger(PACKBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_UINT:
    case PACKBUF_TYPE_SINT:
        return 1;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_IsFloat(PACKBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_FIXED:
        if (value->size==sizeof(float))
        {
            return 1;
        }
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_IsDouble(PACKBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_FIXED:
        if (value->size==sizeof(double))
        {
            return 1;
        }
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_IsIntVector(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_SINT_VECTOR)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsUintVector(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_UINT_VECTOR)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsIntegerVector(PACKBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_UINT_VECTOR:
    case PACKBUF_TYPE_SINT_VECTOR:
        return 1;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_IsFloatVector(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_FLOAT_VECTOR)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsString(PACKBUF_VALUE *value)
{
    return (value->type==PACKBUF_TYPE_STRING)?1:0;
}

unsigned int PACKBUFAPI PackBufValue_IsBinary(PACKBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_STRING:
    case PACKBUF_TYPE_BINARY:
        return 1;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_GetIntSize(PACKBUF_VALUE *value)
{
    unsigned int result = 0;

    switch(value->type)
    {
    case PACKBUF_TYPE_UINT:
        if (value->size <= 1)
        {   /* uint8 */
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            result = 2;
        }else
        if (value->size <= 4)
        {   /* uint32 */
            result = 4;
        }else
        {   /* uint64 */
            result = 8;
        }
        break;

    case PACKBUF_TYPE_SINT:
        if (value->size <= 1)
        {   /* int8 */
            result = 1;
        }else
        if (value->size <= 2)
        {   /* int16 */
            result = 2;
        }else
        if (value->size <= 4)
        {   /* int32 */
            result = 4;
        }else
        {   /* int64 */
            result = 8;
        }
        break;

    case PACKBUF_TYPE_FIXED:
        if (value->size == 4)
        {   /* float */
            result = 4;
        }else
        if (value->size == 8)
        {   /* double */
            result = 8;
        }else
        if (value->size <= 1)
        {   /* uint8 */
            result = 1;
        }else
        if (value->size <= 2)
        {   /* uint16 */
            result = 2;
        }else
        if (value->size <= 4)
        {   /* uint32 */
            result = 4;
        }else
        {   /* uint64 */
            result = 8;
        }
        break;

    case PACKBUF_TYPE_STRING:
        result = 4;
        break;

    case PACKBUF_TYPE_BINARY:
        result = 4;
        break;

    case PACKBUF_TYPE_UINT_VECTOR:
        result = 4;
        break;

    case PACKBUF_TYPE_SINT_VECTOR:
        result = 4;
        break;

    case PACKBUF_TYPE_FLOAT_VECTOR:
        result = 4;
        break;
    }

    return result;
}

unsigned int PACKBUFAPI PackBufValue_GetString(PACKBUF_VALUE *value, PACKBUF_STRING *to)
{
    if ((value->type == PACKBUF_TYPE_STRING) &&
        (value->size >= 1)) //value->size include terminated '0'
    {
        to->length = value->size-1;
        if (value->data[to->length] == 0)   //check terminated '0'
        {
            to->data = (char *)value->data;
            return 1;
        }
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_GetBinary(PACKBUF_VALUE *value, PACKBUF_BINARY *to)
{
    switch (value->type)
    {
    case PACKBUF_TYPE_STRING:
    case PACKBUF_TYPE_BINARY:
        to->data = value->data;
        to->size = value->size;
        return 1;
    }

    return 0;
}

unsigned int PACKBUFAPI PackBufValue_GetVector(PACKBUF_VALUE *value, PACKBUF_VECTOR *vector)
{
    switch(value->type)
    {
    case PACKBUF_TYPE_UINT_VECTOR:
    case PACKBUF_TYPE_SINT_VECTOR:
    case PACKBUF_TYPE_FLOAT_VECTOR:
        vector->buffer = value->data;
        vector->size = value->size;
        vector->position = 0;
        vector->type = value->type;
        return 1;
    }

    return 0;
}

#if (defined(__BORLANDC__))
    #pragma warn +8008
    #pragma warn +8066
#endif

#if 0
void test()
{
    static char buffer[2048];
    PACKBUF packbuf;
    unsigned int size;

    {
        char temp[16]={0,1,2,3,4,5,6,7,8,9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

        PackBuf_Init(&packbuf,buffer,sizeof(buffer));

        //int 8
        PackBuf_PutInt8(&packbuf,1,0);
        PackBuf_PutInt8(&packbuf,2,1);
        PackBuf_PutInt8(&packbuf,3,-1);
        PackBuf_PutInt8(&packbuf,4,127);
        PackBuf_PutInt8(&packbuf,5,-127);
        PackBuf_PutInt8(&packbuf,6,-128);

        //uint8
        PackBuf_PutUint8(&packbuf,1,0);
        PackBuf_PutUint8(&packbuf,2,1);
        PackBuf_PutUint8(&packbuf,3,127);
        PackBuf_PutUint8(&packbuf,4,128);
        PackBuf_PutUint8(&packbuf,5,255);
        PackBuf_PutUint8(&packbuf,6,256);

        //int16
        PackBuf_PutInt16(&packbuf,1,0);
        PackBuf_PutInt16(&packbuf,2,1);
        PackBuf_PutInt16(&packbuf,3,-1);
        PackBuf_PutInt16(&packbuf,4,127);
        PackBuf_PutInt16(&packbuf,5,-127);
        PackBuf_PutInt16(&packbuf,6,-128);
        PackBuf_PutInt16(&packbuf,7,129);
        PackBuf_PutInt16(&packbuf,8,-129);
        PackBuf_PutInt16(&packbuf,9,32767);
        PackBuf_PutInt16(&packbuf,0,-32767);
        PackBuf_PutInt16(&packbuf,0,-32768);

        //uint16
        PackBuf_PutInt16(&packbuf,0,0);
        PackBuf_PutInt16(&packbuf,0,1);
        PackBuf_PutInt16(&packbuf,0,255);
        PackBuf_PutInt16(&packbuf,0,256);
        PackBuf_PutInt16(&packbuf,0,65535);
        PackBuf_PutInt16(&packbuf,0,65536);

        //int32
        PackBuf_PutInt32(&packbuf,0,0x7fffffff);
        PackBuf_PutInt32(&packbuf,0,-1);
        PackBuf_PutInt32(&packbuf,0,-0x7fffffff);
        PackBuf_PutInt32(&packbuf,0,0xffffffff);

        //uint32
        PackBuf_PutUint32(&packbuf,0,0x7fffffff);
        PackBuf_PutUint32(&packbuf,0,0x7fffffff+1);
        PackBuf_PutUint32(&packbuf,0,0xffffffff);
        PackBuf_PutUint32(&packbuf,0,0xffffffff-1);
        PackBuf_PutUint32(&packbuf,0,0xffffffff+1);

        //float
        PackBuf_PutFloat(&packbuf,0,100.002f);
        PackBuf_PutDouble(&packbuf,0,100.002);
        PackBuf_PutFloat(&packbuf,0,100.002f);

        //uint64
        PackBuf_PutUint64(&packbuf,0,(uint64_t)0xffffffff + 0xffff);

        //string & binary
        PackBuf_PutString(&packbuf,0,"hello world!");

        PackBuf_PutBinary(&packbuf,0,temp,sizeof(temp));
        PackBuf_PutBinary(&packbuf,0,temp,sizeof(temp));
        {
            unsigned char x[1024];

            {
                int i;
                for (i=0; i<sizeof(x); i++)
                {
                    x[i] = i;
                }
            }
            PackBuf_PutBinary(&packbuf,0,x,sizeof(x));
        }

        PackBuf_PutString(&packbuf,0,"hello world1!");
        PackBuf_PutString(&packbuf,0,"hello world2!");
        PackBuf_PutString(&packbuf,0,"hello world3!");

        {
            PACKBUF_VECTOR vector;

            if (PackBuf_PutVectorBegin(&packbuf, &vector, PACKBUF_VECTOR_TYPE_UINT) != 0)
            {
                PackBufVector_PutUint(&vector, 100);
                PackBufVector_PutUint(&vector, 200);
                PackBufVector_PutUint(&vector, 300);

                PackBuf_PutVectorEnd(&packbuf, &vector, 100);
            }

            if (PackBuf_PutVectorBegin(&packbuf, &vector, PACKBUF_VECTOR_TYPE_SINT) != 0)
            {
                PackBufVector_PutInt(&vector, 100);
                PackBufVector_PutInt(&vector, -200);
                PackBufVector_PutInt(&vector, -300);

                PackBuf_PutVectorEnd(&packbuf, &vector, 100);
            }

            if (PackBuf_PutVectorBegin(&packbuf, &vector, PACKBUF_VECTOR_TYPE_FLOAT) != 0)
            {
                PackBufVector_PutFloat(&vector, 100.1);
                PackBufVector_PutFloat(&vector, 200.2);
                PackBufVector_PutFloat(&vector, 300.3);

                PackBuf_PutVectorEnd(&packbuf, &vector, 200);
            }

            {
                int iii=0;
            }
        }

        size = PackBuf_Finish(&packbuf);
    }
    {
        int8_t i8;
        uint8_t ui8;
        int16_t i16;
        uint16_t ui16;

        int32_t i32;
        uint32_t ui32;

        int64_t i64;
        uint64_t ui64;

        char temp[16];

        float f;
        double d;

        PACKBUF_VALUE value;

        PackBuf_Init(&packbuf,buffer,size);

        //int8:0
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);

        //int8:1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);

        //int8:-1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);

        //int8:127
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);

        //int8:-127
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);

        //int8:-128
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt8(&value,&i8);



        //uint8:0
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,&ui8);

        //uint8:1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,&ui8);

        //uint8:127
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,&ui8);

        //uint8:128
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,&ui8);

        //uint8:255
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,0);

        //uint8:256
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint8(&value,&ui8);

        //int16_t:0
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:127
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-127
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-128
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:129
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-129
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:32767
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-32767
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);

        //int16_t:-32768
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt16(&value,&i16);


        //uint16:0
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,&ui16);

        //uint16:1
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,&ui16);

        //uint16:255
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,0);

        //uint16:256
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,&ui16);

        //uint16:65535
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,&ui16);

        //uint16:65536
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint16(&value,&ui16);

        //int32
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt32(&value,&i32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt32(&value,&i32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt32(&value,&i32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetInt32(&value,&i32);

        //uint32
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint32(&value,&ui32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint32(&value,&ui32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint32(&value,&ui32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint32(&value,&ui32);
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetUint32(&value,&ui32);

        //float
        PackBuf_GetValue(&packbuf,&value);
        PackBufValue_GetFloat(&value,&f);

        while (PackBuf_GetValue(&packbuf, &value) != 0)
        {
            switch (value.type)
            {
            case PACKBUF_TYPE_UINT:
                PackBufValue_GetUint8(&value,  &ui8);
                PackBufValue_GetUint16(&value, &ui16);
                PackBufValue_GetUint32(&value, &ui32);
                PackBufValue_GetUint64(&value, &ui64);
                break;
            case PACKBUF_TYPE_SINT:
                PackBufValue_GetInt8(&value,  &i8);
                PackBufValue_GetInt16(&value, &i16);
                PackBufValue_GetInt32(&value, &i32);
                PackBufValue_GetInt64(&value, &i64);
                break;
            case PACKBUF_TYPE_FIXED:
                PackBufValue_GetFloat(&value,&f);
                PackBufValue_GetDouble(&value,&d);
                break;
            case PACKBUF_TYPE_STRING:
                {
                    PACKBUF_STRING string;
                    PackBufValue_GetString(&value, &string);
                    string.length = string.length;
                }
                //break;
            case PACKBUF_TYPE_BINARY:
                {
                    PACKBUF_BINARY binary;
                    PackBufValue_GetBinary(&value, &binary);
                    binary.size = binary.size;
                }
                break;
            case PACKBUF_TYPE_UINT_VECTOR:
                {
                    PACKBUF_VECTOR vector;
                    if (PackBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackBufVector_GetUint32(&vector, &ui32) !=0)
                        {
                            ui32=ui32;
                        }
                    }
                }
                break;
            case PACKBUF_TYPE_SINT_VECTOR:
                {
                    PACKBUF_VECTOR vector;
                    if (PackBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackBufVector_GetInt32(&vector, &i32) !=0)
                        {
                            ui32=ui32;
                        }
                    }
                }
                break;
            case PACKBUF_TYPE_FLOAT_VECTOR:
                {
                    PACKBUF_VECTOR vector;
                    if (PackBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackBufVector_GetFloat(&vector, &f) !=0)
                        {
                            ui32=ui32;
                        }
                    }
                }
                break;
            }
        }

        {
            int iii=0;
        }
    }
}
#endif

