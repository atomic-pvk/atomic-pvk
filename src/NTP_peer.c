#include "NTP_peer.h"

#include "NTPTask.h"
#include "NTP_main_utility.h"
#include "NTP_poll.h"
#include "NTP_system_process.h"

// A.5.1.1.  packet()

/*
DUMMY SOLUTION
table should be in the header file but the current configuration and dependency tree make it hard
TODO
*/

/*
 * Dispatch matrix
 *              active  passv  client server bcast */
int table[7][5] = {
    /* nopeer  */ {NEWPS, DSCRD, FXMIT, MANY, NEWBC},
    /* active  */ {PROC, PROC, DSCRD, DSCRD, DSCRD},
    /* passv   */ {PROC, ERR, DSCRD, DSCRD, DSCRD},
    /* client  */ {DSCRD, DSCRD, DSCRD, PROC, DSCRD},
    /* server  */ {DSCRD, DSCRD, DSCRD, DSCRD, DSCRD},
    /* bcast   */ {DSCRD, DSCRD, DSCRD, DSCRD, DSCRD},
    /* bclient */ {DSCRD, DSCRD, DSCRD, DSCRD, PROC}};
/*
 * packet() - process packet and compute offset, delay, and
 * dispersion.
 */
void packet(
    struct ntp_p *p, /* peer structure pointer */
    struct ntp_r *r  /* receive packet pointer */
)
{
        double offset; /* sample offsset */
        double delay;  /* sample delay */
        double disp;   /* sample dispersion */

        /*
         * By golly the packet is valid.  Light up the remaining header
         * fields.  Note that we map stratum 0 (unspecified) to MAXSTRAT
         * to make stratum comparisons simpler and to provide a natural
         * interface for radio clock drivers that operate for
         *  convenience at stratum 0.
         */
        p->leap = r->leap;
        if (r->stratum == 0)
                p->stratum = MAXSTRAT;
        else
                p->stratum = r->stratum;
        p->pmode = r->mode;
        p->ppoll = r->poll;
        p->rootdelay = FP2D(r->rootdelay);
        p->rootdisp = FP2D(r->rootdisp);
        p->refid = r->refid;
        p->reftime = r->reftime;

        /*
         * Verify the server is synchronized with valid stratum and
         * reference time not later than the transmit time.
         */
        if (p->leap == NOSYNC || p->stratum >= MAXSTRAT)
        {
                return; /* unsynchronized */
        }

        /*
         * Verify valid root distance.
         */
        if (r->rootdelay / 2 + r->rootdisp >= MAXDISP || p->reftime >
                                                             r->xmt)
                return; /* invalid header values */

        FreeRTOS_printf(("I am calling poll_update\n"));

        poll_update(p, p->hpoll);
        p->reach |= 1;

        /*
         * Calculate offset, delay and dispersion, then pass to the
         * clock filter.  Note carefully the implied processing.  The
         * first-order difference is done directly in 64-bit arithmetic,
         * then the result is converted to floating double.  All further
         * processing is in floating-double arithmetic with rounding
         * done by the hardware.  This is necessary in order to avoid
         * overflow and preserve precision.
         *
         * The delay calculation is a special case.  In cases where the
         * server and client clocks are running at different rates and
         * with very fast networks, the delay can appear negative.  In
         * order to avoid violating the Principle of Least Astonishment,
         * the delay is clamped not less than the system precision.
         */

        time_t rXmtS = (time_t)(r->xmt >> 32);
        uint32_t rXmtFrac = (uint32_t)(r->xmt & 0xFFFFFFFF);

        time_t rDstS = (time_t)(r->dst >> 32);
        uint32_t rDstFrac = (uint32_t)(r->dst & 0xFFFFFFFF);
        time_t rRecS = (time_t)(r->rec >> 32);
        uint32_t rRecFrac = (uint32_t)(r->rec & 0xFFFFFFFF);
        time_t rOrgS = (time_t)(r->org >> 32);
        uint32_t rOrgFrac = (uint32_t)(r->org & 0xFFFFFFFF);

        FreeRTOS_printf(("\n a double is %u n", sizeof(double)));

        FreeRTOS_printf(("\n\n lol r xmt: %s.%u\n", ctime(&rXmtS), rXmtFrac));
        FreeRTOS_printf(("\n\n lol r dst: %s.%u\n", ctime(&rDstS), rDstFrac));

        time_t tempOffset = rDstS - rXmtS;
        FreeRTOS_printf(("\n\n\n\n tempOffset SS fuck is %d \n\n\n", tempOffset));
        uint32_t tempOffsetf = (uint32_t)rDstFrac - (uint32_t)rXmtFrac;
        FreeRTOS_printf(("\n\n\n\n tempOffset FF fuck is %d \n\n\n", tempOffsetf));


        if (p->pmode == M_BCST)
        {
                // 123.456
                // long long: 123
                // 1235912412390123  = 2024 -
                // #define LFP2D(a) ((double)(a) / FRAC)
                // a / 4294967296 = dahuidhauid.239482490280

                // offset = LFP2D(r->xmt - r->dst)

                // time_t rXmtS = (time_t)(r->xmt >> 32);
                // uint32_t rXmtFrac = (uint32_t)(r->xmt & 0xFFFFFFFF);

                double tempOffset = subtract_uint64_t(r->xmt, r->dst);

                FreeRTOS_printf_wrapper_double("\n\n\n\n tempOffset fuck is %s \n\n\n", tempOffset);

                offset = LFP2D(tempOffset);

                // offset = ((long long)LFP2D(rXmtS - rDstS) << 32) | ((long long)LFP2D(rXmtFrac - rDstFrac));
                delay = BDELAY;
                disp = LOG2D(r->precision) + LOG2D(s.precision) + PHI * 2 * BDELAY;
        }
        else
        {

                //               {
                //         offset = (LFP2D(r->rec - r->org) + LFP2D(r->dst -
                //                                                  r->xmt)) /
                //                  2;
                //         delay = max(LFP2D(r->dst - r->org) - LFP2D(r->rec -
                //                                                    r->xmt),
                //                     LOG2D(s.precision));
                //         disp = LOG2D(r->precision) + LOG2D(s.precision) + PHI * LFP2D(r->dst - r->org);
                //

                offset = (double)add_uint64_t(LFP2D((double)subtract_uint64_t(r->rec, r->org)), LFP2D((double)subtract_uint64_t(r->dst, r->xmt))) / 2;

                delay = max((double)(subtract_uint64_t(LFP2D((double)subtract_uint64_t(r->dst, r->org)), LFP2D((double)subtract_uint64_t(r->rec, r->xmt)))), LOG2D(s.precision));
                disp = LOG2D(r->precision) + LOG2D(s.precision) + PHI * LFP2D(r->dst - r->org);
        }
        // double tempOffset2 = 5;
        // double tempOffset3 = 2;
        // double printedmessage = tempOffset2/tempOffset3;
        // FreeRTOS_printf_wrapper_double("\n\n\n lets see if offset is working naaow: %s", printedmessage);

        FreeRTOS_printf_wrapper_double("\n\n\n lets see if offset is working naaow: %s", offset);
        FreeRTOS_printf_wrapper_double("\n\n\n lets see if offset is working naaow: %s", delay);
        FreeRTOS_printf_wrapper_double("\n\n\n lets see if offset is working naaow: %s", disp);
        // FreeRTOS_printf(("\n\n\n lets see if offset is working: %d\n\n\n", offset)); // = 0
        // FreeRTOS_printf(("\n\n\ndelay is %d\n\n\n", delay));
        // FreeRTOS_printf(("\n\n\ndisp is %d\n\n\n", disp));
        FreeRTOS_printf(("I AM CALLING CLOCK_FILTER\n"));
        clock_filter(p, offset, delay, disp);
}

