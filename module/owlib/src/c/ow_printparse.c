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

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_counters.h"
#include "ow_connection.h"

void _print_owq( struct one_wire_query * owq )
{
    char c[32] ;
    printf("OWQ OneWireQuery structure of %s\n",OWQ_pn(owq).path) ;
    printf("    OneWireQuery size=%lu offset=%lu\n",(unsigned long)OWQ_size(owq),(unsigned long)OWQ_offset(owq)) ;
    Debug_Bytes("OneWireQuery buffer",(BYTE *) OWQ_buffer(owq),OWQ_size(owq)) ;
    printf("    OneWireQuery I=%d U=%u F=%G Y=%d D=%s\n",OWQ_I(owq),OWQ_U(owq),OWQ_F(owq),OWQ_Y(owq),SAFESTRING(ctime_r(&OWQ_D(owq),c))) ;
    printf("--- OneWireQuery done\n") ;
}    