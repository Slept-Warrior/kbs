/*
 * $Id$
 */
#include "bbslib.h"

int main()
{
    FILE *fp;
    char board[80], dir[80], *ptr;
    char brdencode[256], buf[256];
    bcache_t *x1;
    struct fileheader x;
    int i, start, total;

    init_all();
    strsncpy(board, getparm("board"), 32);
    x1 = getbcache(board);
    if (x1 == 0)
        http_fatal("错误的讨论区");
    strcpy(board, x1->filename);
    if (!has_read_perm(currentuser, board))
        http_fatal("错误的讨论区");
    sprintf(dir, "boards/%s/.DIGEST", board);
    fp = fopen(dir, "r");
    if (fp == 0)
        http_fatal("错误的讨论区目录");
    total = file_size(dir) / sizeof(struct fileheader);
    start = atoi(getparm("start"));
    if (strlen(getparm("start")) == 0 || start > total - 20)
        start = total - 20;
    if (start < 0)
        start = 0;
    encode_url(brdencode, board, sizeof(brdencode));
    printf("<nobr><center>\n");
    printf("%s -- [文摘区: %s] 版主[%s] 文章数[%d]<hr class=\"default\">\n", BBSNAME, board, userid_str(x1->BM), total);
    if (total <= 0)
        http_fatal("本讨论区目前没有文章");
    printf("<table width=\"610\" border=\"1\">\n");
    printf("<tr><td>序号</td><td>状态</td><td>作者</td><td>日期</td><td>标题</td><td>人气</td></tr>\n");
    fseek(fp, start * sizeof(struct fileheader), SEEK_SET);
    for (i = 0; i < 20; i++) {
        if (fread(&x, sizeof(x), 1, fp) <= 0)
            break;
        printf("<tr><td>%d<td>%s<td>%s", start + i + 1, flag_str(x.accessed[0]), userid_str(x.owner));
        printf("<td>%12.12s", wwwCTime(atoi(x.filename + 2)) + 4);
        printf("<td><a href=\"bbsgcon?board=%s&file=%s&num=%d\">%s%40.40s</a>\n", brdencode, x.filename, start + i, strncmp(x.title, "Re: ", 4) ? "● " : "", void1(nohtml(x.title)));
    }
    printf("</table><hr>\n");
    if (start > 0) {
        printf("<a href=\"bbsgdoc?board=%s&start=%d\">上一页</a> ", brdencode, start < 20 ? 0 : start - 20);
    }
    if (start < total - 20) {
        printf("<a href=\"bbsgdoc?board=%s&start=%d\">下一页</a> ", brdencode, start + 20);
    }
    printf("<a href=\"bbsdoc?board=%s\">本讨论区</a> ", brdencode);
    ann_get_path(board, buf, sizeof(buf));
    printf("<a href=\"bbs0an?path=%s\">精华区</a> ", http_encode_string(buf, sizeof(buf)));
    printf("<a href=\"bbspst?board=%s\">发表文章</a> ", brdencode);
    fclose(fp);
    http_quit();
}
