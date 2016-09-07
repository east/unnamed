#include <stdio.h>
#include <stdlib.h>

#include <system.h>
#include <evloop.h>

#include <network/layer1.h>

static clib_evloop *ev;
static net_l1_server srv;

static void
cb_on_client(struct net_l1_server *srv,
              struct net_l1_server_client *client)
{
  printf("new client: %p\n", client);
}

static void
cb_on_client_drop(struct net_l1_server *srv,
                  struct net_l1_server_client *client)
{

}

static void
cb_on_client_packet(struct net_l1_server *srv,
                          struct net_l1_server_client *client,
                          const uint8_t *data, int size)
{

}

static void
evloop_on_init(clib_evloop *evloop)
{
  printf("init\n");
  ev = evloop;

  net_addr addr;
  IPV4_SET(addr, 127, 0, 0, 1, L1_DEFAULT_PORT);

  if (!net_l1_server_init(&srv, ev, &addr, cb_on_client,
                  cb_on_client_drop, cb_on_client_packet, NULL))
  {
    printf("failed to bind port \n");
    exit(1);
  }
}

static void evloop_on_destroy(clib_evloop *evloop)
{
  net_l1_server_uninit(&srv, evloop);
}

int
main()
{
  clib_evloop_main(evloop_on_init, evloop_on_destroy);
}
