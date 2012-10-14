#include "pico_config.h"
#include "pico_frame.h"
#include "pico_device.h"
#include "pico_protocol.h"
#include "pico_addressing.h"

#include "pico_eth.h"
#include "pico_arp.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_icmp4.h"
#include "pico_udp.h"
#include "pico_tcp.h"
#include "pico_socket.h"



/* NOTIFICATIONS: distributed notifications for stack internal errors.
 */

int pico_notify_socket_unreachable(struct pico_frame *f)
{
  if (0) {}
#ifdef PICO_SUPPORT_ICMP4 
  else if (IS_IPV4(f)) {
    pico_icmp4_port_unreachable(f);
  }
#endif
#ifdef PICO_SUPPORT_ICMP6 
  else if (IS_IPV6(f)) {
    pico_icmp6_port_unreachable(f);
  }
#endif

  return 0;
}

int pico_notify_proto_unreachable(struct pico_frame *f)
{
  if (0) {}
#ifdef PICO_SUPPORT_ICMP4 
  else if (IS_IPV4(f)) {
    pico_icmp4_proto_unreachable(f);
  }
#endif
#ifdef PICO_SUPPORT_ICMP6 
  else if (IS_IPV6(f)) {
    pico_icmp6_proto_unreachable(f);
  }
#endif
  return 0;
}

int pico_notify_dest_unreachable(struct pico_frame *f)
{
  if (0) {}
#ifdef PICO_SUPPORT_ICMP4 
  else if (IS_IPV4(f)) {
    pico_icmp4_dest_unreachable(f);
  }
#endif
#ifdef PICO_SUPPORT_ICMP6 
  else if (IS_IPV6(f)) {
    pico_icmp6_dest_unreachable(f);
  }
#endif
  return 0;
}

int pico_notify_ttl_expired(struct pico_frame *f)
{
  if (0) {}
#ifdef PICO_SUPPORT_ICMP4 
  else if (IS_IPV4(f)) {
    pico_icmp4_ttl_expired(f);
  }
#endif
#ifdef PICO_SUPPORT_ICMP6 
  else if (IS_IPV6(f)) {
    pico_icmp6_ttl_expired(f);
  }
#endif
  return 0;
}


/* SOCKET LEVEL: interface towards transport */
int pico_socket_receive(struct pico_frame *f)
{
  /* TODO: recognize the correspondant socket */
  return 0;
}

/* TRANSPORT LEVEL: interface towards network */
int pico_transport_receive(struct pico_frame *f)
{
  /* TODO: identify transport level, deliver packet to the 
   * correct destination (e.g. socket)*/
  return 0;
}

int pico_network_receive(struct pico_frame *f)
{
  dbg("Network receive\n");
  if (0) {}
#ifdef PICO_SUPPORT_IPV4
  else if (IS_IPV4(f)) {
    pico_enqueue(pico_proto_ipv4.q_in, f);
  }
#endif
#ifdef PICO_SUPPORT_IPV6
  else if (IS_IPV6(f)) {
    pico_enqueue(pico_proto_ipv6.q_in, f);
  }
#endif
  else {
    dbg("Network not found.\n");
    pico_frame_discard(f);
    return -1;
  }
  return f->buffer_len;
}


/* Network layer: interface towards socket for frame sending */
int pico_network_send(struct pico_frame *f)
{
  if (!f || !f->sock || !f->sock->net) {
    pico_frame_discard(f);
    return -1;
  }
  return f->sock->net->push(f->sock->net, f);
}

int pico_destination_is_local(struct pico_frame *f)
{
  if (0) { }
#ifdef PICO_SUPPORT_IPV4
  else if (IS_IPV4(f)) {
    struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *)f->net_hdr;
    if (pico_ipv4_link_find(&hdr->dst))
      return 1;
  }
#endif
#ifdef PICO_SUPPORT_IPV6
  else if (IS_IPV6(f)) {
  }
#endif
  return 0;
}

int pico_source_is_local(struct pico_frame *f)
{
  if (0) { }
#ifdef PICO_SUPPORT_IPV4
  else if (IS_IPV4(f)) {
    struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *)f->net_hdr;
    if (pico_ipv4_link_find(&hdr->src))
      return 1;
  }
#endif
#ifdef PICO_SUPPORT_IPV6
  else if (IS_IPV6(f)) {
  }
#endif
  return 0;


}


/* DATALINK LEVEL: interface from network to the device
 * and vice versa.
 */

/* The pico_ethernet_receive() function is used by 
 * those devices supporting ETH in order to push packets up 
 * into the stack. 
 */
