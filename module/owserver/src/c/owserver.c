/*
$Id$
    OW_HTML -- OWFS used for the web
    OW -- One-Wire filesystem

    Written 2004 Paul H Alfille

 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* owserver -- responds to requests over a network socket, and processes them on the 1-wire bus/
         Basic idea: control the 1-wire bus and answer queries over a network socket
         Clients can be owperl, owfs, owhttpd, etc...
         Clients can be local or remote
                 Eventually will also allow bounce servers.

         syntax:
                 owserver
                 -u (usb)
                 -d /dev/ttyS1 (serial)
                 -p tcp port
                 e.g. 3001 or 10.183.180.101:3001 or /tmp/1wire
*/

#include "owserver.h"
#include <pthread.h>

/* --- Prototypes ------------ */
static void Handler( int fd ) ;
static void SizeHandler(struct server_msg *sm , struct client_msg *cm, const struct parsedname * pn ) ;
static void * ReadHandler( struct server_msg *sm, struct client_msg *cm, const struct parsedname *pn ) ;
static void WriteHandler(struct server_msg *sm, struct client_msg *cm, const unsigned char *data, const struct parsedname *pn ) ;
static void DirHandler(struct server_msg *sm, struct client_msg *cm, int fd, const struct parsedname * pn ) ;
static void * FromClientAlloc( int fd, struct server_msg *sm ) ;
static int ToClient( int fd, struct client_msg *cm, const char *data ) ;
static void ow_exit( int e ) ;

static void handle_exit(int unused) {
  (void) unused ;
  ow_exit(0);
}

/* read from client, free return pointer if not Null */
static void * FromClientAlloc( int fd, struct server_msg * sm ) {
    char * msg ;
//printf("FromClientAlloc\n");
    if ( readn(fd, sm, sizeof(struct server_msg) ) != sizeof(struct server_msg) ) {
        sm->type = msg_error ;
        return NULL ;
    }

    sm->payload = ntohl(sm->payload) ;
    sm->size    = ntohl(sm->size)    ;
    sm->type    = ntohl(sm->type)    ;
    sm->sg      = ntohl(sm->sg)      ;
    sm->offset  = ntohl(sm->offset)  ;
//printf("FromClientAlloc payload=%d size=%d type=%d tempscale=%X offset=%d\n",sm->payload,sm->size,sm->type,sm->sg,sm->offset);
//printf("<%.4d|%.4d\n",sm->type,sm->payload);
    if ( sm->payload == 0 ) {
         msg = NULL ;
    } else if ( (sm->payload<0) || (sm->payload>MAXBUFFERSIZE) ) {
        sm->type = msg_error ;
        msg = NULL ;
    } else if ( (msg = (char *)malloc((size_t)sm->payload)) ) {
        if ( readn(fd,msg, (size_t)sm->payload) != sm->payload ) {
            sm->type = msg_error ;
            free(msg);
            msg = NULL ;
        }
    }
    return msg ;
}

static int ToClient( int fd, struct client_msg * cm, const char * data ) {
    int nio = 1 ;
    int ret ;
    struct iovec io[] = {
        { cm, sizeof(struct client_msg), } ,
        { data, cm->payload, } ,
    } ;

    if ( data && cm->payload ) {
        ++nio ;
    }
//printf("ToClient payload=%d size=%d, ret=%d, sg=%X offset=%d payload=%s\n",cm->payload,cm->size,cm->ret,cm->sg,cm->offset,data?data:"");
//printf(">%.4d|%.4d\n",cm->ret,cm->payload);

    cm->payload = htonl(cm->payload) ;
    cm->size = htonl(cm->size) ;
    cm->offset = htonl(cm->offset) ;
    cm->ret = htonl(cm->ret) ;
    cm->sg  = htonl(cm->sg) ;

    ret = writev( fd, io, nio ) != (ssize_t)(io[0].iov_len+io[1].iov_len) ;

    cm->payload = ntohl(cm->payload) ;
    cm->size = ntohl(cm->size) ;
    cm->offset = ntohl(cm->offset) ;
    cm->ret = ntohl(cm->ret) ;
    cm->sg   = ntohl(cm->sg) ;

    return ret ;
}

