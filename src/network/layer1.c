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
  packet.ctrl_type = L1_SRV_MAGIC|L1_PINGPONG;
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
  packet.ctrl_type = L1_SRV_MAGIC|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PONG;
  packet.pingpong.token = tmp_token;

  /* send */
  clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                        11, &cl->addr);
}

static void
srv_send_close(net_l1_server *srv, net_addr *addr, uint32_t token)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = token;
  packet.ctrl_type = L1_SRV_MAGIC|L1_CLOSE;

  /* send */
  clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                        6, addr);
}

static void
cl_send_ping(net_l1_client *cl)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = cl->session_token;
  packet.ctrl_type = L1_CL_MAGIC|L1_PINGPONG;
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
  packet.ctrl_type = L1_CL_MAGIC|L1_PINGPONG;
  packet.pingpong.type = L1_PINGPONG_PONG;
  packet.pingpong.token = tmp_token;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        11, &cl->remote_addr);
}

static void
cl_send_close(net_l1_client *cl, uint32_t token)
{
  /* prepare packet */
  l1_header_raw packet;
  packet.token = token;
  packet.ctrl_type = L1_CL_MAGIC|L1_CLOSE;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        6, &cl->remote_addr);
}

static void
free_client_slot(net_l1_server *srv, net_l1_server_client *cl)
{
  int slot = cl->map_slot;

  /* free client slot */
  srv->cb_free_slot(srv, cl);

  srv->num_clients--;
  if (srv->num_clients > 0 && srv->num_clients != slot)
  {
    /* last client moves to open slot */
    srv->client_map[slot] = srv->client_map[srv->num_clients];
    srv->client_map[slot].cl->map_slot = slot;
  }
}

static void
srv_handle_timing(net_l1_server *srv)
{
  int64_t now = time_get();

  //TODO: don't do a full iteration if not necessary
  int i;
  for (i = 0; i < srv->num_clients; i++) {
    net_l1_server_client *cl = srv->client_map[i].cl;

    if (now - cl->last_valid_packet > L1_TIMEOUT*1000) {
      /* session timeout */
      /* try to notify client */
      srv_send_close(srv, &cl->addr, cl->session_token);

      srv->cb_on_client_drop(srv, cl);

      /* free slot, be careful here */
      free_client_slot(srv, cl);
      i--;
    } else if ((cl->last_ping == -1 && now - cl->last_valid_packet > L1_TIMEOUT*1000/2) ||
                (cl->last_ping != -1 && now - cl->last_ping > L1_PING_RETRY_INTERVAL*1000)) {

      /* no response since half of timeout, start/continue pinging */
      cl->last_ping = now;
      cl->cur_ping_token = random_uint32();

      srv_send_ping(srv, cl);
    }
  }
}

static void
cl_handle_timing(net_l1_client *cl)
{
  int64_t now = time_get();

  if (cl->state == L1_CL_STATE_REQUEST_TOKEN ||
        cl->state == L1_CL_STATE_VERIFY_TOKEN) {
    if (now - cl->connecting_since > L1_TIMEOUT*1000) {
      /* session initiation timeout */
      cl->state = L1_CL_STATE_OFFLINE;
      cl->cb_on_drop(cl);
    }
  } else if (cl->state == L1_CL_STATE_ONLINE) {
    /* ping timing */
    if (now - cl->last_valid_packet > L1_TIMEOUT*1000) {
      /* session timeout */
      /* try to notify server */
      cl_send_close(cl, cl->session_token);
      cl->state = L1_CL_STATE_OFFLINE;

      cl->cb_on_drop(cl);
    } else if ((cl->last_ping == -1 && now - cl->last_valid_packet > L1_TIMEOUT*1000/2) ||
                (cl->last_ping != -1 && now - cl->last_ping > L1_PING_RETRY_INTERVAL*1000)) {

      /* no response since half of timeout, start/continue pinging */
      cl->last_ping = now;
      cl->cur_ping_token = random_uint32();

      cl_send_ping(cl);
    }
  }
}

static void
cb_cl_on_timer(void *user)
{
  cl_handle_timing((net_l1_client*)user);
}

