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

/* locks are to handle multithreading */

#include "owfs_config.h"
#include "ow.h"
#include <sys/time.h> /* for gettimeofday */
/* ------- Globals ----------- */

#ifdef OW_MT
pthread_mutex_t busstat_mutex = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t dev_mutex     = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t stat_mutex    = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t cache_mutex   = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t store_mutex   = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t fstat_mutex   = PTHREAD_MUTEX_INITIALIZER ;
pthread_mutex_t dir_mutex     = PTHREAD_MUTEX_INITIALIZER ;
#ifdef __UCLIBC__
/* vsnprintf() doesn't seem to be thread-safe in uClibc
   even if thread-support is enabled. */
pthread_mutex_t uclibc_mutex = PTHREAD_MUTEX_INITIALIZER ;
#endif
#define DEVLOCK      pthread_mutex_lock(&dev_mutex) ;
#define DEVUNLOCK    pthread_mutex_unlock(&dev_mutex) ;
#define DEVLOCKS    10
sem_t devlocks ;
struct devlock {
    unsigned char sn[8] ;
    struct connection_in * in ;
    pthread_mutex_t lock ;
    int users ;
 } DevLock[DEVLOCKS] = { [0 ... DEVLOCKS-1]={"",NULL,PTHREAD_MUTEX_INITIALIZER,0}};
#define SLOTLOCK(slot)      pthread_mutex_lock(&DevLock[slot].lock) ;
#define SLOTUNLOCK(slot)      pthread_mutex_unlock(&DevLock[slot].lock) ;
#endif /* OW_MT */

/* maxslots and multithreading are ints to allow address-of */
#ifdef DEVLOCKS
    int multithreading = 1 ;
    int maxslots = DEVLOCKS ;
#else /* DEVLOCKS */
    int multithreading = 0 ;
    int maxslots = 1 ;
#endif /* DEVLOCKS */

/* Essentially sets up semaphore for device slots */
void LockSetup( void ) {
#ifdef OW_MT
#ifdef __UCLIBC__
    int i ;
    pthread_mutex_init(&busstat_mutex, pmattr);
    pthread_mutex_init(&dev_mutex, pmattr);
    pthread_mutex_init(&stat_mutex, pmattr);
    pthread_mutex_init(&cache_mutex, pmattr);
    pthread_mutex_init(&store_mutex, pmattr);
    pthread_mutex_init(&fstat_mutex, pmattr);
    pthread_mutex_init(&uclibc_mutex, pmattr);
    pthread_mutex_init(&dir_mutex, pmattr);
    for ( i=0 ; i<DEVLOCKS ; ++i) {
        pthread_mutex_init( &DevLock[i].lock, pmattr ) ;
    }
#endif
    sem_init( &devlocks, 0, DEVLOCKS ) ;
#endif /* OW_MT */
}

/* Grabs a device slot, either one already matching, or an empty one */
void LockGet( const struct parsedname * const pn ) {
#ifdef OW_MT
    int i ; /* counter through slots */
    int empty = 0;
    /* Exclude requests that don't need locking */
    /* Basically directories, subdirectories, static and statistic data, and atomic items */
    pn->si->lock = -1 ; /* No slot owned, yet */
    switch( pn->ft->format ) {
    case ft_directory:
    case ft_subdir:
        return ;
    default:
        break ;
    }
    switch( pn->ft->change ) {
    case ft_static:
    case ft_Astable:
    case ft_Avolatile:
    case ft_statistic:
        return ;
    default:
        break ;
    }
//{ int e; sem_getvalue( &devlocks, &e);
//printf("LOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X pre slots=%d\n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],e);
//}
    /* potentially need an empty slot */
    sem_wait( &devlocks ) ;
    /* guaranteed to have a slot available at this point */
//{ int e; sem_getvalue( &devlocks, &e);
//printf("LOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X post slots=%d\n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],e);
//}

    /* Lock the slots table */
    DEVLOCK
    for ( i=0 ; i<DEVLOCKS ; ++i ) {
        if ( DevLock[i].users == 0 ) {
            empty = i ;
        } else if ( DevLock[i].in==pn->in && memcmp(DevLock[i].sn, pn->sn, 8)==0 ) {
            /* found matching slot */
            /* release potential slot */
            sem_post( &devlocks ) ;
            /* Add self to number of users */
            ++DevLock[i].users ;
            /* set my slot */
            pn->si->lock = i ; /* slot owned */
//printf("LOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X FOUND match=%d users=%d\n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],pn->si->lock,DevLock[i].users) ;
            /* Release table -- since I'm a 'user' the slot can't be emptied */
            DEVUNLOCK
            /* Now wait on just this slot */
            SLOTLOCK(i) ;
            return ;
        }
    }
    /* Fall though with 'empty' being the empty slot and the table locked */
    /* claim this slot */
    memcpy(DevLock[empty].sn, pn->sn, 8) ;
    DevLock[empty].in = pn->in ;
    DevLock[empty].users = 1 ;
    pn->si->lock = empty ;
//printf("LOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X NOT FOUND match=%d users=%d\n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],pn->si->lock,DevLock[empty].users) ;
    /* free table -- this slot is properly claimed */
    DEVUNLOCK
    /* Now wait on slot */
    SLOTLOCK(empty) ;
#endif /* OW_MT */
}

