#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "packingbuf.h"

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
#define PACKINGBUF_LENGTH_FIELD_MASK      0x1f
#define PACKINGBUF_LENGTH_FIELD_BITS      5
#define PACKINGBUF_LONG_LENGTH_OCTETS_MAX 4
#define PACKINGBUF_SHORT_LENGTH_MAX       ((1<<PACKINGBUF_LENGTH_FIELD_BITS)-1-PACKINGBUF_LONG_LENGTH_OCTETS_MAX)
#define PACKINGBUF_LONG_LENGTH_MIN        (PACKINGBUF_SHORT_LENGTH_MAX+1)

#define ZIGZAG_SINT8(n)  ((n << 1) ^ (n >> 7))
#define ZIGZAG_SINT16(n) ((n << 1) ^ (n >> 15))
#define ZIGZAG_SINT32(n) ((n << 1) ^ (n >> 31))
#define ZIGZAG_SINT64(n) ((n << 1) ^ (n >> 63))
#define UNZIGZAG(n)      ((n >> 1) ^ (0-(n & 1)))

static inline
void* PackingBuf_EncodeTag(void* dst, const char *tag, uint8_t len)
{
    unsigned char* ptr = (unsigned char*)dst;

    if (tag == NULL)
    {
        *ptr++ = 0;
    }else
    {
        *ptr++ = len++;
        memcpy(ptr, tag, len);
        ptr += len;
    }

    return ptr;
}

unsigned int encode_tag(void *dst, const char *tag, uint8_t size)
{
    unsigned char *ptr = (unsigned char *)dst;
    unsigned int   byteCount = 0;

    //tag header: (1) octets
    *ptr++ = size;
    byteCount++;

    //tag value: (tagValueSize) octets
    if (tag!=NULL && size!= 0)
    {
        memcpy(ptr, tag, size);
        ptr += size;
        ptr[-1] = 0; //force '0' terminated
        byteCount += size;
    }
    
    return byteCount;
}

static inline
unsigned int GetTagValueSize(const char *tag)
{
    unsigned int size = 0;

    if (tag != NULL)
    {
         size = strlen(tag)+1;//include terminated 0
         if (size >= 0xff)
         {
            size = 0xff;//limit to 1 octet
         }
    }

    return size;
}

static
double PackingBuf_atof(const char *s, unsigned int c)
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
int32_t PackingBuf_atoi32(const char *s, unsigned int c)
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
int64_t PackingBuf_atoi64(const char *s, unsigned int c)
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
uint32_t PackingBuf_btoi32(const unsigned char *s, unsigned int c)
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
uint64_t PackingBuf_btoi64(const unsigned char *s, unsigned int c)
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
uint8_t encode_uint32(void* dst, uint32_t val)
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
    if (val < (1<<16))
    {
        *ptr++ = (unsigned char)(val>>8);
        *ptr   = (unsigned char)(val   );
        return 2;
    }else
    if (val < (1<<24))
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
    if (val < (1<<8))
    {
        return 1;
    }else
    if (val < (1<<16))
    {
        return 2;
    }else
    if (val < (1<<24))
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
    if (val < (1<<21))
    {
        *ptr++ = (unsigned char)((val>>14) | 0x80);
        *ptr++ = (unsigned char)((val>>7 ) | 0x80);
        *ptr   = (unsigned char)((val    ) & 0x7f);
        return 3;
    }else
    if (val < (1<<28))
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
                    ,uint8_t    *value)
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
                     ,uint16_t   *value)
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
                     ,uint32_t   *value)
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
                           ,uint64_t   *value)
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

void PACKINGBUFAPI PackingBuf_Init(PACKINGBUF *packingbuf, void *pBuffer, unsigned int nBufferSize)
{
	packingbuf->buffer = (unsigned char*)pBuffer;
	packingbuf->size = nBufferSize;
	packingbuf->position = 0;
}

unsigned int PACKINGBUFAPI PackingBuf_Finish(PACKINGBUF *packingbuf)
{
	return packingbuf->position;
}

void PACKINGBUFAPI PackingBuf_Reset(PACKINGBUF *packingbuf)
{
    if (packingbuf->buffer != NULL)
    {
	    packingbuf->buffer -= packingbuf->position;
    }
	packingbuf->position = 0;
}

