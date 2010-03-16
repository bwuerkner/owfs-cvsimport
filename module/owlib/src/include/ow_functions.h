/*
$Id$
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

\    LICENSE (As of version 2.5p4 2-Oct-2006)
    owlib: GPL v2
    owfs, owhttpd, owftpd, owserver: GPL v2
    owshell(owdir owread owwrite owpresent): GPL v2
    owcapi (libowcapi): GPL v2
    owperl: GPL v2
    owtcl: LGPL v2
    owphp: GPL v2
    owpython: GPL v2
    owsim.tcl: GPL v2
    where GPL v2 is the "Gnu General License version 2"
    and "LGPL v2" is the "Lesser Gnu General License version 2"


    Written 2003 Paul H Alfille
        Fuse code based on "fusexmp" {GPL} by Miklos Szeredi, mszeredi@inf.bme.hu
        Serial code based on "xt" {GPL} by David Querbach, www.realtime.bc.ca
        in turn based on "miniterm" by Sven Goldt, goldt@math.tu.berlin.de
    GPL license
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Other portions based on Dallas Semiconductor Public Domain Kit,
//     ---------------------------------------------------------------------------
    Implementation:
    25-05-2003 iButtonLink device
*/

/* Cannot stand alone -- part of ow.h but separated for clarity */

#ifndef OW_FUNCTION_H			/* tedious wrapper */
#define OW_FUNCTION_H

#define FunctionExists(func)	((func) != NULL)

/* Prototypes */
#define  READ_FUNCTION( fname )  static int fname( struct one_wire_query * owq )
#define  WRITE_FUNCTION( fname )  static int fname( struct one_wire_query * owq )

/* Prototypes for owlib.c -- libow overall control */
void LibSetup(enum opt_program op);
int LibStart(void);
void HandleSignals(void);
void LibStop(void);
void LibClose(void);
int EnterBackground(void);

/* Initial sorting or the device and filetype lists */
void DeviceSort(void);
void DeviceDestroy(void);
/* Pasename processing -- URL/path comprehension */
int filetype_cmp(const void *name, const void *ex);
int FS_ParsedNamePlus(const char *path, const char *file, struct parsedname *pn);
int FS_ParsedNamePlusExt(const char *path, const char *file, int extension, enum ag_index alphanumeric, struct parsedname *pn);
int FS_ParsedName(const char *fn, struct parsedname *pn);
int FS_ParsedName_BackFromRemote(const char *fn, struct parsedname *pn);
void FS_ParsedName_destroy(struct parsedname *pn);
int FS_ParseProperty_for_sibling(char *filename, struct parsedname *pn);
int Parse_SerialNumber(char *sn_char, BYTE * sn) ;

size_t FileLength(const struct parsedname *pn);
size_t FullFileLength(const struct parsedname *pn);
int CheckPresence(struct parsedname *pn);
int ReCheckPresence(struct parsedname *pn);
void FS_devicename(char *buffer, const size_t length, const BYTE * sn, const struct parsedname *pn);
void FS_devicefind(const char *code, struct parsedname *pn);
void FS_devicefindhex(BYTE f, struct parsedname *pn);

const char *FS_DirName(const struct parsedname *pn);

/* Utility functions */
BYTE CRC8(const BYTE * bytes, const size_t length);
BYTE CRC8seeded(const BYTE * bytes, const size_t length, const UINT seed);
BYTE CRC8compute(const BYTE * bytes, const size_t length, const UINT seed);
int CRC16(const BYTE * bytes, const size_t length);
int CRC16seeded(const BYTE * bytes, const size_t length, const UINT seed);
BYTE char2num(const char *s);
BYTE string2num(const char *s);
char num2char(const BYTE n);
void num2string(char *s, const BYTE n);
void string2bytes(const char *str, BYTE * b, const int bytes);
void bytes2string(char *str, const BYTE * b, const int bytes);
int UT_getbit(const BYTE * buf, const int loc);
int UT_get2bit(const BYTE * buf, const int loc);
void UT_setbit(BYTE * buf, const int loc, const int bit);
void UT_set2bit(BYTE * buf, const int loc, const int bits);
void UT_fromDate(const _DATE D, BYTE * data);
_DATE UT_toDate(const BYTE * date);
int FS_busless(char *path);

void Test_and_Close( int * file_descriptor ) ;

/* Cache  and Storage functions */
#include "ow_cache.h"
void FS_LoadDirectoryOnly(struct parsedname *pn_directory, const struct parsedname *pn_original);

int FS_Test_Simultaneous( enum simul_type type, UINT delay, const struct parsedname * pn) ;
int FS_poll_convert(const struct parsedname *pn);

// ow_locks.c
void LockSetup(void);
int LockGet(struct parsedname *pn);
void LockRelease(struct parsedname *pn);

/* 1-wire lowlevel */
void UT_delay(const UINT len);
void UT_delay_us(const unsigned long len);

int tcp_read(int file_descriptor, void *vptr, size_t n, const struct timeval *ptv, size_t * actual_read);
void tcp_read_flush(int file_descriptor);
int tcp_wait(int file_descriptor, const struct timeval *ptv);
ssize_t udp_read(int file_descriptor, void *vptr, size_t n, const struct timeval * ptv, struct sockaddr_in *from, socklen_t *fromlen) ;

