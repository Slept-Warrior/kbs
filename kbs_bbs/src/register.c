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

#define  EMAIL          0x0001 
#define  NICK           0x0002 
#define  REALNAME       0x0004 
#define  ADDR           0x0008
#define  REALEMAIL      0x0010
#define  BADEMAIL       0x0020
#define  NEWREG         0x0040

char    *sysconf_str();
char    *Ctime();

extern char     fromhost[ 60 ];
extern time_t   login_start_time;
time_t          system_time;
extern int convcode;  /* KCN,99.09.05 */
extern int switch_code(); /* KCN,99.09.05 */

int
bad_user_id( userid )
char    *userid;
{
    FILE        *fp;
    char        buf[STRLEN];
    char        *ptr, ch;
    int         i;

    i = 0;
    ptr = userid;
    while( (ch = *ptr++) != '\0' ) {
        i++;
        if( !isalnum( ch ) && ch != '_' )
            return 1;
    }
    if (i<2) return 1;
    if( (fp = fopen( ".badname", "r" )) != NULL ) {
        while( fgets( buf, STRLEN, fp ) != NULL ) {
            ptr = strtok( buf, " \n\t\r" );
            if( ptr != NULL && *ptr != '#')
            {
                if( strcasecmp( ptr, userid ) == 0 ) {
                    if(ptr[13]>47 && ptr[13]<58)/*Haohmaru.99.12.24*/
                    {
                        char timebuf[9];
                        time_t	t,now;
                        strncpy(timebuf,ptr+13,9);
                        t = atol(timebuf);
                        now = time(0);
                        if(now - t > 24*30*3600)
                        {
                            fclose( fp );
                            return 0;
                        }
                    }
                    fclose(fp);
                    return 1;
                }
            }
        }
        fclose(fp);
    }
    return 0;
}

int
compute_user_value( urec )
struct userec *urec;
{
    int         value;

    /* if (urec) has CHATCLOAK permission, don't kick it */
    if( (urec->userlevel & PERM_CHATCLOAK ) && (!(urec->userlevel & PERM_SUICIDE)))
        return 999;


    value = (time(0) - urec->lastlogin) / 60;    /* min */
    if (0 == value) value = 1; /* Leeward 98.03.30 */

    /* 修改: 将永久帐号转为长期帐号, Bigman 2000.8.11 */
    if ((urec->userlevel & PERM_XEMPT) && (!(urec->userlevel & PERM_SUICIDE)) )
    {	if (urec->lastlogin < 988610030)
        return 666; /* 如果没有登录过的 */
        else
            return (667 * 24 * 60 - value)/(60*24);
    }
    /* new user should register in 30 mins */
    if( strcmp( urec->userid, "new" ) == 0 ) {
        return (30 - value) / 60; /* *->/ modified by dong, 1998.12.3 */
    }

    /* 自杀功能,Luzi 1998.10.10 */
    if (urec->userlevel & PERM_SUICIDE)
        return (15 * 24 * 60 - value)/(60*24);
    /**********************/
    if(urec->numlogins <= 3)
        return (15 * 24 * 60 - value)/(60*24);
    if( !(urec->userlevel & PERM_LOGINOK) )
        return (30 * 24 * 60 - value)/(60*24);
    /* if (urec->userlevel & PERM_LONGID)
         return (667 * 24 * 60 - value)/(60*24); */
    return (120 * 24 * 60 - value)/(60*24);
}

