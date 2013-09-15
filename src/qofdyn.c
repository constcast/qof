/**
 ** qofdyn.c
 ** TCP dynamics tracking data structures and algorithms for QoF
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <yaf/qofdyn.h>
#include <yaf/yaftab.h>

#include "qofdyntmi.h"

/** Constant - 2^32 (for sequence number calculations) */
static const uint64_t k2e32 = 0x100000000ULL;

static const float        kFlightBreakIATDev = 2.0;
static const uint32_t     kFlightMinSegments = 10;

static gboolean qf_dyn_enable = FALSE;
static gboolean qf_dyn_enable_rtt = FALSE;
static gboolean qf_dyn_enable_rtx = FALSE;
static uint32_t qf_dyn_bincap = 0;
static uint32_t qf_dyn_binscale = 1;

int qfSeqCompare(uint32_t a, uint32_t b) {
    return a == b ? 0 : ((a - b) & 0x80000000) ? -1 : 1;
}

void qfSeqBitsInit(qfSeqBits_t *sb, uint32_t capacity, uint32_t scale) {
    bimInit(&sb->map, capacity / scale);
    sb->scale = scale;
}

void qfSeqBitsFree(qfSeqBits_t *sb) {
    bimFree(&(sb->map));
}

static int qfDynHasSeqbits(qfDyn_t *qd) { return 0; }

qfSeqStat_t qfSeqBitsSegment(qfSeqBits_t *sb, uint32_t aseq, uint32_t bseq) {
    bimIntersect_t brv;
    uint32_t seqcap, maxseq;
    uint64_t bits;
    
    /* short-circuit on no seqbits */
    if (!sb) return QF_SEQ_INORDER;
    
    /* set seqbase on first segment */
    if (!sb->seqbase) {
        sb->seqbase = (aseq / sb->scale) * sb->scale;
    }
    
    /* move range forward if necessary to cover last sn, noting loss */
    seqcap = sb->map.sz * k64Bits * sb->scale;
    maxseq = sb->seqbase + seqcap;
    while (qfSeqCompare(bseq, maxseq) > 0) {
        bits = bimShiftDown(&sb->map);
        sb->lostseq_ct += ((k64Bits - bimCountSet(bits)) * sb->scale);
        sb->seqbase += sb->scale * k64Bits;
        maxseq = sb->seqbase + seqcap;
    }
    
    /* entire range already seen (or rotated past): signal retransmission */
    if (qfSeqCompare(bseq, sb->seqbase) < 0) return QF_SEQ_REXMIT;
    
    /* clip overlapping bottom of range */
    if (qfSeqCompare(aseq, sb->seqbase) < 0) aseq = sb->seqbase;

    /* set bits in the bitmap */
    brv = bimTestAndSetRange(&sb->map, (aseq - sb->seqbase)/sb->scale,
                                       (bseq - sb->seqbase)/sb->scale);
    
    /* translate intersection to retransmission detect */
    if (brv == BIM_PART || brv == BIM_FULL) {
        return QF_SEQ_REXMIT; // partial intersection -> retransmission
    } else if (brv == BIM_START || brv == BIM_EDGE) {
        return QF_SEQ_INORDER; // overlap at start or both sides -> inorder
    } else if (((aseq - sb->seqbase)/sb->scale) == 0) {
        return QF_SEQ_INORDER; // beginning of range -> presumed inorder
    } else if (bimTestBit(&sb->map, (aseq - sb->seqbase)/sb->scale) - 1) {
        return QF_SEQ_INORDER; // previous bit set -> inorder
    } else {
        return QF_SEQ_REORDER; // otherwise reordered
    }
}

void qfSeqBitsFinalizeLoss(qfSeqBits_t *sb)
{
    uint64_t bits;
    
    while ((bits = bimShiftDown(&sb->map))) {
        sb->lostseq_ct += ((bimHighBitSet(bits) - bimCountSet(bits)) * sb->scale);
        sb->seqbase += sb->scale * k64Bits;
    }
}

