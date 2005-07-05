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

#include "owfs_config.h"
#include "ow_devices.h"

static int FS_dir_seek( void (* dirfunc)(const struct parsedname * const), const struct parsedname * const pn, uint32_t * flags ) ;
static int FS_branchoff( const struct parsedname * const pn) ;
static int FS_devdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) ;
static int FS_alarmdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) ;
static int FS_typedir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) ;
static int FS_realdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2, uint32_t * flags ) ;
static int FS_cache2real( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2, uint32_t * flags  ) ;

/* Calls dirfunc() for each element in directory */
/* void * data is arbitrary user data passed along -- e.g. output file descriptor */
/* pn -- input:
    pn->dev == NULL -- root directory, give list of all devices
    pn->dev non-null, -- device directory, give all properties
    branch aware
    cache aware

   pn -- output (with each call to dirfunc)
    ROOT
    pn->dev set
    pn->sn set appropriately
    pn->ft not set

    DEVICE
    pn->dev and pn->sn still set
    pn->ft loops through
*/
/* path is the path which "pn" parses */
/* FS_dir produces the "invariant" portion of the directory, passing on to
   FS_dir_seek the variable part */
int FS_dir( void (* dirfunc)(const struct parsedname * const), const struct parsedname * const pn ) {
    int ret = 0 ;
    struct parsedname pn2 ;
    uint32_t flags = 0 ;

    if ( pn == NULL || pn->in==NULL ) return -ENODEV ;

    LEVEL_CALL("DIRECTORY path=%s\n",pn->path) ;
    
    STATLOCK;
        AVERAGE_IN(&dir_avg)
        AVERAGE_IN(&all_avg)
    STATUNLOCK;
    FSTATLOCK;
        dir_time = time(NULL) ; // protected by mutex
    FSTATUNLOCK;

    /* Make a copy (shallow) of pn to modify for directory entries */
    memcpy( &pn2, pn , sizeof( struct parsedname ) ) ; /*shallow copy */
    
    if ( pn->dev && (pn->type == pn_real)) { /* device directory */
        /* Device structure is always known for ordinary devices, so don't
        * bother calling ServerDir() */
        //printf("FS_dir: call FS_devdir 1\n");
        ret = FS_devdir( dirfunc, &pn2 ) ;
    } else if ( pn->dev ) { /* device directory */
        /* this one seem to be called when browsing
        * /bus.0/bus.0/system/adapter . Therefor we have to call ServerDir()
        * to find the content */
        if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
            //printf("FS_dir: call ServerDir 2 pn->path=%s\n", pn->path);
            ret = ServerDir(dirfunc, &pn2, &flags) ;
        } else {
            //printf("FS_dir: call FS_devdir 2 pn->path=%s\n", pn->path);
            ret = FS_devdir( dirfunc, &pn2 ) ;
        }
    } else if ( pn->state & pn_alarm ) {  /* root or branch directory -- alarm state */
        ret = FS_dir_seek( dirfunc, &pn2, &flags ) ;
    } else if ( pn->state & pn_uncached ) {  /* root or branch directory -- uncached */
        ret = FS_dir_seek( dirfunc, &pn2, &flags ) ;
    } else if ( pn->type != pn_real ) {  /* stat, sys or set dir */
        /* there are some files with variable sizes, and /system/adapter have variable
        * number of entries and we have to call ServerDir() */
        if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
            //printf("FS_dir: pn->type != pn_real Call ServerDir pn->path=%s\n", pn->path);
            ret = ServerDir(dirfunc, &pn2, &flags) ;
        } else {
            //printf("FS_dir: pn->type != pn_real Call FS_typedir pn->path=%s\n", pn->path);
            ret = FS_typedir( dirfunc, &pn2 ) ;
        }
    } else {
    //printf("FS_dir pid=%ld path=%s call dir_seek\n",pthread_self(), pn->path);
        //printf("FS_dir: pn->si->sg=%X  pn2.pathlength=%d pn2.state=%d pn->state=%X\n", pn->si->sg, pn2.pathlength, pn2.state, pn->state);
    
        if(ShouldReturnBusList(pn)) {
            if ( pn2.pathlength == 0 ) { /* true root */
                if ( !(pn2.state & (pn_bus | pn_alarm | pn_uncached)) ) {
                    struct connection_in *ci ;
    
                    /* restore state */
                    pn2.type = pn_real ;
                    if ( IsLocalCacheEnabled(pn) ) { /* cached */
                        pn2.state = (pn_uncached | (pn->state & pn_text)) ;
                        //printf("state set to %d at uncached\n", pn2.state);
                        dirfunc( &pn2 ) ;
                        /* restore state */
                        pn2.state = pn->state ;
                    }
    
                    ci = indevice ;
                    while(ci) {
                        pn2.state = (pn_bus | (pn->state & pn_text )) ;
                        pn2.bus_nr = ci->index ;
                        dirfunc( &pn2 ) ;
                        ci = ci->next ;
                    }
                    pn2.state = pn->state ;
                    pn2.bus_nr = pn->bus_nr ;
                
                    pn2.state = (pn_normal | (pn->state & pn_text )) ;
                    pn2.type = pn_settings ;
                    dirfunc( &pn2 ) ;
                    pn2.type = pn_system ;
                    dirfunc( &pn2 ) ;
                    pn2.type = pn_statistics ;
                    dirfunc( &pn2 ) ;
                    pn2.type = pn_structure ;
                    dirfunc( &pn2 ) ;
                    
                    pn2.type = pn->type;
                    pn2.state = pn->state;
                }
            }
        }
        
        if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
            //printf("FS_dir: Call ServerDir\n");
            ret = ServerDir(dirfunc, &pn2, &flags) ;
        } else {
            //printf("FS_dir: Call FS_dir_seek\n");
            ret = FS_dir_seek( dirfunc, &pn2, &flags ) ;
        }
    }
    
    if(!(pn->state & pn_alarm)) {
        /* don't show alarm directory in alarm directory */
        /* alarm directory */
        if ( flags & DEV_alarm ) {
                pn2.state = (pn_alarm | (pn->state & pn_text)) ;
                dirfunc( &pn2 ) ;
                pn2.state = pn->state ;
        }
    }
    /* simultaneous directory */
    if ( flags & (DEV_temp|DEV_volt) ) {
        pn2.dev = DeviceSimultaneous ;
        dirfunc( &pn2 ) ;
    }
    STATLOCK;
        AVERAGE_OUT(&dir_avg)
        AVERAGE_OUT(&all_avg)
    STATUNLOCK;
    //printf("FS_dir out ret=%d\n", ret);
    return ret ;
}

