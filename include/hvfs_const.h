/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-12-28 14:09:51 macan>
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

#ifndef __HVFS_CONST_H__
#define __HVFS_CONST_H__

#define HVFS_MAX_NAME_LEN       256
#define MDS_DCONF_MAX_NAME_LEN  64

static char *hvfs_ccolor[] __attribute__((unused))= {
    "\033[0;40;31m",            /* red */
    "\033[0;40;32m",
    "\033[0;40;33m",
    "\033[0;40;34m",
    "\033[0;40;35m",
    "\033[0;40;36m",
    "\033[0;40;37m",
    "\033[0m",
};
#define HVFS_COLOR(x)  (hvfs_ccolor[x])
#define HVFS_COLOR_RED (hvfs_ccolor[0])
#define HVFS_COLOR_GREEN (hvfs_ccolor[1])
#define HVFS_COLOR_YELLOW (hvfs_ccolor[2])
#define HVFS_COLOR_BLUE (hvfs_ccolor[3])
#define HVFS_COLOR_PINK (hvfs_ccolor[4])
#define HVFS_COLOR_YANK (hvfs_ccolor[5])
#define HVFS_COLOR_WHITE (hvfs_ccolor[6])
#define HVFS_COLOR_END (hvfs_ccolor[7])

#define ETXCED          1025    /* TXC Evicted */
#define ECHP            1026    /* Consistent Hash Point error */
#define ERINGCHG        1027    /* Ring Changed */
#define ESPLIT          1028    /* Need SPLIT */
#define ENOTEXIST       1029    /* Not Exist */
#endif