// A.5.2.  clock_filter()

/*
 * clock_filter(p, offset, delay, dispersion) - select the best from the
 * latest eight delay/offset samples.
 */
void clock_filter(
    struct ntp_p *p, /* peer structure pointer */
    double offset,   /* clock offset */
    double delay,    /* roundtrip delay */
    double disp      /* dispersion */
)
{
        struct ntp_f f[NSTAGE]; /* sorted list */
        double dtemp;
        int i;

        FreeRTOS_printf(("we are in clockf\n"));

        /*
         * The clock filter contents consist of eight tuples (offset,
         * delay, dispersion, time).  Shift each tuple to the left,
         * discarding the leftmost one.  As each tuple is shifted,
         * increase the dispersion since the last filter update.  At the
         * same time, copy each tuple to a temporary list.  After this,
         * place the (offset, delay, disp, time) in the vacated
         * rightmost tuple.
         */
        for (i = 1; i < NSTAGE; i++)
        {
                p->f[i] = p->f[i - 1];
                p->f[i].disp += PHI * (c.t - p->t);
                f[i] = p->f[i];
        }
        p->f[0].t = c.t;
        p->f[0].offset = offset;
        p->f[0].delay = delay;
        p->f[0].disp = disp;
        f[0] = p->f[0];

        FreeRTOS_printf(("we are in clockf after value set\n"));

        /*
         * Sort the temporary list of tuples by increasing f[].delay.
         * The first entry on the sorted list represents the best
         * sample, but it might be old.
         */
        dtemp = p->offset;
        p->offset = f[0].offset;
        p->delay = f[0].delay;
        for (i = 0; i < NSTAGE; i++)
        {
                p->disp += f[i].disp / (2 ^ (i + 1));
                p->jitter += SQUARE(f[i].offset - f[0].offset);
        }
        FreeRTOS_printf(("we are after clockf square loop%d\n", p->jitter));
        p->jitter = max(sqrt(p->jitter), LOG2D(s.precision));
        FreeRTOS_printf(("we are in clockf 2%d\n", p->jitter));

        /*
         * Prime directive: use a sample only once and never a sample
         * older than the latest one, but anything goes before first
         * synchronized.
         */
        FreeRTOS_printf(("we are in clockf 2\n"));

        FreeRTOS_printf(("s leap is %d\n", s.leap));
        if (f[0].t - p->t <= 0 && s.leap != NOSYNC)
                return;

        /*
         * Popcorn spike suppressor.  Compare the difference between the
         * last and current offsets to the current jitter.  If greater
         * than SGATE (3) and if the interval since the last offset is
         * less than twice the system poll interval, dump the spike.
         * Otherwise, and if not in a burst, shake out the truechimers.
         */

        FreeRTOS_printf(("before popcorn assoc_t size: %d\n", assoc_table->size));

        FreeRTOS_printf(("fabs(p->offset - dtemp) is equal to: %d\n", fabs(p->offset - dtemp)));
        FreeRTOS_printf(("SGATE * p->jitter is equal to: %d\n", SGATE * p->jitter));
        FreeRTOS_printf(("f[0].t - p->t is equal to: %d\n", f[0].t - p->t));
        if (fabs(p->offset - dtemp) > SGATE * p->jitter && (f[0].t - p->t) < 2 * s.poll)
        {
                FreeRTOS_printf(("Popcorn spike found\n"));
                return;
        }

        p->t = f[0].t;
        if (p->burst == 0)
        {
                FreeRTOS_printf(("I AM CALLING clock_select\n"));
                clock_select();
        }

        return;
}

