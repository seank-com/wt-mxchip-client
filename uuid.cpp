#include "uuid.h"

static char* getChar = "01234567890abcdef";

UUIDClass::UUIDClass()
{
  randomSeed(analogRead(0));
}

char* UUIDClass::create()
{
  int i;
  char* pos = m_uuid;

  // Generate a Version 4 UUID according to RFC4122
  for(i = 0; i < 15; i += 1)
    m_bits[i] = (byte)random(256);

  // Although the UUID contains 128 bits, only 122 of those are random.
  // The other 6 bits are fixed, to indicate a version number.
  m_bits[6] = 0x40 | (0x0F & m_bits[6]); 
  m_bits[8] = 0x80 | (0x3F & m_bits[8]);

  for(i = 0; i < 15; i += 1)
  {
    int high_nibble =  m_bits[i] >> 4;
    int low_nibble = m_bits[i] & 0x0f;
    *pos = getChar[high_nibble]; pos += 1;
    *pos = getChar[low_nibble]; pos += 1;
    if (i == 4 || i == 6 || i == 8 || i == 10)
    {
      *pos = '-'; pos += 1;
    }
  }
  *pos = '\0';

  return m_uuid;
}

UUIDClass uuid;