/* path is the path which "pn" parses */
/* FS_dir_remote is the entry into FS_dir_seek from ServerDir */
/* More checking is done, and the flags are returned */
int FS_dir_remote( void (* dirfunc)(const struct parsedname * const), const struct parsedname * const pn, uint32_t * flags ) {
    int ret = 0 ;
    struct parsedname pn2 ;

    /* initialize flags */
    flags[0] = 0 ;
    if ( pn == NULL || pn->in==NULL ) return -ENODEV ;
    
    STATLOCK;
        AVERAGE_IN(&dir_avg)
        AVERAGE_IN(&all_avg)
    STATUNLOCK;
    FSTATLOCK;
        dir_time = time(NULL) ; // protected by mutex
    FSTATUNLOCK;
    //printf("FS_dir_remote pid=%ld path=%s\n",pthread_self(), pn->path);

    /* Make a copy (shallow) of pn to modify for directory entries */
    memcpy( &pn2, pn , sizeof( struct parsedname ) ) ; /*shallow copy */

    if ( pn->dev && (pn->type == pn_real)) { /* device directory */
      /* Device structure is always know for ordinary devices, so don't
       * bother calling ServerDir() */
      //printf("FS_dir_remote: call FS_devdir 1 pn->path=%s\n", pn->path);
      ret = FS_devdir( dirfunc, &pn2 ) ;
    }
    else if ( pn->dev ) { /* device directory */
      /* this one seem to be called when browsing
       * /bus.0/bus.0/system/adapter . Therefor we have to call ServerDir()
       * to find the content */
      //printf("FS_dir_remote pid=%ld path=%s pn->dev=%p\n",pthread_self(), pn->path, pn->dev);
      if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
	//printf("FS_dir_remote: pn->type != pn_real Call ServerDir %s\n", pn2.path);
	ret = ServerDir(dirfunc, &pn2, flags) ;
      } else {
	//printf("FS_dir_remote: call FS_devdir 2 pn->path=%s\n", pn->path);
	ret = FS_devdir( dirfunc, &pn2 ) ;
      }
    }
    else if ( pn->state & pn_alarm ) {  /* root or branch directory -- alarm state */
      //printf("FS_dir_remote pid=%ld path=%s call dir_seek alarm\n",pthread_self(), pn2.path);
      ret = FS_dir_seek( dirfunc, &pn2, flags ) ;
    }
    else if ( pn->state & pn_uncached ) {  /* root or branch directory -- uncached */
      //printf("FS_dir_remote pid=%ld path=%s call dir_seek uncached\n",pthread_self(), pn->path);
      ret = FS_dir_seek( dirfunc, &pn2, flags ) ;
    }
    else if ( pn->type != pn_real ) {  /* stat, sys or set dir */
      /* there are some files with variable sizes, and /system/adapter have variable
       * number of entries and we have to call ServerDir() */
      if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
	//printf("FS_dir_remote: pn->type != pn_real Call ServerDir %s\n", pn2.path);
	ret = ServerDir(dirfunc, &pn2, flags) ;
      } else {
	//printf("FS_dir_remote: pn->type != pn_real Call FS_typedir %s\n", pn2.path);
	ret = FS_typedir( dirfunc, &pn2 ) ;
      }
    }
    else {
     //printf("FS_dir_remote pid=%ld path=%s call dir_seek\n",pthread_self(), pn->path);

      if(ShouldReturnBusList(pn)) {
	if ( pn->pathlength == 0 ) { /* true root */
	  if ( !(pn->state & (pn_bus | pn_alarm | pn_uncached)) ) {
	    struct connection_in *ci ;

	    /* restore state */
	    pn2.type = pn_real ;
	    if ( IsLocalCacheEnabled(pn) ) { /* cached */
	      pn2.state = (pn_uncached | (pn->state & pn_text)) ;
	      //printf("2state set to %d at uncached\n", pn2.state);
	      dirfunc( &pn2 ) ;
	      /* restore state */
	      pn2.state = pn->state ;
	    }

	    ci = indevice ;
	    while(ci) {
	      pn2.state = (pn_bus | (pn->state & pn_text )) ;
	      pn2.bus_nr = ci->index ;
	      dirfunc( &pn2 ) ;
	      ci = ci->next ;
	    }
	    pn2.state = pn->state ;
	    pn2.bus_nr = pn->bus_nr ;

	    pn2.state = (pn_normal | (pn->state & pn_text )) ;
	    pn2.type = pn_settings ;
	    dirfunc( &pn2 ) ;
	    pn2.type = pn_system ;
	    dirfunc( &pn2 ) ;
	    pn2.type = pn_statistics ;
	    dirfunc( &pn2 ) ;
	    pn2.type = pn_structure ;
	    dirfunc( &pn2 ) ;
	
	    pn2.type = pn->type;
	    pn2.state = pn->state;
	  }
	}
      }
      /* If the specified listed path is remote, then we have
       * to call ServerDir(). Otherwise call FS_dir_seek to search all
       * local in-devices. */
      if ( (pn2.state & pn_bus) && (get_busmode(pn2.in) == bus_remote) ) {
	//printf("FS_dir_remote: call ServerDir\n");
	ret = ServerDir(dirfunc, &pn2, flags) ;
      } else {
	//printf("FS_dir_remote: call FS_dir_seek\n");
	ret = FS_dir_seek( dirfunc, &pn2, flags ) ;
      }
    }
    STATLOCK;
        AVERAGE_OUT(&dir_avg)
        AVERAGE_OUT(&all_avg)
    STATUNLOCK;
