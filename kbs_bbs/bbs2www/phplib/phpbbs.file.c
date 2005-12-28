#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_smth_bbs.h"  

#include "bbs.h"
#include "bbslib.h"


/*
 * refer Ecma-262 
 * '\033'  -> \r (not exactly the same thing, but borrow...)
 * '\n'    -> \n
 * '\\'    -> \\
 * '\''    -> \'
 * '\"'    -> \"
 * '\0'    -> possible start of attachment
 * 0 <= char < 32 -> ignore
 * others  -> passthrough
 */

PHP_FUNCTION(bbs2_readfile)
{
    char *filename;
    int filename_len;
    char *output_buffer;
    int output_buffer_len, output_buffer_size, j;
    char c;
    char *ptr, *cur_ptr;
    long ptrlen;
    int fd;
    int in_chinese = false;
    int chunk_size = 51200;
    struct stat st;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &filename, &filename_len) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        RETURN_LONG(2);
    if (fstat(fd, &st) < 0) {
        close(fd);
        RETURN_LONG(2);
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        RETURN_LONG(2);
    }
    if (st.st_size <= 0) {
        close(fd);
        RETURN_LONG(2);
    }

    ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    ptrlen = st.st_size;
    close(fd);
    if (ptr == NULL)
        RETURN_LONG(-1);

    j = ptrlen;
    if (j > chunk_size) j = chunk_size;
    output_buffer_size = 2 * j + 16;
    output_buffer = (char* )emalloc(output_buffer_size);
    output_buffer_len = 0;
    cur_ptr = ptr;
    strcpy(output_buffer + output_buffer_len, "prints('");
    output_buffer_len += 8;
    while (1) {
        for (; j >= 0 ; j--) {
            c = *cur_ptr;
            if (c == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
                if (ptrlen >= ATTACHMENT_SIZE + sizeof(int) + 2) {
                    if (!memcmp(cur_ptr, ATTACHMENT_PAD, ATTACHMENT_SIZE)) {
                        ptrlen = -ptrlen;
                        break;
                    }
                }
                ptrlen--; cur_ptr++;
                continue;
            }
            if (c < 0) {
                in_chinese = !in_chinese;
                output_buffer[output_buffer_len++] = c;
            } else {
                do {
                    if (c == '\n') c = 'n';
                    else if (c == '\033') c = 'r';
                    else if (c != '\\' && c != '\'' && c != '\"') {
                        if (c >= 32) {
                            output_buffer[output_buffer_len++] = c;
                        }
                        break;
                    }
                    if (in_chinese && c == 'n') {
                        output_buffer[output_buffer_len++] = ' ';
                    }
                    output_buffer[output_buffer_len++] = '\\';
                    output_buffer[output_buffer_len++] = c;
                } while(0);
                in_chinese = false;
            }
            ptrlen--; cur_ptr++;
        }
        if (ptrlen <= 0) break;
        j = ptrlen;
        if (j > chunk_size) j = chunk_size;
        output_buffer_size += 2 * j;
        output_buffer = (char*)erealloc(output_buffer, output_buffer_size);
        if (output_buffer == NULL) RETURN_LONG(3);
    }
    if (in_chinese) {
        output_buffer[output_buffer_len++] = ' ';
    }
    strcpy(output_buffer + output_buffer_len, "');");
    output_buffer_len += 3;
    
    if (ptrlen < 0) { //attachment
        char *attachfilename, *attachptr;
        char buf[1024];
        char *startbufptr, *bufptr;
        long attach_len, attach_pos, newlen;
        int l;

        ptrlen = -ptrlen;
        strcpy(buf, "attach('");
        startbufptr = buf + strlen(buf);
        while(ptrlen > 0) {
            if (((attachfilename = checkattach(cur_ptr, ptrlen, 
                                  &attach_len, &attachptr)) == NULL)) {
                break;
            }
            attach_pos = attachfilename - ptr;
            newlen = attachptr - cur_ptr + attach_len;
            cur_ptr += newlen;
            ptrlen -= newlen;
            if (ptrlen < 0) break;
            bufptr = startbufptr;
            while(*attachfilename != '\0') {
                switch(*attachfilename) {
                    case '\'':
                    case '\"':
                    case '\\':
                        *bufptr++ = '\\'; /* TODO: boundary check */
                        /* break is missing *intentionally* */
                    default:
                        *bufptr++ = *attachfilename++;  /* TODO: boundary check */
                }
            }
            sprintf(bufptr, "', %ld, %ld);", attach_len, attach_pos);  /* TODO: boundary check */

            l = strlen(buf);
            if (output_buffer_len + l > output_buffer_size) {
                output_buffer_size = output_buffer_size + sizeof(buf) * 10;
                output_buffer = (char*)erealloc(output_buffer, output_buffer_size);
                if (output_buffer == NULL) RETURN_LONG(3);
            }
            strcpy(output_buffer + output_buffer_len, buf);
            output_buffer_len += l;
        }
    }
    munmap(ptr, st.st_size);

    RETVAL_STRINGL(output_buffer, output_buffer_len, 0);;
}