int
id_invalid(userid)
char userid[IDLEN];
{
    char *s;
    if (!isalpha(userid[0]))
        return 1;
    for(s=userid;*s != '\0'; s++) {
        if(*s<1 || !isalnum(*s)) {
            return 1;
        }
    }
    return 0;
}
void
new_register()
{
    struct userec       newuser;
    int    allocid, do_try,flag,lockfd;
    char   buf[STRLEN];


    memset( &newuser, 0, sizeof(newuser) );
    getdata(0, 0, "使用GB编码阅读?(\xa8\xcf\xa5\xce BIG5\xbd\x58\xbe\x5c\xc5\xaa\xbd\xd0\xbf\xefN)(Y/N)? [Y]: ", buf, 4, DOECHO, NULL, YEA);
    if (*buf == 'n' || *buf == 'N')
        if (!convcode)
            switch_code();

    ansimore("etc/register", NA);
    do_try = 0;
    while( 1 ) {
        if( ++do_try >= 10 ) {
            prints("\n掰掰，按太多下  <Enter> 了...\n");
            refresh();
            longjmp( byebye, -1 );
        }
        getdata(0,0,"请输入代号: ",newuser.userid,IDLEN+1,DOECHO,NULL,YEA);
        flag = 1;
        if(id_invalid(newuser.userid)==1)
        {
            prints("帐号必须由英文字母或数字组成，并且第一个字符必须是英文字母!\n");
            /*                prints("帐号必须由英文字母或数字，而且帐号第一个字是英文字母!\n");*/
            flag=0;
        }
        if (flag) {
	    if(strlen(newuser.userid) < 2) {
                prints("代号至少需有两个英文字母!\n");
            } else if ( (*newuser.userid == '\0') || bad_user_id( newuser.userid )){
                prints( "系统用字或是不雅的代号。\n" );
            } else if((usernum = searchuser( newuser.userid )) != 0) /*( dosearchuser( newuser.userid ) ) midified by dong , 1998.12.2, change getuser -> searchuser , 1999.10.26*/
	    {
                prints("此帐号已经有人使用\n") ;
            } else {
		/*---	---*/
	      	struct stat lst;
		time_t lnow;
		lnow = time(NULL);
		sethomepath( genbuf, newuser.userid );
		if(!stat(genbuf, &lst) && S_ISDIR(lst.st_mode)
			&& (lnow - lst.st_ctime < 2592000/* 3600*24*30 */) ) {
			prints("目前无法注册帐号%s，请与系统管理人员联系。\n", newuser.userid);
			sprintf(genbuf, "IP %s new id %s failed[home changed in past 30 days]",
					fromhost, newuser.userid);
			report(genbuf);
		} else
		/*---	---*/
		break;
            }
	}
    }

    newuser.firstlogin = newuser.lastlogin = time(NULL) - 13 * 60 * 24 ;
    while( 1 ) {
        char  passbuf[ STRLEN ], passbuf2[ STRLEN ];
        getdata(0,0,"请设定您的密码: ",passbuf,39,NOECHO,NULL,YEA) ;
        if( strlen( passbuf ) < 4 || !strcmp( passbuf, newuser.userid ) ) {
            prints("密码太短或与使用者代号相同, 请重新输入\n") ;
            continue;
        }
        getdata(0,0,"请再输入一次你的密码: ",passbuf2,39,NOECHO,NULL,YEA);
        if( strcmp( passbuf, passbuf2) != 0 ) {
            prints("密码输入错误, 请重新输入密码.\n") ;
            continue;
        }
        
        setpasswd(passbuf,&newuser);
        break;
    }
    newuser.userlevel = PERM_BASIC;
    newuser.userdefine=-1;
    newuser.userdefine&=~DEF_MAILMSG;
    newuser.userdefine&=~DEF_EDITMSG;
    if (convcode)
        newuser.userdefine&=~DEF_USEGB;

    newuser.notemode=-1;
    newuser.unuse1=-1;
    newuser.unuse2=-1;
    newuser.flags[0] = CURSOR_FLAG;
    newuser.flags[0] |= PAGER_FLAG;
    newuser.flags[1] = 0;
    newuser.firstlogin = newuser.lastlogin = time(NULL) ;

    allocid = getnewuserid(newuser.userid)  ;
    if(allocid > MAXUSERS || allocid <= 0) {
        prints("No space for new users on the system!\n\r") ;
        oflush();
	    exit(1) ;
    }

	update_user(&newuser,allocid,1);

    if( !dosearchuser(newuser.userid) ) {
        /* change by KCN 1999.09.08
                fprintf(stderr,"User failed to create\n") ;
        */
        prints("User failed to create %d-%s\n",allocid,newuser.userid) ;
        oflush();
        exit(1) ;
    }
    report( "new account" );
}

