#include "bbs.h"
#include <utime.h>

void cancelpost(char *board, char *userid, struct fileheader *fh, int owned, int autoappend);
int outgo_post(struct fileheader *fh, char *board, char *title)
{
    FILE *foo;

    if ((foo = fopen("innd/out.bntp", "a")) != NULL) {
        fprintf(foo, "%s\t%s\t%s\t%s\t%s\n", board, fh->filename, currentuser->userid, currentuser->username, title);
        fclose(foo);
        return 0;
    }
    return -1;
}

extern char alphabet[];

int get_postfilename(char *filename, char *direct, int use_subdir)
{
    static const char post_sufix[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int fp;
    time_t now;
    int i;
    char fname[255];
    int pid = getpid();
	int rn;
	int len;

    /*
     * 自动生成 POST 文件名 
     */
    now = time(NULL);
	len = strlen(alphabet);
    for (i = 0; i < 10; i++)
	{
		if (use_subdir)
		{
			rn = 0 + (int) (len * 1.0 * rand() / (RAND_MAX + 1.0));
			sprintf(filename, "%c/M.%lu.%c%c", alphabet[rn], now, post_sufix[(pid + i) % 62], post_sufix[(pid * i) % 62]);
		}
		else
			sprintf(filename, "M.%lu.%c%c", now, post_sufix[(pid + i) % 62], post_sufix[(pid * i) % 62]);
        sprintf(fname, "%s/%s", direct, filename);
        if ((fp = open(fname, O_CREAT | O_EXCL | O_WRONLY, 0644)) != -1) {
            break;
        };
    }
    if (fp == -1)
        return -1;
    close(fp);
    return 0;
}

int isowner(struct userec *user, struct fileheader *fileinfo)
{
    time_t posttime;

    if (strcmp(fileinfo->owner, user->userid))
        return 0;
    posttime = get_posttime(fileinfo);
    if (posttime < user->firstlogin)
        return 0;
    return 1;
}

int cmpname(fhdr, name)         /* Haohmaru.99.3.30.比较 某文件名是否和 当前文件 相同 */
struct fileheader *fhdr;
char name[STRLEN];
{
    if (!strncmp(fhdr->filename, name, FILENAME_LEN))
        return 1;
    return 0;
}

int do_del_post(struct userec *user, int ent, struct fileheader *fileinfo, char *direct, char *board, int digestmode, int decpost)
{
    char buf[512];
    char *t;
    int owned, fail;

    strcpy(buf, direct);
    if ((t = strrchr(buf, '/')) != NULL)
        *t = '\0';
/* .post.X not use???! KCN
postbbslog("user","%s",fileinfo->title, -1, currboard); added by alex, 96.9.12 */
/*    if( keep <= 0 ) {*/
    if (fileinfo->id == fileinfo->groupid)
        setboardorigin(board, 1);
    setboardtitle(board, 1);
    //added by bad 2002.8.12
    fail = delete_record(direct, sizeof(struct fileheader), ent, (RECORD_FUNC_ARG) cmpname, fileinfo->filename);
/*
    } else {
        fail = update_file(direct,sizeof(struct fileheader),ent,cmpfilename,
                           cpyfilename);
    }
    */
    owned = isowner(user, fileinfo);
    if (!fail) {
        cancelpost(board, user->userid, fileinfo, owned, 1);
        updatelastpost(board);
/*
        sprintf(buf,"%s/%s",buf,fileinfo->filename) ;
        if(keep >0)  if ( (fn = fopen( buf, "w" )) != NULL ) {
            fprintf( fn, "\n\n\t\t本文章已被 %s 删除.\n",
                     currentuser->userid );
            fclose( fn );
        }
*/
        if (fileinfo->accessed[0] & FILE_MARKED)
            setboardmark(board, 1);
        if ((true != digestmode)        /* 不可以用 “NA ==” 判断：digestmode 三值 */
            &&!((fileinfo->accessed[0] & FILE_MARKED)
                && (fileinfo->accessed[1] & FILE_READ)
                && (fileinfo->accessed[0] & FILE_FORWARDED))) { /* Leeward 98.06.17 在文摘区删文不减文章数目 */
            if (owned) {
                if ((int) user->numposts > 0 && !junkboard(board)) {
                    user->numposts--;   /*自己删除的文章，减少post数 */
                }
            } else if (!strstr(fileinfo->owner, ".") && BMDEL_DECREASE && decpost /*版主删除,减少POST数 */ ) {
                struct userec *lookupuser;
                int id = getuser(fileinfo->owner, &lookupuser);

                if (id && (int) lookupuser->numposts > 0 && !junkboard(board) && strcmp(board, SYSMAIL_BOARD)) {    /* SYSOP MAIL版删文不减文章 Bigman: 2000.8.12 *//* Leeward 98.06.21 adds above later 2 conditions */
                    lookupuser->numposts--;
                }
            }
        }
        utime(fileinfo->filename, 0);
        if (user != NULL)
            bmlog(user->userid, board, 8, 1);
        newbbslog(BBSLOG_USER, "Del '%s' on '%s'", fileinfo->title, board);     /* bbslog */
        return 0;
    }
    return -1;
}

/* by ylsdd 
   unlink action is taked within cancelpost if in mail mode,
   otherwise this item is added to the file '.DELETED' under
   the board's directory, the filename is not changed. 
   Unlike the fb code which moves the file to the deleted
   board.
*/
void cancelpost(board, userid, fh, owned, autoappend)
char *board, *userid;
struct fileheader *fh;
int owned;
int autoappend;
{
    struct fileheader postfile;
    char oldpath[50];
    char newpath[50];
    struct fileheader *ph;
    time_t now;

#ifdef BBSMAIN
    if (uinfo.mode == RMAIL) {
        sprintf(oldpath, "mail/%c/%s/%s", toupper(currentuser->userid[0]), currentuser->userid, fh->filename);
        unlink(oldpath);
        return;
    }
#endif
    if (autoappend)
        ph = &postfile;
    else
        ph = fh;
/*
    sprintf(oldpath, "/board/%s/%s.html", board, fh->filename);
    ca_expire_file(oldpath);*/

    if ((fh->innflag[1] == 'S')
        && (fh->innflag[0] == 'S')
        && (get_posttime(fh) > now - 14 * 86400)) {
        FILE *fp;
        char buf[256];
        char from[STRLEN];
        int len;
        char *ptr;

        setbfile(buf, board, fh->filename);
        if ((fp = fopen(buf, "r")) == NULL)
            return;
        while (fgets(buf, sizeof(buf), fp) != NULL) {
            /*
             * 首先滤掉换行符 
             */
            len = strlen(buf) - 1;
            buf[len] = '\0';
            if (len < 8)
                break;
            if (strncmp("发信人: ", buf, 8) == 0) {
                if ((ptr = strrchr(buf, ')')) == NULL)
                    break;
                *ptr = '\0';
                if ((ptr = strrchr(buf, '(')) == NULL)
                    break;
                strncpy(from, ptr + 1, sizeof(from) - 1);
                from[sizeof(from) - 1] = '\0';
                break;
            }
        }
        fclose(fp);
        sprintf(buf, "%s\t%s\t%s\t%s\t%s\n", board, fh->filename, fh->owner, from, fh->title);
        if ((fp = fopen("innd/cancel.bntp", "a")) != NULL) {
            fputs(buf, fp);
            fclose(fp);
        }
    }
    
    strcpy(postfile.filename, fh->filename);
    fh->filename[0]=(owned) ? 'J':'D';
    setbfile(oldpath,board,postfile.filename);
    setbfile(newpath,board,fh->filename);
    f_mv(oldpath,newpath);
    if (autoappend) {
        bzero(&postfile, sizeof(postfile));
        strcpy(postfile.filename, fh->filename);
        strncpy(postfile.owner, fh->owner, OWNER_LEN-1);
        postfile.owner[OWNER_LEN-1] = 0;
        postfile.id = fh->id;
        postfile.groupid = fh->groupid;
        postfile.reid = fh->reid;
		set_posttime2(&postfile, fh);
    };
    now = time(NULL);
    sprintf(oldpath, "%-32.32s - %s", fh->title, userid);
    strncpy(ph->title, oldpath, STRLEN);
    ph->title[STRLEN - 1] = 0;
    ph->accessed[11] = now / (3600 * 24) % 100; /*localtime(&now)->tm_mday; */
    if (autoappend) {
        setbdir((owned) ? 5 : 4, oldpath, board);
        append_record(oldpath, &postfile, sizeof(postfile));
    }
}


void add_loginfo(char *filepath, struct userec *user, char *currboard, int Anony)
{                               /* POST 最后一行 添加 */
    FILE *fp;
    int color, noidboard;
    char fname[STRLEN];

    noidboard = (anonymousboard(currboard) && Anony);   /* etc/anonymous文件中 是匿名版版名 */
    color = (user->numlogins % 7) + 31; /* 颜色随机变化 */
    sethomefile(fname, user->userid, "signatures");
    fp = fopen(filepath, "a");
    if (!dashf(fname) ||        /* 判断是否已经 存在 签名档 */
        user->signature == 0 || noidboard) {
        fputs("\n--\n", fp);
    } else {                    /*Bigman 2000.8.10修改,减少代码 */
        fprintf(fp, "\n");
    }
    /*
     * 由Bigman增加:2000.8.10 Announce版匿名发文问题 
     */
    if (!strcmp(currboard, "Announce"))
        fprintf(fp, "[m[1;%2dm※ 来源:·%s %s·[FROM: %s][m\n", color, BBS_FULL_NAME, email_domain(), NAME_BBS_CHINESE " BBS站");
    else
        fprintf(fp, "\n[m[1;%2dm※ 来源:·%s %s·[FROM: %s][m\n", color, BBS_FULL_NAME, email_domain(), (noidboard) ? NAME_ANONYMOUS_FROM : fromhost);
    fclose(fp);
    return;
}

void addsignature(FILE * fp, struct userec *user, int sig)
{
    FILE *sigfile;
    int i, valid_ln = 0;
    char tmpsig[MAXSIGLINES][256];
    char inbuf[256];
    char fname[STRLEN];

    if (sig == 0)
        return;
    sethomefile(fname, user->userid, "signatures");
    if ((sigfile = fopen(fname, "r")) == NULL) {
        return;
    }
    fputs("\n--\n", fp);
    for (i = 1; i <= (sig - 1) * MAXSIGLINES && sig != 1; i++) {
        if (!fgets(inbuf, sizeof(inbuf), sigfile)) {
            fclose(sigfile);
            return;
        }
    }
    for (i = 1; i <= MAXSIGLINES; i++) {
        if (fgets(inbuf, sizeof(inbuf), sigfile)) {
            if (inbuf[0] != '\n')
                valid_ln = i;
            strcpy(tmpsig[i - 1], inbuf);
        } else
            break;
    }
    fclose(sigfile);
    for (i = 1; i <= valid_ln; i++)
        fputs(tmpsig[i - 1], fp);
}

int write_posts(char *id, char *board, char *title)
{
    char *ptr;
    time_t now;
    struct posttop postlog, pl;

    if (junkboard(board) || normal_board(board) != 1)
        return 0;
    now = time(0);
    strcpy(postlog.author, id);
    strcpy(postlog.board, board);
    ptr = title;
    if (!strncmp(ptr, "Re: ", 4))
        ptr += 4;
    strncpy(postlog.title, ptr, 65);
    postlog.date = now;
    postlog.number = 1;

    {                           /* added by Leeward 98.04.25 
                                 * TODO: 这个地方有点不妥,每次发文要遍历一次,保存到.Xpost中,
                                 * 用来完成十大发文统计针对ID而不是文章.不好
                                 * KCN */
        int log = 1;
        FILE *fp = fopen(".Xpost", "r");

        if (fp) {
            while (!feof(fp)) {
                fread(&pl, sizeof(pl), 1, fp);
                if (feof(fp))
                    break;

                if (!strcmp(pl.title, postlog.title)
                    && !strcmp(pl.author, postlog.author)
                    && !strcmp(pl.board, postlog.board)) {
                    log = 0;
                    break;
                }
            }
            fclose(fp);
        }

        if (log) {
            append_record(".Xpost", &postlog, sizeof(postlog));
            append_record(".post", &postlog, sizeof(postlog));
        }
    }

/*    append_record(".post.X", &postlog, sizeof(postlog));
*/
    return 0;
}

void write_header(FILE * fp, struct userec *user, int in_mail, char *board, char *title, int Anony, int mode)
{
    int noname;
    char uid[20];
    char uname[40];
    time_t now;

    now = time(0);
    strncpy(uid, user->userid, 20);
    uid[19] = '\0';
    if (in_mail)
#if defined(MAIL_REALNAMES)
        strncpy(uname, user->realname, NAMELEN);
#else
        strncpy(uname, user->username, NAMELEN);
#endif
    else
#if defined(POSTS_REALNAMES)
        strncpy(uname, user->realname, NAMELEN);
#else
        strncpy(uname, user->username, NAMELEN);
#endif
    /*
     * uid[39] = '\0' ; SO FUNNY:-) 定义的 20 这里却用 39 !
     * Leeward: 1997.12.11 
     */
    uname[39] = 0;              /* 其实是写错变量名了! 嘿嘿 */
    if (in_mail)
        fprintf(fp, "寄信人: %s (%s)\n", uid, uname);
    else {
        noname = anonymousboard(board);
        if (mode == 0 && !(noname && Anony)) {
            write_posts(user->userid, board, title);
        }

        if (!strcmp(board, "Announce"))
            /*
             * added By Bigman 
             */
            fprintf(fp, "发信人: %s (%s), 信区: %s       \n", "SYSOP", NAME_SYSOP, board);
        else
            fprintf(fp, "发信人: %s (%s), 信区: %s       \n", (noname && Anony) ? board : uid, (noname && Anony) ? NAME_ANONYMOUS : uname, board);
    }

    fprintf(fp, "标  题: %s\n", title);
    /*
     * 增加转信标记 czz 020819 
     */
    if (mode != 2)
        fprintf(fp, "发信站: %s (%24.24s), 站内信件\n", BBS_FULL_NAME, ctime(&now));
    else
        fprintf(fp, "发信站: %s (%24.24s), 转信\n", BBS_FULL_NAME, ctime(&now));
//    fprintf(fp, "发信站: %s (%24.24s)\n", BBS_FULL_NAME, ctime(&now));
    if (in_mail)
        fprintf(fp, "来  源: %s \n", fromhost);
    fprintf(fp, "\n");

}

void getcross(char *filepath, char *quote_file, struct userec *user, int in_mail, char *board, char *title, int Anony, int mode, char *sourceboard)
{                               /* 把quote_file复制到filepath (转贴或自动发信) */
    FILE *inf, *of;
    char buf[256];
    char owner[256];
    int count;
    time_t now;

    now = time(0);
    inf = fopen(quote_file, "r");
    of = fopen(filepath, "w");
    if (inf == NULL || of == NULL) {
        /*---	---*/
        if (NULL != inf)
            fclose(inf);
        if (NULL != of)
            fclose(of);
        /*---	---*/
#ifdef BBSMAIN
        bbslog("user","%s","Cross Post error");
#endif
        return;
    }
    if (mode == 0 /*转贴 */ ) {
        int normal_file;
        int header_count;

        normal_file = 1;

        write_header(of, user, in_mail, sourceboard, title, Anony, 1 /*不写入 .posts */ );
        if (fgets(buf, 256, inf) != NULL) {
            for (count = 8; buf[count] != ' ' && count < 256; count++)
                owner[count - 8] = buf[count];
        }
        owner[count - 8] = '\0';
        if (in_mail == true)
            fprintf(of, "[1;37m【 以下文字转载自 [32m%s [37m的信箱 】\n", user->userid);
        else
            fprintf(of, "【 以下文字转载自 %s 讨论区 】\n", board);
        if (id_invalid(owner))
            normal_file = 0;
        if (normal_file) {
            for (header_count = 0; header_count < 3; header_count++) {
                if (fgets(buf, 256, inf) == NULL)
                    break;      /*Clear Post header */
            }
            if ((header_count != 2) || (buf[0] != '\n'))
                normal_file = 0;
        }
        if (normal_file)
            fprintf(of, "【 原文由 %s 所发表 】\n", owner);
        else
            fseek(inf, 0, SEEK_SET);

    } else if (mode == 1 /*自动发信 */ ) {
        fprintf(of, "发信人: deliver (自动发信系统), 信区: %s\n", board);
        fprintf(of, "标  题: %s\n", title);
        fprintf(of, "发信站: %s自动发信系统 (%24.24s)\n\n", BBS_FULL_NAME, ctime(&now));
        fprintf(of, "【此篇文章是由自动发信系统所张贴】\n\n");
    } else if (mode == 2) {
        write_header(of, user, in_mail, sourceboard, title, Anony, 0 /*写入 .posts */ );
    }
    while (fgets(buf, 256, inf) != NULL) {
        if ((strstr(buf, "【 以下文字转载自 ") && strstr(buf, "讨论区 】")) || (strstr(buf, "【 原文由") && strstr(buf, "所发表 】")))
            continue;           /* 避免引用重复 */
        else
            fprintf(of, "%s", buf);
    }
    fclose(inf);
    fclose(of);
    /*
     * don't know why 
     * *quote_file = '\0';
     */
}

/* Add by SmallPig */
int post_cross(struct userec *user, char *toboard, char *fromboard, char *title, char *filename, int Anony, int in_mail, char islocal, int mode)
{                               /* (自动生成文件名) 转贴或自动发信 */
    struct fileheader postfile;
    char filepath[STRLEN];
    char buf4[STRLEN], whopost[IDLEN], save_title[STRLEN];
    int aborted, local_article;

    if (!haspostperm(user, toboard) && !mode) {
#ifdef BBSMAIN
        move(1, 0);
        prints("您尚无权限在 %s 发表文章.\n", toboard);
        prints("如果您尚未注册，请在个人工具箱内详细注册身份\n");
        prints("未通过身份注册认证的用户，没有发表文章的权限。\n");
        prints("谢谢合作！ :-) \n");
#endif
        return -1;
    }

    memset(&postfile, 0, sizeof(postfile));

    if (!mode) {
        if (!strstr(title, "(转载)"))
            sprintf(buf4, "%s (转载)", title);
        else
            strcpy(buf4, title);
    } else
        strcpy(buf4, title);
    strncpy(save_title, buf4, STRLEN);

    setbfile(filepath, toboard, "");

    if ((aborted = GET_POSTFILENAME(postfile.filename, filepath)) != 0) {
#ifdef BBSMAIN
        move(3, 0);
        clrtobot();
        prints("\n\n无法创建文件:%d...\n", aborted);
        pressreturn();
        clear();
#endif
        return FULLUPDATE;
    }

    if (mode == 1)
        strcpy(whopost, "deliver");     /* mode==1为自动发信 */
    else
        strcpy(whopost, user->userid);

    strncpy(postfile.owner, whopost, OWNER_LEN);
    postfile.owner[OWNER_LEN-1]=1;
    setbfile(filepath, toboard, postfile.filename);

    local_article = 1; /* default is local article */
    if (islocal != 'l' && islocal != 'L')
	{
		if (is_outgo_board(toboard))
			local_article = 0;
	}

#ifdef BBSMAIN
    modify_user_mode(POSTING);
#endif
    getcross(filepath, filename, user, in_mail, fromboard, title, Anony, mode, toboard);        /*根据fname完成 文件复制 */

    /*
     * Changed by KCN,disable color title 
     */
    if (mode != 1) {
        int i;

        for (i = 0; (i < strlen(save_title)) && (i < STRLEN - 1); i++)
            if (save_title[i] == 0x1b)
                postfile.title[i] = ' ';
            else
                postfile.title[i] = save_title[i];
        postfile.title[i] = 0;
    } else
        strncpy(postfile.title, save_title, STRLEN);
    if (local_article == 1) {   /* local save */
        postfile.innflag[1] = 'L';
        postfile.innflag[0] = 'L';
    } else {
        postfile.innflag[1] = 'S';
        postfile.innflag[0] = 'S';
        outgo_post(&postfile, toboard, save_title);
    }
    /*
     * setbdir(digestmode, buf, currboard );Haohmaru.99.11.26.改成下面一行，因为不管是转贴还是自动发文都不会用到文摘模式 
     */
    if (!strcmp(toboard, "syssecurity")
        && strstr(title, "修改 ")
        && strstr(title, " 的权限"))
        postfile.accessed[0] |= FILE_MARKED;    /* Leeward 98.03.29 */
    if (strstr(title, "发文权限") && mode == 2) {
#ifndef NINE_BUILD
        postfile.accessed[0] |= FILE_MARKED;    /* Haohmaru 99.11.10 */
        postfile.accessed[1] |= FILE_READ;
#endif
        postfile.accessed[0] |= FILE_FORWARDED;
    }
    after_post(user, &postfile, toboard, NULL);
    return 1;
}


int post_file(struct userec *user, char *fromboard, char *filename, char *nboard, char *posttitle, int Anony, int mode)
/* 将某文件 POST 在某版 */
{
    if (getboardnum(nboard, NULL) <= 0) {       /* 搜索要POST的版 ,判断是否存在该版 */
        return -1;
    }
    post_cross(user, nboard, fromboard, posttitle, filename, Anony, false, 'l', mode);  /* post 文件 */
    return 0;
}

int after_post(struct userec *user, struct fileheader *fh, char *boardname, struct fileheader *re)
{
    char buf[256];
    int fd, err = 0, nowid = 0;
    char* p;
#ifdef FILTER
    char oldpath[50], newpath[50];
    int filtered;
    struct boardheader* bh;
#endif

    if ((re == NULL) && (!strncmp(fh->title, "Re:", 3))) {
        strncpy(fh->title, fh->title + 4, STRLEN);
    }
#ifdef FILTER
	setbfile(oldpath, boardname, fh->filename);
    filtered=0;
    bh=getbcache(boardname);
    if (strcmp(fh->owner,"deliver")) {
	if (((bh&&bh->level & PERM_POSTMASK) || normal_board(boardname)) && strcmp(boardname, FILTER_BOARD))
	{
	    if (!strcmp(boardname,"News")||check_badword_str(fh->title,strlen(fh->title))||check_badword(oldpath)) {
			/* FIXME: There is a potential bug here. */
			setbfile(newpath, FILTER_BOARD, fh->filename);
			f_mv(oldpath, newpath);
			strncpy(fh->o_board, boardname, STRLEN - BM_LEN);
			nowid = get_nextid(boardname);
			fh->o_id = nowid;
			if (re == NULL) {
				fh->o_groupid = fh->o_id;
				fh->o_reid = fh->o_id;
			} else {
				fh->o_groupid = re->groupid;
				fh->o_reid = re->id;
			}
			boardname = FILTER_BOARD;
			filtered=1;
	    };
	}
    }
#endif
    setbfile(buf, boardname, DOT_DIR);

    if ((fd = open(buf, O_WRONLY | O_CREAT, 0664)) == -1) {
#ifdef BBSMAIN
        perror(buf);
#endif
        err = 1;
    }
    /*过滤彩色标题*/
    for (p=fh->title;*p;p++) if (*p=='\x1b') *p=' ';
	  
    if (!err) {
        flock(fd, LOCK_EX);
        nowid = get_nextid(boardname);
        fh->id = nowid;
        if (re == NULL) {
            fh->groupid = fh->id;
            fh->reid = fh->id;
        } else {
            fh->groupid = re->groupid;
            fh->reid = re->id;
        }
		set_posttime(fh);
        lseek(fd, 0, SEEK_END);
        if (safewrite(fd, fh, sizeof(fileheader)) == -1) {
            bbslog("user","%s","apprec write err!");
            err = 1;
        }
        flock(fd, LOCK_UN);
        close(fd);
    }
    if (err) {
        bbslog("3error", "Posting '%s' on '%s': append_record failed!", fh->title, boardname);
        setbfile(buf, boardname, fh->filename);
        unlink(buf);
#ifdef BBSMAIN
        pressreturn();
        clear();
#endif
        return 1;
    }
    updatelastpost(boardname);
#ifdef FILTER
    if (filtered)
    	sprintf(buf, "posted '%s' on '%s' filtered", fh->title, fh->o_board);
    else {
#endif
    brc_add_read(fh->id);
    sprintf(buf, "posted '%s' on '%s'", fh->title, boardname);
#ifdef FILTER
    }
#endif
    newbbslog(BBSLOG_USER, "%s", buf);

    if (fh->id == fh->groupid)
        setboardorigin(boardname, 1);
    setboardtitle(boardname, 1);
    if (fh->accessed[0] & FILE_MARKED)
        setboardmark(boardname, 1);
    if (user != NULL)
        bmlog(user->userid, boardname, 2, 1);
#ifdef FILTER
    if (filtered)
	    return 2;
    else
#endif
    return 0;
}

int dele_digest(char *dname, char *direc)
{                               /* 删除文摘内一篇POST, dname=post文件名,direc=文摘目录名 */
    char digest_name[STRLEN];
    char new_dir[STRLEN];
    char buf[STRLEN];
    char *ptr;
    struct fileheader fh;
    int pos;

    strcpy(digest_name, dname);
    strcpy(new_dir, direc);

    digest_name[0] = 'G';
    ptr = strrchr(new_dir, '/') + 1;
    strcpy(ptr, DIGEST_DIR);
    pos = search_record(new_dir, &fh, sizeof(fh), (RECORD_FUNC_ARG) cmpname, digest_name);      /* 文摘目录下 .DIR中 搜索 该POST */
    if (pos <= 0) {
        return -1;
    }
    delete_record(new_dir, sizeof(struct fileheader), pos, (RECORD_FUNC_ARG) cmpname, digest_name);
    *ptr = '\0';
    sprintf(buf, "%s%s", new_dir, digest_name);
    unlink(buf);
    return 0;
}

int mmap_search_apply(int fd, struct fileheader *buf, DIR_APPLY_FUNC func)
{
    struct fileheader *data;
    size_t filesize;
    int total;
    int low, high;
    int ret;

    if (flock(fd, LOCK_EX) == -1)
        return 0;
    BBS_TRY {
        if (safe_mmapfile_handle(fd, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, (void **) &data, &filesize) == 0) {
            flock(fd, LOCK_UN);
            BBS_RETURN(0);
        }
        total = filesize / sizeof(struct fileheader);
        low = 0;
        high = total - 1;
        while (low <= high) {
            int mid, comp;

            mid = (high + low) / 2;
            comp = (buf->id) - ((data + mid)->id);
            if (comp == 0) {
                ret = (*func) (fd, data, mid + 1, total, data, true);
                end_mmapfile((void *) data, filesize, -1);
                flock(fd, LOCK_UN);
                BBS_RETURN(ret);
            } else if (comp < 0)
                high = mid - 1;
            else
                low = mid + 1;
        }
        ret = (*func) (fd, data, low + 1, total, buf, false);
    }
    BBS_CATCH {
    }
    BBS_END end_mmapfile((void *) data, filesize, -1);
    flock(fd, LOCK_UN);
    return ret;
}

int change_dir_post_flag(struct userec *currentuser, char *currboard, int ent, struct fileheader *fileinfo, int flag)
{
    /*---	---*/
    int newent = 0, ret = 1;
    char *ptr, buf[STRLEN];
    char ans[256];
    char genbuf[1024], direct[256];
    struct fileheader mkpost;
    struct flock ldata;
    int fd, size = sizeof(fileheader);

    setbdir(0, direct, currboard);
    strcpy(buf, direct);
    ptr = strrchr(buf, '/') + 1;
    ptr[0] = '\0';
    sprintf(&genbuf[512], "%s%s", buf, fileinfo->filename);
    if (!dashf(&genbuf[512]))
        ret = 0;                /* 借用一下newent :PP   */

    if (ret)
        if ((fd = open(direct, O_RDWR | O_CREAT, 0644)) == -1)
            ret = 0;
    if (ret) {
        ldata.l_type = F_RDLCK;
        ldata.l_whence = 0;
        ldata.l_len = size;
        ldata.l_start = size * (ent - 1);
        if (fcntl(fd, F_SETLKW, &ldata) == -1) {
            bbslog("user","%s","reclock error");
            close(fd);
                                /*---	period	2000-10-20	file should be closed	---*/
            ret = 0;
        }
    }
    if (ret) {
        if (lseek(fd, size * (ent - 1), SEEK_SET) == -1) {
            bbslog("user","%s","subrec seek err");
            /*---	period	2000-10-24	---*/
            ldata.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &ldata);
            close(fd);
            ret = 0;
        }
    }
    if (ret) {
        if (get_record_handle(fd, &mkpost, sizeof(mkpost), ent) == -1) {
            bbslog("user","%s","subrec read err");
            ret = 0;
        }
        if (ret)
            if (strcmp(mkpost.filename, fileinfo->filename))
                ret = 0;
        if (!ret) {
            newent = search_record_back(fd, sizeof(struct fileheader), ent, (RECORD_FUNC_ARG) cmpfileinfoname, fileinfo->filename, &mkpost, 1);
            ret = (newent > 0);
            if (ret)
                memcpy(fileinfo, &mkpost, sizeof(mkpost));
            else {
                ldata.l_type = F_UNLCK;
                fcntl(fd, F_SETLK, &ldata);
                close(fd);
            }
            ent = newent;
        }
    }
    if (!ret)
        return DIRCHANGED;
    switch (flag) {
    case FILE_MARK_FLAG:
        if (fileinfo->accessed[0] & FILE_MARKED)
            fileinfo->accessed[0] = (fileinfo->accessed[0] & ~FILE_MARKED);
        else
            fileinfo->accessed[0] = fileinfo->accessed[0] | FILE_MARKED;
        break;
#ifdef FILTER
    case FILE_CENSOR_FLAG:
	if (fileinfo->accessed[1] & FILE_CENSOR)
	    fileinfo->accessed[1] &= ~FILE_CENSOR;
	else
	    fileinfo->accessed[1] |= FILE_CENSOR;
	break;
#endif
    case FILE_NOREPLY_FLAG:
        if (fileinfo->accessed[1] & FILE_READ)
            fileinfo->accessed[1] &= ~FILE_READ;
        else
            fileinfo->accessed[1] |= FILE_READ;
        break;
    case FILE_SIGN_FLAG:
        if (fileinfo->accessed[0] & FILE_SIGN)
            fileinfo->accessed[0] &= ~FILE_SIGN;
        else
            fileinfo->accessed[0] |= FILE_SIGN;
        break;
    case FILE_DELETE_FLAG:
        if (fileinfo->accessed[1] & FILE_DEL)
            fileinfo->accessed[1] &= ~FILE_DEL;
        else
            fileinfo->accessed[1] |= FILE_DEL;
        break;
    case FILE_DIGEST_FLAG:
        if (fileinfo->accessed[0] & FILE_DIGEST)
            fileinfo->accessed[0] = (fileinfo->accessed[0] & ~FILE_DIGEST);
        else
            fileinfo->accessed[0] = fileinfo->accessed[0] | FILE_DIGEST;
        break;
    case FILE_IMPORT_FLAG:
        fileinfo->accessed[0] |= FILE_IMPORTED;
        break;
    }

    if (lseek(fd, size * (ent - 1), SEEK_SET) == -1) {
        bbslog("user","%s","subrec seek err");
        ldata.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &ldata);
        close(fd);
        return DONOTHING;
    }
    if (safewrite(fd, fileinfo, size) != size) {
        bbslog("user","%s","subrec write err");
        ldata.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &ldata);
        close(fd);
        return DONOTHING;
    }

    ldata.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &ldata);
    close(fd);

    return newent ? DIRCHANGED : PARTUPDATE;
}