PHP_FUNCTION(bbs2_readfile_text)
{
    char *filename;
    int filename_len;
    long maxchar;
    long escape_flag; /* 0(default) - escape <>&, 1 - double escape <>&, 2 - escape <>& and space */
    char *output_buffer;
    int output_buffer_len, output_buffer_size, last_return = 0;
    char c;
    char *ptr, *cur_ptr;
    long ptrlen;
    int in_escape = false, is_last_space = false;
    int fd, i;
    char escape_seq[4][16], escape_seq_len[4];
    struct stat st;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &filename, &filename_len, &maxchar, &escape_flag) == FAILURE) {
        WRONG_PARAM_COUNT;
    }
    
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        RETURN_LONG(2);
    if (fstat(fd, &st) < 0) {
        close(fd);
        RETURN_LONG(2);
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        RETURN_LONG(2);
    }
    if (st.st_size <= 0) {
        close(fd);
        RETURN_LONG(2);
    }

    ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    ptrlen = st.st_size;
    close(fd);
    if (ptr == NULL)
        RETURN_LONG(-1);

    if (!maxchar) {
        maxchar = ptrlen;
    } else if (ptrlen > maxchar) {
        ptrlen = maxchar;
    }
    output_buffer_size = 2 * maxchar;
    output_buffer = (char* )emalloc(output_buffer_size);
    output_buffer_len = 0;
    cur_ptr = ptr;
    if (escape_flag == 1) {
        strcpy(escape_seq[0], "&amp;amp;");
        strcpy(escape_seq[1], "&amp;lt;");
        strcpy(escape_seq[2], "&amp;gt;");
        strcpy(escape_seq[3], "&lt;br/&gt;");
    } else {
        strcpy(escape_seq[0], "&amp;");
        strcpy(escape_seq[1], "&lt;");
        strcpy(escape_seq[2], "&gt;");
        strcpy(escape_seq[3], "<br/>");
    }
    for (i=0;i<4;i++) escape_seq_len[i] = strlen(escape_seq[i]);
    while (ptrlen > 0) {
        c = *cur_ptr;
        if (c == '\0') { //assume ATTACHMENT_PAD[0] is '\0'
            break;
        } else if (c == '\033') {
            in_escape = true;
        } else if (!in_escape) {
            if (output_buffer_len + 16 > output_buffer_size) {
                output_buffer = (char*)erealloc(output_buffer, output_buffer_size += 128);
            }
            if (escape_flag == 2 && c == ' ') {
                if (!is_last_space) {
                    output_buffer[output_buffer_len++] = ' ';
                } else {
                    strcpy(output_buffer + output_buffer_len, "&nbsp;");
                    output_buffer_len += 6;
                }
                is_last_space = !is_last_space;
            } else {
                is_last_space = false;
                switch(c) {
                    case '&':
                        strcpy(output_buffer + output_buffer_len, escape_seq[0]);
                        output_buffer_len += escape_seq_len[0];
                        break;
                    case '<':
                        strcpy(output_buffer + output_buffer_len, escape_seq[1]);
                        output_buffer_len += escape_seq_len[1];
                        break;
                    case '>':
                        strcpy(output_buffer + output_buffer_len, escape_seq[2]);
                        output_buffer_len += escape_seq_len[2];
                        break;
                    case '\n':
                        strcpy(output_buffer + output_buffer_len, escape_seq[3]);
                        output_buffer_len += escape_seq_len[3];
                        last_return = output_buffer_len;
                        is_last_space = true;
                        break;
                    default:
                        if (c < 0 || c >= 32)
                            output_buffer[output_buffer_len++] = c;
                        break;
                }
            }
        } else if (isalpha(c)) {
            in_escape = false;
        }
        ptrlen--; cur_ptr++;
    }

    munmap(ptr, st.st_size);

    RETVAL_STRINGL(output_buffer, last_return, 0);
}







