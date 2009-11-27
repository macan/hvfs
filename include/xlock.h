/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-11-27 09:40:00 macan>
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

#ifndef __XLOCK_H__
#define __XLOCK_H__

#ifdef __KERNEL__

typedef struct rw_semaphore xrwlock_t;
#define xrwlock_rlock down_read
#define xrwlock_runlock up_read
#define xrwlock_wlock down_write
#define xrwlock_wunlock up_write
#define xrwlock_init init_rwsem
#define xrwlock_destroy(l)

#else

typedef pthread_rwlock_t xrwlock_t;
#define xrwlock_rlock pthread_rwlock_rdlock
#define xrwlock_runlock pthread_rwlock_unlock
#define xrwlock_wlock pthread_rwlock_wrlock
#define xrwlock_wunlock pthread_rwlock_unlock
#define xrwlock_init(l) pthread_rwlock_init(l, NULL)
#define xrwlock_destroy pthread_rwlock_destroy

#endif

#endif