int pico_ethernet_receive(struct pico_frame *f)
{
  struct pico_eth_hdr *hdr;
  if (!f || !f->dev || !f->datalink_hdr)
    goto discard;
  hdr = (struct pico_eth_hdr *) f->datalink_hdr;
  f->datalink_len = sizeof(struct pico_eth_hdr);
  if ( (memcmp(hdr->daddr, f->dev->eth->mac.addr, PICO_SIZE_ETH) != 0) && 
    (memcmp(hdr->daddr, PICO_ETHADDR_ANY, PICO_SIZE_ETH) != 0) )
    goto discard;

  f->net_hdr = f->datalink_hdr + f->datalink_len;
  if (hdr->proto == PICO_IDETH_ARP)
    return pico_arp_receive(f);
  if ((hdr->proto == PICO_IDETH_IPV4) || (hdr->proto == PICO_IDETH_IPV6))
    return pico_network_receive(f);
discard:
  pico_frame_discard(f);
  return -1;
}

/* This is called by dev loop in order to ensure correct ethernet addressing.
 * Returns 0 if the destination is unknown, and -1 if the packet is not deliverable
 * due to ethernet addressing (i.e., no arp association was possible. 
 *
 * Only IP packets must pass by this. ARP will always use direct dev->send() function, so
 * we assume IP is used.
 */
int pico_ethernet_send(struct pico_frame *f, void *nexthop)
{
  struct pico_arp *a4 = NULL;
  struct pico_eth *dstmac = NULL;

  if (IS_IPV6(f)) {
    /*TODO: Neighbor solicitation */
    dstmac = NULL;
  }

  else if (IS_IPV4(f)) {
    if (IS_BCAST(f)) {
     dstmac = (struct pico_eth *) PICO_ETHADDR_ANY;
    } else {
      struct pico_ipv4_hdr *hdr = (struct pico_ipv4_hdr *) f->net_hdr;
      a4 = pico_arp_get(&hdr->dst);
      dbg ("ETH SEND TO %08x - proto %d\n", hdr->dst.addr, hdr->proto); 
      if (!a4) {
       if (++ f->failure_count < 4) {
         dbg ("================= ARP REQUIRED: %d =============\n\n", f->failure_count);
         pico_arp_query(f->dev, &hdr->dst);
         return 0;
       } else return -1;
      }
      dstmac = (struct pico_eth *) a4;
    }
    /* This sets destination and source address, then pushes the packet to the device. */
    if (dstmac && (f->start > f->buffer) && ((f->start - f->buffer) >= PICO_SIZE_ETHHDR)) {
      struct pico_eth_hdr *hdr;
      f->start -= PICO_SIZE_ETHHDR;
      f->len += PICO_SIZE_ETHHDR;
      f->datalink_hdr = f->start;
      f->datalink_len = PICO_SIZE_ETHHDR;
      hdr = (struct pico_eth_hdr *) f->datalink_hdr;
      memcpy(hdr->saddr, f->dev->eth->mac.addr, PICO_SIZE_ETH);
      memcpy(hdr->daddr, dstmac, PICO_SIZE_ETH);
      hdr->proto = PICO_IDETH_IPV4;
      return f->dev->send(f->dev, f->start, f->len);
    } else {
      return -1;
    }
  } /* End IPV4 ethernet addressing */
  return -1;
}


/* LOWEST LEVEL: interface towards devices. */
/* Device driver will call this function which returns immediately.
 * Incoming packet will be processed later on in the dev loop.
 */
int pico_stack_recv(struct pico_device *dev, uint8_t *buffer, int len)
{
  struct pico_frame *f;
  if (len <= 0)
    return -1;
  f = pico_frame_alloc(len);
  if (!f)
    return -1;

  /* Association to the device that just received the frame. */
  f->dev = dev;

  /* Setup the start pointer, lenght. */
  f->start = f->buffer;
  f->len = f->buffer_len;
  memcpy(f->buffer, buffer, len);
  return pico_enqueue(dev->q_in, f);
}

int pico_sendto_dev(struct pico_frame *f)
{
  if (!f->dev) {
    pico_frame_discard(f);
    return -1;
  } else {
    return pico_enqueue(f->dev->q_out, f);
  }
}

void pico_stack_loop(void)
{

  while(1) {
    pico_devices_loop(100);
    pico_protocols_loop(100);
    pico_sockets_loop(100);
//    pico_apps_loop(100);
    PICO_IDLE();
  }
}

void pico_stack_init(void)
{

#ifdef PICO_SUPPORT_IPV4
  pico_protocol_init(&pico_proto_ipv4);
#endif

#ifdef PICO_SUPPORT_IPV6
  pico_protocol_init(&pico_proto_ipv6);
#endif

#ifdef PICO_SUPPORT_ICMP4
  pico_protocol_init(&pico_proto_icmp4);
#endif

#ifdef PICO_SUPPORT_UDP
  pico_protocol_init(&pico_proto_udp);
#endif

#ifdef PICO_SUPPORT_TCP
  pico_protocol_init(&pico_proto_tcp);
#endif
}