static char* output_buffer=NULL;
static int output_buffer_len=0;
static int output_buffer_size=0;

void reset_output_buffer() {
    output_buffer=NULL;
    output_buffer_size=0;
    output_buffer_len=0;
}

static void output_printf(const char* buf, uint len)
{
	int bufLen;
	int n,newsize;
	char * newbuf;
	if (output_buffer==NULL) {
		output_buffer=(char* )emalloc(51200); //first 50k
		if (output_buffer==NULL) {
			return;
		}
		output_buffer_size=51200;
	}
	bufLen=strlen(buf);
	if (bufLen>len) {
		bufLen=len;
	}
	n=1+output_buffer_len+bufLen-output_buffer_size;
	if (n>=0) {
		newsize=output_buffer_size+((n/102400)+1)*102400; //n*100k every time
		newbuf=(char*)erealloc(output_buffer,newsize);
		if (newbuf==NULL){
			return;
		}
		output_buffer=newbuf;
		output_buffer_size=newsize;
	}
	memcpy(output_buffer+output_buffer_len,buf,bufLen);
	output_buffer_len+=bufLen;
}

static char* get_output_buffer(){
	return output_buffer;
}

static int get_output_buffer_len(){
	int len=output_buffer_len;
	output_buffer_len=0;
	return len;
}

#if 0
static int new_buffered_output(char *buf, size_t buflen, void *arg)
{
	output_printf(buf,buflen);
	return 0;
}
#endif

static int new_write(const char *buf, uint buflen)
{
	output_printf(buf, buflen);
	return 0;
}

/* ע�⣬�� is_preview Ϊ 1 ��ʱ�򣬵�һ������ filename ���ǹ�Ԥ�����������ݣ��������ļ��� - atppp */
PHP_FUNCTION(bbs_printansifile)
{
    char *filename;
    int filename_len;
    long linkmode,is_tex,is_preview;
    char *ptr;
    long ptrlen;
    int fd;
    struct stat st;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;
    char attachdir[MAXPATH];

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
        is_tex=is_preview=0;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
        is_tex=is_preview=0;
    } else if (ZEND_NUM_ARGS() == 3) {
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        is_tex=is_preview=0;
    } else {
        if (zend_parse_parameters(5 TSRMLS_CC, "slsll", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len,&is_tex,&is_preview) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    }
    if (!is_preview) {
        fd = open(filename, O_RDONLY);
        if (fd < 0)
            RETURN_LONG(2);
        if (fstat(fd, &st) < 0) {
            close(fd);
            RETURN_LONG(2);
        }
        if (!S_ISREG(st.st_mode)) {
            close(fd);
            RETURN_LONG(2);
        }
        if (st.st_size <= 0) {
            close(fd);
            RETURN_LONG(2);
        }

        ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        ptrlen = st.st_size;
        close(fd);
        if (ptr == NULL)
            RETURN_LONG(-1);
    } else {
        ptr = filename;
        ptrlen = filename_len;
        getattachtmppath(attachdir, MAXPATH);
    }
	if ((out = alloc_output(outbuf_len)) == NULL)
	{
		if (!is_preview) munmap(ptr, st.st_size);
        RETURN_LONG(2);
	}
/*
	override_default_output(out, buffered_output);
	override_default_flush(out, flush_buffer);
*/
	/*override_default_output(out, new_buffered_output);
	override_default_flush(out, new_flush_buffer);*/
	override_default_write(out, new_write);

	output_ansi_html(ptr, ptrlen, out, attachlink, is_tex, is_preview ? attachdir : NULL);
	free_output(out);
    if (!is_preview) munmap(ptr, st.st_size);
	RETURN_STRINGL(get_output_buffer(), get_output_buffer_len(),1);
}

