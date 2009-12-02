/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-01 15:26:58 macan>
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

#include "hvfs.h"
#include "xtable.h"
#include "tx.h"
#include "xnet.h"

static inline u64 mds_rdtx()
{
    return atomic64_inc_return(&hmi.mi_tx);
}

struct hvfs_tx *mds_alloc_tx(u16 op, struct xnet_msg *req)
{
    struct hvfs_tx *tx;
    
    /* Step 1: try the fast allocation path */
    tx = mds_txc_alloc_tx(&hmo.txc);
    if (likely(tx))
        goto init_tx;
    /* fall back to slow path */
    tx = zalloc(*tx);
    if (!tx) {
        hvfs_debug(mds, "zalloc() hvfs_tx failed\n");
        return NULL;
    }
    
init_tx:
    tx->op = op;
    tx->state = HVFS_TX_PROCESSING;
    tx->tx = mds_rdtx();
    tx->reqno = req->tx.reqno;
    tx->reqno_site = req->tx.ssite_id;
    tx->req = req;
    tx->txg = mds_get_open_txg();   /* get the current opened TXG */
    atomic_set(&tx->ref, 0);
    INIT_LIST_HEAD(&tx->lru);
    INIT_LIST_HEAD(&tx->tx_list);
    INIT_HLIST_NODE(&tx->hlist);

    /* FIXME: insert in the TXC */
    mds_txc_add(&hmo.txc, tx);
    /* FIXME: tx_list? */
}

void mds_free_tx(struct hvfs_tx *tx)
{
    ASSERT(IS_EMPTY(&tx->lru));
    xlock_lock(&hmo.txc.lock);
    list_add_tail(&tx->lru, &hmo.txc.lru);
    xlock_unlock(&hmo.txc.lock);
}

void mds_pre_free_tx(int hint)
{
    struct hvfs_tx *tx;

    if (HVFS_RANDOM(hint))       /* exec @ p = 1/HINT */
        return;
    
    xlock_lock(&hmo.tx.lock);
    list_for_each_entry(tx, &hmo.tx.lru, lru) {
        xnet_free_msg(tx->req);
        if (tx->rpy)
            xnet_free_msg(tx->rpy);
        if (--hint == 0)
            break;
    }
    xlock_unlock(&hmo.tx.lock);
}

void mds_get_tx(struct hvfs_tx *tx)
{
    atomic_add(&tx->ref);
}

void mds_put_tx(struct hvfs_tx *tx)
{
    atomic_dec(&tx_ref);
}

/*
 * @hsize: hash table size
 * @ftx:   free TXs, for fast TX allocation
 */
int mds_init_txc(struct hvfs_txc *txc, int hsize, int ftx)
{
    int err, i;
    
    INIT_LIST_HEAD(&txc->lru);
    xlock_init(&txc->lock);

    /* regular hash init */
    size = (size == 0) ? MDS_TXC_DEFAULT_SIZE : size;
    txc->txht = zalloc(size * sizeof(struct regular_hash));
    if (!txc->txht) {
        hvfs_err(mds, "TXC hash table allocation failed\n");
        err = -ENOMEM;
        goto out;
    }
    for (i = 0; i < size; i++) {
        INIT_HLIST_HEAD(&txc->txht[i].h);
        xlock_init(&txc->txht[i].lock);
    }
    
    /* pre-allocate TXs */
    ftx = (ftx == 0) ? MDS_TXC_DEFAULT_FTX : ftx;
    txs = zalloc(ftx * sizeof(struct hvfs_tx));
    if (!txs) {
        hvfs_info(mds,  "TXC pre-allocate TX failed\n");
        ftx = 0;
    } else {
        for (i = 0; i < ftx; i++) {
            INIT_LIST_HEAD(&txs->lru);
            /* no need to hold the lock at this moment */
            list_add_tail(&txs->lru, &txc->lru);
        }
    }

    txc->hsize = hsize;
    txc->ftx = ftx;
out:
    return err;
}

/* fast allocate TX in TXC lru list */
struct hvfs_tx *mds_txc_alloc_tx(struct hvfs_txc *txc)
{
    struct list_head *l = NULL;
    struct hvfs_tx *tx = NULL;
    
    xlock_lock(&txc->lock);
    if (ftx > 0) {
        ftx--;
        l = txc->lru.next;
        ASSERT(l != &txc->lru);
        list_del(l);
    }
    xlock_unlock(&txc->lock);

    if (l) {
        /* remove from the TXC */
        tx = list_entry(l, struct hvfs_tx, lru);
        if (!hlist_unhashed(&tx->hlist))
            mds_txc_evict(txc, tx);
    }
    return tx;
}