unsigned int PACKINGBUFAPI PackingBuf_PutNull(PACKINGBUF *packingbuf, const char *tag)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = 0;

    if (packingbuf->buffer != NULL)
    {
        if ((packingbuf->position+tagValueSize+1+1) > packingbuf->size)
        {
            return 0;//oom
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: 1 octet
        packingbuf->buffer[0] = (PACKINGBUF_TYPE_FIXED<<PACKINGBUF_LENGTH_FIELD_BITS)|0;
        packingbuf->buffer++;

        //data: 0 octet
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

static inline
unsigned int put_uint8(PACKINGBUF *packingbuf, const char *tag, uint8_t value, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = get_uint8_encoding_size(value);

    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);
    if (packingbuf->buffer != NULL)
    {
        if ((packingbuf->position+tagValueSize+1+1+byteCount) > packingbuf->size)
        {
            return 0;//oom
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: (1) octet
        packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS)|byteCount;
        packingbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint8(packingbuf->buffer, value);
        packingbuf->buffer += byteCount;
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutUint8(PACKINGBUF *packingbuf, const char *tag, uint8_t value)
{
    return put_uint8(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
}

unsigned int PACKINGBUFAPI PackingBuf_PutInt8(PACKINGBUF *packingbuf, const char *tag, int8_t value)
{
    return put_uint8(packingbuf, tag, (uint8_t)ZIGZAG_SINT8(value) ,PACKINGBUF_TYPE_SINT);
}

static inline
unsigned int put_uint16(PACKINGBUF *packingbuf, const char *tag, uint16_t value, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = get_uint16_encoding_size(value);

    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);
    if (packingbuf->buffer != NULL)
    {
        if ((packingbuf->position+tagValueSize+1+1+byteCount) > packingbuf->size)
        {
            return 0;//oom
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: (1) octet
        packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS)|byteCount;
        packingbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint16(packingbuf->buffer, value);
        packingbuf->buffer += byteCount;
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutUint16(PACKINGBUF *packingbuf, const char *tag, uint16_t value)
{
    return put_uint16(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
}

unsigned int PACKINGBUFAPI PackingBuf_PutInt16(PACKINGBUF *packingbuf, const char *tag, int16_t value)
{
    return put_uint16(packingbuf, tag, (uint16_t)ZIGZAG_SINT16(value), PACKINGBUF_TYPE_SINT);
}

static inline
unsigned int put_fixed32(PACKINGBUF *packingbuf, const char *tag, uint32_t value, uint8_t size, uint8_t type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = size;

    assert(byteCount <= sizeof(value));/*1,2,4*/
    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);

    if (packingbuf->buffer != NULL)
    {
        if (packingbuf->position+tagValueSize+1+1+byteCount > packingbuf->size)
        {
            return 0;
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: 1 octet
        packingbuf->buffer[0] = (type << PACKINGBUF_LENGTH_FIELD_BITS) | (size);
        packingbuf->buffer++;

        //data: (byteCount) octets
        {
            int8_t shift = (size-1)*8;
            do
            {
                packingbuf->buffer[0] = (unsigned char)(value>>shift);
                packingbuf->buffer++;
                shift -= 8;
            }while (shift >= 0);
        }
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

static inline
unsigned int put_uint32(PACKINGBUF *packingbuf, const char *tag, uint32_t value, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = get_uint32_encoding_size(value);

    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);
    if (packingbuf->buffer != NULL)
    {
        if ((packingbuf->position+tagValueSize+1+1+byteCount) > packingbuf->size)
        {
            return 0;//oom
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: (1) octet
        packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS)|byteCount;
        packingbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint32(packingbuf->buffer, value);
        packingbuf->buffer += byteCount;
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutUint32(PACKINGBUF *packingbuf, const char *tag, uint32_t value)
{
    return put_uint32(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
}

unsigned int PACKINGBUFAPI PackingBuf_PutInt32(PACKINGBUF *packingbuf, const char *tag, int32_t value)
{
    return put_uint32(packingbuf, tag, (uint32_t)ZIGZAG_SINT32(value), PACKINGBUF_TYPE_SINT);
}

unsigned int PACKINGBUFAPI PackingBuf_PutUint(PACKINGBUF *packingbuf, const char *tag, unsigned int value)
{
    if (sizeof(int) == 4)
    {
        return put_uint32(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
    }else
    if (sizeof(int) == 2)
    {
        return put_uint16(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
    }else
    if (sizeof(int) == 1)
    {
        return put_uint8(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
    }
    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_PutInt(PACKINGBUF *packingbuf, const char *tag, int value)
{
    if (sizeof(int) == 4)
    {
        return put_uint32(packingbuf, tag, (uint32_t)ZIGZAG_SINT32(value), PACKINGBUF_TYPE_SINT);
    }else
    if (sizeof(int) == 2)
    {
        return put_uint16(packingbuf, tag, (uint32_t)ZIGZAG_SINT16(value), PACKINGBUF_TYPE_SINT);
    }else
    if (sizeof(int) == 1)
    {
        return put_uint8(packingbuf, tag, (uint32_t)ZIGZAG_SINT8(value), PACKINGBUF_TYPE_SINT);
    }
    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_PutFloat(PACKINGBUF *packingbuf, const char *tag, float value)
{
	return put_fixed32(packingbuf, tag, encode_float(value), sizeof(uint32_t), PACKINGBUF_TYPE_FIXED);
}

#if ENABLE_INT64_SUPPORT
static inline
unsigned int put_fixed64(PACKINGBUF *packingbuf, const char *tag, uint64_t value, uint8_t size, uint8_t type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = size;

    assert(byteCount <= sizeof(value));/*1,2,4*/
    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);

    if (packingbuf->buffer != NULL)
    {
        if (packingbuf->position+tagValueSize+1+1+byteCount > packingbuf->size)
        {
            return 0;
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer++;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: 1 octet
        packingbuf->buffer[0] = (type << PACKINGBUF_LENGTH_FIELD_BITS) | (size);
        packingbuf->buffer++;

        //data: (byteCount) octets
        {
            int8_t shift = (size-1)*8;
            do
            {
                packingbuf->buffer[0] = (unsigned char)(value>>shift);
                packingbuf->buffer++;
                shift -= 8;
            }while (shift >= 0);
        }
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

static inline
unsigned int put_uint64(PACKINGBUF *packingbuf, const char *tag, uint64_t value, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = get_uint64_encoding_size(value);

    assert(byteCount <= PACKINGBUF_SHORT_LENGTH_MAX);
    if (packingbuf->buffer != NULL)
    {
        if ((packingbuf->position+tagValueSize+1+1+byteCount) > packingbuf->size)
        {
            return 0;//oom
        }

        //tag header: (1) octets
        packingbuf->buffer[0] = tagValueSize;
        packingbuf->buffer += 1;

        //tag value: (tagValueSize) octets
        if (tagValueSize != 0)
        {
            memcpy(&packingbuf->buffer[0], tag, tagValueSize);
            packingbuf->buffer += tagValueSize;
            packingbuf->buffer[-1] = 0; //force '0' terminated
        }

        //type+length: (1) octet
        packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS)|byteCount;
        packingbuf->buffer++;

        //data: (byteCount) octets [BE]
        encode_uint64(packingbuf->buffer, value);
        packingbuf->buffer += byteCount;
    }

    byteCount += tagValueSize+1+1;//add header length to get total count
    packingbuf->position += byteCount;

	return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutUint64(PACKINGBUF *packingbuf, const char *tag, uint64_t value)
{
    return put_uint64(packingbuf, tag, value, PACKINGBUF_TYPE_UINT);
}

unsigned int PACKINGBUFAPI PackingBuf_PutInt64(PACKINGBUF *packingbuf, const char *tag, int64_t value)
{
    return put_uint64(packingbuf, tag, (uint64_t)ZIGZAG_SINT64(value), PACKINGBUF_TYPE_SINT);
}

#if ENABLE_DOUBLE_SUPPORT
unsigned int PACKINGBUFAPI PackingBuf_PutDouble(PACKINGBUF *packingbuf, const char *tag, double value)
{
	return put_fixed64(packingbuf, tag, encode_double(value), sizeof(uint64_t), PACKINGBUF_TYPE_FIXED);
}
#endif
#endif

static
unsigned int put_binary(PACKINGBUF *packingbuf, const char *tag, const void *buffer, unsigned int size, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = 0;

    if (packingbuf->buffer != NULL)
    {
        if (size <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            if (packingbuf->position+tagValueSize+(1+1+0)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+0)+size > packingbuf->position);
                assert(tagValueSize+(1+1+0)+size > size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length+data: (1+0+size) octets
                packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS) | (size);
                memcpy(&packingbuf->buffer[1], buffer, size);

                size += (1+0);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+1)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+1)+size > packingbuf->position);
                assert(tagValueSize+(1+1+1)+size > size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+1) octets
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+1));
                packingbuf->buffer[1] = (unsigned char)(size);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data:(size) octets
                memcpy(&packingbuf->buffer[2], buffer, size);

                size += (1+1);
                byteCount += size;
                packingbuf->buffer += size;
                packingbuf->position += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+2)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+2)+size > packingbuf->position);
                assert(tagValueSize+(1+1+2)+size > size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+2) octets
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<< PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+2));
                packingbuf->buffer[1] = (unsigned char)((size>>8) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size   ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packingbuf->buffer[3], buffer, size);

                size += (1+2);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+3)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+3)+size > packingbuf->position);
                assert(tagValueSize+(1+1+3)+size > size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+3) octets
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+3));
                packingbuf->buffer[1] = (unsigned char)((size>>16) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size>>8 ) & 0xff);
                packingbuf->buffer[3] = (unsigned char)((size    ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packingbuf->buffer[4], buffer, size);

                size += (1+3);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            if (packingbuf->position+tagValueSize+(1+1+4)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+4)+size > packingbuf->position);
                assert(tagValueSize+(1+1+4)+size > size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+4) octets
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+4));
                packingbuf->buffer[1] = (unsigned char)((size>>24) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size>>16) & 0xff);
                packingbuf->buffer[3] = (unsigned char)((size>>8 ) & 0xff);
                packingbuf->buffer[4] = (unsigned char)((size    ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                memcpy(&packingbuf->buffer[5], buffer, size);

                size += (1+4);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #endif
        #endif
        #endif
        #endif
        }
    }else
    {
        if (size <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            assert(tagValueSize+(1+1+0)+size > size);
            size += tagValueSize+(1+1+0);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+1)+size > size);
            size += tagValueSize+(1+1+1);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+2)+size > size);
            size += tagValueSize+(1+1+2);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+3)+size > size);
            size += tagValueSize+(1+1+3);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(tagValueSize+(1+1+4)+size > size);
            size += tagValueSize+(1+1+4);
        #endif
        #endif
        #endif
        #endif
        }
        assert(packingbuf->position+size > packingbuf->position);
        packingbuf->position += size;
        byteCount += size;
    }

    return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutBinary(PACKINGBUF *packingbuf, const char *tag, const void *buffer, unsigned int size)
{
    return put_binary(packingbuf, tag, buffer, size, PACKINGBUF_TYPE_BINARY);
}