PHP_FUNCTION(bbs_print_article)
{
    char *filename;
    int filename_len;
    long linkmode;
    char *ptr;
    int fd;
    struct stat st;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
    } else 
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        RETURN_LONG(2);
    if (fstat(fd, &st) < 0) {
        close(fd);
        RETURN_LONG(2);
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        RETURN_LONG(2);
    }
    if (st.st_size <= 0) {
        close(fd);
        RETURN_LONG(2);
    }

    ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == NULL)
        RETURN_LONG(-1);
	if ((out = alloc_output(outbuf_len)) == NULL)
	{
		munmap(ptr, st.st_size);
        RETURN_LONG(2);
	}

	override_default_write(out, zend_write);

	output_ansi_text(ptr, st.st_size, out, attachlink);
	free_output(out);
    munmap(ptr, st.st_size);
}

PHP_FUNCTION(bbs_print_article_js)
{
    char *filename;
    int filename_len;
    long linkmode;
    char *ptr;
    int fd;
    struct stat st;
    const int outbuf_len = 4096;
    buffered_output_t *out;
    char* attachlink;
    int attachlink_len;

    if (ZEND_NUM_ARGS() == 1) {
        if (zend_parse_parameters(1 TSRMLS_CC, "s", &filename, &filename_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        linkmode = 1;
        attachlink=NULL;
    } else if (ZEND_NUM_ARGS() == 2) {
        if (zend_parse_parameters(2 TSRMLS_CC, "sl", &filename, &filename_len, &linkmode) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
        attachlink=NULL;
    } else 
        if (zend_parse_parameters(3 TSRMLS_CC, "sls", &filename, &filename_len, &linkmode,&attachlink,&attachlink_len) != SUCCESS) {
            WRONG_PARAM_COUNT;
        }
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        RETURN_LONG(2);
    if (fstat(fd, &st) < 0) {
        close(fd);
        RETURN_LONG(2);
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        RETURN_LONG(2);
    }
    if (st.st_size <= 0) {
        close(fd);
        RETURN_LONG(2);
    }

    ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == NULL)
        RETURN_LONG(-1);
	if ((out = alloc_output(outbuf_len)) == NULL)
	{
		munmap(ptr, st.st_size);
        RETURN_LONG(2);
	}

	override_default_write(out, zend_write);

	output_ansi_javascript(ptr, st.st_size, out, attachlink);
	free_output(out);
    munmap(ptr, st.st_size);
}


/* function bbs_printoriginfile(string board, string filename);
 * ���ԭ�����ݹ��༭
 */
PHP_FUNCTION(bbs_printoriginfile)
{
    char *board,*filename;
    int boardLen,filenameLen;
    FILE* fp;
    const int outbuf_len = 4096;
	char buf[512],path[512];
    buffered_output_t *out;
	int i;
	int skip;
	boardheader_t* bp;

    if ((ZEND_NUM_ARGS() != 2) || (zend_parse_parameters(2 TSRMLS_CC, "ss", &board,&boardLen, &filename,&filenameLen) != SUCCESS)) {
		WRONG_PARAM_COUNT;
    } 
	if ( (bp=getbcache(board))==0) {
		RETURN_LONG(-1);
	}
	setbfile(path, bp->filename, filename);
    fp = fopen(path, "r");
    if (fp == 0)
        RETURN_LONG(-1); //�ļ��޷���ȡ
	if ((out = alloc_output(outbuf_len)) == NULL)
	{
        RETURN_LONG(-2);
	}
	override_default_write(out, zend_write);
	/*override_default_output(out, buffered_output);
	override_default_flush(out, flush_buffer);*/
	
	i=0;    
	skip=0;
    while (skip_attach_fgets(buf, sizeof(buf), fp) != 0) {
		i++;
        if (Origin2(buf))
            break;
		if ((i==1) && (strncmp(buf,"������",6)==0)) {
			skip=1;
		}
		if ((skip) && (i<=4) ){
			continue;
		}
        if (!strcasestr(buf, "</textarea>"))
		{
			int len = strlen(buf);
            BUFFERED_OUTPUT(out, buf, len);
		}
    }
	BUFFERED_FLUSH(out);
	free_output(out);
    RETURN_LONG(0);
}