static void
cb_srv_on_timer(void *user)
{
  srv_handle_timing((net_l1_server*)user);
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

  if ((info & L1_MAGIC_MASK) != L1_CL_MAGIC)
    /* magic doesn't match */
    return;

  int ctrl_type = info & L1_CTRL_MASK;
  uint32_t token = ((uint32_t*)data)[0];

  if (ctrl_type == L1_CL_REQUEST_TOKEN && size == 10) {

    if (net_l1_server_full(srv)) {
      /* 1st. we cannot accept more clients */
      srv_send_close(srv, from, token);
    } else {
      //TODO: verify service id
      /* respond with token */
      /* prepare packet */
      l1_header_raw packet;
      packet.token = token; /* client token */
      packet.ctrl_type = L1_SRV_MAGIC|L1_SRV_TOKEN;
      /* session token */
      packet.new_token = compute_token(srv->secret_seed, *from);

      clib_net_udp_send(srv->udp, (uint8_t*)&packet,
                          10, from);
    }

  } else if (ctrl_type == L1_CL_VERIFY_TOKEN && size == 6 &&
              token == compute_token(srv->secret_seed, *from)) {

    /* matching token received, reserve client slot */
    if (net_l1_server_full(srv)) {
      /* 2nd. we cannot accept more clients */
      srv_send_close(srv, from, token);
    } else {
      /* ask user for client slot */
      net_l1_server_client *cl;
      srv->cb_alloc_slot(srv, &cl);

      ASSERT(cl != NULL, "failed to alloc client slot")

      cl->session_token = token;

      cl->cl_user = NULL;
      cl->addr = *from;

      cl->last_ping = -1;
      cl->last_valid_packet = time_get();

      /* set up map entry */
      cl->map_slot = srv->num_clients++;
      net_l1_server_client_ref *ref =
              &srv->client_map[cl->map_slot];

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

      if (cl->session_token == token) {
        /* token match, valid packet */
        cl->last_valid_packet = time_get();

        if (ctrl_type == L1_PINGPONG && size == 11) {
          if (data[6] == L1_PINGPONG_PING) {
            /* respond with pong */
            srv_send_pong(srv, cl, ((uint32_t*)&data[7])[0]);
          } else if (data[6] == L1_PINGPONG_PONG) {
            /* verify pong */
            if (((uint32_t*)&data[7])[0] == cl->cur_ping_token) {
              //TODO: measure latency
              /* reset ping */
              cl->last_ping = -1;
            }
          }
        } else if (ctrl_type == L1_CLOSE) {
          /* client closed session */
          srv->cb_on_client_drop(srv, cl);
          free_client_slot(srv, cl);
        } else if (ctrl_type == L1_MESSAGE) {
          /* valid client packet */
          srv->cb_on_client_packet(srv, cl, &data[6], size-6);
        }
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
  packet.ctrl_type = L1_CL_MAGIC|L1_CL_VERIFY_TOKEN;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)&packet,
                        6, &cl->remote_addr);
}

static void
cb_on_data_from_srv(void *user, const uint8_t *data,
            int size, net_addr *from)
{
  net_l1_client *cl = user;

  /* don't do anything on offline state */
  if (cl->state == L1_CL_STATE_OFFLINE)
    return;

  cl_handle_timing(cl);

  if (size < L1_MIN_PACKET_SIZE)
    /* invalid size */
    return;

  uint16_t info = ((uint16_t*)&data[L1_TOKEN_SIZE])[0];

  if ((info & L1_MAGIC_MASK) != L1_SRV_MAGIC)
    /* magic doesn't match */
    return;

  int ctrl_type = info & L1_CTRL_MASK;
  uint32_t token = ((uint32_t*)data)[0];

  if (cl->state == L1_CL_STATE_REQUEST_TOKEN) {
    if (token == cl->request_token) {
      if (ctrl_type == L1_SRV_TOKEN && size == 10) {

        /* received the token, next state */
        cl->state = L1_CL_STATE_VERIFY_TOKEN;
        cl->session_token = ((uint32_t*)&data[6])[0];
        verify_token(cl);
        cl->last_request = time_get();
      } else if (ctrl_type == L1_CLOSE) {
        /* received close on token request */
        cl->state = L1_CL_STATE_OFFLINE;
        cl->cb_on_drop(cl);
      }
    }
  } else if (cl->session_token == token) {
    /* valid packet from remote server */
    cl->last_valid_packet = time_get();

    if (cl->state != L1_CL_STATE_ONLINE) {
      cl->state = L1_CL_STATE_ONLINE;
      cl->cb_on_connect(cl);
    }

    if (ctrl_type == L1_PINGPONG && size == 11) {
      if (data[6] == L1_PINGPONG_PING) {
        /* respond with pong */
        cl_send_pong(cl, ((uint32_t*)&data[7])[0]);
      } else if (data[6] == L1_PINGPONG_PONG) {
        /* verify pong */
        if (((uint32_t*)&data[7])[0] == cl->cur_ping_token) {
          //TODO: measure latency
          /* reset ping */
          cl->last_ping = -1;
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
                    net_l1_cb_alloc_slot cb_alloc_slot,
                    net_l1_cb_free_slot cb_free_slot,
                    net_l1_cb_on_client_drop cb_on_client_drop,
                    net_l1_cb_on_client_packet cb_on_client_packet,
                    void *user)
{
  srv->udp = clib_net_udp_new(ev, bind_to, cb_on_data_from_cl, srv);
  srv->timer = clib_evloop_timer_new(ev, false,
                100000, NULL, cb_srv_on_timer, srv);

  if (!srv->udp)
    return false;

  // init seed
  random_fill(srv->secret_seed, sizeof(srv->secret_seed));

  // reset clients
  srv->num_clients = 0;

  srv->cb_on_client = cb_on_client;
  srv->cb_alloc_slot = cb_alloc_slot;
  srv->cb_free_slot = cb_free_slot;
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
  packet.ctrl_type = L1_CL_MAGIC|L1_CL_REQUEST_TOKEN;
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
  int64_t now = time_get();
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

  cl->last_ping = -1;
  cl->last_valid_packet = -1; /* will be set on connect */

  /* init connect */
  cl->state = L1_CL_REQUEST_TOKEN;
  cl->request_token = random_uint32();

  request_token(cl);
  cl->connecting_since = now;
  cl->last_request = now;

  /* init timer */
  cl->timer = clib_evloop_timer_new(ev, false,
                100000, NULL, cb_cl_on_timer, cl);

  return true;
}

void net_l1_server_send(net_l1_server *srv, net_l1_server_client *cl,
                        const uint8_t *data, size_t size)
{
  l1_header_raw *p = (l1_header_raw*)(data - L1_HEADER_SIZE);

  /* prepare packet */
  p->token = cl->session_token;
  p->ctrl_type = L1_SRV_MAGIC|L1_MESSAGE;

  /* send */
  clib_net_udp_send(srv->udp, (uint8_t*)p,
                        size+L1_HEADER_SIZE, &cl->addr);
}

void net_l1_server_client_close(net_l1_server *srv,
                                net_l1_server_client *cl)
{
  srv_send_close(srv, &cl->addr, cl->session_token);
  srv->cb_on_client_drop(srv, cl);
  free_client_slot(srv, cl);
}

void net_l1_client_send(net_l1_client *cl,
                        const uint8_t *data, size_t size)
{
  l1_header_raw *p = (l1_header_raw*)(data - L1_HEADER_SIZE);

  /* prepare packet */
  p->token = cl->session_token;
  p->ctrl_type = L1_CL_MAGIC|L1_MESSAGE;

  /* send */
  clib_net_udp_send(cl->udp, (uint8_t*)p,
                        size+L1_HEADER_SIZE, &cl->remote_addr);
}

void net_l1_server_uninit(net_l1_server *srv, clib_evloop *ev)
{
  /* notify clients */
  int i;
  for (i = 0; i < srv->num_clients; i++) {
    net_l1_server_client *cl = srv->client_map[i].cl;
    srv_send_close(srv, &cl->addr, cl->session_token);
  }

  clib_net_udp_destroy(srv->udp);
}

void net_l1_client_uninit(net_l1_client *cl, clib_evloop *ev)
{
  if (cl->state == L1_CL_STATE_ONLINE)
    cl_send_close(cl, cl->session_token);
  clib_net_udp_destroy(cl->udp);
}

