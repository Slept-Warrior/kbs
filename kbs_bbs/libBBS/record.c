/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/  
    
#include "bbs.h"
    
#define BUFSIZE (MAXUSERS + 244)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
    
int 
safewrite(fd, buf, size) 
 int fd;
    
char *buf;
    
int size;


{
    
int cc, sz = size, origsz = size;
    
char *bp = buf;

    

#ifdef POSTBUG
        if (size == sizeof(struct fileheader)) {
        
char tmp[80];
        
struct stat stbuf;
        
struct fileheader *fbuf = (struct fileheader *) buf;

        

setbpath(tmp, fbuf->filename);
        
if (!isalpha(fbuf->filename[0]) || stat(tmp, &stbuf) == -1)
            
if (fbuf->filename[0] != 'M' || fbuf->filename[1] != '.') {
                
report("safewrite: foiled attempt to write bugged record\n");
                
return origsz;
            
}
    
}
    
#endif                          /* 
 */
        do {
        
cc = write(fd, bp, sz);
        
if ((cc < 0) && (errno != EINTR)) {
            
report("safewrite err!");
            
return -1;
        
}
        
if (cc > 0) {
            
bp += cc;
            
sz -= cc;
        
}
    
} while (sz > 0);
    
return origsz;

}



#ifdef POSTBUG

char bigbuf[10240];

int numtowrite;

int bug_possible = 0;
    

void 
saverecords(filename, size, pos) 
 char *filename;
    
int size, pos;


{
    
int fd;

    
if (!bug_possible)
        return 0;
    
if ((fd = open(filename, O_RDONLY)) == -1)
        return -1;
    
if (pos > 5)
        numtowrite = 5;
    
    else
        numtowrite = 4;
    
lseek(fd, (pos - numtowrite - 1) * size, SEEK_SET);
    
read(fd, bigbuf, numtowrite * size);
    
close(fd);
                /*---	period	2000-10-20	file should be closed	---*/

}
    

void 
restorerecords(filename, size, pos) 
 char *filename;
    
int size, pos;


{
    
int fd;

    
if (!bug_possible)
        return 0;
    
if ((fd = open(filename, O_WRONLY)) == -1)
        return -1;
    
flock(fd, LOCK_EX);
    
lseek(fd, (pos - numtowrite - 1) * size, SEEK_SET);
    
safewrite(fd, bigbuf, numtowrite * size);
    
report("post bug poison set out!");
    
flock(fd, LOCK_UN);
    
bigbuf[0] = '\0';
    
close(fd);

}



#endif                          /* 
 */
    
long 
get_num_records(filename, size) 
 char *filename;
    
int size;


{
    
struct stat st;

    

if (stat(filename, &st) == -1)
        
return 0;
    
return (st.st_size / size);

}


long get_sum_records(char *fpath, int size) /* alex于1996.10.20添加 */ 
{
    
struct stat st;
    
long ans = 0;

    
FILE * fp;
    
fileheader fhdr;
    
char buf[200], *p;

    

if (!(fp = fopen(fpath, "r")))
        
return -1;
    

strcpy(buf, fpath);
    
p = strrchr(buf, '/') + 1;
    

while (fread(&fhdr, size, 1, fp) == 1) {
        
strcpy(p, fhdr.filename);
        
if (stat(buf, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1)
            
ans += st.st_size;
    
}
    
fclose(fp);
    
return ans / 1024;

}
    


int 
append_record(filename, record, size) 
 char *filename;
    
void *record;
    
int size;


{
    
int fd;

    

#ifdef POSTBUG
    int numrecs = (int) get_num_records(filename, size);

    
bug_possible = 1;
    
if (size == sizeof(struct fileheader) && numrecs && (numrecs % 4 == 0))
        
saverecords(filename, size, numrecs + 1);
    
#endif                          /* 
 */
        /*if((fd = open(filename,O_WRONLY|O_CREAT,0644)) == -1) { */ 
        if ((fd = open(filename, O_WRONLY | O_CREAT, 0664)) == -1)
        
 {                     /* Leeward 98.04.27: 0664->Enable write access of WWW-POST programe */
        
perror(filename);
        
return -1;
        
}
    
flock(fd, LOCK_EX);
    
lseek(fd, 0, SEEK_END);
    
if (safewrite(fd, record, size) == -1)
        
report("apprec write err!");
    
flock(fd, LOCK_UN);
    
close(fd);
    
#ifdef POSTBUG
        if (size == sizeof(struct fileheader) && numrecs && (numrecs % 4 == 0))
        
restorerecords(filename, size, numrecs + 1);
    
bug_possible = 0;
    
#endif                          /* 
 */
        return 0;

}


void 
toobigmesg() 
{
    
        /* change by KCN 1999.09.08
           fprintf( stderr, "record size too big!!\n" );
         */ 
} 

/* apply_record进行了预读优化,以减少系统调用次数,提高速度. ylsdd 2001.4.24 */ 
/* COMMAN : use mmap to speed up searching */ 
int apply_record(char *filename, RECORD_FUNC_ARG fptr, int size, void *arg, int applycopy) 
{
    
char *buf, *buf1, *buf2;
    
int i;
    
int file_size;

    

if (applycopy)
        
buf2 = malloc(size);
    
switch (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, (void **) &buf, &file_size, NULL)) {
    
case 0:
        return 0;
    
case 1:
        
for (i = 0, buf1 = buf; i < file_size / size; i++, buf1 += size) {
            
if (applycopy)
                
memcpy(buf2, buf1, size);
            
            else
                
buf2 = buf1;
            
if ((*fptr) (buf2, arg) == QUIT) {
                
end_mmapfile((void *) buf, file_size, -1);
                
if (applycopy)
                    
free(buf2);
                
return QUIT;
            
}
        
}
        
break;
    
}
    
end_mmapfile((void *) buf, file_size, -1);
    
if (applycopy)
        
free(buf2);
    
return 0;

}



