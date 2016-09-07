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

bool
net_l1_server_full(net_l1_server *srv)
{
  return srv->num_clients == NET_L1_SRV_MAX_CLIENTS;
}

static void
srv_send_ping(net_l1_server *srv, net_l1_server_client *cl)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = (L1_SRV_MAGIC<<3)|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PING;
  packet.pingpong.token = cl->cur_ping_token;

  /* send */
  clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                        11, &cl->addr);
}

static void
srv_send_pong(net_l1_server *srv, net_l1_server_client *cl,
                uint32_t tmp_token)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = (L1_SRV_MAGIC<<3)|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PONG;
  packet.pingpong.token = tmp_token;

  /* send */
  clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                        11, &cl->addr);
}

static void
cl_send_ping(net_l1_client *cl)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = (L1_CL_MAGIC<<3)|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PING;
  packet.pingpong.token = cl->cur_ping_token;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        11, &cl->remote_addr);
}

static void
cl_send_pong(net_l1_client *cl, uint32_t tmp_token)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = (L1_CL_MAGIC<<3)|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PONG;
  packet.pingpong.token = tmp_token;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        11, &cl->remote_addr);
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

  if (ctrl_type == L1_CL_REQUEST_TOKEN && size == 10) {

    if (net_l1_server_full(srv)) {
      //TODO: close
    } else {
      //TODO: verify service id
      /* respond with token */
      /* prepare packet */
      l1_header_raw packet;
      packet.token = token; /* client token */
      packet.ctrl_type = (L1_SRV_MAGIC<<3)|L1_SRV_TOKEN;
      /* session token */
      packet.new_token = compute_token(srv->secret_seed, *from);

      clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                          10, from);
    }

  } else if (ctrl_type == L1_CL_VERIFY_TOKEN && size == 6 &&
              token == compute_token(srv->secret_seed, *from)) {

    /* matching token received, reserve client slot */
    if (net_l1_server_full(srv)) {
      //TODO: close
    } else {
      /* find empty slot */
      int slot;
      for (slot = 0; slot < NET_L1_SRV_MAX_CLIENTS; slot++) {
        if (!srv->clients[slot].active)
          break;
      }

      net_l1_server_client *cl = &srv->clients[slot];

      cl->session_token = token;

      cl->active = true;
      cl->cl_user = NULL;
      cl->addr = *from;

      cl->last_ping = -1;
      cl->last_pong = -1;

      /* set up map entry */
      net_l1_server_client_ref *ref =
              &srv->client_map[srv->num_clients++];

      ref->addr = *from;
      ref->cl = cl;

      /* notify user */
      srv->cb_on_client(srv, cl);

      /* first ping */
      cl->cur_ping_token = random_uint32();
      cl->last_ping = time_get();
      srv_send_ping(srv, cl);
    }


  } else {
    /* valid session packet required */
    /* search for matching client slot */

    int i;
    for (i = 0; i < srv->num_clients; i++) {
      if (clib_net_addr_comp(&srv->client_map[i].addr, from) == 0)
        /* found */
        break;
    }

    if (i != srv->num_clients) {
      /* found */
      net_l1_server_client *cl = srv->client_map[i].cl;

      if (ctrl_type == L1_PINGPONG && size == 11) {
        if (data[6] == L1_PINGPONG_PING) {
          /* respond with pong */
          srv_send_pong(srv, cl, ((uint32_t*)&data[7])[0]);
        } else {
          /* verify pong */
          if (((uint32_t*)&data[7])[0] == cl->cur_ping_token) {
            //TODO: measure latency
            cl->last_pong = time_get();
          }
        }
      } else if (ctrl_type == L1_CLOSE) {
        /* client closed session */
        srv->cb_on_client_drop(srv, cl);

        /* free client slot */
        cl->active = false;
        srv->num_clients--;
        if (srv->num_clients > 0 && srv->num_clients != i)
          srv->client_map[i] = srv->client_map[srv->num_clients];
      } else if (ctrl_type == L1_MESSAGE) {
        /* valid client packet */
        srv->cb_on_client_packet(srv, cl, &data[6], size-6);
      }
    }
  }
}

static void
verify_token(net_l1_client *cl)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = (L1_CL_MAGIC<<3)|L1_CL_VERIFY_TOKEN;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        6, &cl->remote_addr);
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

  if (cl->state == L1_CL_STATE_REQUEST_TOKEN) {
    if (ctrl_type == L1_SRV_TOKEN && size == 10 &&
          token == cl->request_token) {

      /* received the token, next state */
      cl->state = L1_CL_STATE_VERIFY_TOKEN;
      cl->session_token = ((uint32_t*)&data[6])[0];
      verify_token(cl);
      cl->last_request = time_get();
    }
  } else if (cl->session_token == token) {
    /* valid packet from remote server */

    if (cl->state != L1_CL_STATE_ONLINE) {
      cl->state = L1_CL_STATE_ONLINE;
      cl->cb_on_connect(cl);
    }

    if (ctrl_type == L1_PINGPONG && size == 11) {
      if (data[6] == L1_PINGPONG_PING) {
        /* respond with pong */
        cl_send_pong(cl, ((uint32_t*)&data[7])[0]);
      } else {
        /* verify pong */
        if (((uint32_t*)&data[7])[0] == cl->cur_ping_token) {
          printf("got pong\n");
          //TODO: measure latency
          cl->last_pong = time_get();
        }
      }
    } else if (ctrl_type == L1_CLOSE) {
      /* server closed session */
      cl->state = L1_CL_STATE_OFFLINE;
      cl->cb_on_drop(cl);
    } else if (ctrl_type == L1_MESSAGE) {
      /* valid server packet */
      cl->cb_on_packet(cl, &data[6], size-6);
    }
  }
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

  return true;
}

static void
request_token(net_l1_client *cl)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->request_token;
  packet.ctrl_type = (L1_CL_MAGIC<<3)|L1_CL_REQUEST_TOKEN;
  packet.service_id = L1_SERVICE_NONE;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        10, &cl->remote_addr);
}

bool
net_l1_client_init(net_l1_client *cl,
                    clib_evloop *ev, net_addr *connect_to,
                    net_l1_cb_on_connect cb_on_connect,
                    net_l1_cb_on_drop cb_on_drop,
                    net_l1_cb_on_packet cb_on_packet,
                    void *user)
{
  net_addr bind_addr;
  ADDR_SET_ANY(&bind_addr, connect_to->type)

  cl->udp = clib_net_udp_new(ev, &bind_addr, cb_on_data_from_srv, cl);

  if (!cl->udp)
    return false;

  cl->remote_addr = *connect_to;

  cl->cb_on_connect = cb_on_connect;
  cl->cb_on_drop = cb_on_drop;
  cl->cb_on_packet = cb_on_packet;

  cl->user = user;

  cl->last_request = -1;
  cl->last_ping = -1;
  cl->last_pong = -1;

  /* init connect */
  cl->state = L1_CL_REQUEST_TOKEN;
  cl->request_token = random_uint32();

  request_token(cl);
  cl->connecting_since = time_get();
  cl->last_request = time_get();

  return true;
}

void net_l1_server_uninit(net_l1_server *srv, clib_evloop *ev)
{
  //TODO: notify clients

  clib_net_udp_destroy(srv->udp);
}

void net_l1_client_uninit(net_l1_client *cl, clib_evloop *ev)
{
  clib_net_udp_destroy(cl->udp);
}

