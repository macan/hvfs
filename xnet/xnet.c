/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-30 17:47:12 macan>
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

TRACING_FLAG(xnet, HVFS_DEFAULT_LEVEL);

struct xnet_msg *xnet_alloc_msg(u8 alloc_flag)
{
    struct xnet_msg *msg;

#ifndef USE_XNET_SIMPLE
    /* fast method */
    if (alloc_flag == XNET_MSG_CACHE)
        return NULL;
    
    /* slow method */
    if (alloc_flag != XNET_MSG_NORMAL)
        return NULL;
#endif
    
    msg = xzalloc(sizeof(struct xnet_msg));
    if (!msg) {
        hvfs_err(xnet, "xzalloc() struct xnet_msg failed\n");
        return NULL;
    }

#ifdef USE_XNET_SIMPLE
    sem_init(&msg->event, 0, 0);
    if (msg)
        atomic64_inc(&g_xnet_prof.msg_alloc);
#endif

    return msg;
}

void xnet_free_msg(struct xnet_msg *msg)
{
    if (!msg)
        return;
    
    /* FIXME: we should check the alloc_flag and auto free flag */
    if (msg->pair)
        xnet_free_msg(msg->pair);
    if (msg->tx.flag & XNET_NEED_DATA_FREE) {
        if (msg->tx.type == XNET_MSG_REQ) {
            /* check and free the siov */
            xnet_msg_free_sdata(msg);
        } else if (msg->tx.type == XNET_MSG_RPY) {
            /* check and free the riov */
            xnet_msg_free_rdata(msg);
        } else {
            /* FIXME: do we need to free the data region */
        }
    }
    xfree(msg);
#ifdef USE_XNET_SIMPLE
    atomic64_inc(&g_xnet_prof.msg_free);
#endif
}

#ifndef USE_XNET_SIMPLE
int xnet_msg_add_sdata(struct xnet_msg *msg, void *addr, int len)
{
    return 0;
}

int xnet_msg_add_rdata(struct xnet_msg *msg, void *addr, int len)
{
    return 0;
}

void xnet_msg_free_sdata(struct xnet_msg *msg)
{
}

void xnet_msg_free_rdata(struct xnet_msg *msg)
{
}

int xnet_send(struct xnet_context *xc, struct xnet_msg *msg)
{
    return -ENOSYS;
}
int xnet_isend(struct xnet_context *xc, struct xnet_msg *msg)
{
    return -ENOSYS;
}

void *mds_gwg;
int xnet_wait_group_add(void *gwg, struct xnet_msg *msg)
{
    return -ENOSYS;
}

int xnet_wait_group_del(void *gwg, struct xnet_msg *msg)
{
    return -ENOSYS;
}
#endif