/*
 * fit() - test if association p is acceptable for synchronization
 */
int fit(
    struct ntp_p *p /* peer structure pointer */
)
{
        /*
         * A stratum error occurs if (1) the server has never been
         * synchronized, (2) the server stratum is invalid.
         */
        if (p->leap == NOSYNC || p->stratum >= MAXSTRAT)
                return (FALSE);

        /*
         * A distance error occurs if the root distance exceeds the
         * distance threshold plus an increment equal to one poll
         * interval.
         */
        if (root_dist(p) > MAXDIST + PHI * LOG2D(s.poll))
                return (FALSE);

        /*
         * A loop error occurs if the remote peer is synchronized to the
         * local peer or the remote peer is synchronized to the current
         * system peer.  Note this is the behavior for IPv4; for IPv6
         * the MD5 hash is used instead.
         */
        if (p->refid == p->dstaddr || p->refid == s.refid)
                return (FALSE);

        /*
         * An unreachable error occurs if the server is unreachable.
         */
        if (p->reach == 0)
                return (FALSE);

        return (TRUE);
}

/*
 * clear() - reinitialize for persistent association, demobilize
 * for ephemeral association.
 */
void clear(
    struct ntp_p *p, /* peer structure pointer */
    int kiss         /* kiss code */
)
{
        int i;

        /*
         * The first thing to do is return all resources to the bank.
         * Typical resources are not detailed here, but they include
         * dynamically allocated structures for keys, certificates, etc.
         * If an ephemeral association and not initialization, return
         * the association memory as well.
         */
        /* return resources */
        if (s.p == p)
                s.p = NULL;

        if (kiss != X_INIT && (p->flags & P_EPHEM))
        {
                free(p);
                return;
        }

        /*
         * Initialize the association fields for general reset.
         */
        memset(BEGIN_CLEAR(p), LEN_CLEAR, 0);
        p->leap = NOSYNC;
        p->stratum = MAXSTRAT;
        p->ppoll = MAXPOLL;
        p->hpoll = MINPOLL;
        p->disp = MAXDISP;
        p->jitter = LOG2D(s.precision);
        p->refid = kiss;
        for (i = 0; i < NSTAGE; i++)
                p->f[i].disp = MAXDISP;

        /*
         * Randomize the first poll just in case thousands of broadcast
         * clients have just been stirred up after a long absence of the
         * broadcast server.
         */
        p->outdate = p->t = c.t;
        p->nextdate = p->outdate + (random() & ((1 << MINPOLL) - 1));
}

