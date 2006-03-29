/*
$Id$
   OWFS and OWHTTPD
   one-wire file system and
   one-wire web server

    By Paul H Alfille
    {c} 2003 GPL
    palfille@earthlink.net
*/

/* OWFS - specific header */

#ifndef OWFS_H
#define OWFS_H

#ifndef OWFS_CONFIG_H
#error Please make sure owfs_config.h is included *before* this header file
#endif

/* Include FUSE -- http://fuse.sf.net */
/* Lot's of version-specific code */

#define FUSE_USE_VERSION 26
#include <fuse.h>
#ifndef FUSE_VERSION
 #ifndef FUSE_MAJOR_VERSION
  #define FUSE_VERSION 11
 #else /* FUSE_MAJOR_VERSION */
  #undef FUSE_MAKE_VERSION
  #define FUSE_MAKE_VERSION(maj,min)  ((maj) * 10 + (min))
  #define FUSE_VERSION FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION)
 #endif /* FUSE_MAJOR_VERSION */
#endif /* FUSE_VERSION */


#include <stdlib.h>

extern struct fuse_operations owfs_oper;

#endif
