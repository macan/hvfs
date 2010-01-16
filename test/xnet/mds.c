/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-30 21:36:20 macan>
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
#include "xnet.h"
#include "mds.h"

#ifdef UNIT_TEST
char *ipaddr1[] = {
    "172.25.38.242",
};

char *ipaddr2[] = {
    "172.25.38.242",
};

short port1[] = {
    8412,
};

short port2[] = {
    8210,
};

void hmr_print(struct hvfs_md_reply *hmr)
{
    struct hvfs_index *hi;
    struct mdu *m;
    struct link_source *ls;
    void *p = hmr->data;

    hvfs_info(xnet, "hmr-> err %d, mdu_no %d, len %d, flag 0x%x.\n",
              hmr->err, hmr->mdu_no, hmr->len, hmr->flag);
    if (!p)
        return;
    hi = (struct hvfs_index *)p;
    hvfs_info(xnet, "hmr-> HI: len %d, flag 0x%x, uuid %lld, hash %lld, itbid %lld, "
              "puuid %lld, psalt %lld\n", hi->namelen, hi->flag, hi->uuid, hi->hash,
              hi->itbid, hi->puuid, hi->psalt);
    p += sizeof(struct hvfs_index);
    if (hmr->flag & MD_REPLY_WITH_MDU) {
        m = (struct mdu *)p;
        hvfs_info(xnet, "hmr->MDU: size %lld, dev %lld, mode 0x%x, nlink %d, uid %d, "
                  "gid %d, flags 0x%x, atime %llx, ctime %llx, mtime %llx, dtime %llx, "
                  "version %d\n", m->size, m->dev, m->mode, m->nlink, m->uid,
                  m->gid, m->flags, m->atime, m->ctime, m->mtime, m->dtime,
                  m->version);
        p += sizeof(struct mdu);
    }
    if (hmr->flag & MD_REPLY_WITH_LS) {
        ls = (struct link_source *)p;
        hvfs_info(xnet, "hmr-> LS: hash %lld, puuid %lld, uuid %lld\n",
                  ls->s_hash, ls->s_puuid, ls->s_uuid);
        p += sizeof(struct link_source);
    }
    if (hmr->flag & MD_REPLY_WITH_BITMAP) {
        hvfs_info(xnet, "hmr-> BM: ...\n");
    }
}

/**
 *@dsite: dest site
 *@nid: name id
 */
int get_send_msg_lookup(int dsite, int nid, u64 puuid, u64 itbid, u64 flag)
{
    char name[HVFS_MAX_NAME_LEN];
    size_t dpayload;
    struct xnet_msg *msg;
    struct hvfs_index *hi;
    struct hvfs_md_reply *hmr;
    int err = 0;
    
    memset(name, 0, sizeof(name));
    snprintf(name, HVFS_MAX_NAME_LEN, "mds-xnet-test-%d", nid);
    dpayload = sizeof(struct hvfs_index) + strlen(name);
    hi = (struct hvfs_index *)xzalloc(dpayload);
    if (!hi) {
        hvfs_err(xnet, "xzalloc() hvfs_index failed\n");
        return -ENOMEM;
    }
    hi->hash = hvfs_hash(puuid, (u64)name, strlen(name), HASH_SEL_EH);
    hi->puuid = puuid;
    hi->itbid = itbid;
    hi->flag = flag;
    memcpy(hi->name, name, strlen(name));
    hi->namelen = strlen(name);
    
    /* alloc one msg and send it to the peer site */
    msg = xnet_alloc_msg(XNET_MSG_NORMAL);
    if (!msg) {
        hvfs_err(xnet, "xnet_alloc_msg() failed\n");
        err = -ENOMEM;
        goto out;
    }

    xnet_msg_fill_tx(msg, XNET_MSG_REQ, XNET_NEED_DATA_FREE |
                     XNET_NEED_REPLY, !dsite, dsite);
    xnet_msg_fill_cmd(msg, HVFS_CLT2MDS_LOOKUP, 0, 0);
    xnet_msg_add_sdata(msg, hi, dpayload);

    hvfs_debug(xnet, "MSG dpayload %lld\n", msg->tx.len);
    err = xnet_send(hmo.xc, msg);
    if (err) {
        hvfs_err(xnet, "xnet_send() failed\n");
        goto out;
    }
    /* this means we have got the repy, parse it! */
    ASSERT(msg->pair, xnet);
    if (msg->pair->tx.err) {
        hvfs_err(xnet, "LOOKUP failed @ MDS site %lld w/ %d\n",
                 msg->pair->tx.ssite_id, msg->pair->tx.err);
        err = msg->pair->tx.err;
        goto out;
    }
    if (msg->pair->xm_datacheck)
        hmr = (struct hvfs_md_reply *)msg->pair->xm_data;
    else {
        hvfs_err(xnet, "Invalid LOOKUP reply from site %lld.\n",
                 msg->pair->tx.ssite_id);
        err = -EFAULT;
        goto out;
    }
    /* now, checking the hmr err */
    if (hmr->err) {
        /* hoo, sth wrong on the MDS */
        hvfs_err(xnet, "MDS Site %lld reply w/ %d\n", 
                 msg->pair->tx.ssite_id, hmr->err);
        xnet_set_auto_free(msg->pair);
        goto out;
    }
    /* ok, we got the correct respond, dump it */
    hmr_print(hmr);
    xnet_set_auto_free(msg->pair);
out:
    return err;
}

int msg_send(int dsite)
{
    return get_send_msg_lookup(dsite, 0, 0, 0,
                               INDEX_LOOKUP | INDEX_BY_NAME | INDEX_ITE_ACTIVE);
}

int msg_wait(int dsite)
{
    while (1) {
        xnet_wait_any(hmo.xc);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct xnet_type_ops ops = {
        .buf_alloc = NULL,
        .buf_free = NULL,
        .recv_handler = mds_client_dispatch,
    };
    int err = 0;
    int dsite;
    short port;
    
    hvfs_info(xnet, "MDS w/ XNET Simple UNIT TESTing ...\n");

    if (argc == 2) {
        port = 8210;
        dsite = 0;
    } else {
        port = 8412;
        dsite = 1;
    }
    
    st_init();
    mds_init(10);

    hmo.xc = xnet_register_type(0, port, !dsite, &ops);
    if (IS_ERR(hmo.xc)) {
        err = PTR_ERR(hmo.xc);
        goto out;
    }

    xnet_update_ipaddr(0, 1, ipaddr1, port1);
    xnet_update_ipaddr(1, 1, ipaddr2, port2);

    SET_TRACING_FLAG(xnet, HVFS_DEBUG);

    if (dsite)
        msg_send(dsite);
    else
        msg_wait(dsite);

    xnet_unregister_type(hmo.xc);
out:
    st_destroy();
    mds_destroy();
    return 0;
}
#endif