unsigned int PACKINGBUFAPI PackingBuf_PutString(PACKINGBUF *packingbuf, const char *tag, const char *s)
{
	unsigned int l=strlen(s)+1;	//include terminated '0'

    return put_binary(packingbuf, tag, s, l, PACKINGBUF_TYPE_STRING);
}

unsigned int PACKINGBUFAPI PackingBuf_PutStringEx(PACKINGBUF *packingbuf, const char *tag, const char *s, unsigned int length)
{
    unsigned int ir=put_binary(packingbuf, tag, s, length+1, PACKINGBUF_TYPE_STRING);//include terminated '0'

    if ((ir!=0) && (packingbuf->buffer!=NULL))
    {
        packingbuf->buffer[-1] = 0;//force terminated '0'
    }

    return ir;
}

static inline
unsigned int put_binary_begin(PACKINGBUF *packingbuf, void **buffer, unsigned int *size)
{
    if (packingbuf->buffer!=NULL)
    {
        //tag header: 1 octet
        //val header: 1 octet
        //data: 0 octets
        //return (pointer & size) according this position
        if ((packingbuf->position+1+1+0) > packingbuf->size)
        {
		    return 0;
        }

        if (buffer!=NULL)
        {
            *buffer=&packingbuf->buffer[1+1+0];
        }
        if (size!=NULL)
        {
            *size=packingbuf->size-(packingbuf->position+1+1+0);
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

	return 1+1+0;
}

static
unsigned int put_binary_end(PACKINGBUF *packingbuf, unsigned int size, const char *tag, unsigned char type)
{
    unsigned int tagValueSize = GetTagValueSize(tag);
    unsigned int byteCount = 0;

    if (packingbuf->buffer != NULL)
    {
        if (size <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            if (packingbuf->position+tagValueSize+(1+1+0)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+0)+size > packingbuf->position);
                assert(tagValueSize+(1+1+0)+size > size);

                memmove(&packingbuf->buffer[tagValueSize+(1+1+0)], &packingbuf->buffer[1+1+0], size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+0) octet
                packingbuf->buffer[0] = (type<<PACKINGBUF_LENGTH_FIELD_BITS) | (size);

                //data: (size) octets
                //memcpy(&packingbuf->buffer[1], buffer, size);

                size += (1+0);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+1)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+1)+size > packingbuf->position);
                assert(tagValueSize+(1+1+1)+size > size);

                memmove(&packingbuf->buffer[tagValueSize+(1+1+1)], &packingbuf->buffer[1+1+0], size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: (1+1) octets
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+1));
                packingbuf->buffer[1] = (unsigned char)(size);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: (size) octets
                //memcpy(&packingbuf->buffer[2],buffer,size);

                size += (1+1);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+2)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+2)+size > packingbuf->position);
                assert(tagValueSize+(1+1+2)+size > size);

                memmove(&packingbuf->buffer[tagValueSize+(1+1+2)], &packingbuf->buffer[1+1+0], size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: 1+2 octet
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+2));
                packingbuf->buffer[1] = (unsigned char)((size>>8) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size   ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packingbuf->buffer[3],buffer,size);

                size += (1+2);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            if (packingbuf->position+tagValueSize+(1+1+3)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+1+3)+size > packingbuf->position);
                assert(tagValueSize+(1+1+3)+size > size);

                memmove(&packingbuf->buffer[tagValueSize+(1+1+3)], &packingbuf->buffer[1+1+0], size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: 1+3 octet
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+3));
                packingbuf->buffer[1] = (unsigned char)((size>>16) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size>>8 ) & 0xff);
                packingbuf->buffer[3] = (unsigned char)((size    ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packingbuf->buffer[4],buffer,size);

                size += (1+3);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            if (packingbuf->position+tagValueSize+(1+4)+size < packingbuf->size)
            {
                assert(packingbuf->position+tagValueSize+(1+4)+size > packingbuf->position);
                assert(tagValueSize+(1+4)+size > size);

                memmove(&packingbuf->buffer[tagValueSize+(1+1+4)], &packingbuf->buffer[1+1+0], size);

                //tag header: (1) octets
                packingbuf->buffer[0] = tagValueSize;
                packingbuf->buffer++;
                packingbuf->position++;
                byteCount++;

                //tag value: (tagValueSize) octets
                if (tagValueSize != 0)
                {
                    memcpy(&packingbuf->buffer[0], tag, tagValueSize);
                    packingbuf->buffer += tagValueSize;
                    packingbuf->buffer[-1] = 0; //force '0' terminated
                    packingbuf->position += tagValueSize;
                    byteCount += tagValueSize;
                }

                //type+length: 1+4 octet
                size -= PACKINGBUF_LONG_LENGTH_MIN;
                packingbuf->buffer[0] = (unsigned char)((type<<PACKINGBUF_LENGTH_FIELD_BITS) | (PACKINGBUF_SHORT_LENGTH_MAX+4));
                packingbuf->buffer[1] = (unsigned char)((size>>24) & 0xff);
                packingbuf->buffer[2] = (unsigned char)((size>>16) & 0xff);
                packingbuf->buffer[3] = (unsigned char)((size>>8 ) & 0xff);
                packingbuf->buffer[4] = (unsigned char)((size    ) & 0xff);
                size += PACKINGBUF_LONG_LENGTH_MIN;

                //data: size octets
                //memcpy(&packingbuf->buffer[5],buffer,size);

                size += (1+4);
                packingbuf->buffer += size;
                packingbuf->position += size;
                byteCount += size;
            }
        #endif
        #endif
        #endif
        #endif
        }
    }else
    {
        if (size <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            assert(tagValueSize+(1+1+0)+size > size);
            size += tagValueSize+(1+1+0);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 1)
        }else
        if (size <= 0xff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+1)+size > size);
            size += tagValueSize+(1+1+1);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 2)
        }else
        if (size <= 0xffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+2)+size > size);
            size += tagValueSize+(1+1+2);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX >= 3)
        }else
        if (size <= 0xffffff+PACKINGBUF_LONG_LENGTH_MIN)
        {
            assert(tagValueSize+(1+1+3)+size > size);
            size += tagValueSize+(1+1+3);
        #if (PACKINGBUF_LONG_LENGTH_OCTETS_MAX == 4)
        }else
        {
            assert(tagValueSize+(1+1+4)+size > size);
            size += tagValueSize+(1+1+4);
        #endif
        #endif
        #endif
        #endif
        }
        assert(packingbuf->position+size > packingbuf->position);
        packingbuf->position += size;
        byteCount += size;
    }

    return byteCount;
}

unsigned int PACKINGBUFAPI PackingBuf_PutBinaryBegin(PACKINGBUF *packingbuf, void **buffer, unsigned int *size)
{
    return put_binary_begin(packingbuf, buffer, size);
}

unsigned int PACKINGBUFAPI PackingBuf_PutBinaryEnd(PACKINGBUF *packingbuf, unsigned int size, const char *tag)
{
    return put_binary_end(packingbuf, size, tag, PACKINGBUF_TYPE_BINARY);
}

unsigned int PACKINGBUFAPI PackingBuf_PutStringBegin(PACKINGBUF *packingbuf, void **buffer, unsigned int *size)
{
    return put_binary_begin(packingbuf, buffer, size);
}