int change_post_flag(char *currBM, struct userec *currentuser, int digestmode, char *currboard, int ent, struct fileheader *fileinfo, char *direct, int flag, int prompt)
{
    /*---	---*/
    int newent = 0, ret = 1;
    char *ptr, buf[STRLEN];
    char ans[256];
    char genbuf[1024];
    struct fileheader mkpost, mkpost2;
    struct flock ldata;
    int fd, size = sizeof(fileheader), orgent;
#ifdef FILTER
    int filedes;
    int nowid = 0;
    char oldpath[50], newpath[50], buffer[256];
    struct fileheader *newfh = fileinfo;
#endif

    /*---	---*/

    if (!chk_currBM(currBM, currentuser))
        return DONOTHING;

    if (flag == FILE_DIGEST_FLAG && (digestmode == 1 || digestmode == 4 || digestmode == 5))
        return DONOTHING;
    if (flag == FILE_MARK_FLAG && (digestmode == 1 || digestmode == 4 || digestmode == 5))
        return DONOTHING;
    if (flag == FILE_IMPORT_FLAG && (digestmode == 4 || digestmode == 5))
        return DONOTHING;
    if (flag == FILE_DELETE_FLAG && (digestmode == 4 || digestmode == 5))
        return DONOTHING;
    if ((flag == FILE_MARK_FLAG || flag == FILE_DELETE_FLAG) && (!strcmp(currboard, "syssecurity")
		 || !strcmp(currboard, FILTER_BOARD)))
        return DONOTHING;       /* Leeward 98.03.29 */
    /*
     * Haohmaru.98.10.12.主题模式下不允许mark文章 
     */
    if (flag == FILE_TITLE_FLAG && digestmode != 0)
        return DONOTHING;
    if (flag == FILE_NOREPLY_FLAG && digestmode != 0)
        return DONOTHING;

    if ((digestmode != DIR_MODE_NORMAL) && (digestmode != DIR_MODE_DIGEST)) {
        setbdir(0, genbuf, currboard);
        orgent = search_record(genbuf, &mkpost2, sizeof(struct fileheader), (RECORD_FUNC_ARG) cmpfileinfoname, fileinfo->filename);
        if (!orgent) {
#ifdef BBSMAIN
            move(2, 0);
            prints(" 该文件可能已经被删除\n");
            clrtobot();
            pressreturn();
#endif
            return FULLUPDATE;
        }
    }
    strcpy(buf, direct);
    ptr = strrchr(buf, '/') + 1;
    ptr[0] = '\0';
    sprintf(&genbuf[512], "%s%s", buf, fileinfo->filename);
    if (!dashf(&genbuf[512]))
        ret = 0;                /* 借用一下newent :PP   */

    if (ret)
        if ((fd = open(direct, O_RDWR | O_CREAT, 0644)) == -1)
            ret = 0;
    if (ret) {
        ldata.l_type = F_RDLCK;
        ldata.l_whence = 0;
        ldata.l_len = size;
        ldata.l_start = size * (ent - 1);
        if (fcntl(fd, F_SETLKW, &ldata) == -1) {
            bbslog("user","%s","reclock error");
            close(fd);
                                /*---	period	2000-10-20	file should be closed	---*/
            ret = 0;
        }
    }
    if (ret) {
        if (lseek(fd, size * (ent - 1), SEEK_SET) == -1) {
            bbslog("user","%s","subrec seek err");
            /*---	period	2000-10-24	---*/
            ldata.l_type = F_UNLCK;
            fcntl(fd, F_SETLK, &ldata);
            close(fd);
            ret = 0;
        }
    }
    if (ret) {
        if (get_record_handle(fd, &mkpost, sizeof(mkpost), ent) == -1) {
            bbslog("user","%s","subrec read err");
            ret = 0;
        }
        if (ret)
            if (strcmp(mkpost.filename, fileinfo->filename))
                ret = 0;
        if (!ret) {
            newent = search_record_back(fd, sizeof(struct fileheader), ent, (RECORD_FUNC_ARG) cmpfileinfoname, fileinfo->filename, &mkpost, 1);
            ret = (newent > 0);
            if (ret)
                memcpy(fileinfo, &mkpost, sizeof(mkpost));
            else {
                ldata.l_type = F_UNLCK;
                fcntl(fd, F_SETLK, &ldata);
                close(fd);
            }
            ent = newent;
        }
    }
    if (!ret) {
#ifdef BBSMAIN
        move(2, 0);
        prints(" 文章列表发生变动，文章[%s]可能已被删除．\n", fileinfo->title);
        clrtobot();
        pressreturn();
#endif
        return DIRCHANGED;
    }
    switch (flag) {
    case FILE_MARK_FLAG:
        if (fileinfo->accessed[0] & FILE_MARKED) {      //added by bad 2002.8.7 mark file mode added
            fileinfo->accessed[0] = (fileinfo->accessed[0] & ~FILE_MARKED);
            bmlog(currentuser->userid, currboard, 7, 1);
        } else {
            fileinfo->accessed[0] = fileinfo->accessed[0] | FILE_MARKED;
            bmlog(currentuser->userid, currboard, 6, 1);
        }
        setboardmark(currboard, 1);
        break;
    case FILE_NOREPLY_FLAG:
        if (fileinfo->accessed[1] & FILE_READ) {
            fileinfo->accessed[1] &= ~FILE_READ;
#ifdef BBSMAIN
            if (prompt)
                a_prompt(-1, " 该文章已取消不可re模式, 请按 Enter 继续 << ", ans);
#endif
        } else {
            fileinfo->accessed[1] |= FILE_READ;
#ifdef BBSMAIN
            if (prompt)
                a_prompt(-1, " 该文章已设为不可re模式, 请按 Enter 继续 << ", ans);
#endif
            /*
             * Bigman:2000.8.29 sysmail版处理添加版务姓名 
             */
            if (!strcmp(currboard, SYSMAIL_BOARD)) {
                sprintf(ans, "〖%s〗 处理: %s", currentuser->userid, fileinfo->title);
                strncpy(fileinfo->title, ans, STRLEN);
                fileinfo->title[STRLEN - 1] = 0;
            }
        }
        break;
    case FILE_SIGN_FLAG:
        if (fileinfo->accessed[0] & FILE_SIGN) {
            fileinfo->accessed[0] &= ~FILE_SIGN;
#ifdef BBSMAIN
            if (prompt)
                a_prompt(-1, " 该文章已撤消标记模式, 请按 Enter 继续 << ", ans);
#endif
        } else {
            fileinfo->accessed[0] |= FILE_SIGN;
#ifdef BBSMAIN
            if (prompt)
                a_prompt(-1, " 该文章已设为标记模式, 请按 Enter 继续 << ", ans);
#endif
        }
        break;
#ifdef FILTER
    case FILE_CENSOR_FLAG:
	if (!strcmp(currboard, FILTER_BOARD))
	{
		if (fileinfo->accessed[1] & FILE_CENSOR) {
#ifdef BBSMAIN
			if (prompt)
			a_prompt(-1, " 该文章已经通过审核, 请按 Enter 继续 << ", ans);
#endif
		} else {
			fileinfo->accessed[1] |= FILE_CENSOR;
			setbfile(oldpath, FILTER_BOARD, fileinfo->filename);
			setbfile(newpath, fileinfo->o_board, fileinfo->filename);
			f_cp(oldpath, newpath, 0);

			setbfile(buffer, fileinfo->o_board, DOT_DIR);
			if ((filedes = open(buffer, O_WRONLY | O_CREAT, 0664)) == -1) {
#ifdef BBSMAIN
                perror(buffer);
#endif  
			}
			flock(filedes, LOCK_EX);
			nowid = get_nextid(fileinfo->o_board);
			newfh->id = nowid;
			if (fileinfo->o_id == fileinfo->o_groupid)
				newfh->groupid = newfh->reid = newfh->id;
			else {
				newfh->groupid = fileinfo->o_groupid;
				newfh->reid = fileinfo->o_reid;
			}
			lseek(filedes, 0, SEEK_END);
			if (safewrite(filedes, newfh, sizeof(fileheader)) == -1) {
				bbslog("user","%s","apprec write err!");
			}
			flock(filedes, LOCK_UN);
			close(filedes);
			updatelastpost(fileinfo->o_board);
			brc_add_read(newfh->id);
			if (newfh->id == newfh->groupid)
				setboardorigin(fileinfo->o_board, 1);
			setboardtitle(fileinfo->o_board, 1);
			if (newfh->accessed[0] & FILE_MARKED)
				setboardmark(fileinfo->o_board, 1);
		}
	}
	break;
#endif /* FILTER */
    case FILE_DELETE_FLAG:
        if (fileinfo->accessed[1] & FILE_DEL)
            fileinfo->accessed[1] &= ~FILE_DEL;
        else
            fileinfo->accessed[1] |= FILE_DEL;
        break;
    case FILE_DIGEST_FLAG:
        if (fileinfo->accessed[0] & FILE_DIGEST) {      /* 如果已经是文摘的话，则从文摘中删除该post */
            fileinfo->accessed[0] = (fileinfo->accessed[0] & ~FILE_DIGEST);
            bmlog(currentuser->userid, currboard, 4, 1);
            dele_digest(fileinfo->filename, direct);
        } else {
            struct fileheader digest;
            char *ptr, buf[64];

            memcpy(&digest, fileinfo, sizeof(digest));
            if (digestmode)
                strncpy(digest.title, mkpost2.title, STRLEN);
            digest.filename[0] = 'G';
            strcpy(buf, direct);
            ptr = strrchr(buf, '/') + 1;
            ptr[0] = '\0';
            sprintf(genbuf, "%s%s", buf, digest.filename);
            bmlog(currentuser->userid, currboard, 3, 1);
            if (dashf(genbuf)) {
                fileinfo->accessed[0] = fileinfo->accessed[0] | FILE_DIGEST;
            } else {
                digest.accessed[0] = 0;
                sprintf(&genbuf[512], "%s%s", buf, fileinfo->filename);
                link(&genbuf[512], genbuf);
                strcpy(ptr, DIGEST_DIR);
                if (get_num_records(buf, sizeof(digest)) > MAX_DIGEST) {
                    ldata.l_type = F_UNLCK;
                    fcntl(fd, F_SETLK, &ldata);
                    close(fd);
#ifdef BBSMAIN
                    move(3, 0);
                    clrtobot();
                    move(4, 10);
                    prints("抱歉，你的文摘文章已经超过 %d 篇，无法再加入...\n", MAX_DIGEST);
                    pressanykey();
#endif
                    return PARTUPDATE;
                }
                append_record(buf, &digest, sizeof(digest));    /* 文摘目录下添加 .DIR */
                fileinfo->accessed[0] = fileinfo->accessed[0] | FILE_DIGEST;
            }
        }
        break;
    case FILE_TITLE_FLAG:
        fileinfo->groupid = fileinfo->id;
        fileinfo->reid = fileinfo->id;
        if (!strncmp(fileinfo->title, "Re:", 3)) {
            strcpy(buf, fileinfo->title + 4);
            strcpy(fileinfo->title, buf);
        }
        break;
    case FILE_IMPORT_FLAG:
        fileinfo->accessed[0] |= FILE_IMPORTED;
        break;
    }

    if (lseek(fd, size * (ent - 1), SEEK_SET) == -1) {
        bbslog("user","%s","subrec seek err");
        ldata.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &ldata);
        close(fd);
        return DONOTHING;
    }
    if (safewrite(fd, fileinfo, size) != size) {
        bbslog("user","%s","subrec write err");
        ldata.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &ldata);
        close(fd);
        return DONOTHING;
    }

    ldata.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &ldata);
    close(fd);
    if ((digestmode != DIR_MODE_NORMAL) && (DIR_MODE_DIGEST))
        change_dir_post_flag(currentuser, currboard, orgent, &mkpost2, flag);

    return newent ? DIRCHANGED : PARTUPDATE;
}