//printf("FS_dir out ret=%d\n", ret);
    return ret ;
}

/* path is the path which "pn" parses */
/* FS_dir_seek produces the data that can vary: device lists, etc. */
static int FS_dir_seek( void (* dirfunc)(const struct parsedname * const), const struct parsedname * const pn, uint32_t * flags ) {
    int ret = 0 ;
#ifdef OW_MT
    pthread_t thread ;
    int threadbad = 1;
    void * v ;
    int rt ;

    /* Embedded function */
    void * Dir2( void * vp ) {
        struct parsedname *pn2 = (struct parsedname *)vp ;
        struct parsedname pnnext ;
        struct stateinfo si ;
        int eret;
        memcpy( &pnnext, pn2 , sizeof(struct parsedname) ) ;
        /* we need a different state (search state) for a different bus -- subtle error */
        si.sg = pn2->si->sg ;   // reuse cacheon, tempscale etc
        pnnext.si = &si ;
        pnnext.in = pn2->in->next ;
        eret = FS_dir_seek(dirfunc,&pnnext,flags) ;
        pthread_exit((void *)eret);
        return (void *)eret;
    }

    if(!(pn->state & pn_bus)) {
      threadbad = pn->in==NULL || pn->in->next==NULL || pthread_create( &thread, NULL, Dir2, (void *)pn ) ;
    }
#endif /* OW_MT */

    /* is this a remote bus? */
    if ( get_busmode(pn->in) == bus_remote ) {
      //printf("FS_dir_seek: Call ServerDir %s\n", pn->path);
        ret = ServerDir(dirfunc,pn,flags) ;
    } else { /* local bus */
        if ( pn->state & pn_alarm ) {  /* root or branch directory -- alarm state */
            //printf("FS_dir_seek: Call FS_alarmdir %s\n", pn->path);
            ret = FS_alarmdir(dirfunc,pn) ;
        } else {
            if ( (pn->state&pn_uncached) || !IsLocalCacheEnabled(pn) || timeout.dir==0 ) {
                //printf("FS_dir_seek: call FS_realdir bus %d\n", pn->in->index);
                 ret = FS_realdir( dirfunc, pn, flags ) ;
            } else {
                //printf("FS_dir_seek: call FS_cache2real bus %d\n", pn->in->index);
                ret = FS_cache2real( dirfunc, pn, flags ) ;
            }
        }
    }
    //printf("FS_dir_seek4 pid=%ld adapter=%d ret=%d\n",pthread_self(), pn->in->index,ret);
#ifdef OW_MT
    /* See if next bus was also queried */
    if ( threadbad == 0 ) { /* was a thread created? */
        if ( pthread_join( thread, &v ) ) {
            return ret ; /* wait for it (or return only this result) */
        }
        rt = (int) v ;
        if ( rt >= 0 ) return rt ; /* is it an error return? Then return this one */
    }
#endif /* OW_MT */
    return ret ;
}