char *
trim( s )
char *s;
{
    static char buf[ 256 ];
    char *l, *r;

    buf[ 0 ] = '\0' ;
    r = s + strlen( s ) - 1;

    for (l = s ; strchr(" \t\r\n", *l) && *l; l++);

    /* if all space, *l is null here, we just return null */
    if (*l != '\0') {
        for ( ; strchr(" \t\r\n", *r) && r >= l ; r-- );
        strncpy( buf, l, r - l + 1 );
    }
    return buf;
}

int
invalid_realmail( userid, email, msize )
char    *userid, *email;
int     msize;
{
    FILE        *fn;
    char        *emailfile, ans[4],fname[STRLEN];

    if( (emailfile = sysconf_str( "EMAILFILE" )) == NULL )
        return 0;

    if( strchr( email, '@' ) && valid_ident( email ) )
        return 0;
    /*
        ansimore( emailfile, NA );
        getdata(t_lines-1,0,"您要现在 email-post 吗? (Y/N) [Y]: ",
            ans,2,DOECHO,NULL,YEA);
        while( *ans != 'n' && *ans != 'N' ) {
    */
    sprintf( fname, "tmp/email_%s", userid );
    if( (fn = fopen( fname, "r" )) != NULL ) {
        fgets( genbuf, STRLEN, fn );
        fclose( fn );
        strtok( genbuf, "\n" );
        if (!valid_ident( genbuf )) {
        } else if( strchr( genbuf, '@' ) != NULL ) {
            unlink(fname);
            strncpy( email, genbuf, msize );
            move( 10, 0 );
            prints( "恭贺您!! 您已通过身份验证, 成为本站公民. \n" );
            prints( "         本站为您所提供的额外服务, \n" );
            prints( "         包括Mail,Post,Message,Talk 等. \n" );
            prints( "  \n" );
            prints( "建议您,  先四处浏览一下, \n" );
            prints( "         不懂的地方, 请在 sysop 板留言, \n" );
            prints( "         本站会派专人为您解答. \n" );
            getdata( 18 ,0, "请按 <Enter>  <<  ", ans,2,DOECHO,NULL ,YEA);
            return 0;
        }
    }
    return 1;
}

