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

#include "owlib_config.h"
#include "ow.h"

/* ------- Prototypes ----------- */
static int BUS_verify(const unsigned char * const serialnumber) ;

/* ------- Functions ------------ */

// BUS_verify called from BUS_normalverify or BUS_alarmverify
//   tests if device is present in requested mode
//   serialnumber is 1-wire device address (64 bits)
//   return 0 good, 1 bad
int BUS_alarmverify(struct parsedname * const pn) {
    unsigned char ec = 0xEC ;
    int ret ;
    struct device * dev = pn->dev ;
    pn->dev = NULL ;
    ret=BUS_select(pn) ;
    pn->dev = dev ;
    ret || (ret=BUS_send_data( &ec , 1 )) || (ret=BUS_verify(pn->sn)) ;
    return ret ;
}

// BUS_verify called from BUS_normalverify or BUS_alarmverify
//   tests if device is present in requested mode
//   serialnumber is 1-wire device address (64 bits)
//   return 0 good, 1 bad
int BUS_normalverify(struct parsedname * const pn) {
    unsigned char fo = 0xF0 ;
    int ret ;
    struct device * dev = pn->dev ;
    pn->dev = NULL ;
    ret=BUS_select(pn) ;
    pn->dev = dev ;
    ret || (ret=BUS_send_data( &fo , 1 )) || (ret=BUS_verify(pn->sn)) ;
    return ret ;
}

// BUS_verify called from BUS_normalverify or BUS_alarmverify
//   tests if device is present in requested mode
//   serialnumber is 1-wire device address (64 bits)
//   return 0 good, 1 bad
static int BUS_verify(const unsigned char * const serialnumber) {
   unsigned char buffer[24] ;
   int i, goodbits=0 ;
   // set all bits at first
   memset( buffer , 0xFF , 24 ) ;

   // now set or clear apropriate bits for search
   for (i = 0; i < 64; i++) UT_setbit(buffer,3*i+2,UT_getbit(serialnumber,i)) ;

   // send/recieve the transfer buffer
   if ( BUS_sendback_data(buffer,buffer,24) ) return 1 ;
   for ( i=0 ; (i<64) && (goodbits<64) ; i++ )
//   for ( i=0 ; (i<64) && (goodbits<8) ; i++ )
   {
       switch (UT_getbit(buffer,3*i)<<1 | UT_getbit(buffer,3*i+1)) {
       case 0:
           break ;
       case 1:
           if ( ! UT_getbit(serialnumber,i) ) goodbits++ ;
           break ;
       case 2:
           if ( UT_getbit(serialnumber,i) ) goodbits++ ;
           break ;
       case 3: // No device on line
           return 1 ;
       }
   }

      // check to see if there were enough good bits to be successful
      // remember 1 is bad!
   return goodbits<8 ;
}