void qfDynConfig(gboolean enable,
                 gboolean enable_rtt,
                 gboolean enable_rtx,
                 uint32_t span,
                 uint32_t scale)
{
    qf_dyn_enable = enable || enable_rtt || enable_rtx;
    qf_dyn_enable_rtt = enable_rtt;
    qf_dyn_enable_rtx = enable_rtx;
    qf_dyn_bincap = span;
    qf_dyn_binscale = scale;
}

void qfDynFree(qfDyn_t      *qd)
{
    // qfSeqBitsFree(&qd->sb);
}

void qfDynEcn(qfDyn_t *qd,
              uint8_t ecnbits)
{
    if (ecnbits & 0x01) qd->dynflags |= QF_DYN_ECT0;
    if (ecnbits & 0x02) qd->dynflags |= QF_DYN_ECT1;
    if ((ecnbits & 0x03) == 0x03) qd->dynflags |= QF_DYN_CE;
}

void qfDynClose(qfDyn_t *qd) {
    // count loss
    // add last flight
}



void qfDynDumpStats() {
    /* no more ring overruns, nothing to print */
}

// FIXME graveyard -- old seq/ack walk code

#if 0
static gboolean qfDynRttWalkSeq(qfDyn_t     *qd,
                                uint32_t    seq,
                                uint32_t    oct,
                                uint32_t    tsecr,
                                uint32_t    ms)
{
    unsigned nrttc;
    
    /* short-circuit if we're looking for an ack */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) == QF_DYN_RTTW_SA) return FALSE;
    
    /* try rttc measurement if we're looking for a seq */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) == QF_DYN_RTTW_AS) {
        if (tsecr && (qfSeqCompare(tsecr, qd->rtt_next_tsack) >= 0)) {
            /* found via TS, update and minimize rttc. */
            nrttc = ms - qd->rtt_next_lms;
            if (!qd->rttc || nrttc < qd->rttc) {
                qd->rttc = nrttc;
            }
            //                if (qd->rttc && qd->rttm) {
            //                    sstMeanAdd(&qd->rtt, qd->rttc + qd->rttm);
            //                }
        } else if (!qd->rtt_next_tsack) {
            /* no timestamps for this flow. RTTC remains 0, go to SA state. */
            qd->rttc = 0;
            //          if (qd->rttm) {
            //              sstMeanAdd(&qd->rtt, qd->rttm);
            //          }
        } else {
            /* not found. skip state switch. */
            return FALSE;
        }
    }
    
    /* updated rttc or in initial state; take seq and look for ack. */
    qd->dynflags = (qd->dynflags & ~QF_DYN_RTTW_AS) | QF_DYN_RTTW_SA;
    qd->rtt_next_lms = ms;
    qd->rtt_next_tsack = seq + oct;
    
    return TRUE;
}

static gboolean qfDynRttWalkAck  (qfDyn_t     *qd,
                                  uint32_t    ack,
                                  uint32_t    tsval,
                                  uint32_t    ms)
{
    /* short-circuit if we're not looking for an ack */
    if ((qd->dynflags & QF_DYN_RTTW_STATE) != QF_DYN_RTTW_SA) return FALSE;
    
    /* try rttm measurement */
    if (qfSeqCompare(ack, qd->rtt_next_tsack) >= 0) {
        /* yep, got the ack we're looking for */
        qd->rttm = ms - qd->rtt_next_lms;
        if (qd->rttm) {
            sstMeanAdd(&qd->rtt, qd->rttc + qd->rttm);
        }
        
        /* set next expected tsecr */
        qd->dynflags = (qd->dynflags & ~QF_DYN_RTTW_SA) | QF_DYN_RTTW_AS;
        qd->rtt_next_lms = ms;
        qd->rtt_next_tsack = tsval;
        return TRUE;
    }
    
    return FALSE;
}
#endif
