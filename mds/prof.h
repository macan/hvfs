/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-22 17:18:56 macan>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __MDS_PROF_H__
#define __MDS_PROF_H__

/* statistics for client */
struct mds_client_prof 
{
    atomic64_t gdt_lookup;
    atomic64_t lookup;
    atomic64_t create;
    atomic64_t unlink;
    atomic64_t mkdir;
    atomic64_t getattr;
    atomic64_t setattr;
    atomic64_t rename;
    atomic64_t link;
    atomic64_t bmlookup;        /* # of bitmap lookup */
};

struct mds_ring_prof 
{
    atomic64_t reqout;
    atomic64_t update;          /* # of ring update msg */
    atomic64_t size;            /* total size of ring update msgs */
};

struct mds_mds_prof
{
    atomic64_t gdt_lookup;      /* # of GDT lookup between MDSs */
    atomic64_t rupdate_in;      /* # of remote update IN */
    atomic64_t rupdate_out;     /* # of remote update OUT */
    atomic64_t split;           /* # of splits original at this MDS */
    atomic64_t recover_split;   /* # of splits recovered */
    atomic64_t bitmap_in;       /* # of bitmap lookup IN */
    atomic64_t bitmap_out;      /* # of bitmap lookup OUT */
};

struct mds_mdsl_prof
{
    atomic64_t itb_load;        /* # of ITBs load from MDSL */
    atomic64_t itb_wb;          /* # of ITBs writed to MDSL */
    atomic64_t bitmap;          /* # of bitmap lookup IN */
};

struct mds_txg_prof
{
    atomic64_t tx;              /* # of tx from start to now */
    atomic64_t txg;             /* # of txg from start to now */
    atomic64_t itb;             /* # of commited ITBs from start to now */
    atomic64_t len;             /* length of TXG writed back in [start, now] */
    atomic64_t time;            /* total time for TXG write back (ms) */
    atomic64_t amem;            /* total memory allocated for TXG write back[.] */
    atomic64_t cmem;            /* current memory allocated(KB) */
};

struct mds_cbht_prof
{
    atomic64_t aitb;            /* # of active ITBs */
    atomic64_t lookup;          /* # of lookup */
    atomic64_t modify;          /* # of modify ops */
    atomic64_t split;           /* # of splits */
    atomic64_t buckets;         /* # of buckets */
    atomic64_t depth;           /* the depth of the CBHT */
};

struct mds_itb_prof 
{
    atomic64_t conflict;        /* # of conflicts in ITB */
    atomic64_t pseudo_conflict; /* # of pseudo conflicts in ITB */
    atomic64_t rsearch_depth;   /* total read search depth */
    atomic64_t wsearch_depth;   /* total write search depth */
    atomic64_t cowed;           /* # of COWed ITBs */
    atomic64_t async_unlink;    /* # of async unlinks */
};

struct mds_prof
{
    time_t ts;
    struct mds_client_prof client;
    struct mds_ring_prof ring;
    struct mds_mds_prof mds;
    struct mds_mdsl_prof mdsl;
    struct mds_txg_prof txg;
    struct mds_cbht_prof cbht;
    struct mds_itb_prof itb;
};

#define mds_cbht_prof_rw(hi) do {                   \
        if (hi->flag & INDEX_LOOKUP)                \
            atomic64_inc(&hmo.prof.cbht.lookup);    \
        else                                        \
            atomic64_inc(&hmo.prof.cbht.modify);    \
    } while (0)

#define mds_cbht_prof_split() do {              \
        atomic64_inc(&hmo.prof.cbht.split);     \
    } while (0)

#define mds_cbht_prof_buckets() do {            \
        atomic64_inc(&hmo.prof.cbht.buckets);   \
    } while (0)

#define mds_cbht_prof_depth(dep) do {              \
        atomic64_set(&hmo.prof.cbht.depth, (dep)); \
    } while (0)

#define mds_itb_prof_cow() do {                 \
        atomic64_inc(&hmo.prof.itb.cowed);      \
    } while (0)
#endif