inline int mds_txc_hash(u64 site_id, u64 reqno, struct hvfs_txc *txc)
{
    u64 val1, val2;

    val1 = hash_64(site_id, 64);
    val2 = hash_64(reqno, 64);
    val1 = val1 ^ (val2 ^ GOLDEN_RATIO_PRIME);

    return val1 % txc->hsize;   /* FIXME: need more faster! */
}

/* TXC insert function
 *
 * @tx:
 */
int mds_txc_add(struct hvfs_txc *txc, struct hvfs_tx *tx)
{
    struct regular_hash *rh;
    int i;

    if (tx->op == HVFS_TX_FORGET)
        return 0;

    ASSERT(hlist_unhashed(&tx->hlist));

    /* compute the hash value and select the RH entry */
    i = mds_txc_hash(tx->reqin_site, tx->reqno, txc);
    rh = txc->txht[i];

    xlock_lock(&rh->lock);
    hlist_add_head(&tx->hlist, &rh->h);
    xlock_unlock(&rh->lock);

    return 0;
}

/* TXC search function, w/ TX got, you MUST put it!
 *
 * @site: source site
 * @reqno:source site reqno
 */
struct hvfs_tx *mds_txc_search(struct hvfs_txc *txc, u64 site, u64 reqno)
{
    struct regular_hash *rh;
    struct hvfs_tx *tx;
    struct hlist_node *l;
    int i;

    i = mds_txc_hash(site, reqno, txc);
    rh = txc->txht[i];

    xlock_lock(&rh->lock);
    hlist_for_each_entry(tx, l, &rh->h, hlist) {
        if (unlikely(tx->reqin_site == site && tx->reqno == reqno)) {
            found = 1;
            mds_get_tx(tx);
            break;
        }
    }
    xlock_unlock(&rh->lock);

    if (likely(!found))
        tx = NULL;
    return tx;
}

/* Evict the TX from TXC, waiting for tx->ref to zero!
 */
int mds_txc_evict(struct hvfs_txc *txc, struct hvfs_tx *tx)
{
    struct regular_hash *rh;
    int i;

    while (atomic_read(&tx->ref))
        xsleep(0);

    i = mds_txc_hash(site, reqno, txc);
    rh = txc->txht[i];
    xlock_lock(&rh->lock);
    hlist_del_init(&tx->hlist);
    xlock_unlock(&rh->lock);

    /* free all the TX resources */
    ASSERT(tx->state >= HVFS_TX_DONE);
    xnet_free_msg(tx->req);
    if (tx->rpy)
        xnet_free_msg(tx->rpy);

    return 0;
}

void mds_tx_done(struct hvfs_tx *tx)
{
    if (tx->state == HVFS_TX_PROCESSING)
        tx->state = HVFS_TX_DONE;
    else {
        hvfs_err(mds, "Invalid TX state 0x%x when calling mds_tx_done.\n");
    }
    if (tx->op == HVFS_TX_FORGET) {
        xlock_lock(&hmo.txc.lock);
        list_add_tail(&tx->lru, &hmo.txc.lru);
        xlock_unlock(&hmo.txc.lock);
        mds_pre_free_tx(HVFS_TX_PRE_FREE_HINT);
    }
    mds_put_txg(tx->txg);
}

void mds_tx_reply(struct hvfs_tx *tx)
{
    if (tx->state <= HVFS_TX_DONE)
        tx->state = HVFS_TX_ACKED;
    /* if tx->state == HVFS_TX_COMMITED, do not change it */

    if (tx->op == HVFS_TX_NOCOMMIT) {
        /* need reply, but no commit */
        xlock_lock(&hmo.txc.lock);
        list_add_tail(&tx->lru, &hmo.txc.lru);
        xlock_unlock(&hmo.txc.lock);
        mds_pre_free_tx(HVFS_TX_PRE_FREE_HINT);
    }
}

/*
 * OK to add to the TXC LRU list, called after TXG commited!
 */
void mds_tx_commit(struct hvfs_tx *tx)
{
    if (tx->state < HVFS_TX_DONE) {
        hvfs_err(mds, "Invalid TX state 0x%x when calling mds_tx_commit.\n", 
                 tx-.state);
    } else {
        if (tx->op == HVFS_TX_NORMAL) {
            xlock_lock(&hmo.txc.lock);
            list_add_tail(&tx->lru, &hmo.txc.lru);
            xlock_unlock(&hmo.txc.lock);
            mds_pre_free_tx(HVFS_TX_FREE_HINT);
        }
    }
}