# packbuf

lightweight data interchange format,like google's protobuf

一种协议交换格式

0)在不能使用protobuf协议的地方(比如WSN，6LoWPAN环境)使用

1)头部字节

  |7|6|5|4|3|2|1|0|
  
   -----   length       bit 4,3,2,1,0
   
   type  ---------      bit 7,6,5 

1.1) type

  type:0~7

1.2) length
length:0~27  短数据格式，后跟0~27字节的数据
       28~31 长数据格式，后跟1~4字节(28为1字节,29为2字节,30为3字节,31为4字节;BE格式;~4G)的长度值(传输值为n-28),再后面紧跟n字节的数据

2)type

  0  uint(无符号整数，BE格式，从非零字节存储，长度记录到length字段)
  
  1  sint(有符号整数，先用ZigZag映射到无符号整数，再按照无符号整数格式存储)
  
  2  float(存储float/double数据，将浮点数据映射到相应长度无符号整数，再按BE存储，float(uint32_t)/double(uint64_t)
  
  3  string(zero terminated) 0~27字节，短格式存储；28~4G,长格式存储
  
  4  binary(octet string)    0~27字节，短格式存储；28~4G,长格式存储
  
  5  uint vector(无符号整数数组，BE格式，按照Base 128 Varints 格式存储)
  
  6  sint vector(有符号整数数组，先用ZigZag映射到无符号整数，再按照无符号整数数组存储)
  
  7  float vector(float/double数组，先映射为uint32_t/uint64_t，再按照无符号整数数组存储)

3)tag(不同于头部中的type，这里供应用程序用)

  方案1:占一个字节(0~255)，后跟头部及数据（用于空间要求极为苛刻的环境，比如不能启用数据包分组的无线环境）
  
  方案2:占两个字节(0~65535)，后跟头部及数据（比Name省空间）

  tag长度需要通过PackBuf_Init()时指定。

