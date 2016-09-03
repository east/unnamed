#include <stdint.h>

#include <system.h>
#include <evloop.h>
#include <net.h>

#include "layer1.h"

#include <net_udp.h>

static int
unpack_header(l1_header_info* dst,
                      uint8_t *data,
                      int size)
{
  if (size < L1_MIN_PACKET_SIZE)
    /* invalid size */
    return 1; 

  /* set token */
  dst->token = ((uint32_t*)data)[0];

  uint16_t info = ((uint16_t*)&data[4])[0];

  if (info >> 3 != L1_SERVER_MAGIC)
    /* magic doesn't match */
    return 1;

  dst->ctrl_type = info & 0x7;

  /* success */
  return 0;
}

static void
cb_on_data(void *user, const uint8_t *data,
            int size, net_addr *from)
{

}

bool
net_l1_server_init(net_l1_server *srv,
                    clib_evloop *ev, net_addr *bind_to,
                    net_l1_cb_on_client cb_on_client,
                    net_l1_cb_on_client_drop cb_on_client_drop,
                    net_l1_cb_on_client_packet cb_on_client_packet,
                    void *user)
{
  srv->udp = clib_net_udp_new(ev, bind_to, cb_on_data, srv);

  if (!srv->udp)
    return false;
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