static void Handler( int fd ) {
    char * retbuffer = NULL ;
    struct server_msg sm ;
    struct client_msg cm = {0,0,0,0,0,0,} ; /* default values */
    char * path = FromClientAlloc( fd, &sm ) ;

    switch( (enum msg_type) sm.type ) {
    case msg_full:
    case msg_size:
    case msg_read:
    case msg_write:
    case msg_dir:
        if ( (path==NULL) || (memchr( path, 0, (size_t)sm.payload)==NULL) ) { /* Bad string -- no trailing null */
            cm.ret = -EBADMSG ;
        } else {
            struct parsedname pn ;
            struct stateinfo si ;
	    memset(&si, 0, sizeof(struct stateinfo));
            pn.si = &si ;
	    //printf("Handler: path=%s\n",path);
            /* Parse the path string */
            if ( (cm.ret=FS_ParsedName( path, &pn )) ) {
	      //printf("Handler: Error parsedname\n");
	      break ;
	    }
            /* Use client persistent settings (temp scale, discplay mode ...) */
            si.sg.int32 = sm.sg ;
            switch( (enum msg_type) sm.type ) {
            case msg_full:
	        //cm.ret = (pn.dev&&pn.ft)?FullFileLength(&pn):0 ;
	        if(pn.dev && pn.ft)
		  SizeHandler( &sm, &cm, &pn ) ;
		else
		  cm.ret = 0;
		//printf("msg_full: cm.ret=%d\n", cm.ret);
                break ;
            case msg_size:
	        //cm.ret = (pn.dev&&pn.ft)?FileLength(&pn):0 ;
	        if(pn.dev && pn.ft)
		  SizeHandler( &sm, &cm, &pn ) ;
		else
		  cm.ret = 0;
		//printf("msg_size: cm.ret=%d\n", cm.ret);
                break ;
            case msg_read:
                retbuffer = ReadHandler( &sm , &cm, &pn ) ;
                break ;
            case msg_write: {
                    int pathlen = strlen( path ) + 1 ; /* include trailing null */
                    int datasize = sm.payload - pathlen ;
                    if ( (datasize<=0) || (datasize<sm.size) ) {
                        cm.ret = -EMSGSIZE ;
                    } else {
                        unsigned char * data = &path[pathlen] ;
                        WriteHandler( &sm, &cm, data, &pn ) ;
                    }
                }
                break ;
            case msg_dir:
                DirHandler( &sm, &cm, fd, &pn ) ;
                break ;
            }
            FS_ParsedName_destroy(&pn) ;
        }
        /* Fall through */
    case msg_nop:
        break ;
    default:
        cm.ret = -EIO ;
        break ;
    }

    if (path) free(path) ;
    ToClient( fd, &cm, retbuffer ) ;
    if ( retbuffer ) free(retbuffer) ;
}

/* Read, called from Handler with the following caveates: */
/* path is path, already parsed, and null terminated */
/* sm has been read, cm has been zeroed */
/* pn is configured */
/* Read, will return: */
/* cm fully constructed */
/* a malloc'ed string, that must be free'd by Handler */
/* The length of string in cm.payload */
/* If cm.payload is 0, then a NULL string is returned */
/* cm.ret is also set to an error <0 or the read length */
static void * ReadHandler(struct server_msg *sm , struct client_msg *cm, const struct parsedname * pn ) {
    char * retbuffer = NULL ;
//printf("ReadHandler:\n");
    cm->payload = FullFileLength(pn) ;
    if ( cm->payload > sm->size ) cm->payload = sm->size ;
    cm->offset = sm->offset ;

    if ( cm->payload <= 0 ) {
        cm->payload = 0 ;
        cm->ret = 0 ;
    } else if ( cm->payload > MAXBUFFERSIZE ) {
        cm->payload = 0 ;
        cm->ret = -EMSGSIZE ;
    } else if ( (retbuffer = (char *)calloc(1, (size_t)cm->payload))==NULL ) {
    /* Allocate return buffer */
//printf("Handler: Allocating buffer size=%d\n",cm.payload);
        cm->payload = 0 ;
        cm->ret = -ENOBUFS ;
    } else if ( (cm->ret = FS_read_postparse(retbuffer,(size_t)cm->payload,(off_t)cm->offset,pn)) <= 0 ) {
        cm->payload = 0 ;
        free(retbuffer) ;
        retbuffer = NULL ;
    } else {
        cm->size = cm->ret ;
//printf("Handler: READ done string = %s\n",retbuffer);
    }
    return retbuffer ;
}

/* Write, called from Handler with the following caveates: */
/* path is path, already parsed, and null terminated */
/* sm has been read, cm has been zeroed */
/* pn is configured */
/* data is what will be written, of sm->size length */
/* Read, will return: */
/* cm fully constructed */
/* cm.ret is also set to an error <0 or the written length */
static void WriteHandler(struct server_msg *sm, struct client_msg *cm, const unsigned char *data, const struct parsedname *pn ) {
    int ret = FS_write_postparse(data,(size_t)sm->size,(off_t)sm->offset,pn) ;
//printf("Handler: WRITE done\n");
    if ( ret<0 ) {
        cm->size = 0 ;
        cm->sg = sm->sg ;
    } else {
        cm->size = ret ;
        cm->sg = pn->si->sg.int32 ;
    }
}