unsigned int PACKINGBUFAPI PackingBuf_PutStringEnd(PACKINGBUF *packingbuf, unsigned int length, const char *tag)
{
    unsigned int ir=put_binary_end(packingbuf, length+1, tag, PACKINGBUF_TYPE_STRING);//include terminated '0'

    if ((ir!=0) && (packingbuf->buffer!=NULL))
    {
        packingbuf->buffer[-1]=0;//force terminated '0'
    }

    return ir;
}

static
unsigned int vector_put_varint8(PACKINGBUF_VECTOR *vector, uint8_t value)
{
    unsigned char buffer[2];//the largest 8 bits integer needs 2 bytes
    unsigned char count;
    unsigned char size = encode_varint8(buffer,value);

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
unsigned int vector_put_varint16(PACKINGBUF_VECTOR *vector, uint16_t value)
{
    unsigned char buffer[3];//the largest 16 bits integer needs 3 bytes
    unsigned char count;
    unsigned char size = encode_varint16(buffer,value);

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
unsigned int vector_put_varint32(PACKINGBUF_VECTOR *vector, uint32_t value)
{
    unsigned char buffer[5];//the largest 32 bits integer needs 5 bytes
    unsigned char count;
    unsigned char size = encode_varint32(buffer,value);

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

unsigned int PACKINGBUFAPI PackingBufVector_PutUint8(PACKINGBUF_VECTOR *vector, uint8_t value)
{
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint8(vector, value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int8_t v=(int8_t)value;
        return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(v));
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutUint16(PACKINGBUF_VECTOR *vector, uint16_t value)
{
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint16(vector, value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int16_t v=(int16_t)value;
        return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(v));
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutUint32(PACKINGBUF_VECTOR *vector, uint32_t value)
{
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int32_t v=(int32_t)value;
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutInt8(PACKINGBUF_VECTOR *vector, int8_t value)
{
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint8(vector, (uint32_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutInt16(PACKINGBUF_VECTOR *vector, int16_t value)
{
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint16(vector, (uint32_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutInt32(PACKINGBUF_VECTOR *vector, int32_t value)
{
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutInt(PACKINGBUF_VECTOR *vector, int value)
{
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)ZIGZAG_SINT8(value));
        }
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)ZIGZAG_SINT16(value));
        }
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(value));
        }
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)value);
        }
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)value);
        }
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)value);
        }
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutUint(PACKINGBUF_VECTOR *vector, unsigned int value)
{
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        if (sizeof(int)==1)
        {
            return vector_put_varint8(vector, (uint8_t)value);
        }
        if (sizeof(int)==2)
        {
            return vector_put_varint16(vector, (uint16_t)value);
        }
        if (sizeof(int)==4)
        {
            return vector_put_varint32(vector, (uint32_t)value);
        }
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        if (sizeof(int)==1)
        {
            int8_t v=(int8_t)value;
            return vector_put_varint32(vector, (uint8_t)ZIGZAG_SINT8(v));
        }
        if (sizeof(int)==2)
        {
            int16_t v=(int16_t)value;
            return vector_put_varint32(vector, (uint16_t)ZIGZAG_SINT16(v));
        }
        if (sizeof(int)==4)
        {
            int32_t v=(int32_t)value;
            return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
        }
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float((float)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutFloat(PACKINGBUF_VECTOR *vector, float value)
{
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint32(vector, encode_float(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint32(vector, (uint32_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int32_t v=(int32_t)value;
        return vector_put_varint32(vector, (uint32_t)ZIGZAG_SINT32(v));
    }else
    {
    }

    return 0;
}

#if ENABLE_INT64_SUPPORT
static
unsigned int vector_put_varint64(PACKINGBUF_VECTOR *vector, uint64_t value)
{
    unsigned char buffer[10];//the largest 64 bits integer needs 10 bytes
    unsigned char count;
    unsigned char size = encode_varint64(buffer,value);

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

unsigned int PACKINGBUFAPI PackingBufVector_PutUint64(PACKINGBUF_VECTOR *vector, uint64_t value)
{
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int64_t v=(int64_t)value;
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(v));
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double((double)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutInt64(PACKINGBUF_VECTOR *vector, int64_t value)
{
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double((double)value));
    }else
    {
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufVector_PutDouble(PACKINGBUF_VECTOR *vector, double value)
{
    if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
    {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
        return vector_put_varint64(vector, encode_double(value));
    }else
    if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
    {   /* PACKINGBUF_TYPE_UINT_VECTOR */
        return vector_put_varint64(vector, (uint64_t)value);
    }else
    if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
    {   /* PACKINGBUF_TYPE_SINT_VECTOR */
        int64_t v=(int64_t)value;
        return vector_put_varint64(vector, (uint64_t)ZIGZAG_SINT64(v));
    }else
    {
    }

    return 0;
}
#endif

#define DEFINE_PACKINGBUFVECTOR_GETSINT32(NAME,TYPE)                              \
unsigned int PACKINGBUFAPI PackingBufVector_Get##NAME(PACKINGBUF_VECTOR *vector,TYPE *value) \
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
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)              \
            {   /* PACKINGBUF_TYPE_SINT_VECTOR */                         \
                *value = (TYPE)((int32_t)UNZIGZAG(temp));           \
            }else                                                   \
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)              \
            {   /* PACKINGBUF_TYPE_UINT_VECTOR */                         \
                *value = (TYPE)temp;                                \
            }else                                                   \
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)             \
            {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */                        \
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
DEFINE_PACKINGBUFVECTOR_GETSINT32(Int8,   int8_t);
DEFINE_PACKINGBUFVECTOR_GETSINT32(Int16,  int16_t);
DEFINE_PACKINGBUFVECTOR_GETSINT32(Int32,  int32_t);

#define DEFINE_PACKINGBUFVECTOR_GETUINT32(NAME,TYPE)                              \
unsigned int PACKINGBUFAPI PackingBufVector_Get##NAME(PACKINGBUF_VECTOR *vector,TYPE *value) \
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
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)              \
            {   /* PACKINGBUF_TYPE_UINT_VECTOR */                         \
                *value = (TYPE)temp;                                \
            }else                                                   \
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)              \
            {   /* PACKINGBUF_TYPE_SINT_VECTOR */                         \
                *value = (TYPE)((int32_t)UNZIGZAG(temp));           \
            }else                                                   \
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)             \
            {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */                        \
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
DEFINE_PACKINGBUFVECTOR_GETUINT32(Uint8,  uint8_t);
DEFINE_PACKINGBUFVECTOR_GETUINT32(Uint16, uint16_t);
DEFINE_PACKINGBUFVECTOR_GETUINT32(Uint32, uint32_t);

#if 0
unsigned int PACKINGBUFAPI PackingBufVector_GetUint32(PACKINGBUF_VECTOR *vector, uint32_t *value)
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
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {   /* PACKINGBUF_TYPE_UINT_VECTOR */
                *value = temp;
            }else
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
            {   /* PACKINGBUF_TYPE_SINT_VECTOR */
                *value = (uint32_t)((int32_t)UNZIGZAG(temp));
            }else
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
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

unsigned int PACKINGBUFAPI PackingBufVector_GetFloat(PACKINGBUF_VECTOR *vector,float *value)
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
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
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
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = (float)temp;
            }else
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
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
unsigned int PACKINGBUFAPI PackingBufVector_GetInt64(PACKINGBUF_VECTOR *vector, int64_t *value)
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
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
            {   /* PACKINGBUF_TYPE_SINT_VECTOR */
                *value = (int64_t)UNZIGZAG(temp);
            }else
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {   /* PACKINGBUF_TYPE_UINT_VECTOR */
                *value = (int64_t)temp;
            }else
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
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

unsigned int PACKINGBUFAPI PackingBufVector_GetUint64(PACKINGBUF_VECTOR *vector, uint64_t *value)
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
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {   /* PACKINGBUF_TYPE_UINT_VECTOR */
                *value = temp;
            }else
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
            {   /* PACKINGBUF_TYPE_SINT_VECTOR */
                *value = (uint64_t)((int64_t)UNZIGZAG(temp));
            }else
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKINGBUF_TYPE_FLOAT_VECTOR */
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
unsigned int PACKINGBUFAPI PackingBufVector_GetDouble(PACKINGBUF_VECTOR *vector,double *value)
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
            if (vector->type == PACKINGBUF_TYPE_FLOAT_VECTOR)
            {   /* PACKBUF_TYPE_FLOAT_VECTOR */
                if (size > 5)
                {   //Double-precision
                    *value = decode_double(temp);
                }else
                {   //Single-precision
                    *value = (double)decode_float((uint32_t)temp);
                }
            }else
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {   /* PACKBUF_TYPE_UINT_VECTOR */
                *value = (double)temp;
            }else
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
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
unsigned int PACKINGBUFAPI PackingBufVector_GetInt8(PACKINGBUF_VECTOR *vector, int8_t *value)
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
            if (vector->type == PACKINGBUF_TYPE_SINT_VECTOR)
            {
                *value = (int8_t)UNZIGZAG(temp);
            }else
            if (vector->type == PACKINGBUF_TYPE_UINT_VECTOR)
            {
                *value = (int8_t)temp;
            }else
            {
                //PACKINGBUF_TYPE_FLOAT_VECTOR
                *value = (int8_t)decode_float(temp);
            }
        }
        return size;
    }

    return 0;
}
#endif

