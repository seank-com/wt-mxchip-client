
#ifndef uuid_h
#define uuid_h

#include "Arduino.h"

class UUIDClass
{
  public:
    UUIDClass();

    char* create();

  private:
    byte m_bits[16];
    char m_uuid[37];
};
extern UUIDClass uuid;

#endif