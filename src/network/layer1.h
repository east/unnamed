#ifndef NETWORK_LAYER1_H
#define NETWORK_LAYER1_H

enum
{
  L1_MIN_PACKET_SIZE=6,

  L1_CLIENT_MAGIC=0x1e2a,
  L1_SERVER_MAGIC=0x0e7c,
};

// control types
enum
{
  // client -> server
  L1_REQUEST_TOKEN=0x0,
  L1_VERIFY_TOKEN=0x1,
  L1_CL_MAX,

  // server -> client
  L1_IGNORE_REQUEST=0x0,
  L1_TOKEN=0x1,
  L1_CHANGE_TOKEN=0x2,
  L1_INVALID_TOKEN=0x3,
  L1_SRV_MAX,

  // both <->
  L1_PINGPONG=0x6,
  L1_MESSAGE=0x7,
};

typedef struct
{
  uint32_t token;
  int ctrl_type;

  union {
    // L1_IGNORE_REQUEST
    uint8_t reason;
    // L1_TOKEN, L1_CHANGE_TOKEN
    uint32_t new_token;
    // L1_REQUEST_TOKEN
    uint32_t cl_token;
  };
} l1_header_info;

#endif