/*---   Added by period   2000-10-26  ---*/ 
/*---	也可以考虑用一次读入CheckStep个记录的方法.	---*
 *---	就是在内存占用和系统IO之间作个选择		---*/ 
/*#ifdef _DEBUG_*/ 
#define _FREE_MEMORY_
    
#ifndef _FREE_MEMORY_
#  ifndef _FREE_IO_
#    error
#  endif
#else                           /* 
 */
#  ifdef _FREE_IO_
#    error
#  endif
#endif                          /* 
 */
    
/* COMMAN : use mmap to speed up searching */ 
int search_record_back(
int fd, /* file handle */ 
                       int size, /* record size */ 
                       int start, /* where to start reverse search */ 
                       RECORD_FUNC_ARG fptr, /* compare function */ 
                       void *farg, /* additional param to call fptr() / original record */ 
                       void *rptr, /* record data buffer to be used for reading idx file */ 
                       int sorted) /* if records in file are sorted */ 
{
    
char *buf, *buf1;
    
int i;
    
int filesize;

    
switch (safe_mmapfile_handle(fd, O_RDONLY, PROT_READ, MAP_SHARED, (void **) &buf, &filesize)) {
    
case 0:
        return 0;
    
case 1:
        
if (start > filesize / size)
            start = filesize / size;
        
for (i = start, buf1 = buf + size * (start - 1); i > 0; i--, buf1 -= size) {
            
if ((*fptr) (farg, buf1)) {
                
if (rptr)
                    
memcpy(rptr, buf1, size);
                
end_mmapfile((void *) buf, filesize, -1);
                
return i;
            
}
        
}
    
}
    
end_mmapfile((void *) buf, filesize, -1);
    
return 0;

}



/*---   End of Addition     ---*/ 
    
/* search_record进行了预读优化,以减少系统调用次数,提高速度. ylsdd, 2001.4.24 */ 
/* COMMAN : use mmap to improve search speed */ 
int 
search_record(char *filename, void *rptr, int size, RECORD_FUNC_ARG fptr, void *farg) 
{
    
int i;
    
char *buf, *buf1;
    
int filesize;

    
switch (safe_mmapfile(filename, O_RDONLY, PROT_READ, MAP_SHARED, (void **) &buf, &filesize, NULL)) {
    
case 0:
        return 0;
    
case 1:
        
for (i = 0, buf1 = buf; i < filesize / size; i++, buf1 += size) {
            
if ((*fptr) (farg, buf1)) {
                
if (rptr)
                    
memcpy(rptr, buf1, size);
                
end_mmapfile((void *) buf, filesize, -1);
                
return i + 1;
            
}
        
}
    
}
    
end_mmapfile((void *) buf, filesize, -1);
    
return 0;

}
    

int 
get_record_handle(fd, rptr, size, id) 
 int fd;
    
