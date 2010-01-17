/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-01 13:04:44 macan>
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

#ifndef __HVFS_SITE_H__
#define __HVFS_SITE_H__

/*
 * SITE format: used 20bits
 *
 * |----not used----|--site type--|--site #--|
 *
 * site type: 3bits, ex. <1: client>, <2: mds>, <3: mdsl>, <..>, ...
 * site #: 17bits
 */
#define HVFS_SITE_TYPE_CLIENT   0x01
#define HVFS_SITE_TYPE_MDS      0x02
#define HVFS_SITE_TYPE_MDSL     0x03
#define HVFS_SITE_TYPE_RING     0x04
#define HVFS_SITE_TYPE_ROOT     0x05
#define HVFS_SITE_TYPE_AMC      0x06 /* another metadata client */

#define HVFS_SITE_TYPE_MASK     (0x7 << 17)

#define HVFS_IS_CLIENT(site) (((site & HVFS_SITE_TYPE_MASK) >> 17) ==   \
                              HVFS_SITE_TYPE_CLIENT)

#define HVFS_IS_MDS(site) (((site & HVFS_SITE_TYPE_MASK) >> 17) ==  \
                           HVFS_SITE_TYPE_MDS)

#define HVFS_IS_MDSL(site) (((site & HVFS_SITE_TYPE_MASK) >> 17) == \
                            HVFS_SITE_TYPE_MDSL)

#define HVFS_IS_RING(site) (((site & HVFS_SITE_TYPE_MASK) >> 17) == \
                            HVFS_SITE_TYPE_RING)

#define HVFS_IS_ROOT(site) (((site & HVFS_SITE_TYPE_MASK) >> 17) == \
                            HVFS_SITE_TYPE_ROOT)

#define HVFS_SITE_N_MASK        ((1 << 17) - 1)

#define HVFS_CLIENT(n) ((HVFS_SITE_TYPE_CLIENT << 17) | (n & HVFS_SITE_N_MASK))

#define HVFS_MDS(n) ((HVFS_SITE_TYPE_MDS << 17) | (n & HVFS_SITE_N_MASK))

#define HVFS_MDSL(n) ((HVFS_SITE_TYPE_MDSL << 17) | (n & HVFS_SITE_N_MASK))

#define HVFS_RING(n) ((HVFS_SITE_TYPE_RING << 17) | (n & HVFS_SITE_N_MASK))

#define HVFS_ROOT(n) ((HVFS_SITE_TYPE_ROOT << 17) | (n & HVFS_SITE_N_MASK))
#endif
