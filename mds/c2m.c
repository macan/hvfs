/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-29 14:10:45 macan>
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
#include "mds.h"
#include "xtable.h"
#include "tx.h"
#include "xnet.h"

/* NOTE: how to alloc a hmr very fast? */
static inline
struct hvfs_md_reply *get_hmr(void)
{
    return xzalloc(sizeof(struct hvfs_md_reply));
}

static inline 
void mds_send_reply(struct hvfs_tx *tx, struct hvfs_md_reply *hmr, 
                    int err)
{
#ifdef HVFS_DEBUG_LATENCY
    lib_timer_def();
    lib_timer_start(&begin);
#endif
    tx->rpy = xnet_alloc_msg(XNET_MSG_CACHE);
    if (!tx->rpy) {
        hvfs_err(mds, "xnet_alloc_msg() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    hvfs_debug(mds, "Send REPLY(err %d) to %lld: hmr->len %d, hmr->flag 0x%x\n", 
               err, tx->reqin_site, hmr->len, hmr->flag);
    hmr->err = err;
    if (!hmr->err) {
#ifdef XNET_EAGER_WRITEV
        xnet_msg_add_sdata(tx->rpy, &tx->rpy->tx, sizeof(struct xnet_msg_tx));
#endif
        xnet_msg_add_sdata(tx->rpy, hmr, sizeof(*hmr));
        if (hmr->len)
            xnet_msg_add_sdata(tx->rpy, hmr->data, hmr->len);
    } else {
        /* we should change the TX to the FORGET level */
        tx->op = HVFS_TX_FORGET;
        xnet_msg_set_err(tx->rpy, hmr->err);
        /* then, we should free the hmr and any allocated buffers */
        if (hmr->data)
            xfree(hmr->data);
        xfree(hmr);
    }
        
    xnet_msg_fill_tx(tx->rpy, XNET_MSG_RPY, XNET_NEED_DATA_FREE, hmo.site_id,
                     tx->reqin_site);
    xnet_msg_fill_reqno(tx->rpy, tx->req->tx.reqno);
    xnet_msg_fill_cmd(tx->rpy, XNET_RPY_DATA, 0, 0);
    /* match the original request at the source site */
    tx->rpy->tx.handle = tx->req->tx.handle;

    xnet_wait_group_add(mds_gwg, tx->rpy);
    if (xnet_isend(hmo.xc, tx->rpy)) {
        hvfs_err(mds, "xnet_isend() failed\n");
        /* do not retry myself, client is forced to retry */
        xnet_wait_group_del(mds_gwg, tx->rpy);
        /* FIXME: should we free the tx->rpy? */
    }
    /* FIXME: state machine of TX, MSG */
    mds_tx_done(tx);
    mds_tx_reply(tx);
#ifdef HVFS_DEBUG_LATENCY
    lib_timer_stop(&end);
    lib_timer_echo_plus(&begin, &end, 1, "REPLY");
#endif
}

/* STATFS */
void mds_statfs(struct hvfs_tx *tx)
{
    struct statfs *s = (struct statfs *)xzalloc(sizeof (struct statfs));

    if (!s) {
        hvfs_err(mds, "xzalloc() failed\n");
        mds_free_tx(tx);
        return;
    }
    s->f_files = atomic64_read(&hmi.mi_dnum) + 
        atomic64_read(&hmi.mi_fnum);
    s->f_ffree = (HVFS_MAX_UUID_PER_MDS - atomic64_read(&hmi.mi_uuid));

    tx->rpy = xnet_alloc_msg(XNET_MSG_NORMAL);
    if (!tx->rpy) {
        hvfs_err(mds, "xnet_alloc_msg() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    
#ifdef XNET_EAGER_WRITEV
    xnet_msg_add_sdata(tx->rpy, &tx->rpy->tx, sizeof(struct xnet_msg_tx));
#endif
    xnet_msg_add_sdata(tx->rpy, s, sizeof(struct statfs));

    xnet_msg_fill_tx(tx->rpy, XNET_MSG_RPY, XNET_NEED_DATA_FREE, hmo.site_id,
                     tx->reqin_site);
    xnet_msg_fill_reqno(tx->rpy, tx->req->tx.reqno);
    xnet_msg_fill_cmd(tx->rpy, XNET_RPY_DATA, 0, 0);
    /* match the original request at the source site */
    tx->rpy->tx.handle = tx->req->tx.handle;

    mds_tx_done(tx);

    xnet_wait_group_add(mds_gwg, tx->rpy);
    if (xnet_isend(hmo.xc, tx->rpy)) {
        hvfs_err(mds, "xnet_isend() failed\n");
        /* do not retry myself, client is forced to retry */
        xnet_wait_group_del(mds_gwg, tx->rpy);
    }
    /* FIXME: state machine of TX, MSG */
    mds_tx_reply(tx);
}

/* LOOKUP */
void mds_lookup(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid LOOKUP request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }

    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }
    hvfs_debug(mds, "LOOKUP %lld %lld %llx %s %d\n", 
               hi->puuid, hi->itbid, hi->hash, hi->name, hi->namelen);
    
    if (hi->flag & INDEX_BY_NAME && !hi->hash) {
        hi->hash = hvfs_hash(hi->puuid, (u64)(hi->name), hi->namelen, 
                             HASH_SEL_EH);
    }
    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* search in the CBHT */
    hi->flag |= INDEX_LOOKUP;
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* CREATE */
void mds_create(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

#ifdef HVFS_DEBUG_LATENCY
    struct timeval begin, end;
    lib_timer_start(&begin);
#endif
    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid CREATE request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }

    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }

    if (hi->flag & INDEX_BY_NAME && !hi->hash)
        hi->hash = hvfs_hash(hi->puuid, (u64)hi->name, hi->namelen, 
                             HASH_SEL_EH);

    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* create in the CBHT */
    hi->flag |= INDEX_CREATE | INDEX_BY_NAME;
    if (!hi->dlen) {
        /* we may got zero payload create */
        hi->data = NULL;
    } else
        hi->data = tx->req->xm_data + sizeof(*hi) + hi->namelen;
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

#ifdef HVFS_DEBUG_LATENCY
    lib_timer_stop(&end);
    lib_timer_echo_plus(&begin, &end, 1, "CREATE");
#endif
actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* RELEASE */
void mds_release(struct hvfs_tx *tx)
{
    /* FIXME */
    hvfs_info(mds, "Not implement yet.\n");
}

/* UPDATE */
void mds_update(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid LOOKUP request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }

    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }

    if (hi->flag & INDEX_BY_NAME && !hi->hash) {
        hi->hash = hvfs_hash(hi->puuid, (u64)hi->name, hi->namelen, 
                             HASH_SEL_EH);
    }
    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* search in the CBHT */
    hi->flag |= INDEX_MDU_UPDATE;
    if (!hi->dlen) {
        hvfs_warning(mds, "UPDATE w/ zero length data payload.\n");
        /* FIXME: we should drop the TX */
        mds_free_tx(tx);
        return;
    }
    hi->data = tx->req->xm_data + sizeof(*hi) + hi->namelen;
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* LINKADD */
void mds_linkadd(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid LINKADD request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }
    
    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }

    if (hi->flag & INDEX_BY_NAME && !hi->hash) {
        hi->hash = hvfs_hash(hi->puuid, (u64)hi->name, hi->namelen, 
                             HASH_SEL_EH);
    }
    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* search in the CBHT */
    hi->flag |= INDEX_LINK_ADD;
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* UNLINK */
void mds_unlink(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid UNLINK request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }
    
    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }

    if (hi->flag & INDEX_BY_NAME && !hi->hash) {
        hi->hash = hvfs_hash(hi->puuid, (u64)hi->name, hi->namelen, 
                             HASH_SEL_EH);
    }
    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* search in the CBHT */
    hi->flag |= INDEX_UNLINK;
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() faield\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* symlink */
void mds_symlink(struct hvfs_tx *tx)
{
    struct hvfs_index *hi = NULL;
    struct hvfs_md_reply *hmr;
    int err;

    /* sanity checking */
    if (tx->req->tx.len < sizeof(*hi)) {
        hvfs_err(mds, "Invalid LINKADD request %lld received\n", 
                 tx->req->tx.reqno);
        err = -EINVAL;
        goto send_rpy;
    }
    
    if (tx->req->xm_datacheck)
        hi = tx->req->xm_data;
    else {
        hvfs_err(mds, "Internal error, data lossing ...\n");
        err = -EFAULT;
        goto send_rpy;
    }

    if (hi->flag & INDEX_BY_NAME && !hi->hash) {
        hi->hash = hvfs_hash(hi->puuid, (u64)hi->name, hi->namelen, 
                             HASH_SEL_EH);
    }
    /* alloc hmr */
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }

    /* search in the CBHT */
    hi->flag |= (INDEX_CREATE | INDEX_SYMLINK);
    if (!hi->dlen) {
        hvfs_warning(mds, "SYMLINK w/ zero length symname.\n");
        /* FIXME: we should drop the TX */
        mds_free_tx(tx);
        return;
    }
    hi->data = tx->req->xm_data + sizeof(*hi) + hi->namelen; /* symname */
    err = mds_cbht_search(hi, hmr, tx->txg, &tx->txg);

actually_send:
    return mds_send_reply(tx, hmr, err);
send_rpy:
    hmr = get_hmr();
    if (!hmr) {
        hvfs_err(mds, "get_hmr() failed\n");
        /* do not retry myself */
        mds_free_tx(tx);
        return;
    }
    goto actually_send;
}

/* LOAD BITMAP */
void mds_lb(struct hvfs_tx *tx)
{
    /* FIXME */
    hvfs_info(mds, "Not implement yet.\n");
}