void *rptr;
    
int size, id;


{
    
if (lseek(fd, size * (id - 1), SEEK_SET) == -1)
        
return -1;
    
if (read(fd, rptr, size) != size)
        
return -1;
    
return 0;

}
    
int 
get_record(filename, rptr, size, id) 
 char *filename;
    
void *rptr;
    
int size, id;


{
    
int fd;
    
int ret;

    

if ((fd = open(filename, O_RDONLY, 0)) == -1)
        
return -1;
    
ret = get_record_handle(fd, rptr, size, id);
    
close(fd);
    
return ret;

}
    

int 
get_records(filename, rptr, size, id, number) 
 char *filename;
    
void *rptr;
    
int size, id, number;


{
    
int fd;
    
int n;

    

if ((fd = open(filename, O_RDONLY, 0)) == -1)
        
return -1;
    
if (lseek(fd, size * (id - 1), SEEK_SET) == -1) {
        
close(fd);
        
return 0;
    
}
    
if ((n = read(fd, rptr, size * number)) == -1) {
        
close(fd);
        
return -1;
    
}
    
close(fd);
    
return (n / size);

}
    

int 
substitute_record(filename, rptr, size, id) 
 char *filename;
    
void *rptr;
    
int size, id;


{
    
        /* add by KCN */ 
    struct flock ldata;
    
int retval;
    

int fd;

    
#ifdef POSTBUG
        if (size == sizeof(struct fileheader) && (id > 1) && ((id - 1) % 4 == 0))
        
saverecords(filename, size, id);
    
#endif                          /* 
 */
        if ((fd = open(filename, O_WRONLY | O_CREAT, 0644)) == -1)
        
return -1;
    
        /* change by KCN
           flock(fd,LOCK_EX) ;
         */ 
        ldata.l_type = F_WRLCK;
    
ldata.l_whence = 0;
    
ldata.l_len = size;
    
ldata.l_start = size * (id - 1);
    
if ((retval = fcntl(fd, F_SETLKW, &ldata)) == -1) {
        
report("reclock error");
        
close(fd);
                        /*---	period	2000-10-20	file should be closed	---*/
        
return -1;
    
}
    

if (lseek(fd, size * (id - 1), SEEK_SET) == -1) {
        
report("subrec seek err");
        
        /*---	period	2000-10-24	---*/ 
            ldata.l_type = F_UNLCK;
        
fcntl(fd, F_SETLK, &ldata);
        
close(fd);
        
return -1;
    
}
    
if (safewrite(fd, rptr, size) != size)
        
report("subrec write err");
    
        /* change by KCN
           flock(fd,LOCK_UN) ;
         */ 
        ldata.l_type = F_UNLCK;
    
fcntl(fd, F_SETLK, &ldata);
    

close(fd);
    
#ifdef POSTBUG
        if (size == sizeof(struct fileheader) && (id > 1) && ((id - 1) % 4 == 0))
        
restorerecords(filename, size, id);
    
#endif                          /* 
 */
        return 0;

}



/*
void
tmpfilename( filename, tmpfile, deleted )
char    *filename, *tmpfile, *deleted;
{
    char        *ptr, delfname[STRLEN], tmpfname[STRLEN];

    strcpy( tmpfile, filename );
#ifdef BBSMAIN
    if (true == checkreadonly(currboard))
    {
        sprintf(delfname,".%sdeleted",currboard);
        sprintf(tmpfname,".%stmpfile",currboard);
        if( (ptr = strchr( tmpfile, '/' )) != NULL ) {
            strcpy( ptr+1, delfname );
            strcpy( deleted, tmpfile );
            strcpy( ptr+1, tmpfname );
        } else {
            strcpy( deleted, delfname );
            strcpy( tmpfile, tmpfname );
        }
        return;
    }
    else
#endif
    {
        sprintf(delfname , ".deleted%d",getpid());
        sprintf(tmpfname , ".tmpfile%d",getpid());
    }

    if( (ptr = strrchr( tmpfile, '/' )) != NULL ) {
        strcpy( ptr+1, delfname );
        strcpy( deleted, tmpfile );
        strcpy( ptr+1, tmpfname );
    } else {
        strcpy( deleted, delfname );
        strcpy( tmpfile, tmpfname );
    }
}
*/ 

