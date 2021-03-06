##
# Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
#                           <macan@ncic.ac.cn>
#
# Time-stamp: <2009-12-29 15:24:32 macan>
#
# This is the makefile for HVFS project.
#
# Armed with EMACS.

CC = gcc
LD = gcc
AR = ar

INC_PATH = $(HOME_PATH)/include
LIB_PATH = $(HOME_PATH)/lib
MDS = $(HOME_PATH)/mds
XNET = $(HOME_PATH)/xnet
TEST = $(HOME_PATH)/test

COMPILE_DATE = `date`
COMPILE_HOST = `hostname`

# Debuging the memory and locks
XCFLAGS = -O2 -rdynamic -g -pg -std=gnu99

CFLAGS += $(XCFLAGS)
CFLAGS += -Wall -DCDATE="\"$(COMPILE_DATE)\"" \
			-DCHOST="\"$(COMPILE_HOST)\"" -I$(INC_PATH) \
			-I$(LIB_PATH) -I$(MDS) -D_GNU_SOURCE\
			-DHVFS_TRACING -DHVFS_DEBUG_MEMORY -DHVFS_DEBUG_LOCK_ \
            -D_USE_SPINLOCK_ -DHVFS_DEBUG_LATENCY_ -DXNET_BLOCKING \
            -DXNET_EAGER_WRITEV_
LFLAGS += -lrt -lpthread

# Region for depend files
TEST_MDS_SOURCE = cbht.c tx.c dh.c cmd_sender.c
TEST_XNET_SOURCE = xnet.c mds.c
MDS_AR_SOURCE = itb.c mds.c txg.c cbht.c tx.c prof.c conf.c dh.c xtable.c \
				dispatch.c c2m.c
LIB_AR_SOURCE = lib.c ring.c hash.c time.c bitmap.c xlock.c
XNET_AR_SOURCE = xnet.c xnet_simple.c
INC_H_SOURCE = atomic.h err.h hvfs.h hvfs_common.h hvfs_const.h hvfs_k.h \
				hvfs_u.h ite.h mds_api.h mdsl_api.h memory.h site.h tx.h \
				tracing.h txg.h xhash.h xlist.h xlock.h xnet.h xtable.h \
                xprof.h
MDS_H_SOURCE = mds.h cbht.h dh.h itb.h prof.h
LIB_H_SOURCE = lib.h ring.h

inc_h_depend_files = $(patsubst %.h, $(INC_PATH)/%.h, $(INC_H_SOURCE))
mds_h_depend_files = $(patsubst %.h, $(MDS)/%.h, $(MDS_H_SOURCE))
lib_h_depend_files = $(patsubst %.h, $(LIB_PATH)/%.h, $(LIB_H_SOURCE))
header_depend_files = $(inc_h_depend_files) $(mds_h_depend_files) \
						$(lib_h_depend_files)

ut_depend_files = $(patsubst %.c, $(TEST)/mds/%.c, $(TEST_MDS_SOURCE)) \
					$(patsubst %.c, $(TEST)/xnet/%.c, $(TEST_XNET_SOURCE)) \
					$(header_depend_files)
mds_depend_files = $(patsubst %.c, $(MDS)/%.c, $(MDS_AR_SOURCE)) \
					$(header_depend_files)
lib_depend_files = $(patsubst %.c, $(LIB_PATH)/%.c, $(LIB_AR_SOURCE)) \
					$(header_depend_files)
xnet_depend_files = $(patsubst %.c, $(XNET)/%.c, $(XNET_AR_SOURCE)) \
					$(header_depend_files)

HVFS_LIB = $(LIB_PATH)/libhvfs.a
MDS_LIB = $(MDS)/libmds.a
XNET_LIB = $(XNET)/libxnet.a