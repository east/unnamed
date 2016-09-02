#include <stdio.h>
#include <stdlib.h>

#include <system.h>
#include <evloop.h>

#include <network/layer1.h>

static clib_evloop *ev;

static void
evloop_on_init(clib_evloop *evloop)
{
  printf("init\n");
  ev = evloop;


}

static void evloop_on_destroy(clib_evloop *evloop)
{
}

int
main()
{
  clib_evloop_main(evloop_on_init, evloop_on_destroy);
}