int delete_record(char *filename, int size, int id, RECORD_FUNC_ARG filecheck, void *arg) 
{
    
int fdr, filesize;
    
char *ptr;
    
int ret;

    
if (id <= 0)
        return 0;
    
switch (safe_mmapfile(filename, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, (void **) &ptr, &filesize, &fdr)) {
    
case 0:
        
return -1;
    
case 1:
        
ret = 0;
        
if (filecheck) {
            
if (!(*filecheck) (ptr + (id - 1) * size, arg)) {
                
for (id = 0; id * size < filesize; id++)
                    
if ((*filecheck) (ptr + (id - 1) * size, arg))
                        
break;
                
if (id * size >= filesize)
                    
ret = -2;
            
}
        
}
        
if (ret == 0) {
            
memcpy(ptr + (id - 1) * size, ptr + id * size, filesize - size * id);
            
ftruncate(fdr, filesize - size);
        
}
        
break;
    
case 2:
        
ret = -3;
    
}
    
end_mmapfile(ptr, filesize, fdr);
    
return ret;

}
    

int 
delete_range(filename, id1, id2, del_mode) 
 char *filename;
    
int id1, id2, del_mode;


{
    
#define DEL_RANGE_BUF 2048
    struct fileheader *savefhdr;
    
struct fileheader *readfhdr;
    

struct fileheader *delfhdr;
    
int fdr;
    
int count, totalcount, delcount, remaincount, keepcount;
    
int pos_read, pos_write, pos_end;
    
int i;

    
#ifdef BBSMAIN
    int savedigestmode;

    
        /*digestmode=4, 5的情形或者允许区段删除,或者不允许,这可以在
           调用函数中或者任何地方给定, 这里的代码是按照不允许删除写的,
           但是为了修理任何缘故造成的临时文件故障(比如自动删除机), 还是
           尝试了一下打开操作; tmpfile是否对每种模式独立, 这个还是值得
           商榷的.  -- ylsdd */ 
        if (digestmode == 4 || digestmode == 5) {       /* KCN:暂不允许 */
        
return 0;
    
}
    

#endif                          /* 
 */
        
if ((fdr = open(filename, O_RDWR, 0)) == -1) {
        
return -2;
    
}
    

flock(fdr, LOCK_EX);
    

pos_end = lseek(fdr, 0, SEEK_END);
    
delcount = 0;
    
if (pos_end == -1) {
        
close(fdr);
        
return -2;
    
}
    
totalcount = pos_end / sizeof(struct fileheader);
    
pos_end = totalcount * sizeof(struct fileheader);
    
if (id2 != -1) {
        
pos_read = sizeof(struct fileheader) * id2;
    
}
    
    else
        
pos_read = pos_end;
    

if (id2 == -1)
        id2 = totalcount;
    
if (id1 != 0) {
        
pos_write = sizeof(struct fileheader) * (id1 - 1);
        
count = id1;
        
if (id1 > totalcount) {
            
#ifdef BBSMAIN
                prints("开始文章号大于文章总数");
            
pressanykey();
            
#endif                          /* 
 */
                return 0;
        
}
    
}
    
    else {
        
pos_write = 0;
        
count = 1;
        
id2 = totalcount;
    
}
    

if (id2 > totalcount) {
        
#ifdef BBSMAIN
        char buf[3];

        
getdata(6, 0, "文章编号大于文章总数，确认删除 (Y/N)? [N]: ", buf, 2, DOECHO, NULL, true);
        
if (*buf != 'Y' && *buf != 'y') {
            
close(fdr);
            
return -3;
        
}
        
#else                           /* 
 */
            close(fdr);
        
return -3;
        
#endif                          /* 
 */
            pos_read = pos_end;
        
id2 = totalcount;
    
}
    
savefhdr = (struct fileheader *) malloc(DEL_RANGE_BUF * sizeof(struct fileheader));
    
readfhdr = (struct fileheader *) malloc(DEL_RANGE_BUF * sizeof(struct fileheader));
    
delfhdr = (struct fileheader *) malloc(DEL_RANGE_BUF * sizeof(struct fileheader));
    
if ((id1 != 0) && (del_mode == 0)) {       /*rangle mark del */
        
while (count <= id2) {
            
int i;
            
int readcount;

            
lseek(fdr, pos_write, SEEK_SET);
            
readcount = read(fdr, savefhdr, DEL_RANGE_BUF * sizeof(struct fileheader)) / sizeof(struct fileheader);
            
for (i = 0; i < readcount; i++, count++) {
                
if (count > id2)
                    break;      /*del end */
                
if (!(savefhdr[i].accessed[0] & FILE_MARKED))
                    
savefhdr[i].accessed[1] |= FILE_DEL;
            
}
            
lseek(fdr, pos_write, SEEK_SET);
            
write(fdr, savefhdr, i * sizeof(struct fileheader));
            
pos_write += i * sizeof(struct fileheader);
        
} 
close(fdr);
        
free(savefhdr);
        
free(readfhdr);
        
free(delfhdr);
        
return 0;
    
}
    
remaincount = count - 1;
    
keepcount = 0;
    
lseek(fdr, pos_write, SEEK_SET);
    
#ifdef BBSMAIN
        savedigestmode = digestmode;
    
digestmode = 4;
    
#endif                          /* 
 */
        while (count <= id2) {
        
int readcount;
        
lseek(fdr, (count - 1) * sizeof(struct fileheader), SEEK_SET);
        
readcount = read(fdr, savefhdr, DEL_RANGE_BUF * sizeof(struct fileheader)) / sizeof(struct fileheader);
        
/*        if (readcount==0) break; */ 
            for (i = 0; i < readcount; i++, count++) {
            
if (count > id2)
                break;          /*del end */
            
if (((savefhdr[i].accessed[0] & FILE_MARKED) && del_mode != 2) || ((id1 == 0) && (!(savefhdr[i].accessed[1] & FILE_DEL))))
                
 {
                
memcpy(&readfhdr[keepcount], &savefhdr[i], sizeof(struct fileheader));
                
readfhdr[keepcount].accessed[1] &= ~FILE_DEL;
                
keepcount++;
                
remaincount++;
                
if (keepcount >= DEL_RANGE_BUF) {
                    
lseek(fdr, pos_write, SEEK_SET);
                    
write(fdr, readfhdr, DEL_RANGE_BUF * sizeof(struct fileheader));
                    
pos_write += keepcount * sizeof(struct fileheader);
                    
keepcount = 0;
                
}
                
}
            
#ifdef BBSMAIN
                else if (uinfo.mode != RMAIL) {
                
int j;
                
memcpy(&delfhdr[delcount], &savefhdr[i], sizeof(struct fileheader));
                
delcount++;
                
if (delcount >= DEL_RANGE_BUF) {
                    
for (j = 0; j < DEL_RANGE_BUF; j++)
                        
cancelpost(currboard, currentuser->userid, 
&delfhdr[j], !strcmp(delfhdr[j].owner, currentuser->userid), 0);
                    
delcount = 0;
                    
setbdir(digestmode, genbuf, currboard);
                    
append_record(genbuf, (char *) delfhdr, DEL_RANGE_BUF * sizeof(struct fileheader));
                
}              /*need clear delcount */
            
}                  /*if !Reading mail */
            
#endif                          /* 
 */
        } /*for readcount */ 
    } 
if (keepcount) {
        
lseek(fdr, pos_write, SEEK_SET);
        
write(fdr, readfhdr, keepcount * sizeof(struct fileheader));
    
}
    

while (1) {
        
int readcount;

        
lseek(fdr, pos_read, SEEK_SET);
        
readcount = read(fdr, savefhdr, DEL_RANGE_BUF * sizeof(struct fileheader)) / sizeof(struct fileheader);
        
if (readcount == 0)
            break;
        

lseek(fdr, remaincount * sizeof(struct fileheader), SEEK_SET);
        
write(fdr, savefhdr, readcount * sizeof(struct fileheader));
        
pos_read += readcount * sizeof(struct fileheader);
        
remaincount += readcount;
    
} 
ftruncate(fdr, remaincount * sizeof(struct fileheader));
    
close(fdr);
    
#ifdef BBSMAIN
        if ((uinfo.mode != RMAIL) && delcount) {
        
int j;

        
for (j = 0; j < delcount; j++)
            
cancelpost(currboard, currentuser->userid, 
&delfhdr[j], !strcmp(delfhdr[j].owner, currentuser->userid), 0);
        
setbdir(digestmode, genbuf, currboard);
        
append_record(genbuf, (char *) delfhdr, delcount * sizeof(struct fileheader));
    
}
    
digestmode = savedigestmode;
    
#endif                          /* 
 */
        free(savefhdr);
    
free(readfhdr);
    
free(delfhdr);
    
return 0;

}


