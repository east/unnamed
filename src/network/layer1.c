#include <stdint.h>

#include "layer1.h"

static int
unpack_header(l1_header_info* dst,
                      uint8_t *data,
                      int size)
{
  if (size < L1_MIN_PACKET_SIZE)
    // invalid size
    return 1; 

  // set token
  dst->token = ((uint32_t*)data)[0];

  uint16_t info = ((uint16_t*)&data[4])[0];

  if (info >> 3 != L1_SERVER_MAGIC)
    // magic doesn't match
    return 1;

  dst->ctrl_type = info & 0x7;

  // success
  return 0;
}

