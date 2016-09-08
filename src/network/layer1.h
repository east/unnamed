#ifndef NETWORK_LAYER1_H
#define NETWORK_LAYER1_H

/* TODO: remove */
#include <net_udp.h>

enum
{
  /* TODO: move */
  L1_DEFAULT_PORT=40040,
  L1_TIMEOUT=10*1000,
  L1_PING_RETRY_INTERVAL=500,

  L1_HEADER_SIZE=6,
  L1_MIN_PACKET_SIZE=L1_HEADER_SIZE,
  L1_TOKEN_SIZE=4,
  L1_SECRET_SEED_SIZE=8,

  L1_CL_MAGIC=0x1e2a,
  L1_SRV_MAGIC=0x0e7c,

  /* service ids */
  L1_SERVICE_NONE=0,
};

/* control types */
enum
{
  /* client -> server */
  L1_CL_REQUEST_TOKEN=0x0,
  L1_CL_VERIFY_TOKEN=0x1,
  L1_CL_MAX,

  /* server -> client */
  L1_SRV_TOKEN=0x0,
  L1_SRV_MAX,

  /* both <-> */
  L1_CLOSE=0x5,
  L1_PINGPONG=0x6,
  L1_MESSAGE=0x7,

  /* ping/pong */
  L1_PINGPONG_PING=0,
  L1_PINGPONG_PONG,
};

typedef struct __attribute__((__packed__))
{
  uint32_t token;
  uint16_t ctrl_type;

  union {
    /* L1_IGNORE_REQUEST */
    uint16_t reason;
    /* L1_TOKEN, L1_CHANGE_TOKEN */
    uint32_t new_token;
    /* L1_REQUEST_TOKEN */
    uint32_t service_id;
    /* L1_PINGPONG */
    struct __attribute__((__packed__)) {
      uint8_t type;
      uint32_t token;
    } pingpong;
  };
} l1_header_raw;

/* server */
enum
{
  NET_L1_SRV_MAX_CLIENTS=256,
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
   struct net_l1_server_client *client,
   const uint8_t *data, int size);

typedef struct net_l1_server_client
{
  bool active;
  int map_slot;
  void *cl_user;
  net_addr addr;

  uint32_t session_token;
 
  /* ping timers */
  int64_t last_ping;

  int64_t last_valid_packet;

  /* pong token needs to match */
  uint32_t cur_ping_token;

} net_l1_server_client;

typedef struct net_l1_server_client_ref
{
  net_addr addr;
  net_l1_server_client *cl;
} net_l1_server_client_ref;

typedef struct net_l1_server
{
  clib_net_udp *udp;
  struct clib_evloop_timer *timer;

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

bool net_l1_server_full(net_l1_server *srv);
void net_l1_server_send(net_l1_server *srv, net_l1_server_client *cl,
                        const uint8_t *data, size_t size);
void net_l1_server_client_close(net_l1_server *srv,
                                net_l1_server_client *cl);

void net_l1_server_uninit(net_l1_server *srv, clib_evloop *ev);

/* client */
struct net_l1_client;

/* callbacks */
typedef void (*net_l1_cb_on_connect)
  (struct net_l1_client *client);
typedef void (*net_l1_cb_on_packet)
  (struct net_l1_client *client,
   const uint8_t *data, int size);
typedef void (*net_l1_cb_on_drop)
  (struct net_l1_client *client);


enum
{
  /* client states */
  L1_CL_STATE_REQUEST_TOKEN=0,
  L1_CL_STATE_VERIFY_TOKEN,
  L1_CL_STATE_ONLINE,
  L1_CL_STATE_OFFLINE,
};

typedef struct net_l1_client
{
  clib_net_udp *udp;
  struct clib_evloop_timer *timer;

  int state;

  uint32_t session_token;

  int64_t connecting_since;
  /* last token/verification request */
  int64_t last_request;
  uint32_t request_token;
  /* ping timers */
  int64_t last_ping;

  int64_t last_valid_packet;

  /* pong token needs to match */
  uint32_t cur_ping_token;

  net_addr remote_addr;

  net_l1_cb_on_connect cb_on_connect;
  net_l1_cb_on_drop cb_on_drop;
  net_l1_cb_on_packet cb_on_packet;
  void *user;
} net_l1_client;

bool
net_l1_client_init(net_l1_client *cl,
                    clib_evloop *ev, net_addr *connect_to,
                    net_l1_cb_on_connect cb_on_connect,
                    net_l1_cb_on_drop cb_on_drop,
                    net_l1_cb_on_packet cb_on_packet,
                    void *user);

void net_l1_client_send(net_l1_client *cl,
                        const uint8_t *data, size_t size);

void net_l1_client_uninit(net_l1_client *cl, clib_evloop *ev);

#endif
