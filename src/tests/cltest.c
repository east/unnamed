#include <stdio.h>
#include <stdlib.h>

#include <system.h>
#include <evloop.h>

#include <network/layer1.h>

static clib_evloop *ev;
static net_l1_client cl;

static void
cb_on_connect(struct net_l1_client *client)
{
  printf("connected\n");
    
  /* send some data */
  unsigned char data[] = {"this is a test"};
  net_l1_client_send(client, data, sizeof(data));
}

static void
cb_on_packet(struct net_l1_client *client,
              const uint8_t *data, int size)
{

}

static void
cb_on_drop(struct net_l1_client *client)
{
  printf("we got dropped\n");
}

static void
evloop_on_init(clib_evloop *evloop)
{
  printf("init\n");
  ev = evloop;

  net_addr addr;
  IPV4_SET(addr, 127, 0, 0, 1, L1_DEFAULT_PORT);

  if (!net_l1_client_init(&cl, ev, &addr, cb_on_connect,
                  cb_on_drop, cb_on_packet, NULL))
  {
    printf("failed to bind port \n");
    exit(1);
  }
}

static void evloop_on_destroy(clib_evloop *evloop)
{
  printf("on destroy\n");
  net_l1_client_uninit(&cl, evloop);
}

int
main()
{
  clib_evloop_main(evloop_on_init, evloop_on_destroy);
}
