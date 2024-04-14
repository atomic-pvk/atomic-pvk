#ifndef NTP_POLL_H
#define NTP_POLL_H

#include "NTP_TDMG.h"

/*
 * Poll process parameters and constants
 */
#define UNREACH 12 /* unreach counter threshold */
#define BCOUNT 8   /* packets in a burst */
#define BTIME 2    /* burst interval (s) */

void poll(struct ntp_p *, struct ntp_s, struct ntp_c);      /* poll process */
void poll_update(struct ntp_p *, int, struct ntp_c);        /* update the poll interval */
void peer_xmit(struct ntp_p *, struct ntp_s, struct ntp_c); /* transmit a packet */

#endif /* NTP_POLL_H */