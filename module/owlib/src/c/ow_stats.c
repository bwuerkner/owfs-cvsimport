/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
	email: palfille@earthlink.net
	Released under the GPL
	See the header file: ow.h for full attribution
	1wire/iButton system from Dallas Semiconductor
*/

/* General Device File format:
    This device file corresponds to a specific 1wire/iButton chip type
	( or a closely related family of chips )

	The connection to the larger program is through the "device" data structure,
	  which must be declared in the acompanying header file.

	The device structure holds the
	  family code,
	  name,
	  device type (chip, interface or pseudo)
	  number of properties,
	  list of property structures, called "filetype".

	Each filetype structure holds the
	  name,
	  estimated length (in bytes),
	  aggregate structure pointer,
	  data format,
	  read function,
	  write funtion,
	  generic data pointer

	The aggregate structure, is present for properties that several members
	(e.g. pages of memory or entries in a temperature log. It holds:
	  number of elements
	  whether the members are lettered or numbered
	  whether the elements are stored together and split, or separately and joined
*/

/* Stats are a pseudo-device -- they are a file-system entry and handled as such,
     but have a different caching type to distiguish their handling */

#include "owfs_config.h"
#include "ow_stats.h"

/* ------- Prototypes ----------- */
/* Statistics reporting */
 uREAD_FUNCTION( FS_stat ) ;
 uREAD_FUNCTION( FS_time ) ;
 uREAD_FUNCTION( FS_elapsed ) ;

/* -------- Structures ---------- */
struct filetype stats_cache[] = {
    {"tries"      ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_tries  ,  } ,
    {"hits"       ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_hits   ,  } ,
    {"misses"     ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_misses ,  } ,
    {"flips"      ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_flips  ,  } ,
    {"additions"  ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_adds   ,  } ,
    {"deletions"  ,  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_dels   ,  } ,
    {"expirations",  15, NULL, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & cache_expired,  } ,
}
 ;
struct device d_stats_cache = { "stats_cache", "stats_cache", dev_statistic, NFT(stats_cache), stats_cache } ;

struct aggregate Aread = { 3 , ag_numbers, ag_separate, } ;
struct filetype stats_read[] = {
    {"calls"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_calls            ,  } ,
    {"cachesuccess"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_cache            ,  } ,
    {"cachebytes"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_cachebytes       ,  } ,
    {"arraytries"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_array            ,  } ,
    {"arraysuccess"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_arraysuccess     ,  } ,
    {"aggregatetries"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_aggregate        ,  } ,
    {"aggregatesuccess", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_aggregatesuccess ,  } ,
    {"success"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_success          ,  } ,
    {"bytes"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_bytes            ,  } ,
    {"tries"           , 15, &Aread, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL},   read_tries            ,  } ,
}
 ;
struct device d_stats_read = { "stats_read", "stats_read", dev_statistic, NFT(stats_read), stats_read } ;

struct filetype stats_write[] = {
    {"calls"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_calls            ,  } ,
    {"arraytries"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_array            ,  } ,
    {"arraysuccess"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_arraysuccess     ,  } ,
    {"aggregatetries"  , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_aggregate        ,  } ,
    {"aggregatesuccess", 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_aggregatesuccess ,  } ,
    {"success"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_success          ,  } ,
    {"bytes"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & write_bytes            ,  } ,
    {"tries"           , 15, &Aread, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL},   write_tries            ,  } ,
}
 ;
struct device d_stats_write = { "stats_write", "stats_write", dev_statistic, NFT(stats_write), stats_write } ;

struct filetype stats_directory[] = {
    {"maxdepth"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_depth   ,  } ,
    {"success"         , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & dir_success ,  } ,
    {"tries"           , 15, &Aread, ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL},   dir_tries   ,  } ,
}
 ;
struct device d_stats_directory = { "stats_directory", "stats_directory", dev_statistic, NFT(stats_directory), stats_directory } ;

struct filetype stats_bus[] = {
    {"bus_time"        , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_time}, {v:NULL}, & bus_time     ,  } ,
    {"pause_time"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_time}, {v:NULL}, & bus_pause    ,  } ,
    {"elapsed_time"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_elapsed}, {v:NULL}, NULL        ,  } ,
    {"locks"           , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & bus_locks    ,  } ,
    {"CRC8_tries"      , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc8_tries   ,  } ,
    {"CRC8_errors"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc8_errors  ,  } ,
    {"CRC16_tries"     , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc16_tries  ,  } ,
    {"CRC16_errors"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & crc16_errors ,  } ,
    {"read_timeout"    , 15, NULL  , ft_unsigned, ft_statistic, {u:FS_stat}, {v:NULL}, & read_timeout ,  } ,
}
 ;
struct device d_stats_bus = { "stats_bus", "stats_bus", dev_statistic, NFT(stats_bus), stats_bus } ;


/* ------- Functions ------------ */

static int FS_stat(unsigned int * u , const struct parsedname * pn) {
    int dindex = pn->extension ;
	if (dindex<0) dindex = 0 ;
    if (pn->ft->data == NULL) return -ENOENT ;
	*u =  ((unsigned int *)pn->ft->data)[dindex] ;
	return 0 ;
}

static int FS_time(unsigned int * u , const struct parsedname * pn) {
    int dindex = pn->extension ;
	struct timeval * tv = (struct timeval *) pn->ft->data ;
	long ul ; /* seconds and microseconds */
	if (dindex<0) dindex = 0 ;
    if (tv == NULL) return -ENOENT ;

//printf("TIME1 sec=%ld usec=%ld\n",tv->tv_sec,tv->tv_usec) ;
    BUS_lock() ; /* to prevent simultaneous changes to bus timing variables */
		ul = tv[dindex].tv_usec ;
		tv[dindex].tv_usec = ul % 1000000 ;
		tv[dindex].tv_sec += ul / 1000000 ;
	//printf("TIME2 sec=%ld usec=%ld\n",tv->tv_sec,tv->tv_usec) ;
		if ( ul<0 ) {
			tv[dindex].tv_usec += 1000000 ;
			--tv[dindex].tv_sec ;
		}
		ul = tv[dindex].tv_sec ; /* now can unlock */
		--bus_locks ; /* statistics */ /* don't include this lock -- only to block statistics variables */
	BUS_unlock() ;
//printf("TIME3 sec=%ld usec=%ld\n",tv->tv_sec,tv->tv_usec) ;
    *u = ul ;
	return 0 ;
}

static int FS_elapsed(unsigned int * u , const struct parsedname * pn) {
//printf("ELAPSE start=%u, now=%u, diff=%u\n",start_time,time(NULL),time(NULL)-start_time) ;
    (void) pn ;
    *u = time(NULL)-start_time ;
	return 0 ;
}