unsigned int PACKINGBUFAPI PackingBuf_PutVectorBegin(PACKINGBUF *packingbuf, PACKINGBUF_VECTOR *vector, enum PACKINGBUF_VECTOR_TYPE vectorType)
{
    unsigned int ir = put_binary_begin(packingbuf, (void**)&vector->buffer, &vector->size);
    if (ir != 0)
    {
        vector->type = vectorType;
        vector->position = 0;
        return ir;
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_PutVectorEnd(PACKINGBUF *packingbuf, PACKINGBUF_VECTOR *vector, const char *tag)
{
    return put_binary_end(packingbuf, vector->position, tag, vector->type);
}

unsigned int PACKINGBUFAPI PackingBuf_Find(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value, const char *tag, uint8_t casecmp)
{
    unsigned char *buffer = packingbuf->buffer;
    unsigned int   position = packingbuf->position;

    do
    {
        unsigned int tagValueSize;
        unsigned int valueSize;
        uint8_t      tagMatched = 0;

        if ((position+1) > packingbuf->size)
        {
            break;
        }

        //tag header
        tagValueSize = *buffer++;
        position++;

        //tag value & type-length
        if ((position+tagValueSize+1) > packingbuf->size)
        {
            break;
        }

        //tag value
        if (tagValueSize != 0)
        {
            if (buffer[tagValueSize-1] != 0)
            {
                break;//bad tag value
            }
            if (tag != NULL)
            {
                if (0==((casecmp==0)?strcmp(tag,buffer):strcmpi(tag,buffer)))
                {
                    tagMatched = 1;
                    value->tag = buffer;
                }
            }

            buffer += tagValueSize;
            position += tagValueSize;
        }else
        {
            if (tag == NULL)
            {
                tagMatched = 1;
                value->tag = NULL;
            }
        }

        if (tagMatched)
        {
            //type-length
            value->type =(buffer[0]>>PACKINGBUF_LENGTH_FIELD_BITS)&0xff;
            valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
            buffer++;
            position++;

            if (valueSize <= PACKINGBUF_SHORT_LENGTH_MAX)
            {
                //0 octet length(extented), [valueSize] octets data
                if ((position+0+valueSize) > packingbuf->size)
                {
                    break;
                }

                value->data = buffer;
                value->size = valueSize;
            }else
            {
                uint8_t index;
                uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

                //[count] octets length(extented)
                if ((position+count) > packingbuf->size)
                {
                    break;
                }

                valueSize = 0;
                for (index=1; index<=count; index++)
                {
                    valueSize = (valueSize<<8)|buffer[index];
                }

                valueSize += PACKINGBUF_LONG_LENGTH_MIN;
                //[count] octets length(extented), [valueSize] octets data
                if ((position+count+valueSize) > packingbuf->size)
                {
                    break;
                }
                if ((position+count+valueSize) < valueSize)
                {
                    break;
                }

                value->data = &buffer[count];
                value->size = valueSize;

                valueSize += count;
            }

            //add size of (tag-header,tag-value & value-hdr)
            valueSize += 1+tagValueSize+1;
            return valueSize;
        }else
        {
            //type-length
            valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
            if (valueSize > PACKINGBUF_SHORT_LENGTH_MAX)
            {
                uint8_t index;
                uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

                if ((position+1+count) > packingbuf->size)
                {
                    break;
                }

                valueSize = 0;
                for (index=1; index<=count; index++)
                {
                    valueSize = (valueSize<<8)|buffer[index];
                }

                valueSize += PACKINGBUF_LONG_LENGTH_MIN;
                valueSize += count;
            }

            //add size of (value-hdr)
            valueSize += 1;
            if ((position+valueSize) > packingbuf->size)
            {
                break;
            }

            buffer += valueSize;
            position += valueSize;
        }
    }while(1);

    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_Count(PACKINGBUF *packingbuf, int origin)
{
    unsigned char *buffer = packingbuf->buffer;
    unsigned int   position = packingbuf->position;
    unsigned int   count = 0;

    if (origin)
    {
    	buffer -= position;
	    position=0;
    }

    do
    {
        unsigned int tagValueSize;
        unsigned int valueSize;

        if ((position+1) > packingbuf->size)
        {
            break;
        }

        //tag header
        tagValueSize = *buffer++;
        position++;

        //tag value & type-length
        if ((position+tagValueSize+1) > packingbuf->size)
        {
            break;
        }

        //tag value
        if (tagValueSize != 0)
        {
            if (buffer[tagValueSize-1] != 0)
            {
                break;//bad tag value
            }

            buffer += tagValueSize;
            position += tagValueSize;
        }

        //type-length
        valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
        if (valueSize > PACKINGBUF_SHORT_LENGTH_MAX)
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

            if ((position+1+count) > packingbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKINGBUF_LONG_LENGTH_MIN;
            valueSize += count;
        }

        //add size of (value-hdr)
        valueSize += 1;
        if ((position+valueSize) > packingbuf->size)
        {
            break;
        }

        buffer += valueSize;
        position += valueSize;
        count ++;
    }while(1);

    return count;
}

unsigned int PACKINGBUFAPI PackingBuf_Skip(PACKINGBUF *packingbuf)
{
    do
    {
        unsigned char *buffer = packingbuf->buffer;
        unsigned int   position = packingbuf->position;
        unsigned int   tagValueSize;
        unsigned int   valueSize;

        if ((position+1) > packingbuf->size)
        {
            break;
        }

        //tag header
        tagValueSize = *buffer++;
        position++;

        //tag value & type-length
        if ((position+tagValueSize+1) > packingbuf->size)
        {
            break;
        }

        //tag value
        if (tagValueSize != 0)
        {
            if (buffer[tagValueSize-1] != 0)
            {
                break;//bad tag value
            }
            buffer += tagValueSize;
            position += tagValueSize;
        }

        //type-length
        valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
        if (valueSize > PACKINGBUF_SHORT_LENGTH_MAX)
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

            if ((position+1+count) > packingbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKINGBUF_LONG_LENGTH_MIN;
            valueSize += count;
        }

        //add size of (tag-header,tag-value & value-hdr)
        valueSize += 1+tagValueSize+1;
        if ((packingbuf->position+valueSize) > packingbuf->size)
        {
            break;
        }

        packingbuf->buffer += valueSize;
        packingbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_Get(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value)
{
    do
    {
        unsigned char *buffer = packingbuf->buffer;
        unsigned int   position = packingbuf->position;
        unsigned int   tagValueSize;
        unsigned int   valueSize;

        if ((position+1) > packingbuf->size)
        {
            break;
        }

        //tag header
        tagValueSize = *buffer++;
        position++;

        //tag value & type-length
        if ((position+tagValueSize+1) > packingbuf->size)
        {
            break;
        }

        //tag value
        if (tagValueSize != 0)
        {
            if (buffer[tagValueSize-1] != 0)
            {
                break;//bad tag value
            }
            value->tag = buffer;
            buffer += tagValueSize;
            position += tagValueSize;
        }else
        {
            value->tag = NULL;
        }

        //type-length
        value->type =(buffer[0]>>PACKINGBUF_LENGTH_FIELD_BITS)&0xff;
        valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
        if (valueSize <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            //1 octet hdr, 0 octet length(extented), [valueSize] octets data
            if ((position+1+0+valueSize) > packingbuf->size)
            {
                break;
            }

            value->data = &buffer[1];
            value->size = valueSize;
        }else
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

            //1 octet hdr, [count] octets length(extented)
            if ((position+1+count) > packingbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKINGBUF_LONG_LENGTH_MIN;
            //1 octet hdr, [count] octets length(extented), [valueSize] octets data
            if ((position+1+count+valueSize) > packingbuf->size)
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

        //add size of (tag-header,tag-value & value-hdr)
        valueSize += 1+tagValueSize+1;
        packingbuf->buffer += valueSize;
        packingbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKINGBUFAPI PackingBuf_Peek(PACKINGBUF *packingbuf, PACKINGBUF_VALUE *value)
{
    do
    {
        unsigned char *buffer = packingbuf->buffer;
        unsigned int   position = packingbuf->position;
        unsigned int   tagValueSize;
        unsigned int   valueSize;

        if ((position+1) > packingbuf->size)
        {
            break;
        }

        //tag header
        tagValueSize = *buffer++;
        position++;

        //tag value & type-length
        if ((position+tagValueSize+1) > packingbuf->size)
        {
            break;
        }

        //tag value
        if (tagValueSize != 0)
        {
            if (buffer[tagValueSize-1] != 0)
            {
                break;//bad tag value
            }
            value->tag = buffer;
            buffer += tagValueSize;
            position += tagValueSize;
        }else
        {
            value->tag = NULL;
        }

        //type-length
        value->type =(buffer[0]>>PACKINGBUF_LENGTH_FIELD_BITS)&0xff;
        valueSize = (buffer[0]&PACKINGBUF_LENGTH_FIELD_MASK);
        if (valueSize <= PACKINGBUF_SHORT_LENGTH_MAX)
        {
            //1 octet hdr, 0 octet length(extented), [valueSize] octets data
            if ((position+1+0+valueSize) > packingbuf->size)
            {
                break;
            }

            value->data = &buffer[1];
            value->size = valueSize;
        }else
        {
            uint8_t index;
            uint8_t count = (uint8_t)(valueSize-PACKINGBUF_SHORT_LENGTH_MAX);

            //1 octet hdr, [count] octets length(extented)
            if ((position+1+count) > packingbuf->size)
            {
                break;
            }

            valueSize = 0;
            for (index=1; index<=count; index++)
            {
                valueSize = (valueSize<<8)|buffer[index];
            }

            valueSize += PACKINGBUF_LONG_LENGTH_MIN;
            //1 octet hdr, [count] octets length(extented), [valueSize] octets data
            if ((position+1+count+valueSize) > packingbuf->size)
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

        //add size of (tag-header,tag-value & value-hdr)
        valueSize += 1+tagValueSize+1;
        //packingbuf->buffer += valueSize;
        //packingbuf->position += valueSize;

        return valueSize;
    }while(0);

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_GetFloat(PACKINGBUF_VALUE *value, float *dst)
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
    case PACKINGBUF_TYPE_UINT:
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

    case PACKINGBUF_TYPE_SINT:
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

    case PACKINGBUF_TYPE_FIXED:
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

    case PACKINGBUF_TYPE_STRING:
        if (dst != NULL)
        {
            *dst = (float)PackingBuf_atof((char *)value->data,value->size);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_BINARY:
        if (dst != NULL)
        {
            temp.u32 = PackingBuf_btoi32(value->data,value->size);
            *dst = (float)decode_float(temp.u32);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_UINT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = (float)(temp.u32);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_SINT_VECTOR:
        if (dst != NULL)
        {
            temp.u32 = 0;
            decode_varint32(value->data,value->data+value->size,&temp.u32);
            *dst = (float)((int32_t)UNZIGZAG(temp.u32));
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_FLOAT_VECTOR:
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
unsigned int PACKINGBUFAPI PackingBufValue_GetDouble(PACKINGBUF_VALUE *value, double *dst)
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
    case PACKINGBUF_TYPE_UINT:
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

    case PACKINGBUF_TYPE_SINT:
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

    case PACKINGBUF_TYPE_FIXED:
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

    case PACKINGBUF_TYPE_STRING:
        if (dst != NULL)
        {
            *dst = (double)PackingBuf_atof((char *)value->data,value->size);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_BINARY:
        if (dst != NULL)
        {
            temp.u64 = PackingBuf_btoi64(value->data,value->size);
            *dst = (double)decode_double(temp.u64);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_UINT_VECTOR:
        if (dst != NULL)
        {
            temp.u64 = 0;
            decode_varint64(value->data,value->data+value->size,&temp.u64);
            *dst = (double)(temp.u64);
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_SINT_VECTOR:
        if (dst != NULL)
        {
            temp.u64 = 0;
            decode_varint64(value->data,value->data+value->size,&temp.u64);
            *dst = (double)((int64_t)UNZIGZAG(temp.u64));
        }
        result = sizeof(*dst);
        break;

    case PACKINGBUF_TYPE_FLOAT_VECTOR:
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
#define DEFINE_PACKINGBUFVALUE_GET(NAME,TYPE)                                            \
unsigned int PACKINGBUFAPI PackingBufValue_Get##NAME(PACKINGBUF_VALUE *value,TYPE *dst)  \
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
    case PACKINGBUF_TYPE_UINT:                                            \
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
    case PACKINGBUF_TYPE_SINT:                                            \
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
    case PACKINGBUF_TYPE_FIXED:                                           \
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
    case PACKINGBUF_TYPE_STRING:                                          \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* int32 */                                         \
                temp.i32 = PackingBuf_atoi32((char *)value->data,value->size);    \
                *dst = (TYPE)(temp.i32);                            \
            }else                                                   \
            {   /* int64 */                                         \
                temp.i64 = PackingBuf_atoi64((char *)value->data,value->size);    \
                *dst = (TYPE)(temp.i64);                            \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_BINARY:                                          \
        if (dst != NULL)                                            \
        {                                                           \
            if (sizeof(*dst) <= 4)                                  \
            {   /* uint32 */                                        \
                temp.u32 = PackingBuf_btoi32(value->data,value->size);    \
                *dst = (TYPE)(temp.u32);                            \
            }else                                                   \
            {   /* uint64 */                                        \
                temp.u64 = PackingBuf_btoi64(value->data,value->size);    \
                *dst = (TYPE)(temp.u64);                            \
            }                                                       \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_UINT_VECTOR:                                     \
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
    case PACKINGBUF_TYPE_SINT_VECTOR:                                     \
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
    case PACKINGBUF_TYPE_FLOAT_VECTOR:                                    \
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
#define DEFINE_PACKINGBUFVALUE_GET(NAME,TYPE)                                            \
unsigned int PACKINGBUFAPI PackingBufValue_Get##NAME(PACKINGBUF_VALUE *value,TYPE *dst)  \
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
    case PACKINGBUF_TYPE_UINT:                                            \
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
    case PACKINGBUF_TYPE_SINT:                                      \
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
    case PACKINGBUF_TYPE_FIXED:                                     \
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
    case PACKINGBUF_TYPE_STRING:                                          \
        if (dst != NULL)                                            \
        {   /* int32 */                                         \
            temp.i32 = PackingBuf_atoi32((char *)value->data,value->size);    \
            *dst = (TYPE)(temp.i32);                            \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_BINARY:                                          \
        if (dst != NULL)                                            \
        {   /* uint32 */                                        \
            temp.u32 = PackingBuf_btoi32(value->data,value->size);    \
            *dst = (TYPE)(temp.u32);                            \
        }                                                       \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_UINT_VECTOR:                                     \
        if (dst != NULL)                                            \
        {   /* uint32 */                                        \
            temp.u32 = 0;                                       \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)(temp.u32);                            \
        }                                                       \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_SINT_VECTOR:                                     \
        if (dst != NULL)                                            \
        {   /* uint32 */                                        \
            temp.u32 = 0;                                       \
            decode_varint32(value->data,value->data+value->size,&temp.u32); \
            *dst = (TYPE)((int32_t)UNZIGZAG(temp.u32));         \
        }                                                           \
        result = sizeof(*dst);                                      \
        break;                                                      \
                                                                    \
    case PACKINGBUF_TYPE_FLOAT_VECTOR:                                    \
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
DEFINE_PACKINGBUFVALUE_GET(Int8,  int8_t);
DEFINE_PACKINGBUFVALUE_GET(Int16, int16_t);
DEFINE_PACKINGBUFVALUE_GET(Int32, int32_t);
#if ENABLE_INT64_SUPPORT
DEFINE_PACKINGBUFVALUE_GET(Int64, int64_t);
#endif

//uint 8,16,32,64
DEFINE_PACKINGBUFVALUE_GET(Uint8,  uint8_t);
DEFINE_PACKINGBUFVALUE_GET(Uint16, uint16_t);
DEFINE_PACKINGBUFVALUE_GET(Uint32, uint32_t);
#if ENABLE_INT64_SUPPORT
DEFINE_PACKINGBUFVALUE_GET(Uint64, uint64_t);
#endif

//int & uint
DEFINE_PACKINGBUFVALUE_GET(Int,  int);
DEFINE_PACKINGBUFVALUE_GET(Uint, unsigned int);

unsigned int PACKINGBUFAPI PackingBufValue_IsNull(PACKINGBUF_VALUE *value)
{
    return ((value->type==PACKINGBUF_TYPE_FIXED) && (value->size==0))?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsInt(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_SINT)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsUint(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_UINT)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsInteger(PACKINGBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_UINT:
    case PACKINGBUF_TYPE_SINT:
        return 1;
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsFloat(PACKINGBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_FIXED:
        if (value->size==sizeof(float))
        {
            return 1;
        }
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsDouble(PACKINGBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_FIXED:
        if (value->size==sizeof(double))
        {
            return 1;
        }
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsIntVector(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_SINT_VECTOR)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsUintVector(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_UINT_VECTOR)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsIntegerVector(PACKINGBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_UINT_VECTOR:
    case PACKINGBUF_TYPE_SINT_VECTOR:
        return 1;
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsFloatVector(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_FLOAT_VECTOR)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsString(PACKINGBUF_VALUE *value)
{
    return (value->type==PACKINGBUF_TYPE_STRING)?1:0;
}

unsigned int PACKINGBUFAPI PackingBufValue_IsBinary(PACKINGBUF_VALUE *value)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_STRING:
    case PACKINGBUF_TYPE_BINARY:
        return 1;
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_GetIntSize(PACKINGBUF_VALUE *value)
{
    unsigned int result = 0;

    switch(value->type)
    {
    case PACKINGBUF_TYPE_UINT:
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

    case PACKINGBUF_TYPE_SINT:
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

    case PACKINGBUF_TYPE_FIXED:
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

    case PACKINGBUF_TYPE_STRING:
        result = 4;
        break;

    case PACKINGBUF_TYPE_BINARY:
        result = 4;
        break;

    case PACKINGBUF_TYPE_UINT_VECTOR:
        result = 4;
        break;

    case PACKINGBUF_TYPE_SINT_VECTOR:
        result = 4;
        break;

    case PACKINGBUF_TYPE_FLOAT_VECTOR:
        result = 4;
        break;
    }

    return result;
}

unsigned int PACKINGBUFAPI PackingBufValue_GetString(PACKINGBUF_VALUE *value, PACKINGBUF_STRING *to)
{
    if ((value->type == PACKINGBUF_TYPE_STRING) &&
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

unsigned int PACKINGBUFAPI PackingBufValue_GetBinary(PACKINGBUF_VALUE *value, PACKINGBUF_BINARY *to)
{
    switch (value->type)
    {
    case PACKINGBUF_TYPE_STRING:
    case PACKINGBUF_TYPE_BINARY:
        to->data = value->data;
        to->size = value->size;
        return 1;
    }

    return 0;
}

unsigned int PACKINGBUFAPI PackingBufValue_GetVector(PACKINGBUF_VALUE *value, PACKINGBUF_VECTOR *vector)
{
    switch(value->type)
    {
    case PACKINGBUF_TYPE_UINT_VECTOR:
    case PACKINGBUF_TYPE_SINT_VECTOR:
    case PACKINGBUF_TYPE_FLOAT_VECTOR:
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
void PackingBuf_test()
{
    static char buffer[2048];
    PACKINGBUF packingbuf;
    unsigned int size;

    {
        char temp[16]={0,1,2,3,4,5,6,7,8,9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

        PackingBuf_Init(&packingbuf,buffer,sizeof(buffer));

        //int 8
        PackingBuf_PutInt8(&packingbuf,"1",0);
        PackingBuf_PutInt8(&packingbuf,"2",1);
        PackingBuf_PutInt8(&packingbuf,"3",-1);
        PackingBuf_PutInt8(&packingbuf,"4",127);
        PackingBuf_PutInt8(&packingbuf,"5",-127);
        PackingBuf_PutInt8(&packingbuf,"6",-128);

        //uint8
        PackingBuf_PutUint8(&packingbuf,"1",0);
        PackingBuf_PutUint8(&packingbuf,"2",1);
        PackingBuf_PutUint8(&packingbuf,"3",127);
        PackingBuf_PutUint8(&packingbuf,"4",128);
        PackingBuf_PutUint8(&packingbuf,"5",255);
        PackingBuf_PutUint8(&packingbuf,"6",256);

        //int16
        PackingBuf_PutInt16(&packingbuf,"1",0);
        PackingBuf_PutInt16(&packingbuf,"2",1);
        PackingBuf_PutInt16(&packingbuf,"3",-1);
        PackingBuf_PutInt16(&packingbuf,"4",127);
        PackingBuf_PutInt16(&packingbuf,"5",-127);
        PackingBuf_PutInt16(&packingbuf,"6",-128);
        PackingBuf_PutInt16(&packingbuf,"7",129);
        PackingBuf_PutInt16(&packingbuf,"8",-129);
        PackingBuf_PutInt16(&packingbuf,"9",32767);
        PackingBuf_PutInt16(&packingbuf,"0",-32767);
        PackingBuf_PutInt16(&packingbuf,"0",-32768);

        //uint16
        PackingBuf_PutInt16(&packingbuf,"0",0);
        PackingBuf_PutInt16(&packingbuf,"0",1);
        PackingBuf_PutInt16(&packingbuf,"0",255);
        PackingBuf_PutInt16(&packingbuf,"0",256);
        PackingBuf_PutInt16(&packingbuf,"0",65535);
        PackingBuf_PutInt16(&packingbuf,"0",65536);

        //int32
        PackingBuf_PutInt32(&packingbuf,"0",0x7fffffff);
        PackingBuf_PutInt32(&packingbuf,"0",-1);
        PackingBuf_PutInt32(&packingbuf,"0",-0x7fffffff);
        PackingBuf_PutInt32(&packingbuf,"0",0xffffffff);

        //uint32
        PackingBuf_PutUint32(&packingbuf,"0",0x7fffffff);
        PackingBuf_PutUint32(&packingbuf,"0",0x7fffffff+1);
        PackingBuf_PutUint32(&packingbuf,"0",0xffffffff);
        PackingBuf_PutUint32(&packingbuf,"0",0xffffffff-1);
        PackingBuf_PutUint32(&packingbuf,"0",0xffffffff+1);

        //float
        PackingBuf_PutFloat(&packingbuf,"0",100.002f);
        PackingBuf_PutDouble(&packingbuf,"0",100.002);
        PackingBuf_PutFloat(&packingbuf,"0",100.002f);

        //uint64
        PackingBuf_PutUint64(&packingbuf,"0",(uint64_t)0xffffffff + 0xffff);

        //string & binary
        PackingBuf_PutString(&packingbuf,"0","hello world!");

        PackingBuf_PutBinary(&packingbuf,"0",temp,sizeof(temp));
        PackingBuf_PutBinary(&packingbuf,"0",temp,sizeof(temp));
        {
            unsigned char x[1024];

            {
                int i;
                for (i=0; i<sizeof(x); i++)
                {
                    x[i] = i;
                }
            }
            PackingBuf_PutBinary(&packingbuf,"0",x,sizeof(x));
        }

        PackingBuf_PutString(&packingbuf,"0","hello world1!");
        PackingBuf_PutString(&packingbuf,"0","hello world2!");
        PackingBuf_PutString(&packingbuf,"0","hello world3!");

        {
            PACKINGBUF_VECTOR vector;

            if (PackingBuf_PutVectorBegin(&packingbuf, &vector, PACKINGBUF_VECTOR_TYPE_UINT) != 0)
            {
                PackingBufVector_PutUint(&vector, 100);
                PackingBufVector_PutUint(&vector, 200);
                PackingBufVector_PutUint(&vector, 300);

                PackingBuf_PutVectorEnd(&packingbuf, &vector, "100");
            }

            if (PackingBuf_PutVectorBegin(&packingbuf, &vector, PACKINGBUF_VECTOR_TYPE_SINT) != 0)
            {
                PackingBufVector_PutInt(&vector, 100);
                PackingBufVector_PutInt(&vector, -200);
                PackingBufVector_PutInt(&vector, -300);

                PackingBuf_PutVectorEnd(&packingbuf, &vector, "200");
            }

            if (PackingBuf_PutVectorBegin(&packingbuf, &vector, PACKINGBUF_VECTOR_TYPE_FLOAT) != 0)
            {
                PackingBufVector_PutFloat(&vector, 100.1);
                PackingBufVector_PutFloat(&vector, 200.2);
                PackingBufVector_PutFloat(&vector, 300.3);

                PackingBuf_PutVectorEnd(&packingbuf, &vector, "300");
            }

            {
                int iii=0;
            }
        }

        size = PackingBuf_Finish(&packingbuf);
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

        PACKINGBUF_VALUE value;

        PackingBuf_Init(&packingbuf,buffer,size);

        PackingBuf_FindValue(&packingbuf, &value, "300", 0);

        //int8:0
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);

        //int8:1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);

        //int8:-1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);

        //int8:127
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);

        //int8:-127
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);

        //int8:-128
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt8(&value,&i8);



        //uint8:0
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,&ui8);

        //uint8:1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,&ui8);

        //uint8:127
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,&ui8);

        //uint8:128
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,&ui8);

        //uint8:255
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,0);

        //uint8:256
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint8(&value,&ui8);

        //int16_t:0
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:127
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-127
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-128
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:129
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-129
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:32767
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-32767
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);

        //int16_t:-32768
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt16(&value,&i16);


        //uint16:0
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,&ui16);

        //uint16:1
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,&ui16);

        //uint16:255
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,0);

        //uint16:256
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,&ui16);

        //uint16:65535
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,&ui16);

        //uint16:65536
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint16(&value,&ui16);

        //int32
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt32(&value,&i32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt32(&value,&i32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt32(&value,&i32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetInt32(&value,&i32);

        //uint32
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint32(&value,&ui32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint32(&value,&ui32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint32(&value,&ui32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint32(&value,&ui32);
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetUint32(&value,&ui32);

        //float
        PackingBuf_GetValue(&packingbuf,&value);
        PackingBufValue_GetFloat(&value,&f);

        while (PackingBuf_GetValue(&packingbuf, &value) != 0)
        {
            switch (value.type)
            {
            case PACKINGBUF_TYPE_UINT:
                PackingBufValue_GetUint8(&value,  &ui8);
                PackingBufValue_GetUint16(&value, &ui16);
                PackingBufValue_GetUint32(&value, &ui32);
                PackingBufValue_GetUint64(&value, &ui64);
                break;
            case PACKINGBUF_TYPE_SINT:
                PackingBufValue_GetInt8(&value,  &i8);
                PackingBufValue_GetInt16(&value, &i16);
                PackingBufValue_GetInt32(&value, &i32);
                PackingBufValue_GetInt64(&value, &i64);
                break;
            case PACKINGBUF_TYPE_FIXED:
                PackingBufValue_GetFloat(&value,&f);
                PackingBufValue_GetDouble(&value,&d);
                break;
            case PACKINGBUF_TYPE_STRING:
                {
                    PACKINGBUF_STRING string;
                    PackingBufValue_GetString(&value, &string);
                    string.length = string.length;
                }
                //break;
            case PACKINGBUF_TYPE_BINARY:
                {
                    PACKINGBUF_BINARY binary;
                    PackingBufValue_GetBinary(&value, &binary);
                    binary.size = binary.size;
                }
                break;
            case PACKINGBUF_TYPE_UINT_VECTOR:
                {
                    PACKINGBUF_VECTOR vector;
                    if (PackingBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackingBufVector_GetUint32(&vector, &ui32) !=0)
                        {
                            ui32=ui32;
                        }
                    }
                }
                break;
            case PACKINGBUF_TYPE_SINT_VECTOR:
                {
                    PACKINGBUF_VECTOR vector;
                    if (PackingBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackingBufVector_GetInt32(&vector, &i32) !=0)
                        {
                            ui32=ui32;
                        }
                    }
                }
                break;
            case PACKINGBUF_TYPE_FLOAT_VECTOR:
                {
                    PACKINGBUF_VECTOR vector;
                    if (PackingBufValue_GetVector(&value, &vector) != 0)
                    {
                        while(PackingBufVector_GetFloat(&vector, &f) !=0)
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

