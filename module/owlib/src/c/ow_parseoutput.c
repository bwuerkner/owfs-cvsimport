/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
$ID: $
*/

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_counters.h"
#include "ow_connection.h"

/* ------- Prototypes ----------- */
static int FS_output_integer(struct one_wire_query * owq) ;
static int FS_output_unsigned(struct one_wire_query * owq) ;
static int FS_output_float(struct one_wire_query * owq) ;
static int FS_output_date(struct one_wire_query * owq) ;
static int FS_output_yesno(struct one_wire_query * owq) ;
static int FS_output_offset_and_size(char * string, size_t length, struct one_wire_query * owq) ;
static FS_output_array_with_commas( struct one_wire_query * owq  ;
static FS_output_array_no_commas( struct one_wire_query * owq  ;

/*
Change in strategy 6/2006:
Now use CheckPresence as primary method of finding correct bus
Can break down cases into:
1. bad ParsedName -- no read possible
2. structure -- read from 1st bus (local)
3. specified bus (picked up in ParsedName) -- use that
4. statistics, settings, Simultaneous, Thermostat -- use first or specified
5. real -- use caced, if error, delete cache entry and try twice more.
*/

/* ---------------------------------------------- */
/* Filesystem callback functions                  */
/* ---------------------------------------------- */

int FS_output_owq( struct one_wire_query * owq)
{
    switch (OWQ_pn(owq).extension) {
        case -2:
            return FS_output_unsigned(owq) ;
        case -1:
            switch (OWQ_pn(owq).ft->format) {
                case ft_binary:
                    return FS_output_array_no_commas(owq) ;
                default:
                    return FS_output_array_with_commas(owq) ;
            }
        default:
            switch (OWQ_pn(owq).ft->format) {
                case ft_integer:
                    return FS_output_integer(owq) ;
                case ft_yesno:
                case ft_bitfield:
                    return FS_output_yesno(owq) ;
                case ft_unsigned:
                    return FS_output_unsigned(owq) ;
                case ft_temperature:
                case ft_tempgap:
                case ft_float:
                    return FS_output_float(owq) ;
                case ft_date:
                    return FS_output_date(owq) ;
                case ft_vascii:
                case ft_ascii:
                    return FS_output_integer(owq) ;
                case ft_binary:
                    return FS_output_integer(owq) ;
            }
    }
}


static int FS_output_integer(struct one_wire_query * owq)
{
	/* should only need suglen+1, but uClibc's snprintf()
	   seem to trash 'len' if not increased */
	int len;
    char c[PROPERTY_LENGTH_INTEGER+2] ;

	UCLIBCLOCK;
	len = snprintf(c, PROPERTY_LENGTH_INTEGER + 1, "%*d", PROPERTY_LENGTH_INTEGER, OWQ_I(owq));
	UCLIBCUNLOCK;
    if ((len < 0) || ((size_t) len > PROPERTY_LENGTH_INTEGER)) {
        return -EMSGSIZE;
    }
	return FS_output_offset_and_size(c,PROPERTY_LENGTH_INTEGER,owq) ;
}

static int FS_output_unsigned(struct one_wire_query * owq)
{
    /* should only need suglen+1, but uClibc's snprintf()
       seem to trash 'len' if not increased */
    int len;
    char c[PROPERTY_LENGTH_UNSIGNED+2] ;

    UCLIBCLOCK;
    len = snprintf(c, PROPERTY_LENGTH_UNSIGNED + 1, "%*u", PROPERTY_LENGTH_UNSIGNED, OWQ_U(owq));
    UCLIBCUNLOCK;
    if ((len < 0) || ((size_t) len > PROPERTY_LENGTH_UNSIGNED)) {
        return -EMSGSIZE;
    }
    return FS_output_offset_and_size(c,PROPERTY_LENGTH_UNSIGNED,owq) ;
}

static int FS_output_float(struct one_wire_query * owq)
{
    /* should only need suglen+1, but uClibc's snprintf()
       seem to trash 'len' if not increased */
    int len;
    char c[PROPERTY_LENGTH_FLOAT+2] ;
    _FLOAT F ;

    switch (OWQ_pn(owq).ft->format) {
        case ft_temperature:
            F = Temperature(OW_F(owq),&OW_pn(owq)) ;
            break ;
        case ft_tempgap:
            F = TemperatureGap(OW_F(owq),&OW_pn(owq)) ;
            break ;
        default:
            F = OW_F(owq) ;
            break ;
    }

    UCLIBCLOCK;
    len = snprintf(c, PROPERTY_LENGTH_FLOAT + 1, "%*G", PROPERTY_LENGTH_FLOAT, F);
    UCLIBCUNLOCK;
    if ((len < 0) || ((size_t) len > PROPERTY_LENGTH_FLOAT)) {
        return -EMSGSIZE;
    }
    return FS_output_offset_and_size(c,PROPERTY_LENGTH_FLOAT,owq) ;
}

static int FS_output_date(struct one_wire_query * owq)
{
    char c[PROPERTY_LENTH_DATE+2];
    if (OWQ_size(owq) < PROPERTY_LENGTH_DATE)
        return -EMSGSIZE;
    ctime_r(&OWQ_D(owq), c);
    return FS_output_offset_and_size(c,PROPERTY_LENGTH_DATE,owq) ;
}

static int FS_output_yesno(struct one_wire_query * owq)
{
    if (OWQ_size(owq) < PROPERTY_LENGTH_YEDSNO)
        return -EMSGSIZE;
    OWQ_buffer(owq)[0] = ((OWQ_Y(owq)&0x1)==0) ? '0' : '1' ;
    return PROPERTY_LENGTH_YESNO;
}

static int FS_output_offset_and_size(char * string, size_t length, struct one_wire_query * owq)
{
    size_t copy_length = length ;
    off_t offset = OWQ_offset(owq) ;
    if (offset>length) return -EFAULT ; 
    copy_length -= offset ;
    if ( copy_length > OWQ_size(owq) ) copy_length = OWQ_size(owq) ;
    memcpy(OWQ_buffer(owq), &string[offset], copy_length) ;
    return copy_length ;
}

int FS_output_ascii(struct one_wire_query * owq)
{
    return FS_output_offset_and_size(OWQ_mem(owq),OWQ_length(owq),owq) ;
}

/* If the string is properly terminated, we can use a simpler routine */
int FS_output_ascii_z(size_t suglen, struct one_wire_query * owq)
{
	return FS_output_ascii(buf, size, offset, answer, strlen(answer));
}

static FS_output_array_with_commas( struct one_wire_query * owq )
{
    struct one_wire_query owq_single ;
    size_t extension ;
    int len ;
    size_t used_size = 0 ;
    size_t remaining_size = OWQ_size(owq) ;

    if (OWQ_offset(owq)>0) return -EFAULT ;

    for ( extension = 0 ; extension < OWQ_pn(owq).extension ; ++extension ) {
        memcpy( &owq_single, owq, sizeof(owq_single) ) ;
        OWQ_pn(&owq_single).extension = extension ;
        memcpy(OWQ_val(owq_single),OWQ_array(owq)[extension],sizeof(union value_object)) ;
        if ( used_size > 0 ) {
            if ( remaining_size == 0 ) return -EFAULT ;
            OWQ_buffer(owq)[used_size] = ',' ;
            ++used_size ;
            --remaining_size ;
        }
        OWQ_buffer(&owq_single) = &OWQ_buffer(owq)[used_size] ;
        OWQ_size(&owq_single) = remaining_size ;
        len = FS_output_owq(&owq_single) ;
        if ( len < 0 ) return len ;
        remaining_size -= len ;
        used_size += len ;
    }
    return used_size ;
}

static FS_output_array_no_commas( struct one_wire_query * owq )
{
    struct one_wire_query owq_single ;
    size_t extension ;
    int len ;
    size_t used_size = 0 ;
    size_t remaining_size = OWQ_size(owq) ;

    if (OWQ_offset(owq)>0) return -EFAULT ;

    for ( extension = 0 ; extension < OWQ_pn(owq).extension ; ++extension ) {
        memcpy( &owq_single, owq, sizeof(owq_single) ) ;
        OWQ_pn(&owq_single).extension = extension ;
        memcpy(OWQ_val(owq_single),OWQ_array(owq)[extension],sizeof(union value_object)) ;
        OWQ_buffer(&owq_single) = &OWQ_buffer(owq)[used_size] ;
        OWQ_size(&owq_single) = remaining_size ;
        len = FS_output_owq(&owq_single) ;
        if ( len < 0 ) return len ;
        remaining_size -= len ;
        used_size += len ;
    }
    return used_size ;
}
