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

#include <linux/limits.h>
#include <termios.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h> /* for gettimeofday */

#include "owlib_config.h"
#include "ow.h"

/* Global variables */
char devport[PATH_MAX] ; /* Device name (COM port) */
int devfd = -1; /*file descriptor for serial port*/
static struct timeval tv ; /* statistics */

/* ---------------------------------------------- */
/* raw COM port interface routines                */
/* ---------------------------------------------- */
//
//open serial port
// set global devfd and devport
/* return 0=good
 *        -errno = cannot opon
 *        -EFAULT = already open
 */
int COM_open( const char * port ) {
    struct termios newSerialTio; /*new serial port settings*/
    /* return 0 for success, 1 for failure */
    if ( devfd>=0 ) {
        syslog( LOG_NOTICE, "Only one port should be specified, try '-h' for help.\n" ) ;
    } else {
        strncpy( devport, port, PATH_MAX ) ;
        if ((devfd = open(devport, O_RDWR | O_NONBLOCK)) < 0) {
		    int ret = errno ;
            syslog( LOG_ERR,"Cannot open port: %s error=%d\n",devport,errno) ;
            return -ret ;
        }
        tcgetattr(devfd, &oldSerialTio);
        tcgetattr(devfd, &newSerialTio);

        speed = B9600 ;
        cfsetospeed(&newSerialTio, speed);
        cfsetispeed(&newSerialTio, speed);

        // Set to non-canonical mode, and no RTS/CTS handshaking
        newSerialTio.c_iflag = IGNBRK|IGNPAR|IXANY;
        newSerialTio.c_oflag &= ~(OPOST);
        newSerialTio.c_cflag = CLOCAL|CS8|CREAD;
        newSerialTio.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|IEXTEN|ISIG);
        newSerialTio.c_cc[VMIN] = 0;
        newSerialTio.c_cc[VTIME] = 3;

        tcsetattr(devfd, TCSAFLUSH, &newSerialTio);

//        fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL, 0) & ~O_NONBLOCK);
        return 0 ;
    }
    return -EFAULT ;
}

void COM_close( void ) {
    // restore tty settings
    if ( devfd > -1 ) {
        tcsetattr(devfd, TCSAFLUSH, &oldSerialTio);
        COM_flush();
        close(devfd);
        devfd=-1 ;
    }
}

void COM_flush( void ) {
    tcflush(devfd, TCIOFLUSH);
}

void COM_break( void ) {
    tcsendbreak(devfd, 0);
}

void COM_speed(speed_t new_baud) {
     struct termios t;

     // read the attribute structure
     if (tcgetattr(devfd, &t) < 0) return;

     // set baud in structure
     cfsetospeed(&t, new_baud);
     cfsetispeed(&t, new_baud);

     // change baud on port
     if (tcsetattr(devfd, TCSAFLUSH, &t) < 0) {
         if ( new_baud != B9600 ) COM_speed(B9600) ;
         return ;
     }
     speed = new_baud ;
     return ;
}

/* Lock the COM port for a complete conversation */
void BUS_lock( void ) {
    ++ bus_locks ; /* statistics */
    flock( devfd, LOCK_EX) ;
	gettimeofday( &tv , NULL ) ; /* for statistics */
}

/* Lock the COM port for a complete conversation */
void BUS_unlock( void ) {
    struct timeval tv2 ;
	gettimeofday( &tv2, NULL ) ;
	bus_time.tv_sec += tv2.tv_sec - tv.tv_sec ;
	bus_time.tv_usec += tv2.tv_usec - tv.tv_usec ;
	if ( bus_time.tv_usec > 100000000 ) {
        bus_time.tv_usec -= 100000000 ;
        bus_time.tv_sec  += 100 ;
	} else if ( bus_time.tv_usec < -100000000 ) {
        bus_time.tv_usec += 100000000 ;
        bus_time.tv_sec  -= 100 ;
	}
    flock( devfd, LOCK_UN) ;
}
