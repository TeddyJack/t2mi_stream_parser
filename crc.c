#include "crc.h"


#define CRC32_WIDTH    (8 * sizeof(crc32))
#define CRC32_TOPBIT   (1 << (CRC32_WIDTH - 1))
#define CRC8_WIDTH    (8 * sizeof(crc8))
#define CRC8_TOPBIT   (1 << (CRC8_WIDTH - 1))


crc32
crc32Slow(unsigned char const message[], int nBytes, crc32 remainder)
{
	int            byte;
	unsigned char  bit;

    for (byte = 0; byte < nBytes; ++byte)						// Perform modulo-2 division, a byte at a time.
    {
        remainder ^= (message[byte] << (CRC32_WIDTH - 8));		// Bring the next byte into the remainder

        for (bit = 8; bit > 0; --bit)							// Perform modulo-2 division, a bit at a time.
        {
            if (remainder & CRC32_TOPBIT)						// Try to divide the current data bit.
                remainder = (remainder << 1) ^ CRC32_POLYNOMIAL;
            else
                remainder = (remainder << 1);
        }
    }

	return (remainder ^ CRC32_FINAL_XOR_VALUE);					// The final remainder is the CRC result.
}

crc8
crc8Slow(unsigned char const message[], int nBytes, crc8 remainder)
{
	int            byte;
	unsigned char  bit;

	for (byte = 0; byte < nBytes; ++byte)
	{
		remainder ^= (message[byte] << (CRC8_WIDTH - 8));

		for (bit = 8; bit > 0; --bit)
		{
			if (remainder & CRC8_TOPBIT)
				remainder = (remainder << 1) ^ CRC8_POLYNOMIAL;
			else
				remainder = (remainder << 1);
		}
	}

	return (remainder ^ CRC8_FINAL_XOR_VALUE);
}