/*
    gcc packbuf.c example.c -o example
*/

#include "packbuf.h"

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

int main()
{
    test();
    return 0;
}