void
check_register_info()
{
    char        *newregfile;
    int         perm;
    time_t      code;
    FILE        *fin, *fout,*dp;
    char        ans[4],buf[STRLEN];
    extern int showansi;

    clear();
    sprintf(buf,"%s",email_domain());
    if( !(urec->userlevel & PERM_BASIC) ) {
        urec->userlevel = 0;
        return;
    }
    /*urec->userlevel |= PERM_DEFAULT;*/
    perm = PERM_DEFAULT & sysconf_eval( "AUTOSET_PERM" );

    /*    if( sysconf_str( "IDENTFILE" ) != NULL ) {  commented out by netty to save time */
    while ( strlen( urec->username ) < 2 ) {
        getdata( 2, 0, "请输入您的昵称:(例如,打倒北约) << ", urec->username, NAMELEN,DOECHO,NULL ,YEA);
        strcpy(uinfo.username,urec->username);
        UPDATE_UTMP_STR(username,uinfo);
    }
    if ( strlen( urec->realname ) < 2 ) {
        move( 3, 0 );
        prints( "请输入您的真实姓名: (站长会帮您保密的 !)\n" );
        getdata( 4, 0, "> ", urec->realname, NAMELEN,DOECHO,NULL,YEA);
    }
    if ( strlen( urec->address ) < 6 ) {
        move( 5, 0 );
        prints( "您目前填写的地址是‘%s’，长度小于 [1m[37m6[m，系统认为其过于简短。\n", strlen( urec->address ) > 0 ? urec->address : "空地址" ); /* Leeward 98.04.26 */
        getdata( 6, 0, "请详细填写您的住址：", urec->address, NAMELEN,DOECHO,NULL,YEA);
    }
    if ( strchr( urec->email, '@' ) == NULL ) {
        move( 3, 0 );
        prints( "只有本站的合法公民才能够完全享有各种功能， \n" );
        /* alex           prints( "成为本站合法公民有两种办法：\n\n" );
                    prints( "1. 如果你有合法的email信箱(非BBS), \n");
                                prints( "       你可以用回认证信的方式来通过认证。 \n\n" );
                    prints( "2. 如果你没有email信箱(非BBS)，你可以在进入本站以后，\n" );
                    prints( "       在'个人工具箱'内 详细注册真实身份，\n" );
                    prints( "       SYSOPs 会尽快 检查并确认你的注册单。\n" );
                    move( 17, 0 );
                    prints( "电子信箱格式为: xxx@xxx.xxx.edu.cn \n" );
                    getdata( 18, 0, "请输入电子信箱: (不能提供者按 <Enter>) << "
                                    , urec->email, STRLEN,DOECHO,NULL,YEA);
                    if ((strchr( urec->email, '@' ) == NULL )) { 
                        sprintf( genbuf, "%s.bbs@%s", urec->userid,buf );
                        strncpy( urec->email, genbuf, STRLEN);
                    }
        alex, 因为取消了email功能 , 97.7 */
        prints( "成为本站合法公民的方法如下：\n\n" );
        prints( "你的帐号的第一次登录后的 72 小时内（[1m[33m不是指上 BBS 72小时[m），\n");
        prints( "    你处于新手上路期间, 请四处参观学习网络使用方法和各种礼仪。\n");
        prints( "    在此期间，不能注册成为合法公民。\n\n");
        prints( "过了这开始的 72 小时, 你就可以在进入本站以后，\n" );
        prints( "    在‘个人工具箱’内详细注册真实身份，\n" );
        prints( "    SYSOPs 会尽快检查并确认你的注册单。\n\n" );
        /* Leeward adds below 98.04.26 */
        prints( "[1m[33m如果你已经通过了注册，成为了合法公民，却依然还是看到了本信息，\n那是因为你没有在‘个人工具箱’内设定‘电子邮件信箱’。[m\n请从‘主选单’进入‘个人工具箱’内，再进入‘设定个人资料’一项进行设定。\n如果你实在没有任何可用的‘电子邮件信箱’可以设定，又不愿意看到本信息，\n可以使用 [1m[33m%s.bbs@smth.org[m 进行设定。\n注意∶上面给出的电子邮件信箱不能接收电子邮件，仅仅是用来使系统不再显示本信息。", currentuser->userid);
        pressreturn();
    }
    if(!strcmp(currentuser->userid,"SYSOP"))
    {
        currentuser->userlevel=~0;
        currentuser->userlevel&=~PERM_SUICIDE; /* Leeward 98.10.13 */
        currentuser->userlevel&=~PERM_DENYMAIL; /* Bigman 2000.9.22 */
    }
    if(!(currentuser->userlevel&PERM_LOGINOK))
    {
        if( HAS_PERM( PERM_SYSOP ))
            return;
        if(!invalid_realmail( currentuser->userid, currentuser->realemail, STRLEN-16 ))
        {
            currentuser->userlevel |= PERM_DEFAULT;
            if( HAS_PERM( PERM_DENYPOST ) && !HAS_PERM( PERM_SYSOP ) )
                currentuser->userlevel &= ~PERM_POST;
        }else {
            /* added by netty to automatically send a mail to new user. */
            /* begin of check if local email-addr  */
            /*       if (
                       (!strstr( urec->email, "@bbs.") ) &&
                       (!strstr( urec->email, ".bbs@") )&&
                       (!invalidaddr(urec->email))&&
                       sysconf_str( "EMAILFILE" )!=NULL) 
                   {
                       move( 15, 0 );
                       prints( "您的电子信箱  尚须通过回信验证...  \n" );
                       prints( "      SYSOP 将寄一封验证信给您,\n" );
                       prints( "      您只要回信, 就可以成为本站合格公民.\n" );
                       getdata( 19 ,0, "您要 SYSOP 寄这一封信吗?(Y/N) [Y] << ", ans,2,DOECHO,NULL,YEA);
                       if ( *ans != 'n' && *ans != 'N' ) {
                       code=(time(0)/2)+(rand()/10);
                       sethomefile(genbuf,urec->userid,"mailcheck");
                       if((dp=fopen(genbuf,"w"))==NULL)
                       {
                            fclose(dp);
                            return;
                       }
                       fprintf(dp,"%9.9d\n",code);
                       fclose(dp);
                       sprintf( genbuf, "/usr/lib/sendmail -f SYSOP.bbs@%s %s ", 
                             email_domain(), urec->email );
                       fout = popen( genbuf, "w" );
                       fin  = fopen( sysconf_str( "EMAILFILE" ), "r" );
                       if ((fin != NULL) && (fout != NULL)) {
                           fprintf( fout, "Reply-To: SYSOP.bbs@%s\n", email_domain());
                           fprintf( fout, "From: SYSOP.bbs@%s\n",  email_domain() ); 
                           fprintf( fout, "To: %s\n", urec->email);
                           fprintf( fout, "Subject: @%s@[-%9.9d-]firebird mail check.\n", urec->userid ,code);
                           fprintf( fout, "X-Forwarded-By: SYSOP \n" );
                           fprintf( fout, "X-Disclaimer: None\n");
                           fprintf( fout, "\n");
                           fprintf(fout,"您的基本资料如下：\n",urec->userid);
                           fprintf(fout,"使用者代号：%s (%s)\n",urec->userid,urec->username);
                           fprintf(fout,"姓      名：%s\n",urec->realname);
                           fprintf(fout,"上站位置  ：%s\n",urec->lasthost);
                           fprintf(fout,"电子邮件  ：%s\n\n",urec->email);
                           fprintf(fout,"亲爱的 %s(%s):\n",urec->userid,urec->username);
                           while (fgets( genbuf, 255, fin ) != NULL ) {
                               if (genbuf[0] == '.' && genbuf[ 1 ] == '\n')
                                    fputs( ". \n", fout );
                               else fputs( genbuf, fout );
                           }
                           fprintf(fout, ".\n");                                    
                           fclose( fin );
                           fclose( fout );                                     
                       }
                       getdata( 20 ,0, "信已寄出, SYSOP 将等您回信哦!! 请按 <Enter> << ", ans,2,DOECHO,NULL ,YEA);
                       }
                   }else
                   {
                    showansi=1;
                    if(sysconf_str( "EMAILFILE" )!=NULL)
                    {
                      prints("\n你的电子邮件地址 【[33m%s[m】\n",urec->email);
                      prints("并非 Unix 帐号，系统不会投递身份确认信，请到[32m工具箱[m中修改..\n");
                      pressanykey();
                    }
                   }
            deleted by alex, remove email certify */

            clear(); /* Leeward 98.05.14 */
            move( 12, 0 );
            prints( "你还没有通过身份认证，将会没有talk,mail,message,post权...  \n" );
            prints( "如果你要成为本站的注册用户，\n\n");
            prints( "请在[31m个人工具箱[0m内[31m详细注册身份[0m\n" );
            prints( "您只要确实详细填写了你的真实身份资料, \n");
            prints( "在站长替你手工认证了以后，就可以成为本站合格公民.\n" );
            pressreturn();
        }
        /* end of check if local email-addr */
        /*  above lines added by netty...  */
    }
    newregfile = sysconf_str( "NEWREGFILE" );
    if( currentuser->lastlogin - currentuser->firstlogin < 3*86400 &&
            !HAS_PERM( PERM_SYSOP) && newregfile != NULL ) {
        currentuser->userlevel &= ~(perm);
        ansimore( newregfile, YEA );
    }
    if( HAS_PERM( PERM_DENYPOST ) && !HAS_PERM( PERM_SYSOP ) )
    {
        currentuser->userlevel &= ~PERM_POST;
    }
}