char get_article_flag(struct fileheader *ent, struct userec *user, char* boardname,int is_bm)
{
    char unread_mark = (DEFINE(user, DEF_UNREADMARK) ? '*' : 'N');
    char type;

    if (strcmp(user->userid,"guest"))
        type = brc_unread(ent->id) ? unread_mark : ' ';
    else
        type = ' ';
    if ((ent->accessed[0] & FILE_DIGEST)) {
        if (type == ' ')
            type = 'g';
        else
            type = 'G';
    }
    if (ent->accessed[0] & FILE_MARKED) {
        switch (type) {
        case ' ':
            type = 'm';
            break;
        case '*':
        case 'N':
            type = 'M';
            break;
        case 'g':
            type = 'b';
            break;
        case 'G':
            type = 'B';
            break;
        }
    }
    if (is_bm && (ent->accessed[1] & FILE_READ)) {
        switch (type) {
        case 'g':
        case 'G':
            type = 'O';
            break;
        case 'm':
        case 'M':
            type = 'U';
            break;
        case 'b':
        case 'B':
            type = '8';
            break;
        case ' ':
        case '*':
        case 'N':
        default:
            type = ';';
            break;
        }
    } else if ((is_bm || HAS_PERM(user, PERM_OBOARDS)) && (ent->accessed[0] & FILE_SIGN)) {
        type = '#';
#ifdef FILTER
    } else if (HAS_PERM(user, PERM_OBOARDS) && (ent->accessed[1] & FILE_CENSOR)&&!strcmp(boardname,FILTER_BOARD)) {
	type = '@';
#endif
    }

    if (is_bm && (ent->accessed[1] & FILE_DEL)) {
        type = 'X';
    }

    return type;
}