/* Device directory -- all from memory */
static int FS_devdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) {
    struct filetype * lastft = & (pn2->dev->ft[pn2->dev->nft]) ; /* last filetype struct */
    struct filetype * firstft ; /* first filetype struct */
    char s[33] ;
    size_t len ;

    STATLOCK;
        ++dir_dev.calls ;
    STATUNLOCK;
    if ( pn2->subdir ) { /* indevice subdir, name prepends */
//printf("DIR device subdirectory\n");
        strcpy( s , pn2->subdir->name ) ;
        strcat( s , "/" ) ;
        len = strlen(s) ;
        firstft = pn2->subdir  + 1 ;
    } else {
//printf("DIR device directory\n");
        len = 0 ;
        firstft = pn2->dev->ft ;
    }
    for ( pn2->ft=firstft ; pn2->ft < lastft ; ++pn2->ft ) { /* loop through filetypes */
        if ( len ) { /* subdir */
            /* test start of name for directory name */
            if ( strncmp( pn2->ft->name , s , len ) ) break ;
        } else { /* primary device directory */
            if ( strchr( pn2->ft->name, '/' ) ) continue ;
        }
        if ( pn2->ft->ag ) {
            for ( pn2->extension=(pn2->ft->format==ft_bitfield)?-2:-1 ; pn2->extension < pn2->ft->ag->elements ; ++pn2->extension ) {
                dirfunc( pn2 ) ;
                STATLOCK;
                    ++dir_dev.entries ;
                STATUNLOCK;
            }
        } else {
            pn2->extension = 0 ;
            dirfunc( pn2 ) ;
            STATLOCK;
                ++dir_dev.entries ;
            STATUNLOCK;
        }
    }
    return 0 ;
}