void LockRelease( const struct parsedname * const pn ) {
#ifdef OW_MT
    int lock = pn->si->lock ;
    /* No slot for this device */
    if ( lock < 0 ) return ;
//printf("UNLOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X PRE slot=%d users=%d \n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],lock,DevLock[lock].users) ;
    /* Lock the table before manipulating slots */
    DEVLOCK
        SLOTUNLOCK(lock)
        if ( (--DevLock[lock].users) == 0 ) sem_post( &devlocks ) ;
//printf("UNLOCK %.2X.%.2X%.2X%.2X%.2X%.2X%.2X POST slot=%d users=%d \n",pn->sn[0],pn->sn[1],pn->sn[2],pn->sn[3],pn->sn[4],pn->sn[5],pn->sn[6],lock,DevLock[lock].users) ;
    DEVUNLOCK
#endif /* OW_MT */
}

/* Special note on locking:
     The bus lock is universal -- only one thread can hold it
     Therefore, we don't need a STATLOCK for bus_locks and bus_unlocks or bus_time
     Actually, the new design has several busses, so a separate lock
*/

void BUS_lock( const struct parsedname * pn ) {
#ifdef OW_MT
    pthread_mutex_lock( &(pn->in->bus_mutex) ) ;
#endif /* OW_MT */
    gettimeofday( &(pn->in->last_lock) , NULL ) ; /* for statistics */

    /* Has anyone else seen problems with failing BUS_select? and does
     * this code help anyone?  It makes some difference for me,
     * especially with I'm polling the buttons on the LCD-display
     * 5 times per second.
     */
#if 0
    /* I have noticed that BUS_select() sometimes fail when the
     * 1-wire bus is _very_ busy.
     * One easy solution would be to make a small delay after a
     * process has unlocked the bus (let the bus be idle),
     * and a another waiting process is locking it (and usually
     * begin sending BUS_select()).
     */
  #if 1
    /* Only wait 5ms if needed... This seems to be overkill to test..
     * Why not always wait?
     */
    {
      struct timeval time_diff;
      int ms;
      // first time last_unlock is uninitialized, but I don't care...
      time_diff.tv_sec = (pn->in->last_lock.tv_sec - pn->in->last_unlock.tv_sec) ;
      time_diff.tv_usec = (pn->in->last_lock.tv_usec - pn->in->last_unlock.tv_usec) ;
      if ( time_diff.tv_usec >= 1000000 ) {
	time_diff.tv_usec -= 1000000 ;
	++time_diff.tv_sec;
      } else if ( time_diff.tv_usec < 0 ) {
	time_diff.tv_usec += 1000000 ;
	--time_diff.tv_sec;
      }
      ms = 5;
      if(pn->dev != NULL) {
	if(pn->type == pn_real) {
	  if(pn->sn[0] == 0xFF) {
	    ms = 20;  // extra long wait for LCD
	  }
	}
      }
      if(!time_diff.tv_sec && (time_diff.tv_usec < ms*1000))
	UT_delay(ms - (time_diff.tv_usec/1000));
    }
  #else
    /* Always wait 5ms */
    UT_delay(5) ;
  #endif

#endif

    BUSSTATLOCK
        ++ bus_locks ; /* statistics */
    BUSSTATUNLOCK
}

void BUS_unlock( const struct parsedname * pn ) {
    gettimeofday( &(pn->in->last_unlock), NULL ) ;

    /* avoid update if system-clock have changed */
    BUSSTATLOCK
      if((pn->in->last_unlock.tv_sec >= pn->in->last_lock.tv_sec) &&
	 ((pn->in->last_unlock.tv_sec-pn->in->last_lock.tv_sec) < 60)) {
	bus_time.tv_sec += (pn->in->last_unlock.tv_sec - pn->in->last_lock.tv_sec) ;
        bus_time.tv_usec += (pn->in->last_unlock.tv_usec - pn->in->last_lock.tv_usec) ;
        if ( bus_time.tv_usec >= 1000000 ) {
	  bus_time.tv_usec -= 1000000 ;
	  ++bus_time.tv_sec;
        } else if ( bus_time.tv_usec < 0 ) {
	  bus_time.tv_usec += 1000000 ;
	  --bus_time.tv_sec;
        }
      }
      ++ bus_unlocks ; /* statistics */
    BUSSTATUNLOCK
#ifdef OW_MT
    pthread_mutex_unlock( &(pn->in->bus_mutex) ) ;
#endif /* OW_MT */
}
