/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    /* REQUIRES */
    assert(sr);

    /* Initialize cache and cache cleanup thread */
    sr_arpcache_init(&(sr->cache));

    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);
    
    /* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  /* REQUIRES */
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* fill in code here */
  /* The received packet is an arp packet.*/
  if (packet[12] == 0x08 && packet[13] == 0x06) {
    handle_arppacket(sr, packet, len, interface);
  }

}/* end sr_ForwardPacket */

/* Converts a uint8_t array into a uint32_t. */
uint32_t bit_size_conversion(uint8_t bytes[]) {
  int i;
  uint32_t thirty_two = 0x0000;

  for (i = 0; i < 4; i++) {
    thirty_two = (thirty_two << (i * 8)) || bytes[i];
  }

  return thirty_two;
}

void handle_arppacket(struct sr_instance* sr,
                      uint8_t * packet, 
                      unsigned int len,
                      char* interface) {
  /* The packet is an arp request. */
  if (packet[21] == 0x01) {
    /* Check if the request is for this router. */
    /* Leave blank for now, the interface should handle it. */
      /* 
      Set the Opcode to reply
      Swap the destination and source addresses
      replace the source address
      */
      uint8_t packet_copy[len];
      memcpy(packet_copy, &packet, len);

      /* Ethernet Information. */
      uint8_t src_ether[ETHER_ADDR_LEN];
      uint8_t dest_ether[ETHER_ADDR_LEN];

      /* ARP packet information. */
      uint8_t src_hdw[ETHER_ADDR_LEN];
      uint8_t src_pcl[4];
      uint8_t des_hdw[ETHER_ADDR_LEN];
      unsigned char * iface_addr = sr_get_interface(sr, interface)->addr;
      memcpy(des_hdw[0], iface_addr, ETHER_ADDR_LEN);
      uint8_t des_pcl[4];

      /* Save the destination and source address information. */
      memcpy(dest_ether, &packet[0], ETHER_ADDR_LEN);
      memcpy(src_ether, &packet[6], ETHER_ADDR_LEN);

      memcpy(src_hdw, &packet[22], ETHER_ADDR_LEN);
      memcpy(src_pcl, &packet[28], 4);
      memcpy(des_pcl, &packet[38], 4);

      /* Write to the packet copy. */
      packet_copy[21] = 0x02;
      memcpy(&packet_copy[0], &dest_ether, ETHER_ADDR_LEN);
      memcpy(&packet_copy[6], src_ether, ETHER_ADDR_LEN);
      memcpy(&packet_copy[22], des_hdw, ETHER_ADDR_LEN);
      memcpy(&packet_copy[28], des_pcl, 4);
      memcpy(&packet_copy[32], src_hdw, ETHER_ADDR_LEN);
      memcpy(&packet_copy[38], des_pcl, 4);

      /* Send the ARP reply. */
      sr_send_packet(sr, packet_copy, sizeof(packet_copy), interface);

    /* If the request is not for this router, destroy it. */

  /* The packet is an arp reply. */
  } else if (packet[21] == 0x02) {
    /* Cache reply. */
    unsigned char mac[ETHER_ADDR_LEN];
    
    memcpy(mac[0], (unsigned char *) packet[22], ETHER_ADDR_LEN);
    uint8_t packet_ip[4];
    memcpy(packet_ip, &packet[28], 4);
    uint32_t ip = bit_size_conversion(packet_ip);

    struct sr_arpreq *requests = sr_arpcache_insert(&sr->cache, mac, ip);
    
    /* 
    Go through request queue and send queued packets
    for this arp.
    */
    struct sr_packet *rpacket;
    if (requests != NULL) {
      for(rpacket = requests->packets; rpacket != NULL; rpacket = rpacket->next) {
        sr_send_packet(sr, rpacket->buf, sizeof(rpacket->buf), rpacket->iface);

      }
    }

    /* Remove the request queue. */
    sr_arpreq_destroy(&sr->cache, requests);

  }
}