/* Note -- alarm directory is smaller, no adapters or stats or uncached */
static int FS_alarmdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) {
    int ret ;
    unsigned char sn[8] ;

    /* STATISCTICS */
    STATLOCK;
        ++dir_main.calls ;
    STATUNLOCK;
//printf("DIR alarm directory\n");

    BUSLOCK(pn2);
    pn2->ft = NULL ; /* just in case not properly set */
    /* Turn off all DS2409s */
    FS_branchoff(pn2) ;
    (ret=BUS_select(pn2)) || (ret=BUS_first_alarm(sn,pn2)) ;
    if(ret) {
      BUSUNLOCK(pn2);
      if(ret == -ENODEV) return 0; /* no more alarms is ok */
      return ret ;
    }
    while (ret==0) {
        char ID[] = "XX";
        STATLOCK;
            ++dir_main.entries ;
        STATUNLOCK;
        memcpy( pn2->sn, sn, 8 ) ;
        /* Search for known 1-wire device -- keyed to device name (family code in HEX) */
        num2string( ID, sn[0] ) ;
        FS_devicefind( ID, pn2 ) ;  // lookup ID and set pn2.dev
        DIRLOCK;
            dirfunc( pn2 ) ;
        DIRUNLOCK;
        pn2->dev = NULL ; /* clear for the rest of directory listing */
        (ret=BUS_select(pn2)) || (ret=BUS_next_alarm(sn,pn2)) ;
//printf("ALARM sn: %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X ret=%d\n",sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7],ret);
    }
    BUSUNLOCK(pn2);
    if(ret == -ENODEV) return 0; /* no more alarms is ok */
    return ret ;
}

static int FS_branchoff( const struct parsedname * const pn ) {
    int ret ;
    unsigned char cmd[] = { 0xCC, 0x66, } ;

    /* Turn off all DS2409s */
    if ( (ret=BUS_reset(pn)) || (ret=BUS_send_data(cmd,2,pn)) ) return ret ;
    return ret ;
}

/* A directory of devices -- either main or branch */
/* not within a device, nor alarm state */
/* Also, adapters and stats handled elsewhere */
/* Scan the directory from the BUS and add to cache */
static int FS_realdir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2, uint32_t * flags ) {
    unsigned char sn[8] ;
    int dindex = 0 ;
    int ret ;

    /* STATISCTICS */
    STATLOCK;
        ++dir_main.calls ;
    STATUNLOCK;

    flags[0] = 0 ; /* start out with no flags set */

    BUSLOCK(pn2);
    
    /* Operate at dev level, not filetype */
    pn2->ft = NULL ;
    /* Turn off all DS2409s */
    FS_branchoff(pn2) ;
    /* it appears that plugging in a new device sends a "presence pulse" that screws up BUS_first */
    /* Actually it's probably stale information in the stateinfo structure */
    (ret=BUS_select(pn2)) || (ret=BUS_first(sn,pn2)) ;
    if(ret) {
      BUSUNLOCK(pn2);
      if(ret == -ENODEV) return 0; /* no more devices is ok */
      return -EIO ;
    }
    while (ret==0) {
        char ID[] = "XX";
#if 0
	{
	  char tmp[17];
	  bytes2string(tmp, sn, 8) ;
	  tmp[16] = 0;
	  printf("FS_realdir: add sn=%s to bus=%d\n", tmp, pn2->in->index);
	}
#endif
	Cache_Add_Dir(sn,dindex,pn2) ;
	++dindex ;
	
	memcpy( pn2->sn, sn, 8 ) ;
	/* Search for known 1-wire device -- keyed to device name (family code in HEX) */
	num2string( ID, sn[0] ) ;
	FS_devicefind( ID, pn2 ) ;  // lookup ID and set pn2.dev
	//printf("DIR adapter=%d, element=%d, sn=%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X\n",pn2->in->index,dindex,pn2->sn[0],pn2->sn[1],pn2->sn[2],pn2->sn[3],pn2->sn[4],pn2->sn[5],pn2->sn[6],pn2->sn[7]);
	
	/* dirfunc() may need to call FS_fstat() and that will make a
	   checkpresence and BUS_lock if bus_nr isn't cached here at
	   once. Deadlock will occour. (owftpd needs it)
	*/
	Cache_Add_Device(pn2->in->index, pn2);
	
	DIRLOCK;
	dirfunc( pn2 ) ;
	flags[0] |= pn2->dev->flags ;
	DIRUNLOCK;
	pn2->dev = NULL ; /* clear for the rest of directory listing */
	(ret=BUS_select(pn2)) || (ret=BUS_next(sn,pn2)) ;
    }
    BUSUNLOCK(pn2);

#ifdef OW_USB
    if(dindex > 0) {
      if((get_busmode(pn2->in) == bus_usb) &&
	 !pn2->in->connin.usb.ds1420_address[0]) {
	/* No DS1420 found on the 1-wire bus, probably a single DS2480 adapter
	 * save last found 1-wire device as a unique identifier. It's perhaps
	 * not correct, but it will only be used if the adapter is disconnected
	 * from the USB-bus.
	 */
#if 1
	char tmp[17];
	bytes2string(tmp, sn, 8);
	tmp[16] = '\000';
	LEVEL_DEFAULT("Set DS9490 [%s] unique id (no DS1420) to %s\n", pn2->in->name, tmp);
#endif
	memcpy(pn2->in->connin.usb.ds1420_address, sn, 8);
      }
    }
#endif

    STATLOCK;
    dir_main.entries += dindex ;
    STATUNLOCK;
    Cache_Del_Dir(dindex,pn2) ;  // end with a null entry
    if(ret == -ENODEV) return 0 ; // no more devices is ok */
    return ret ;
}

