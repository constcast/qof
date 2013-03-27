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

#ifndef _QOF_DYN_H_
#define _QOF_DYN_H_

#include <yaf/autoinc.h>
#include <yaf/yaftab.h>

/** Sequence number bin structure used to track whether a 
    given sequence number has been seen */
typedef struct qfSeqBin_st {
    uint64_t    *bin;
    size_t      bincount;
    size_t      opcount;
    size_t      scale;
    size_t      binbase;
    uint32_t    seqbase;
} qfSeqBin_t;

/** Sequence number bin result codes */
typedef enum qfSeqBinRes_en {
    /** No intersection between given range and seen sequence numbers */
    QF_SEQBIN_NO_ISECT,
    /** Partial intersection between given range and seen sequence numbers */
    QF_SEQBIN_PART_ISECT,
    /** Full intersection between given range and seen sequence numbers */
    QF_SEQBIN_FULL_ISECT,
    /** Given range is out of range of the tracker, and will not be tracked */
    QF_SEQBIN_OUT_OF_RANGE
} qfSeqBinRes_t;

/**
 * Compare sequence numbers A and B, accounting for 2e31 wraparound.
 *
 * @param a value to compare
 * @param b value to compare
 * @return >0 if a > b, 0 if a == b, <0 if a < b.
 */

int qfSeqCompare(uint32_t a, uint32_t b);

void qfSeqBinInit(qfSeqBin_t     *sb,
                  size_t         capacity,
                  size_t         scale);

void qfSeqBinFree(qfSeqBin_t *sb);

qfSeqBinRes_t qfSeqBinTestAndSet(qfSeqBin_t      *sb,
                                 uint32_t        aseq,
                                 uint32_t        bseq);

struct qfSeqTime_st;
typedef struct qfSeqTime_st qfSeqTime_t;

/** Ring of sequence number / time tuples */
typedef struct qfSeqRing_st {
    qfSeqTime_t     *bin;
    size_t          bincount;
    size_t          opcount;
    size_t          overcount;
    size_t          head;
    size_t          tail;
} qfSeqRing_t;

void qfSeqRingInit(qfSeqRing_t              *sr,
                   size_t                   capacity);

void qfSeqRingFree(qfSeqRing_t              *sr);

void qfSeqRingAddSample(qfSeqRing_t         *sr,
                        uint32_t            seq,
                        uint64_t            ms);

uint32_t qfSeqRingRTT(qfSeqRing_t           *sr,
                      uint32_t              ack,
                      uint64_t              ms);

#endif /* idem */
