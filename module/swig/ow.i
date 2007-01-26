/* File: ow.i */
/* $Id$ */

%module OW

%include "typemaps.i"

%init %{
#include "../../../src/include/config.h"
#include "owfs_config.h"
#include "ow.h"
LibSetup(opt_swig) ;
%}

%{
#include "../../../src/include/config.h"
#include "owfs_config.h"
#include "ow.h"

#if OW_MT
    pthread_t main_threadid ;
    #define IS_MAINTHREAD (main_threadid == pthread_self())
#else /* OW_MT */
    #define IS_MAINTHREAD 1
#endif /* OW_MT */

char *version( ) {
    return VERSION;
}


int init( const char * dev ) {
    int ret = 1 ; /* Good initialization */
    //    LibSetup(opt_swig) ; /* Done in %init section */
    if ( OWLIB_can_init_start() || owopt_packed(dev) ) {
        ret = 0 ; // error
    } else {
        LibStart() ;
    }
    OWLIB_can_init_end() ;
    return ret ; 
}

int put( const char * path, const char * value ) {
    int s ;
    int ret = 1 ; /* good result */
    /* Overall flag for valid setup */
    if ( value==NULL) return 0 ;
    if ( OWLIB_can_access_start() ) {
        ret = 0 ;
    } else {
        s = strlen(value) ;
        if ( FS_write(path,value,s,0) < 0  ) ret = 0 ;
    }
    OWLIB_can_access_end() ;
    return ret ;
}

static void getdircallback( void * v, const struct parsedname * const pn2 ) {
    struct charblob * cb = v ;
    char buf[OW_FULLNAME_MAX+2] ;
    FS_DirName( buf, OW_FULLNAME_MAX, pn2 ) ;
    CharblobAdd( buf, strlen(buf), cb ) ;
    if ( IsDir(pn2) ) CharblobAddChar( '/', cb ) ;
}
/*
  Get a directory,  returning a copy of the contents in *buffer (which must be free-ed elsewhere)
  return length of string, or <0 for error
  *buffer will be returned as NULL on error
 */
static void getdir( char ** buffer, const struct parsedname * pn ) {
    struct charblob cb ;
    CharblobInit( &cb ) ;
    if ( FS_dir( getdircallback, &cb, pn ) >= 0 ) {
        *buffer = strdup( cb.blob ) ;
    }
    CharblobClear( &cb ) ;
}

/*
  Get a value,  returning a copy of the contents in *buffer (which must be free-ed elsewhere)
  return length of string, or <0 for error
  *buffer will be returned as NULL on error
 */
static void getval( char ** buffer, const struct parsedname * pn ) {
    ssize_t s ;
    s = FullFileLength(pn) ;
    if ( s <= 0 ) return ;
    if ( (*buffer = malloc(s+1))==NULL ) return ;
    if ( s = FS_read_postparse( *buffer, s, 0, pn ) < 0 ) {
        free(*buffer) ;
        *buffer = NULL ;
    }
    buffer[s] = '\0' ; // shorten to actual returned length
}

char * get( const char * path ) {
    struct parsedname pn ;
    char * buf = NULL ;

    /* Check the parameters */
    if ( path==NULL ) path="/" ;

    if ( strlen(path) > PATH_MAX ) {
        // buf = NULL ;
    } else if ( OWLIB_can_access_start() ) { /* Check for prior init */
        // buf = NULL ;
    } else if ( FS_ParsedName( path, &pn ) ) { /* Can we parse the input string */
        // buf = NULL ;
    } else {
        if ( IsDir( &pn ) ) { /* A directory of some kind */
            getdir( &buf, &pn ) ;
        } else { /* A regular file */
            getval( &buf, &pn ) ;
        }
        FS_ParsedName_destroy(&pn) ;
    }
    OWLIB_can_access_end() ;
    return buf ;
}

void finish( void ) {
    OWLIB_can_finish_start() ;
    LibClose() ;
    OWLIB_can_finish_end() ;
}

void set_error_print(int val) {
    Global.error_print = val;
}

int get_error_print(void) {
    return Global.error_print;
}

void set_error_level(int val) {
    Global.error_level = val;
}

int get_error_level(void) {
    return Global.error_level;
}

%}
%typemap(newfree) char * { if ($1) free($1) ; }
%newobject get ;

extern char *version( );
extern int init( const char * dev ) ;
extern char * get( const char * path ) ;
extern int put( const char * path, const char * value ) ;
extern void finish( void ) ;
extern void set_error_print(int);
extern int get_error_print(void);
extern void set_error_level(int);
extern int get_error_level(void);
