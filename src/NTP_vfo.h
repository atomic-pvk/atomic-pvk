#ifndef NTP_VFO_H
#define NTP_VFO_H

#include "NTP_TDMG.h"

/*
 * Clock discipline parameters and constants
 */
#define STEPT .128      /* step threshold (s) */
#define WATCH 900       /* stepout threshold (s) */
#define PANICT 1000     /* panic threshold (s) */
#define PLL 65536       /* PLL loop gain */
#define FLL MAXPOLL + 1 /* FLL loop gain */
#define AVG 4           /* parameter averaging constant */
#define ALLAN 1500      /* compromise Allan intercept (s) */
#define LIMIT 30        /* poll-adjust threshold */
#define MAXFREQ 500e-6  /* frequency tolerance (500 ppm) */
#define PGATE 4         /* poll-adjust gate */

int local_clock(struct ntp_p *, double, struct ntp_s, struct ntp_c); /* clock discipline */
void rstclock(int, double, double, struct ntp_s, struct ntp_c);      /* clock state transition */
void clock_adjust(struct ntp_s, struct ntp_c);                       /* one-second timer process */

#endif /* NTP_VFO_H */