/* Dir, called from Handler with the following caveates: */
/* path is path, already parsed, and null terminated */
/* sm has been read, cm has been zeroed */
/* pn is configured */
/* Read, will return: */
/* cm fully constructed for error message or null marker (end of directory elements */
/* cm.ret is also set to an error or 0 */
static void DirHandler(struct server_msg *sm , struct client_msg *cm, int fd, const struct parsedname * pn ) {
    uint32_t flags ;
    /* nested function -- callback for directory entries */
    /* return the full path length, including current entry */
    void directory( const struct parsedname * const pn2 ) {
        char *retbuffer ;
        size_t _pathlen = strlen(pn2->path) + 1 ;
//printf("DirHandler: preloop\n");
        /* Note, path preloaded into retbuffer */
	// writev() seem to read whole block, so clear it
        if( !(retbuffer = (char *)calloc(1, _pathlen + OW_FULLNAME_MAX + 2)) ) {
	  //printf("malloc error\n");
            return;
        }
        memcpy(retbuffer, pn2->path, _pathlen);
#if 0
	printf("DirHandler: pn=%p pn->in=%p\n", pn, pn->in);
	printf("DirHandler: pn2=%p pn2->in=%p\n", pn2, pn2->in);
	printf("DirHandler: pn2->state=%d pn2->in->busmode=%d\n", pn2->state, pn2->in->busmode);
	printf("DirHandler: pn->state=%d pn->in->busmode=%d\n", pn->state, pn->in->busmode);
        if((pn->state & pn_bus)) {
	  printf("DirHandler: bus was specified to DirHandler, so assume correct path\n");
	} else
#endif
	  {
	  if ( (_pathlen <2) || (retbuffer[_pathlen-2] !='/') ) {
            strcpy( &retbuffer[_pathlen-1] , "/" ) ;
            ++_pathlen ;
	  }
	  //printf("DirHandler: DIR preloop [%s]\n",retbuffer);
	  FS_DirName( &retbuffer[_pathlen-1], OW_FULLNAME_MAX, pn2 ) ;
	  }
        cm->size = strlen(retbuffer) ;
	//printf("DirHandler: loop size=%d [%s]\n",cm->size, retbuffer);
        cm->ret = 0 ;
	ToClient(fd,cm,retbuffer) ;
        free(retbuffer);
    }

    cm->payload = strlen(pn->path) + 1 + OW_FULLNAME_MAX + 2 ;
    cm->sg = sm->sg ;

    //printf("DirHandler: pn->path=[%s] bus=%d\n", pn->path, pn->bus_nr);

    cm->ret = FS_dir_remote( directory, pn, &flags ) ;
    cm->offset = flags ; /* send the flags in the offset message */
    //printf("DirHandler: DIR done ret=%d flags=%d\n", cm->ret, flags);
    /* Now null entry to show end of directy listing */
    cm->payload = cm->size = 0 ;
}

/* Size, called from Handler with the following caveates: */
/* sm has been read, cm has been zeroed */
/* pn is configured */
/* Size, will return: */
/* cm fully constructed for error message or null marker (end of directory elements */
/* cm.ret is also set to an error or 0 */
static void SizeHandler(struct server_msg *sm , struct client_msg *cm, const struct parsedname * pn ) {

    cm->payload = 0 ;
    cm->sg = sm->sg ;

    //printf("SizeHandler: pn->path=[%s] bus=%d\n", pn->path, pn->bus_nr);

    cm->ret = FS_size_remote( pn ) ;
    //printf("Handler: SIZE done ret=%d flags=%ul\n", cm->ret, flags);

    /* Now null entry to show end of directy listing */
    cm->payload = cm->size = 0 ;
}

int main( int argc , char ** argv ) {
    char c ;

    if ( argc>0 ) progname = strdup(argv[0]) ;
    LibSetup() ;

    while ( (c=getopt_long(argc,argv,OWLIB_OPT,owopts_long,NULL)) != -1 ) {
        switch (c) {
        case 'h':
            fprintf(stderr,
            "Usage: %s ttyDevice -p tcpPort [options] \n"
            "   or: %s [options] -d ttyDevice -p tcpPort \n"
            "    -p port   -- tcp port for server process (e.g. 3001)\n" ,
            argv[0],argv[0] ) ;
            break ;
        case 'V':
            fprintf(stderr,
            "%s version:\n\t" VERSION "\n",argv[0] ) ;
            break ;
        default:
            break;
        }
        if ( owopt(c,optarg) ) ow_exit(0) ; /* rest of message */
    }

    /* non-option arguments */
    while ( optind < argc ) {
        OW_ArgGeneric(argv[optind]) ;
        ++optind ;
    }

    if ( outdevices==0 ) {
        fprintf(stderr, "No TCP port specified (-p)\n%s -h for help\n",argv[0]);
        ow_exit(1);
    }

    /*
     * Now we drop privledges and become a daemon.
     */
    if ( LibStart() ) ow_exit(1) ;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, handle_exit);
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);

    ServerProcess( Handler,ow_exit ) ;
    ow_exit(0) ;
    return 0 ;
}

static void ow_exit( int e ) {
    LibClose() ;
    exit( e ) ;
}