// A.5.3.  fast_xmit()

/*
 * fast_xmit() - transmit a reply packet for receive packet r
 */
void fast_xmit(
    struct ntp_r *r, /* receive packet pointer */
    int mode,        /* association mode */
    int auth         /* authentication code */
)
{
        struct ntp_x x;

        /*
         * Initialize header and transmit timestamp.  Note that the
         * transmit version is copied from the receive version.  This is
         * for backward compatibility.
         */
        x.version = r->version;
        x.srcaddr = r->dstaddr;
        x.dstaddr = r->srcaddr;
        x.leap = s.leap;
        x.mode = mode;
        if (s.stratum == MAXSTRAT)
                x.stratum = 0;
        else
                x.stratum = s.stratum;
        x.poll = r->poll;
        x.precision = s.precision;
        x.rootdelay = D2FP(s.rootdelay);
        x.rootdisp = D2FP(s.rootdisp);
        x.refid = s.refid;
        x.reftime = s.reftime;
        x.org = r->xmt;
        x.rec = r->dst;
        x.xmt = get_time();

        /*
         * If the authentication code is A.NONE, include only the
         * header; if A.CRYPTO, send a crypto-NAK; if A.OK, send a valid
         * MAC.  Use the key ID in the received packet and the key in
         * the local key cache.
         */
        if (auth != A_NONE)
        {
                if (auth == A_CRYPTO)
                {
                        x.keyid = 0;
                }
                else
                {
                        x.keyid = r->keyid;
                        x.dgst = md5(x.keyid);
                }
        }
        xmit_packet(&x);
}

// A.5.4.  access()

/*
 * access() - determine access restrictions
 */
int access(
    struct ntp_r *r /* receive packet pointer */
)
{
        /*
         * The access control list is an ordered set of tuples
         * consisting of an address, mask, and restrict word containing
         * defined bits.  The list is searched for the first match on
         * the source address (r->srcaddr) and the associated restrict
         * word is returned.
         */
        return (/* access bits */ 1); // TODO ALL HAVE ACCESS AT ALL TIME CURRENTLY
}

/*
 * receive() - receive packet and decode modes
 */
