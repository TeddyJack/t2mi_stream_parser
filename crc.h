
#define FALSE	0
#define TRUE	!FALSE


typedef unsigned char  crc8;

#define CRC8_POLYNOMIAL			0xD5
#define CRC8_INITIAL_REMAINDER	0x00
#define CRC8_FINAL_XOR_VALUE	0x00

typedef unsigned long  crc32;

#define CRC32_POLYNOMIAL		0x04C11DB7
#define CRC32_INITIAL_REMAINDER	0xFFFFFFFF
#define CRC32_FINAL_XOR_VALUE	0x00000000


crc32   crc32Slow(unsigned char const message[], int nBytes, crc32 remainder);
crc8    crc8Slow(unsigned char const message[], int nBytes, crc8 remainder);
