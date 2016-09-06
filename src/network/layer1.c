#include <stdint.h>

#include <system.h>
#include <evloop.h>
#include <net.h>

#include <external/md5/md5.h>

#include "layer1.h"

#include <net_udp.h>

static uint32_t
compute_token(uint8_t *secret_seed, net_addr a)
{
  md5_state_t md5;
  uint32_t digest[4];
  md5_init(&md5);

  // append seed
  md5_append(&md5, (md5_byte_t*)secret_seed, L1_SECRET_SEED_SIZE);
  // append octets
  if (a.type == CLIB_NET_IPV4) {
    md5_append(&md5, (md5_byte_t*)&a.octs[0], 4);
  } else if (a.type == CLIB_NET_IPV6) {
    md5_append(&md5, (md5_byte_t*)&a.octs[0], 16);
  } else {
    ASSERT(false, "unexpected addr type")
  }
  // append port
  md5_append(&md5, (md5_byte_t*)&a.port, sizeof(a.port));

  md5_finish(&md5, (md5_byte_t*)digest);

  // complete token
  return digest[0] ^ digest[1] ^ digest[2] ^ digest[3];
}

static void
cb_on_data_from_cl(void *user, const uint8_t *data,
            int size, net_addr *from)
{
  net_l1_server *srv = user;

  if (size < L1_MIN_PACKET_SIZE)
    /* invalid size */
    return;

  uint16_t info = ((uint16_t*)&data[L1_TOKEN_SIZE])[0];

  if (info >> 3 != L1_CL_MAGIC)
    /* magic doesn't match */
    return;

  int ctrl_type = info & 0x7;
  uint32_t token = ((uint32_t*)data)[0];

  if (ctrl_type == L1_CL_REQUEST_TOKEN) {
    printf("token request\n");
  } else if (ctrl_type == L1_CL_VERIFY_TOKEN) {
    printf("verify token\n");
  } else if (ctrl_type == L1_CL_CLOSE) {
    printf("close\n");
  }
}

static void
cb_on_data_from_srv(void *user, const uint8_t *data,
            int size, net_addr *from)
{
  net_l1_client *cl = user;

  if (size < L1_MIN_PACKET_SIZE)
    /* invalid size */
    return;

  uint16_t info = ((uint16_t*)&data[L1_TOKEN_SIZE])[0];

  if (info >> 3 != L1_SRV_MAGIC)
    /* magic doesn't match */
    return;

  int ctrl_type = info & 0x7;
  uint32_t token = ((uint32_t*)data)[0];
  //TODO:
}

bool
net_l1_server_init(net_l1_server *srv,
                    clib_evloop *ev, net_addr *bind_to,
                    net_l1_cb_on_client cb_on_client,
                    net_l1_cb_on_client_drop cb_on_client_drop,
                    net_l1_cb_on_client_packet cb_on_client_packet,
                    void *user)
{
  srv->udp = clib_net_udp_new(ev, bind_to, cb_on_data_from_cl, srv);

  if (!srv->udp)
    return false;

  // init seed
  random_fill(srv->secret_seed, sizeof(srv->secret_seed));

  // reset clients
  srv->num_clients = 0;
  int i;
  for (i = 0; i < NET_L1_SRV_MAX_CLIENTS; i++)
    srv->clients[i].active = false;

  srv->cb_on_client = cb_on_client;
  srv->cb_on_client_drop = cb_on_client_drop;
  srv->cb_on_client_packet = cb_on_client_packet;

  srv->user = user;
}

void net_l1_server_destroy(net_l1_server *srv, clib_evloop *ev)
{
  //TODO:
}

bool
net_l1_client_init(net_l1_client *cl,
                    clib_evloop *ev, net_addr *bind_to,
                    net_l1_cb_on_connect cb_on_connect,
                    net_l1_cb_on_drop cb_on_drop,
                    net_l1_cb_on_packet cb_on_packet,
                    void *user)
{

}
