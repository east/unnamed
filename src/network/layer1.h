#ifndef NETWORK_LAYER1_H
#define NETWORK_LAYER1_H

/* TODO: remove */
#include <net_udp.h>

enum
{
  /* TODO: move */
  L1_DEFAULT_PORT=40040,

  L1_MIN_PACKET_SIZE=6,
  L1_TOKEN_SIZE=4,
  L1_SECRET_SEED_SIZE=8,

  L1_CL_MAGIC=0x1e2a,
  L1_SRV_MAGIC=0x0e7c,
};

/* control types */
enum
{
  /* client -> server */
  L1_CL_REQUEST_TOKEN=0x0,
  L1_CL_VERIFY_TOKEN=0x1,
  L1_CL_CLOSE=0x2,
  L1_CL_MAX,

  /* server -> client */
  L1_SRV_IGNORE_REQUEST=0x0,
  L1_SRV_TOKEN=0x1,
  L1_SRV_INVALID_TOKEN=0x2,
  L1_SRV_MAX,

  /* both <-> */
  L1_PINGPONG=0x6,
  L1_MESSAGE=0x7,
};

typedef struct
{
  uint32_t token;
  int ctrl_type;

  union {
    /* L1_IGNORE_REQUEST */
    uint8_t reason;
    /* L1_TOKEN, L1_CHANGE_TOKEN */
    uint32_t new_token;
    /* L1_REQUEST_TOKEN */
    uint32_t cl_token;
  };
} l1_header_info;

/* server */
enum
{
  NET_L1_SRV_MAX_CLIENTS=2048,
};

struct net_l1_server;
struct net_l1_server_client;

/* callbacks */
typedef void (*net_l1_cb_on_client)
  (struct net_l1_server *srv,
   struct net_l1_server_client *client);
typedef void (*net_l1_cb_on_client_drop)
  (struct net_l1_server *srv,
   struct net_l1_server_client *client);
typedef void (*net_l1_cb_on_client_packet)
  (struct net_l1_server *srv,
   struct net_l1_server_client *client, uint8_t *data, int size);

typedef struct net_l1_server_client
{
  bool active;
  void *cl_user;
  net_addr addr;
} net_l1_server_client;

typedef struct net_l1_server_client_ref
{
  net_addr addr;
  net_l1_server_client *cl; 
} net_l1_server_client_ref;

typedef struct net_l1_server
{
  clib_net_udp *udp;

  uint8_t secret_seed[L1_SECRET_SEED_SIZE];

  int num_clients;
  net_l1_server_client_ref client_map[NET_L1_SRV_MAX_CLIENTS];
  net_l1_server_client clients[NET_L1_SRV_MAX_CLIENTS];

  net_l1_cb_on_client cb_on_client;
  net_l1_cb_on_client_drop cb_on_client_drop;
  net_l1_cb_on_client_packet cb_on_client_packet;

  void *user;
} net_l1_server;

bool
net_l1_server_init(net_l1_server *srv,
                    clib_evloop *ev, net_addr *bind_to,
                    net_l1_cb_on_client cb_on_client,
                    net_l1_cb_on_client_drop cb_on_client_drop,
                    net_l1_cb_on_client_packet cb_on_client_packet,
                    void *user);

/* client */
struct net_l1_client;

/* callbacks */
typedef void (*net_l1_cb_on_connect)
  (void *user, struct net_l1_client *client);
typedef void (*net_l1_cb_on_packet)
  (void *user, struct net_l1_client *client);
typedef void (*net_l1_cb_on_drop)
  (void *user, struct net_l1_client *client);

typedef struct net_l1_client
{
  clib_net_udp *udp;

  net_l1_cb_on_connect cb_on_connect;
  net_l1_cb_on_drop cb_on_drop;
  net_l1_cb_on_packet cb_on_packet;
  void *user;
} net_l1_client;

bool
net_l1_client_init(net_l1_client *cl,
                    clib_evloop *ev, net_addr *bind_to,
                    net_l1_cb_on_connect cb_on_connect,
                    net_l1_cb_on_drop cb_on_drop,
                    net_l1_cb_on_packet cb_on_packet,
                    void *user);

#endif