void receive(
    struct ntp_r *r /* receive packet pointer */
                    // I am adding this to the function signature to make it compile and since I do not see any other way to "check" the system structure pointer
)
{
        struct ntp_p *p; /* peer structure pointer */
        int auth;        /* authentication code */
        int has_mac;     /* size of MAC */
        int synch;       /* synchronized switch */

        /*
         * Check access control lists.  The intent here is to implement
         * a whitelist of those IP addresses specifically accepted
         * and/or a blacklist of those IP addresses specifically
         * rejected.  There could be different lists for authenticated
         * clients and unauthenticated clients.
         */
        if (!access(r))
                return; /* access denied */

        /*
         * The version must not be in the future.  Format checks include
         * packet length, MAC length and extension field lengths, if
         * present.
         */
        if (r->version > VERSION /* or format error */)
                return; /* format error */

        /*
         * Authentication is conditioned by two switches that can be
         * specified on a per-client basis.
         *
         * P_NOPEER     do not mobilize an association unless
         *              authenticated.
         * P_NOTRUST    do not allow access unless authenticated
         *              (implies P_NOPEER).
         *
         * There are four outcomes:
         *
         * A_NONE       the packet has no MAC.
         * A_OK         the packet has a MAC and authentication
         *               succeeds.
         * A_ERROR      the packet has a MAC and authentication fails.
         * A_CRYPTO     crypto-NAK.  The MAC has four octets only.
         *
         * Note: The AUTH (x, y) macro is used to filter outcomes.  If x
         * is zero, acceptable outcomes of y are NONE and OK.  If x is
         * one, the only acceptable outcome of y is OK.
         */

        has_mac = /* length of MAC field */ 0;
        if (has_mac == 0)
        {
                auth = A_NONE; /* not required */
        }
        else if (has_mac == 4)
        {
                auth = A_CRYPTO; /* crypto-NAK */
        }
        else
        {
                if (r->mac != md5(r->keyid))
                        auth = A_ERROR; /* auth error */
                else
                        auth = A_OK; /* auth OK */
        }

        /*
         * Find association and dispatch code.  If there is no
         * association to match, the value of p->hmode is assumed NULL.
         */
        p = find_assoc(r);
        switch (table[(unsigned int)(p->hmode)][(unsigned int)(r->mode) - 1]) // PACKET MODE IS INDEXED FROM 1
        {                                                                     // WHEN TABLE IS INDEXED FROM 0

        /*
         * Client packet and no association.  Send server reply without
         * saving state.
         */
        case FXMIT:

                /*
                 * If unicast destination address, send server packet.
                 * If authentication fails, send a crypto-NAK packet.
                 */

                /* not multicast dstaddr */
                if (0)
                {
                        if (AUTH(p->flags & P_NOTRUST, auth))
                                fast_xmit(r, M_SERV, auth);
                        else if (auth == A_ERROR)
                                fast_xmit(r, M_SERV, A_CRYPTO);
                        return; /* M_SERV packet sent */
                }

                /*
                 * This must be manycast.  Do not respond if we are not
                 * synchronized or if our stratum is above the
                 * manycaster.
                 */
                if (s.leap == NOSYNC || s.stratum > r->stratum)
                        return;

                /*
                 * Respond only if authentication is OK.  Note that the
                 * unicast address is used, not the multicast.
                 */
                if (AUTH(p->flags & P_NOTRUST, auth))
                        fast_xmit(r, M_SERV, auth);
                return;

        /*
         * New manycast client ephemeral association.  It is mobilized
         * in the same version as in the packet.  If authentication
         * fails, ignore the packet.  Verify the server packet by
         * comparing the r->org timestamp in the packet with the p->xmt
         * timestamp in the multicast client association.  If they
         * match, the server packet is authentic.  Details omitted.
         */
        case MANY:
                if (!AUTH(p->flags & (P_NOTRUST | P_NOPEER), auth))
                        return; /* authentication error */

                p = mobilize(r->srcaddr, r->dstaddr, r->version, M_CLNT,
                             r->keyid, P_EPHEM);
                break;

                /*
                 * New symmetric passive association.  It is mobilized in the
                 * same version as in the packet.  If authentication fails,
                 * send a crypto-NAK packet.  If restrict no-moblize, send a
                 * symmetric active packet instead.
                 */
        case NEWPS:
                if (!AUTH(p->flags & P_NOTRUST, auth))
                {
                        if (auth == A_ERROR)
                                fast_xmit(r, M_SACT, A_CRYPTO);
                        return; /* crypto-NAK packet sent */
                }
                if (!AUTH(p->flags & P_NOPEER, auth))
                {
                        fast_xmit(r, M_SACT, auth);
                        return; /* M_SACT packet sent */
                }
                p = mobilize(r->srcaddr, r->dstaddr, r->version, M_PASV,
                             r->keyid, P_EPHEM);
                break;

        /*
         * New broadcast client association.  It is mobilized in the
         * same version as in the packet.  If authentication fails,
         * ignore the packet.  Note this code does not support the
         * initial volley feature in the reference implementation.
         */
        case NEWBC:
                if (!AUTH(p->flags & (P_NOTRUST | P_NOPEER), auth))
                        return; /* authentication error */

                if (!(s.flags & S_BCSTENAB))
                        return; /* broadcast not enabled */

                p = mobilize(r->srcaddr, r->dstaddr, r->version, M_BCLN,
                             r->keyid, P_EPHEM);
                break; /* processing continues *

/*
* Process packet.  Placeholdler only.
*/
        case PROC:
                // p = mobilize(r->srcaddr, r->dstaddr, r->version, M_SERV,
                //              r->keyid, P_EPHEM); // TODO //
                break; /* processing continues */

        /*
         * Invalid mode combination.  We get here only in case of
         * ephemeral associations, so the correct action is simply to
         * toss it.
         */
        case ERR:
                clear(p, X_ERROR);
                return; /* invalid mode combination */

        /*
         * No match; just discard the packet.
         */
        case DSCRD:
                return; /* orphan abandoned */
        }
        /*
         * Next comes a rigorous schedule of timestamp checking.  If the
         * transmit timestamp is zero, the server is horribly broken.
         */
        time_t rXmtInSeconds = (time_t)((r->xmt >> 32) - 2208988800ull);
        uint32_t rXmtFrac = (uint32_t)(r->xmt & 0xFFFFFFFF);

        if (rXmtInSeconds == 0 && rXmtFrac == 0)
                return; /* invalid timestamp */

        /*
         * If the transmit timestamp duplicates a previous one, the
         * packet is a replay.
         */
        time_t pXmtInSeconds = (time_t)((p->xmt >> 32) - 2208988800ull);
        uint32_t pXmtFrac = (uint32_t)(p->xmt & 0xFFFFFFFF);

        if ((rXmtInSeconds == pXmtInSeconds) && (rXmtFrac == pXmtFrac))
        {
                return; /* duplicate packet */
        }

        /*
         * If this is a broadcast mode packet, skip further checking.
         * If the origin timestamp is zero, the sender has not yet heard
         * from us.  Otherwise, if the origin timestamp does not match
         * the transmit timestamp, the packet is bogus.
         */
        time_t rOrgInSeconds = (time_t)((r->org >> 32) - 2208988800ull);
        uint32_t rOrgFrac = (uint32_t)(r->org & 0xFFFFFFFF);
        synch = TRUE;
        if (r->mode != M_BCST)
        {
                if ((rOrgInSeconds == 0) && (rOrgFrac == 0))
                {
                        FreeRTOS_printf(("rOrgInSeconds == 0\n"));
                        synch = FALSE; /* unsynchronized */
                }

                else if ((rOrgInSeconds != pXmtInSeconds) && (rOrgFrac != pXmtFrac))
                {
                        FreeRTOS_printf(("rOrgInSeconds != pXmtInSeconds\n"));
                        synch = FALSE; /* bogus packet */
                }
        }
        else
        {
                FreeRTOS_printf(("r->mode == M_BCST\n"));
        }

        /*
         * Update the origin and destination timestamps.  If
         * unsynchronized or bogus, abandon ship.
         */
        p->org = r->xmt;
        p->rec = r->dst;
        if (!synch)
                return; /* unsynch */

        /*
         * The timestamps are valid and the receive packet matches the
         * last one sent.  If the packet is a crypto-NAK, the server
         * might have just changed keys.  We demobilize the association
         * and wait for better times.
         */
        if (auth == A_CRYPTO)
        {
                clear(p, X_CRYPTO);
                return; /* crypto-NAK */
        }

        /*
         * If the association is authenticated, the key ID is nonzero
         * and received packets must be authenticated.  This is designed
         * to avoid a bait-and-switch attack, which was possible in past
         * versions.
         */
        if (!AUTH(p->keyid || (p->flags & P_NOTRUST), auth))
                return; /* bad auth */

        /*
         * Everything possible has been done to validate the timestamps
         * and prevent bad guys from disrupting the protocol or
         * injecting bogus data.  Earn some revenue.
         */
        FreeRTOS_printf(("calling packet\n"));
        packet(p, r);
}

// double sqrt(double number) {
//     double error = 0.00001;  // Define the precision of your answer
//     double s = number;

//     if(number == 0) return 0.0;

//     while ((s - number / s) > error || (number / s - s) > error) {
//         s = (s + number / s) / 2;
//     }

//     return s;
// }