/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2009-11-27 09:38:16 macan>
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

#ifndef __MEMORY_H__
#define __MEMORY_H__

#ifdef __KERNEL__

#define MEM_OVERFLOW_MAGIC 0xf0d0000000000000

#ifdef HVFS_DEBUG_MEMORY
#define MEM_OVERFLOW_CHECK(ptr) do {                                    \
        if (ptr) {                                                      \
            u64 size = *(u64 *)(ptr - sizeof(u64));                     \
            if (size & MEM_OVERFLOW_MAGIC != MEM_OVERFLOW_MAGIC) {      \
                hvfs_debug(lib, "Memory overflow happened @ %p\n", ptr); \
            }                                                           \
        }                                                               \
    } while (0)
#else
#define MEM_OVERFLOW_CHECK(ptr)
#endif
    
static inline void *xzalloc(size_t size) 
{
    void *m = xmalloc(size);
    memset(m, 0, size);
    return m;
}

static inline void *xmalloc(size_t size)
{
    void *m;

    size += sizeof(u64);
    /* If buffer overflow happens, then the system will crash! */
    if (size < (PAGE_SIZE << 7)) {
        m = kmalloc(size, GFP_KERNEL);
    } else {
        m = vmalloc(size);
    }
    *(u64 *)m = size | MEM_OVERFLOW_MAGIC; /* overflow magic */
    return m + sizeof(u64);
}

static inline void xfree(void *ptr)
{
    MEM_OVERFLOW_CHECK(ptr);
    ptr -= sizeof(u64);
    if (ptr < VMALLOC_START || ptr > VMALLOC_END)
        kfree(ptr);
    else
        vfree(ptr);
}

static inline void *xrealloc(void *ptr, size_t size)
{
    void *m;
    size_t osize;

    size += sizeof(u64);

    MEM_OVERFLOW_CHECK(ptr);
    if (size < (PAGE_SIZE << 7)) {
        /* use kmalloc */
        m = kmalloc(size, GFP_KERNEL);
    } else {
        /* use vmalloc */
        m = vmalloc(size);
    }
    *(u64 *)m = size | MEM_OVERFLOW_MAGIC; /* overflow magic */
    m += sizeof(u64);
    if (ptr) {
        osize = *(u64 *)(ptr - sizeof(u64));
        osize = osize & (~MEM_OVERFLOW_MAGIC);
        memcpy(m, ptr, (osize > size ? size : osize) - sizeof(u64));
    }
    return m;
}

#else  /* !__KERNEL */
static inline void *xzalloc(size_t size)
{
    void *m = malloc(size);
    if (likely(m))
        memset(m, 0, size);
    return m;
}

#define xmalloc malloc
#define xfree free
#define xrealloc realloc

#endif

#endif