void FS_LoadPath( unsigned char * sn, const struct parsedname * const pn ) {
    if ( pn->pathlength==0 ) {
        memset(sn,0,8) ;
    } else {
        memcpy( sn,pn->bp[pn->pathlength-1].sn,7) ;
        sn[7] = pn->bp[pn->pathlength-1].branch ;
    }
}

/* A directory of devices -- either main or branch */
/* not within a device, nor alarm state */
/* Also, adapters and stats handled elsewhere */
/* Cache2Real try the cache first, else can directory from bus (and add to cache) */
static int FS_cache2real( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2, uint32_t * flags ) {
    unsigned char sn[8] ;
    int dindex = 0 ;

    /* Test to see whether we should get the directory "directly" */
    if ( (pn2->state & pn_uncached) || Cache_Get_Dir(sn,0,pn2 ) ) {
        //printf("FS_cache2real: didn't find anything at bus %d\n", pn2->in->index);
        return FS_realdir(dirfunc,pn2,flags) ;
    }
    /* STATISCTICS */
    STATLOCK;
        ++dir_main.calls ;
    STATUNLOCK;

    /* Get directory from the cache */
    /* FIXME: First entry was valid in the cache, but after 1ms the 2nd entry might
     * be old and invalidated. This would result into a incomplete directory listing. */
    do {
        char ID[] = "XX";
        memcpy( pn2->sn, sn, 8 ) ;
        /* Search for known 1-wire device -- keyed to device name (family code in HEX) */
        num2string( ID, sn[0] ) ;
        FS_devicefind( ID, pn2 ) ;  // lookup ID and set pn2.dev
        DIRLOCK;
            dirfunc( pn2 ) ;
            flags[0] |= pn2->dev->flags ;
        DIRUNLOCK;
        pn2->dev = NULL ; /* clear for the rest of directory listing */
    } while ( Cache_Get_Dir( sn, ++dindex, pn2 )==0 ) ;
    STATLOCK;
        dir_main.entries += dindex ;
    STATUNLOCK;
    return 0 ;
}

/* Show the pn->type (statistics, system, ...) entries */
/* Only the top levels, the rest will be shown by FS_devdir */
static int FS_typedir( void (* dirfunc)(const struct parsedname * const), struct parsedname * const pn2 ) {
    void action( const void * t, const VISIT which, const int depth ) {
        (void) depth ;
//printf("Action\n") ;
        switch(which) {
        case leaf:
        case postorder:
            pn2->dev = ((const struct device_opaque *)t)->key ;
            dirfunc( pn2 ) ;
        default:
            break ;
        }
    } ;
    twalk( Tree[pn2->type],action) ;
    pn2->dev = NULL ;
    return 0 ;
}