int ClientAddr(char *sname, struct connection_in *in);
int ClientConnect(struct connection_in *in);
void FreeClientAddr(struct connection_in *in);

void ServerProcess(void (*HandlerRoutine) (int file_descriptor), void (*Exit) (int errcode));
int ServerOutSetup(struct connection_out *out);

int AliasFile(const ASCII * file) ;

speed_t COM_MakeBaud( int raw_baud ) ;
int COM_BaudRate( speed_t B_baud ) ;
void COM_BaudRestrict( speed_t * B_baud, ... ) ;

void FS_help(const char *arg);

void update_max_delay(struct connection_in *connection);

int ServerPresence(const struct parsedname *pn);
int ServerRead(struct one_wire_query *owq);
int ServerWrite(struct one_wire_query *owq);
int ServerDir(void (*dirfunc) (void *, const struct parsedname *), void *v, const struct parsedname *pn, uint32_t * flags);

/* High-level callback functions */
int FS_dir(void (*dirfunc) (void *, const struct parsedname *), void *v, struct parsedname *pn);
int FS_dir_remote(void (*dirfunc) (void *, const struct parsedname *), void *v, const struct parsedname *pn, uint32_t * flags);
void FS_alias_subst(void (*dirfunc) (void *, const struct parsedname *), void *v, const struct parsedname *pn) ;

int FS_write(const char *path, const char *buf, const size_t size, const off_t offset);
int FS_write_postparse(struct one_wire_query *owq);
int FS_write_local(struct one_wire_query *owq);

int FS_read(const char *path, char *buf, const size_t size, const off_t offset);
//int FS_read_postparse(char *buf, const size_t size, const off_t offset,
//                    const struct parsedname *pn);
int FS_read_postparse(struct one_wire_query *owq);
int FS_read_distribute(struct one_wire_query *owq);
int FS_read_fake(struct one_wire_query *owq);
int FS_read_tester(struct one_wire_query *owq);
int FS_r_aggregate_all(struct one_wire_query *owq);
int FS_read_local( struct one_wire_query *owq);

int FS_r_sibling_F(_FLOAT *F, const char * sibling, struct one_wire_query *owq) ;
int FS_r_sibling_U(  UINT *U, const char * sibling, struct one_wire_query *owq) ;
int FS_r_sibling_Y(   INT *Y, const char * sibling, struct one_wire_query *owq) ;
int FS_r_sibling_D( _DATE *D, const char * sibling, struct one_wire_query *owq) ;
int FS_r_sibling_binary(BYTE * data, size_t * size, const char * sibling, struct one_wire_query *owq) ;

int FS_w_sibling_bitwork(UINT set, UINT mask, const char * sibling, struct one_wire_query *owq) ;

int FS_w_sibling_F(_FLOAT F, const char * sibling, struct one_wire_query *owq) ;
int FS_w_sibling_U(  UINT U, const char * sibling, struct one_wire_query *owq) ;
int FS_w_sibling_Y(   INT Y, const char * sibling, struct one_wire_query *owq) ;
int FS_w_sibling_D( _DATE D, const char * sibling, struct one_wire_query *owq) ;

void FS_del_sibling(const char * sibling, struct one_wire_query *owq) ;

int FS_fstat(const char *path, struct stat *stbuf);
int FS_fstat_postparse(struct stat *stbuf, const struct parsedname *pn);

/* iteration functions for splitting writes to buffers */
int COMMON_readwrite_paged(struct one_wire_query *owq, size_t page, size_t pagelen, int (*readwritefunc) (BYTE *, size_t, off_t, struct parsedname *));
int COMMON_OWQ_readwrite_paged(struct one_wire_query *owq, size_t page, size_t pagelen, int (*readwritefunc) (struct one_wire_query *, size_t, size_t));

int COMMON_read_memory_F0(struct one_wire_query *owq, size_t page, size_t pagesize);
int COMMON_read_memory_crc16_A5(struct one_wire_query *owq, size_t page, size_t pagesize);
int COMMON_read_memory_crc16_AA(struct one_wire_query *owq, size_t page, size_t pagesize);
int COMMON_read_memory_toss_counter(struct one_wire_query *owq, size_t page, size_t pagesize);
int COMMON_read_memory_plus_counter(BYTE * extra, size_t page, size_t pagesize, struct parsedname *pn);

int COMMON_write_eprom_mem_owq(struct one_wire_query * owq) ;
int COMMON_write_eprom_status(const BYTE * data, size_t size, off_t offset, const struct parsedname *pn);

int COMMON_offset_process( int (*func) (struct one_wire_query *), struct one_wire_query * owq, off_t shift_offset) ;

void BUS_lock(const struct parsedname *pn);
void BUS_unlock(const struct parsedname *pn);
void BUS_lock_in(struct connection_in *in);
void BUS_unlock_in(struct connection_in *in);

/* API wrappers for swig and owcapi */
void API_setup(enum opt_program opt);
int API_init(const char *command_line);
void API_set_error_level(const char *params);
void API_set_error_print(const char *params);
void API_finish(void);
int API_access_start(void);
void API_access_end(void);

#endif							/* OW_FUNCTION_H */
