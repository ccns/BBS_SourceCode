/*-------------------------------------------------------*/
/* talk.c	( NTHU CS MapleBBS Ver 3.00 )		 */
/*-------------------------------------------------------*/
/* target : talk/query/friend(pal) routines	 	 */
/* create : 95/03/29				 	 */
/* update : 2000/01/02				 	 */
/*-------------------------------------------------------*/

#define	_MODES_C_

#include "bbs.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#undef	APRIL_FIRST

typedef struct
{
  int pal_no;
  char ship[46];
}  PAL_SHIP;

#ifdef  HAVE_PIP_FIGHT
  void (*p)();
#endif



static int pal_count;
static int *pal_pool;
static int bmw_modetype;
#ifdef HAVE_BOARD_PAL
static int board_pals;
#endif
extern UCACHE *ushm;
extern XZ xz[];
static PAL_SHIP *pal_ship;
static int can_see(UTMP *up);


#ifdef EVERY_Z
extern int vio_fd;		/* Thor.0725: 為talk,chat可用^z作準備 */
extern int holdon_fd;
#endif


typedef struct
{
  int curcol, curln;
  int sline, eline;
}      talk_win;


typedef struct
{
	UTMP *utmp;
	int type;
}	PICKUP;


void my_query();

static void
reset_utmp()
{
  cutmp->pid = currpid;
  cutmp->userno = cuser.userno;
  cutmp->mode = bbsmode;
  cutmp->ufo = cuser.ufo;
  strcpy(cutmp->userid, cuser.userid);
  strcpy(cutmp->username, cuser.username);
  strcpy(cutmp->realname, cuser.realname);
  str_ncpy(cutmp->from, fromhost, sizeof(cutmp->from));
}


/* ----------------------------------------------------- */
/* 記錄 pal 的 user number				 */
/* ----------------------------------------------------- */

#ifdef	HAVE_BOARD_PAL
#define PICKUP_WAYS	(7)
int pickup_way=1;
#else
#define	PICKUP_WAYS	(6)
int pickup_way=1;
#endif

static char page_requestor[40];

#if 0
#ifdef	HAVE_SHOWNUMMSG
typedef struct
{
  int year;
  int month;
  int day;
  int weekday;
  char *sendmsg;
  char *recvmsg;
}       MESSAGE;

static MESSAGE *MessageTable[] =
{

};
#endif
#endif

char *
bmode(up, simple)
  UTMP *up;
  int simple;
{
  static char modestr[32];
#ifdef	HAVE_SHOWNUMMSG
  char *nums[9] = {"一","二","三","四","五","六","七","八","九"};
#endif
  int mode;
  char *word;
  
  if(!up)
    return "不在站上";
    
#ifdef	HAVE_SHOWNUMMSG
  if(up->num_msg > 9)
  {
    strcpy(modestr,"被灌爆了");
    return (modestr);
  }
  else if(up->num_msg > 0)
  {
    sprintf(modestr,"收到%s封訊息",nums[up->num_msg-1]);
    return (modestr);
  }
#endif  
  
  mode = up->mode;
  if(mode == M_IDLE)
  {
    word = up->mateid;
  }
  else
  {
    word = ModeTypeTable[mode];
  }

  if (simple)
    return (word);

  if (mode < M_TALK || mode > M_QUERY)
    return (word);

  if ((mode != M_QUERY && !HAS_PERM(PERM_SEECLOAK) && (up->ufo & UFO_CLOAK))||(can_see(up)==2 && !HAS_PERM(PERM_SYSOP)))
    return (word); /* Thor.980805: 註解: 隱身的人不會被知道跟誰talk */
  
  sprintf(modestr, "%s %s", word, up->mateid);
  return (modestr);
}


/* ----------------------------------------------------- */
/* 好友名單：查詢狀態、友誼描述				 */
/* ----------------------------------------------------- */


/* Thor: 傳回值 -> 0 for good, 1 for normal, 2 for bad */


#if 0
static int
pal_belong(userid, uno)
  char *userid;
  int uno;
{
  PAL *head, *tail;
  char *fimage = NULL, fpath[64];
  int fsize, ans;

  ans = 1;			/* 找不到為 normal */

  if (*userid)
  {
    usr_fpath(fpath, userid, FN_PAL);

    fimage = f_map(fpath, &fsize);
    if (fimage != (char *) -1)
    {
      head = (PAL *) fimage;
      tail = (PAL *) (fimage + fsize);
      do
      {
	if (uno == head->userno)
	{
	  ans = head->ftype;	/* Thor: 原為 2-, 現直接傳回 */
	  break;
	}
      } while (++head < tail);
      munmap(fimage, fsize);
    }
  }

  return ans;
}
#endif

static void
copyship(ship,userno)
  char *ship;
  int userno;
{
  PAL_SHIP *shead;
  shead = pal_ship;
  if(userno == cuser.userno)
  {
    strcpy(ship,"那就是我");
	return;
  }
  if (shead)
  {
    while(shead->pal_no)
    {
      if(shead->pal_no == userno)
      {
        strcpy(ship,shead->ship);
        return;
      }
      shead++;
    } 
  }
  return;

}

static int
can_see(up)
  UTMP *up;
{
  int count, *cache, datum, mid;

  if ((cache = up->pal_spool))
  {
    for (count = up->pal_max; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if ((-cuser.userno) == datum)
        return 2;
      if ((-cuser.userno) > datum)
      {
        cache += (++mid);
        count -= mid;
      }
      else
      {
        count = mid;
      }
    }
  }
  if ((cache = up->pal_spool))
  {
    for (count = up->pal_max; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if (cuser.userno == datum)
        return 1;
      if (cuser.userno > datum)
      {
        cache += (++mid);
        count -= mid;
      }
      else
      {
        count = mid;
      }
    }
  }
  return 0;

}

static int
is_bad(userno)
  int userno;
{
  int count, *cache, datum, mid;

  if ((cache = pal_pool))
  {
    for (count = pal_count; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if ((-userno) == datum)
        return 1;
      if ((-userno) > datum)
      {
        cache += (++mid);
        count -= mid;
      }
      else
      {
        count = mid;
      }
    }
  }
  return 0;
}

#ifdef	HAVE_BANMSG
static int
can_banmsg(up)
  UTMP *up;
{
  int count, *cache, datum, mid;

  if ((cache = up->banmsg_spool))
  {
    for (count = up->banmsg_max; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if (cuser.userno == datum)
        return 1;
      if (cuser.userno > datum)
      {
        cache += (++mid);
        count -= mid;
      }
      else
      {
        count = mid;
      }
    }
  }
  return 0;

}
#endif


/*static */int
can_message(up)
  UTMP *up;
{
  int self, ufo, can;

  if (up->userno == (self = cuser.userno))
    return NA;

  ufo = up->ufo;

//  if(ufo & UFO_REJECT)
//    return NA;

  if (HAS_PERM(PERM_SYSOP | PERM_ACCOUNTS|PERM_CHATROOM))	/* 站長、帳號 */
    return YEA;

#ifdef	HAVE_BANMSG
  if(can_banmsg(up))		/* 拒收訊息 */
    return NA;  
#endif  

  if ( ufo & (UFO_MESSAGE))	/* 遠離塵囂 */
    return NA;

  if (!(ufo & UFO_QUIET))
    return YEA;			/* 呼叫器沒關掉，亦無損友 */

  can = 0;			/* 找不到為 normal */
  can = can_see(up);
  return (ufo & UFO_QUIET)
    ? can == 1			/* 只有好友可以 */
    : can < 2 			/* 只要不是損友 */ ;
}


static int
can_override(up)
  UTMP *up;
{
  int self, ufo, can;

  if (up->userno == (self = cuser.userno))
    return NA;

  ufo = up->ufo;
    
  if(ufo & (UFO_REJECT|UFO_NET))
    return NA;

  if (HAS_PERM(PERM_SYSOP | PERM_ACCOUNTS|PERM_CHATROOM))	/* 站長、帳號 */
    return YEA;

  if (ufo & UFO_PAGER1)		/* 遠離塵囂 */
    return NA;

  if (!(ufo & UFO_PAGER))
    return YEA;			/* 呼叫器沒關掉，亦無損友 */

  can = 0;			/* 找不到為 normal */

  can = can_see(up);
  return (ufo & UFO_PAGER)
    ? can == 1			/* 只有好友可以 */
    : can < 2 /* 只要不是損友 */ ;
}

/* ----------------------------------------------------- */
/* 好友名單：新增、刪除、修改、載入、同步		 */
/* ----------------------------------------------------- */

#ifdef	HAVE_BOARD_PAL
int
is_boardpal(up)
  UTMP *up;
{
  return cutmp->board_pal == up->board_pal;
}
#endif

int
is_pal(userno)
  int userno;
{
  int count, *cache, datum, mid;

  if ((cache = pal_pool))
  {
    for (count = pal_count; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if (userno == datum)
	return 1;
      if (userno > datum)
      {
	cache += (++mid);
	count -= mid;
      }
      else
      {
	count = mid;
      }
    }
  }
  return 0;
}

#ifdef	HAVE_BANMSG
int
is_banmsg(userno)
  int userno;
{
  int count, *cache, datum, mid;

  if ((cache = cutmp->banmsg_spool))
  {
    for (count = cutmp->banmsg_max; count > 0;)
    {
      datum = cache[mid = count >> 1];
      if (userno == datum)
	return 1;
      if (userno > datum)
      {
	cache += (++mid);
	count -= mid;
      }
      else
      {
	count = mid;
      }
    }
  }
  return 0;
}
#endif

static int
int_cmp(a, b)
  int *a;
  int *b;
{
  return *a - *b;
}


void
pal_cache()
{
  int count, fsize, ufo ,fd;
  int *plist, *cache;
  int ship_total;
  PAL *phead, *ptail;
  PAL_SHIP *shead;
  char *fimage, fpath[64];
  UTMP *up;

  up = cutmp;
  cutmp->userno = cuser.userno;

  cache = NULL;
  ship_total = count = 0;
  ufo = cuser.ufo & ~( UFO_BIFF | UFO_BIFFN | UFO_REJECT | UFO_FCACHE);
  /* Thor.980805: 跟 BIFF好像沒有太大關係 */

  fsize = 0;
  usr_fpath(fpath, cuser.userid, FN_PAL);
  fimage = f_img(fpath, &fsize);
  if((fsize > (PAL_MAX * sizeof(PAL))) && (fd = open(fpath,O_RDWR)))
  {
    ftruncate(fd, PAL_MAX * sizeof(PAL));
    close(fd);
  }
  if (fimage != NULL)
  {
    if (fsize > (PAL_MAX * sizeof(PAL)))
    {
      sprintf(fpath, "%-13s%d > %d * %d\n", cuser.userid, fsize, PAL_MAX, sizeof(PAL));
      f_cat(FN_PAL_LOG, fpath);
      fsize = PAL_MAX * sizeof(PAL);
    }

    count = fsize / sizeof(PAL);
    if (count)
    {
      cache = plist = up->pal_spool;
      phead = (PAL *) fimage;
      ptail = (PAL *) (fimage + fsize);
      ufo |= UFO_REJECT;
      do
      {
	if (phead->ftype & PAL_BAD)
	{
	  *plist++ = -(phead->userno);
	}
	else
	{
	  *plist++ = phead->userno;
	}
	if (strlen(phead->ship) > 0)
	  ship_total++;
      } while (++phead < ptail);

      if (count > 0)
      {
	ufo |= UFO_FCACHE;
	if (count > 1)
	  xsort(cache, count, sizeof(int), (void *)int_cmp);
      }
      else
      {
	cache = NULL;
      }
      
    }
  }

  pal_pool = cache;
  up->pal_max = pal_count = count;
  if (cutmp)
  {
    ufo = (ufo & ~(UFO_UTMP_MASK | UFO_REJECT)) | (cutmp->ufo & UFO_UTMP_MASK);
    /* Thor.980805: 解決 cutmp.ufo和 cuser.ufo的同步問題 */
    cutmp->ufo = ufo & (~UFO_REJECT);
  }
  if(pal_ship)
  {
    free(pal_ship);
    pal_ship = NULL;
  }
  
  
  if(ship_total)
  {
     shead = pal_ship = (PAL_SHIP *)malloc((ship_total+1)*sizeof(PAL_SHIP));  
     phead = (PAL *) fimage;
     ptail = (PAL *) (fimage + fsize);
     if(pal_ship)
     {
       memset(pal_ship,0,(ship_total+1)*sizeof(PAL_SHIP));
       do
       {
         if(strlen(phead->ship)>0)
         {
           strcpy(shead->ship,phead->ship);
           shead->pal_no = phead->userno;
           shead++;
         }
       } while (++phead < ptail);  
     }
     else
       pal_ship = NULL;
  }
  else
    pal_ship = NULL;
  
  if(fimage) free(fimage);
  cuser.ufo = ufo;
}

/* 2003.01.01 verit : 重整 上站通知名單 */
void 
aloha_sync(void)
{
  char fpath[128];
  int fd,size=0;
  struct stat st;

  usr_fpath(fpath, cuser.userid, FN_FRIEND_BENZ);
  if ((fd = open(fpath, O_RDWR, 0600)) < 0)
      return;

  outz("★ 資料整理稽核中，請稍候 \033[5m...\033[m");
  refresh();

  if (!fstat(fd, &st) && (size = st.st_size) > 0)
  {
    BMW *pbase, *phead, *ptail;
    int userno;

    pbase = phead = (BMW *) malloc(size);
    size = read(fd, pbase, size);
    if (size >= sizeof(BMW))
    {
      ptail = (BMW *) ((char *) pbase + size);
      while (phead < ptail)
      {
        userno = phead->recver;
        if (userno > 0 && userno == acct_userno(phead->userid))
        {
          phead++;
          continue;
        }
        ptail--;
        if (phead >= ptail) break;
        memcpy(phead, ptail, sizeof(BMW));
      }

      size = (char *) ptail - (char *) pbase;
      if (size > 0)
      {
        //xsort(pbase, size / sizeof(BMW), sizeof(BMW), str_cmp);
        lseek(fd, 0, SEEK_SET);
        write(fd, pbase, size);
        ftruncate(fd, size);
      }
    }
    free(pbase);
  }
  close(fd);

  if (size <= 0)
    unlink(fpath);
}
      

void
pal_sync(fpath)
  char *fpath;
{
  int fd, size=0;
  struct stat st;
  char buf[64];

  if (!fpath)
  {
    fpath = buf;
    usr_fpath(fpath, cuser.userid, FN_PAL);
  }

  if ((fd = open(fpath, O_RDWR, 0600)) < 0)
    return;

  outz("★ 資料整理稽核中，請稍候 \033[5m...\033[m");
  refresh();

  if (!fstat(fd, &st) && (size = st.st_size) > 0)
  {
    PAL *pbase, *phead, *ptail;
    int userno;

    pbase = phead = (PAL *) malloc(size);
    size = read(fd, pbase, size);
    if (size >= sizeof(PAL))
    {
      ptail = (PAL *) ((char *) pbase + size);
      while (phead < ptail)
      {
	userno = phead->userno;
	if (userno > 0 && userno == acct_userno(phead->userid))
	{
	  phead++;
	  continue;
	}

	ptail--;
	if (phead >= ptail)
	  break;
	memcpy(phead, ptail, sizeof(PAL));
      }

      size = (char *) ptail - (char *) pbase;
      if (size > 0)
      {
	if (size > sizeof(PAL))
	{
	  if (size > PAL_ALMR * sizeof(PAL))
	    vmsg("您的好友名單太多，請善加整理");
	  xsort(pbase, size / sizeof(PAL), sizeof(PAL), (void *)str_cmp);
	}

	/* Thor.0709: 是否要加上消除重覆的好友的動作? */

	lseek(fd, 0, SEEK_SET);
	write(fd, pbase, size);
	ftruncate(fd, size);
      }
    }
    free(pbase);
  }
  close(fd);

  if (size <= 0)
    unlink(fpath);
}


/* ----------------------------------------------------- */
/* 好友名單：選單式操作界面描述				 */
/* ----------------------------------------------------- */


static int pal_add();


static void
pal_item(num, pal)
  int num;
  PAL *pal;
{
  prints("%6d %-3s%-14s%s\n", num, pal->ftype & PAL_BAD ? "Ｘ" : "",
    pal->userid, pal->ship);
}


static int
pal_body(xo)
  XO *xo;
{
  PAL *pal;
  int num, max, tail;

  max = xo->max;
  if (max <= 0)
  {
    if (vans("要交新朋友嗎(Y/N)？[N] ") == 'y')
      return pal_add(xo);
    return XO_QUIT;
  }

  pal = (PAL *) xo_pool;
  num = xo->top;
  tail = num + XO_TALL;
  if (max > tail)
    max = tail;

  move(3, 0);
  do
  {
    pal_item(++num, pal++);
  } while (num < max);
  clrtobot();

  return XO_NONE;
}


static int
pal_head(xo)
  XO *xo;
{
  vs_head("好友名單", str_site);
  outs("\
  [←]離開 a)新增 c)修改 d)刪除 m)寄信 s)整理 [/?]搜尋 [q]查詢 [h]elp\n\
\033[30;47m  編號    代 號         友       誼                                           \033[m");
  return pal_body(xo);
}


static int
pal_load(xo)
  XO *xo;
{
  xo_load(xo, sizeof(PAL));
  return pal_body(xo);
}


static int
pal_init(xo)
  XO *xo;
{
  xo_load(xo, sizeof(PAL));
  return pal_head(xo);
}


static void
pal_edit(pal, echo)
  PAL *pal;
  int echo;
{
  if (echo == DOECHO)
    memset(pal, 0, sizeof(PAL));
  vget(b_lines, 0, "友誼：", pal->ship, sizeof(pal->ship), echo);
  pal->ftype = vans("損友(Y/N)？[N] ") == 'y' ? PAL_BAD : 0;
}


static int
pal_search(xo, step)
  XO *xo;
  int step;
{
  int num, pos, max;
  static char buf[IDLEN + 1];
  int fsize;
  PAL *phead;
  char *fimage = NULL, fpath[64];

  if (vget(b_lines, 0, msg_uid, buf, IDLEN + 1, GCARRY))
  {
    int buflen;
    char bufl[IDLEN + 1];

    usr_fpath(fpath, cuser.userid, FN_PAL);
    fimage = f_img(fpath, &fsize);
    phead = (PAL *) fimage;

    str_lower(bufl, buf);
    buflen = strlen(bufl);
    
    pos = num = xo->pos;
    max = xo->max;
    do
    {
      pos += step;
      if (pos < 0) 
        pos = max - 1;
      else if (pos >= max)
	    pos = 0;
   
      if (str_str((phead + pos)->userid, bufl) || str_str((phead + pos)->ship, bufl))
      {
		move(b_lines, 0);
		clrtoeol();
	    if(fimage) 
		  free(fimage);
		return pos + XO_MOVE;
      }

    } while (pos != num);
  }
  if(fimage) 
    free(fimage);

  return XO_FOOT;
}

static int
pal_search_forward(xo)
  XO *xo;
{
  return pal_search(xo, 1); /* step = +1 */
}

static int
pal_search_backward(xo)
  XO *xo;
{
  return pal_search(xo, -1); /* step = -1 */
}


static int
pal_add(xo)
  XO *xo;
{
  ACCT acct;
  int userno;

  if (xo->max >= PAL_MAX)
  {
    vmsg("您的好友名單太多，請善加整理");
    return XO_FOOT;
  }

  userno = acct_get(msg_uid, &acct);

  /* jhlin : for board_moderator, temporarily */

#if 1				/* Thor.0709: 不重覆加入 */
  /* if (is_pal(userno)) */
  if ((xo->dir[0] == 'u') && (is_pal(userno) || is_bad(userno)))
              /* lkchu.981201: 只有在 moderator board 才可重覆加入 */
  {
    vmsg("名單中已有此好友");
    return XO_FOOT;
  }
  else if (userno == cuser.userno)      /* lkchu.981201: 好友名單不可加自己 */
  {
    vmsg("自己不須加入好友名單中");
    return XO_FOOT;
  }
#endif

  if ((userno > 0) /* && !pal_state(cutmp, userno, NULL) */ )
  {
    PAL pal;

    pal_edit(&pal, DOECHO);
    strcpy(pal.userid, acct.userid);
    pal.userno = userno;
    rec_add(xo->dir, &pal, sizeof(PAL));
#if 0
    if ((xo->dir[0] == 'u') && (pal.userno != cuser.userno) && !(pal.ftype & PAL_BAD))
    {
      char fpath[64];
      BMW bmw;

      /* bmw.caller = cutmp; */		/* lkchu.981201: frienz 中 utmp 無效 */
      bmw.recver = cuser.userno;
      strcpy(bmw.userid, cuser.userid);
      usr_fpath(fpath, pal.userid, FN_FRIEND_BENZ);
      rec_add(fpath, &bmw, sizeof(BMW));
    }
#endif
    xo->pos = XO_TAIL /* xo->max */ ;
    xo_load(xo, sizeof(PAL));
  }

#if 1				/* Thor.0709: 好友名單同步 */
  pal_cache();
#endif

  return pal_head(xo);
}





static int
pal_delete(xo)
  XO *xo;
{
  if (vans(msg_del_ny) == 'y')
  {
#if 0
    if (xo->dir[0] == 'u')
    {
      PAL *pal;

      pal = (PAL *) xo_pool + (xo->pos - xo->top);
      if (!(pal->ftype & PAL_BAD))
      {
        char fpath[64];

        usr_fpath(fpath, pal->userid, FN_FRIEND_BENZ);
        rec_del(fpath, sizeof(BMW), 0, cmpbmw, NULL);
      }
    }
#endif

    if (!rec_del(xo->dir, sizeof(PAL), xo->pos, NULL, NULL))
    {

#if 1				/* Thor.0709: 好友名單同步 */
      pal_cache();
#endif

      return pal_load(xo);
    }
  }
  return XO_FOOT;
}


static int
pal_change(xo)
  XO *xo;
{
  PAL *pal, mate;
  int pos, cur;
#if 0
  char old_ftype, fpath[64];
#endif
  
  pos = xo->pos;
  cur = pos - xo->top;
  pal = (PAL *) xo_pool + cur;

#if 0
  old_ftype = pal->ftype;
  usr_fpath(fpath, pal->userid, FN_FRIEND_BENZ);
#endif
    
  mate = *pal;
  pal_edit(pal, GCARRY);
  if (memcmp(pal, &mate, sizeof(PAL)))
  {
    rec_put(xo->dir, pal, sizeof(PAL), pos);
    move(3 + cur, 0);
    pal_item(++pos, pal);
  }

#if 0
  if (xo->dir[0] == 'b')	/* lkchu.981201: moderator board */
    return XO_FOOT;
  
  if ( !(old_ftype & PAL_BAD) && (pal->ftype & PAL_BAD) )
      /* lkchu.981201: 原本是好友, 改成損友要 rec_del */
  {
    rec_del(fpath, sizeof(BMW), 0, cmpbmw, NULL);
  }
  else if ( (old_ftype & PAL_BAD) && !(pal->ftype & PAL_BAD) )
      /* lkchu.981201: 原本是損友, 改成好友要 rec_add */
  {
    BMW bmw;

    /* bmw.caller = cutmp; */	/* lkchu.981201: frienz 中 utmp 無效 */
    bmw.recver = cuser.userno;
    strcpy(bmw.userid, cuser.userid);
    rec_add(fpath, &bmw, sizeof(BMW));
  }  
#endif

  return XO_FOOT;
}


static int
pal_mail(xo)
  XO *xo;
{
  PAL *pal;
  char *userid;

  pal = (PAL *) xo_pool + (xo->pos - xo->top);
  userid = pal->userid;
  if (*userid)
  {
    vs_bar("寄  信");
    prints("收信人：%s", userid);
    my_send(userid);
  }
  return pal_head(xo);
}


static int
pal_sort(xo)
  XO *xo;
{
  pal_sync(xo->dir);
  return pal_load(xo);
}


static int
pal_query(xo)
  XO *xo;
{
  PAL *pal;

  pal = (PAL *) xo_pool + (xo->pos - xo->top);
  move(1, 0);
  clrtobot();
  /* move(2, 0); *//* Thor.0810: 可以不加嗎? */
  /* if(*pal->userid) *//* Thor.0806:偶爾好玩一下, 應該沒差 */
  my_query(pal->userid, 1);
  return pal_head(xo);
}


static int
pal_help(xo)
  XO *xo;
{
  film_out(FILM_PAL, -1);
  return pal_head(xo);
}


KeyFunc pal_cb[] =
{
  {XO_INIT, pal_init},
  {XO_LOAD, pal_load},
  {XO_HEAD, pal_head},
  {XO_BODY, pal_body},

  {'a', pal_add},
  {'c', pal_change},
  {'d', pal_delete},
  {'m', pal_mail},
  {'q', pal_query},
  {'s', pal_sort},
  {'/', pal_search_forward},
  {'?', pal_search_backward},
#if 0
  {'w' | XO_DL, (int (*)())"bin/bbcall.so:pal_bbc"},
#endif

  {'h', pal_help}
};


int
t_pal()
{
  XO *xo;
  char fpath[64];

  usr_fpath(fpath, cuser.userid, FN_PAL);
  xz[XZ_PAL - XO_ZONE].xo = xo = xo_new(fpath);
  xover(XZ_PAL);
  pal_cache();
  free(xo);

  return 0;
}


#if 0
int
t_pal()
{
  XO *xo;

  xo = pal_xo;
  if (xo == NULL)
  {
    char fpath[64];

    usr_fpath(fpath, cuser.userid, FN_PAL);
    pal_xo = xo = xo_new(fpath);
  }
  xover(XZ_PAL);
  pal_cache();
  return 0;
}
#endif


/* ----------------------------------------------------- */
/* 訊息列表: 選單式操作界面描述 by lkchu                 */
/* ----------------------------------------------------- */


/*static */void bmw_edit();


static void
bmw_item(num, bmw)
  int num;
  BMW *bmw;
{
  struct tm *ptime = localtime(&bmw->btime);

  if(!(bmw_modetype & BMW_MODE))
  {
    if (bmw->sender == cuser.userno)
    {
      /* lkchu.990206: 好友廣撥 */
      if (!(*bmw->userid))
        strcpy(bmw->userid, "眾家好友");
      
      prints("%5d %02d:%02d %-13s☆%-50.50s\n", num, ptime->tm_hour, ptime->tm_min,
        bmw->userid, bmw->msg);
    }
    else
    {
      if(strstr(bmw->msg,"★廣播"))
        prints("%5d \033[36;1m%02d:%02d %-13s★%-50.50s\033[m\n", num, ptime->tm_hour, ptime->tm_min,
          bmw->userid, (bmw->msg)+8);
      else
        prints("%5d \033[32m%02d:%02d %-13s★%-50.50s\033[m\n", num, ptime->tm_hour, ptime->tm_min,
          bmw->userid, bmw->msg);
    }
  }
  else
  {
    if (bmw->sender == cuser.userno)
    {
      if (!(*bmw->userid))
        strcpy(bmw->userid, "眾家好友");

      prints("%5d %-13s☆%-57.57s\n", num,bmw->userid, bmw->msg);
    }
    else
    {
      if(strstr(bmw->msg,"★廣播"))
        prints("%5d \033[36;1m%-13s★%-57.57s\033[m\n", num,
          bmw->userid, (bmw->msg)+8);
      else
        prints("%5d \033[32m%-13s★%-57.57s\033[m\n", num,
          bmw->userid, bmw->msg);
    }  
  }
}


static int
bmw_body(xo)
  XO *xo;
{
  BMW *bmw;
  int num, max, tail;

  move(3, 0);
  clrtobot();
  max = xo->max;
  if (max <= 0)
  {
    vmsg("先前並無熱訊呼叫");
    return XO_QUIT;
  }

  bmw = (BMW *) xo_pool;
  num = xo->top;
  tail = num + XO_TALL;
  if (max > tail)
    max = tail;

  do
  {
    bmw_item(++num, bmw++);
  } while (num < max);

  return XO_NONE;
}


static int
bmw_head(xo)
  XO *xo;
{
  vs_head("察看訊息", str_site);
  if(bmw_modetype & BMW_MODE)
  {
    outs("\
  [←]離開  [d]刪除  [m]寄信  [w]快訊  [s]更新  [→]查詢  [h]elp\n\
\033[30;47m 編號 代 號        內       容                                                \033[m");
  }
  else
  {
    outs("\
  [←]離開  [d]刪除  [m]寄信  [w]快訊  [s]更新  [→]查詢  [h]elp\n\
\033[30;47m 編號 時 間 代 號        內       容                                          \033[m");
  }
  return bmw_body(xo);
}


static int
bmw_load(xo)
  XO *xo;
{
  xo_load(xo, sizeof(BMW));
  return bmw_body(xo);
}


static int
bmw_init(xo)
  XO *xo;
{
  xo_load(xo, sizeof(BMW));
  return bmw_head(xo);
}


static int
bmw_delete(xo)
  XO *xo;
{
  if (vans(msg_del_ny) == 'y')
    if (!rec_del(xo->dir, sizeof(BMW), xo->pos, NULL, NULL))
      return bmw_load(xo);

  return XO_FOOT;
}


static int
bmw_mail(xo)
  XO *xo;
{
  BMW *bmw;
  char *userid;

  bmw = (BMW *) xo_pool + (xo->pos - xo->top);
  userid = bmw->userid;
  if (*userid)
  {
    vs_bar("寄  信");
    prints("收信人：%s", userid);
    my_send(userid);
  }
  return bmw_head(xo);
}


static int
bmw_query(xo)
  XO *xo;
{
  BMW *bmw;

  bmw = (BMW *) xo_pool + (xo->pos - xo->top);
  move(1, 0);
  clrtobot();
  /* move(2, 0); *//* Thor.0810: 可以不加嗎? */
  /* if(*pal->userid) *//* Thor.0806:偶爾好玩一下, 應該沒差 */
  my_query(bmw->userid, 1);
  return bmw_head(xo);
}


static int
bmw_write(xo)
  XO *xo;
{
  if (HAS_PERM(PERM_PAGE))
  {
    UTMP *up = NULL;
    BMW *benz;

    benz = (BMW *) xo_pool + (xo->pos - xo->top);
    if( (benz->caller >= ushm->uslot && benz->caller < ushm->uslot + MAXACTIVE) && (benz->caller && benz->caller->userno == benz->sender) && can_message(benz->caller))
    {
      up = benz->caller;
    }
    
    if ((up || (up = utmp_find(benz->sender))) && can_message(up))
        /* lkchu.990104: 讓以前的 bmw 也可以回 */
    {
      if (((up->ufo & UFO_CLOAK) && !HAS_PERM(PERM_SEECLOAK))||(can_see(up)==2 && !HAS_PERM(PERM_SYSOP))||benz->sender==0)
      {
        return XO_NONE;		/* lkchu.990111: 隱形囉 */
      }
      else
      {
        BMW bmw;
        char buf[20];
#ifdef  HAVE_SHOWNUMMSG
        if(up->num_msg > 9 && (up->ufo & UFO_MAXMSG) && !HAS_PERM(PERM_SYSOP))
        {
          vmsg("對方已經被水球灌爆了!!");
          return XO_INIT;
        }
#endif
        sprintf(buf, "★[%s]", up->userid);
        bmw_edit(up, buf, &bmw, 0);
      }
    }
#ifdef	HAVE_BANMSG    
    else if(up && !(up->ufo & UFO_MESSAGE) && can_banmsg(up))
    {
      vmsg("對方不想聽到您的聲音!!");
      return XO_INIT;
    }
#endif    
  }
  return XO_NONE;
}

static int
bmw_mode(xo)
  XO *xo;
{
  bmw_modetype ^= BMW_MODE;
  return bmw_init(xo);
}


static int
bmw_help(xo)
  XO *xo;
{
  film_out(FILM_BMW, -1);
  return bmw_head(xo);
}


KeyFunc bmw_cb[] =
{
  {XO_INIT, bmw_init},
  {XO_LOAD, bmw_load},
  {XO_HEAD, bmw_head},
  {XO_BODY, bmw_body},
  
  {'d', bmw_delete},
  {'m', bmw_mail},
  {'w', bmw_write},
  {'q', bmw_query},
  {Ctrl('Q'), bmw_query},
  {'s', bmw_init},
  {KEY_TAB, bmw_mode}, 
  {'h', bmw_help}
};


int
t_bmw()
{
  xover(XZ_BMW);
  return 0;
}


#ifdef HAVE_MODERATED_BOARD
/* ----------------------------------------------------- */
/* 板友名單：moderated board				 */
/* ----------------------------------------------------- */


#define FN_FIMAGE	"fimage"


static void
bm_image()
{
  int fd;
  char fpath[80];

  brd_fpath(fpath, currboard, FN_PAL);
  if ((fd = open(fpath, O_RDONLY)) >= 0)
  {
    struct stat st;
    PAL *pal, *up;
    int count;

    fstat(fd, &st);
    if ((pal = (PAL *) malloc(count = st.st_size)))
    {
      count = read(fd, pal, count) / sizeof(PAL);
      if (count > 0)
      {
	int *userno,*ubase;
	int c = count;
	
	ubase = userno = (int *)malloc(count*sizeof(int));

	up = pal;
	do
	{
#ifdef HAVE_WATER_LIST
	  *userno++ = (up->ftype == PAL_BAD) ? -(up->userno) : up->userno;
#else
	  *userno++ = up->userno;
#endif	  
	  up++;
	} while (--c);

	if (count > 1)		/* Thor: 多排序有益身體健康... */
	  xsort(ubase, count, sizeof(int), int_cmp);

	brd_fpath(fpath, currboard, FN_FIMAGE);
	if ((count = open(fpath, O_WRONLY | O_CREAT | O_TRUNC, 0600)) >= 0)
	{
	  write(count, ubase, (char *) userno - (char *) ubase);
	  close(count);
	  free(ubase);
	}
      }
      else  /* Thor.980811: lkchu patch: 與 friend 同步 */
      {
	brd_fpath(fpath, currboard, FN_FIMAGE);
        unlink(fpath);
      }
      free(pal);
    }
    close(fd);
  }
}


int
bm_belong(board)
  char *board;
{
  int fsize, count, result,wcount;
  char fpath[80];

  result = 0;

  brd_fpath(fpath, board, FN_FIMAGE);	/* Thor: fimage要先 sort過! */
  board = f_img(fpath, &fsize);

  if (board != NULL)
  {
    wcount = count = fsize / sizeof(int);

    if (count > 0)
    {
      int userno, *up;
      int datum, mid;

      userno = cuser.userno;
      up = (int *) board;

      while (count > 0)
      {
	datum = up[mid = count >> 1];
	if (userno == datum)
	{
	  result = BRD_R_BIT | BRD_W_BIT;
	  break;
	}
	if (userno > datum)
	{
	  up += (++mid);
	  count -= mid;
	}
	else
	{
	  count = mid;
	}
      }
#ifdef HAVE_WATER_LIST
      up = (int *) board;
      while (wcount > 0)
      {
        datum = up[mid = wcount >> 1];
        if(-userno == datum)
        {
          result = BRD_R_BIT;
          break;
        }
        if (-userno > datum)
        {
          up += (++mid);
          wcount -= mid;
        }
        else
        {
          wcount = mid;
        }
      }
#endif
    }
    free(board);
  }
  return result;
}


int
XoBM(xo)
  XO *xo;
{
  if ((bbstate & STAT_BOARD) /*&& (bbstate & STAT_MODERATED)*/)
    /* && (cbhdr.readlevel == PERM_SYSOP)) */
    /*
     * Thor.1020: bbstate有 STAT_MODERATED就代表符合MODERATED BOARD,
     * 不需再check readlevel PERM_SYSOP
     */
  {
    XO *xt;
    char fpath[80];

    brd_fpath(fpath, currboard, FN_PAL);
    xz[XZ_PAL - XO_ZONE].xo = xt = xo_new(fpath);
    xover(XZ_PAL);		/* Thor: 進xover前, pal_xo 一定要 ready */

    /* build userno image to speed up, maybe upgreade to shm */

    bm_image();

    free(xt);

    return XO_INIT /* or post_init(xo) */ ;
  }
  return XO_NONE;
}
#endif


/* ------------------------------------- */
/* 真實動作				 */
/* ------------------------------------- */


static void
showplans(userid)
  char *userid;
{
  int i;
  FILE *fp;
  char buf[256];

  usr_fpath(buf, userid, FN_PLANS);

  if ((fp = fopen(buf, "r")))
  {
    move(7,0);
//    outs(" [名片]：\n\n");
    i = MAXQUERYLINES;

    while (i-- && fgets(buf, sizeof(buf), fp))
    {
      outx(buf);
    }
    fclose(fp);
  }
  else
  {
//    outs(" [沒有名片]\n\n");
  }
}


static void
do_query(acct, paling)
  ACCT *acct;
  int paling;			/* 是否正在設定好友名單 */
{
  UTMP *up;
  int userno,mail, rich;
  char *userid;

  utmp_mode(M_QUERY);
  userno = acct->userno;
  userid = acct->userid;
  strcpy(cutmp->mateid, userid);

  prints(" %s(%s) 上站 %d 次，文章 %d 篇，%s認證。\n",
    userid, (HAS_PERM(PERM_SYSOPX) || !(acct->userlevel & PERM_DENYNICK)) ? acct->username : GUEST_1, acct->numlogins, acct->numposts,
    acct->userlevel & PERM_VALID ? "已" : "未");

  prints(" 上次(\033[1;33m%s\033[m)來自(%s)\n", 
  Ctime(&acct->lastlogin), ((acct->ufo & UFO_HIDEDN)&&!HAS_PERM(PERM_SYSOP)) ? 
  HIDEDN_SRC : acct->lasthost);

#if defined(REALINFO) && defined(QUERY_REALNAMES)
  if (HAS_PERM(PERM_BASIC))
    prints(" 真實姓名: %s", acct->realname);
  else
#endif
    if (HAS_PERM(PERM_SYSOP))
      prints(" 真實姓名: %s", acct->realname);
    
  /* 假設上次 login 已過 6 小時，便不在站上，減少 utmp_find */
  
  outs(" [動態] ");
  /*  up = (acct->lastlogin < time(0) - 6 * 3600) ? NULL : utmp_find(userno);  */
  up = utmp_find(userno);
  outs("\033[1;36m");
  outs((!up || (up && ((!HAS_PERM(PERM_SEECLOAK) && (up->ufo & UFO_CLOAK)) || (can_see(up)==2)) && !HAS_PERM(PERM_SYSOP))) ? "不在站上":bmode(up, 1));
  outs("\033[m");
  /* Thor.981108: 為滿足 cigar 徹底隱身的要求, 不過用 finger還是可以看到:p */
  
#if 0
  /* Query 時可同時看其友誼描述或惡行 */

  if ((!paling) && (state = pal_state(cutmp, userno, ship)))
  {
    prints(" [%s友] %s", state > 0 ? "好" : "損", ship);
  }
#endif

  mail = m_query(userid);
  if(mail)
    prints(" [信箱] \033[1;31m有 %d 封新情書\033[m\n",mail);
  else
    prints(" [信箱] 都看過了\n");

  char fortune[7][9] = {"窮困阿宅", "家境普通", "家境小康", "家境富有", "財力雄厚", "富可敵國", "I'm Rich"};

  if     (acct->money >= 100000000)
    rich=6;
  else if(acct->money >=  10000000)
    rich=5;
  else if(acct->money >=   1000000)
    rich=4;    
  else if(acct->money >=    100000)
    rich=3;
  else if(acct->money >=     10000)
    rich=2; 
  else if(acct->money >=      1000)
    rich=1; 
  else
    rich=0;

  if (acct->point1 > 10) 
    prints(" [優良積分] \033[1;32m%d\033[m [劣文] %d [經濟] %s", acct->point1, acct->point2, fortune[rich]);
  else if (acct->point2 > 1) 
    prints(" [優良積分] %d [劣文] \033[1;31m%d\033[m [經濟] %s", acct->point1, acct->point2, fortune[rich]);
  else
    prints(" [優良積分] %d [劣文] %d [經濟] %s", acct->point1, acct->point2, fortune[rich]);

  if (paling == 2) 
    ;
  else
  {
    /* deny nick can't have plans. statue.2001.02.11 */
    if(HAS_PERM(PERM_SYSOPX)) 
      showplans(userid);
    else if(!(acct->userlevel & PERM_DENYNICK) && (acct->userlevel & PERM_VALID))
      showplans(userid);
  }   
  
  vmsg(NULL);

  move(b_lines, 0);
  clrtoeol();
}


void
my_query(userid, paling)
  char *userid;
  int paling;			/* 是否正在設定好友名單 */
{
  ACCT acct;

  if (acct_load(&acct, userid) >= 0)
  {
    do_query(&acct, paling);
  }
  else
  {
    vmsg(err_uid);
  }
}


/* ----------------------------------------------------- */
/* BMW : bbs message write routines			 */
/* ----------------------------------------------------- */


#define	BMW_FORMAT	"\033[1;33;46m★%s \033[37;45m %s \033[m"
#define BMW_FORMAT_BC	"\033[1;37;45m★%s \033[1;33;46m %s \033[m"
/* patch by visor : BMW_LOCAL_MAX >= BMW_PER_USER 
   以免進入無線迴圈                               */
#define	BMW_LOCAL_MAX	10	


static BMW bmw_lslot[BMW_LOCAL_MAX],bmw_sentlot[BMW_LOCAL_MAX];
//static BMW bmw_lslot[BMW_LOCAL_MAX];
static int bmw_locus;

static int
bmw_send(callee, bmw)
  UTMP *callee;
  BMW *bmw;
{
  BMW *mpool, *mhead, *mtail, **mslot;
  int i;
  pid_t pid;
  time_t texpire;

  if ((callee->userno != bmw->recver) || (pid = callee->pid) <= 0)
    return 1;

  /* sem_lock(BSEM_ENTER); */

  /* find callee's available slot */

  mslot = callee->mslot;
  i = 0;

  for (;;)
  {
    if (mslot[i] == NULL)
      break;

    if (++i >= BMW_PER_USER)
    {
      /* sem_lock(BSEM_LEAVE); */
      return 1;
    }
  }

  /* find available BMW slot in pool */

  texpire = time(&bmw->btime) - BMW_EXPIRE;

  mpool = ushm->mpool;
  mhead = ushm->mbase;
  if (mhead < mpool)
    mhead = mpool;
  mtail = mpool + BMW_MAX;

  do
  {
    if (++mhead >= mtail)
      mhead = mpool;
  } while (mhead->btime > texpire);

  *mhead = *bmw;
  ushm->mbase = mslot[i] = mhead;
  /* Thor.981206: 需注意, 若ushm mapping不同, 
                  則不同隻 bbsd 互call會core dump,
                  除非這也用offset, 不過除了 -i, 應該是非必要 */


  /* sem_lock(BSEM_LEAVE); */
  return kill(pid, SIGUSR2);
}


/*static */void
bmw_edit(up, hint, bmw, cc)
  UTMP *up;
  char *hint;
  BMW *bmw;
  int cc;
{
  uschar *str;
  screenline sl[2];

  if (up)
    bmw->recver = up->userno;	/* 先記下 userno 作為 check */

  if (!cc)
    save_foot(sl);

  str = bmw->msg;
  
  memset(str,0,sizeof(bmw->msg));
  
  str[0] = cc;
  str[1] = '\0';

  if (vget(b_lines - 1, 0, hint, str, 58 , GCARRY) &&
                                     /* lkchu.990103: 新介面只允許 48 個字元 */
    vans("確定要送出《熱訊》嗎(Y/N)？[Y] ") != 'n')
  {
#if 0
    FILE *fp;
#endif
    char *userid, fpath[64];


    bmw->caller = cutmp;
    bmw->sender = cuser.userno;
    strcpy(bmw->userid, userid = cuser.userid);

    if (up)
    {
      if (bmw_send(up, bmw))
      {
	vmsg(MSG_USR_LEFT);
        if (!cc)
          restore_foot(sl);	
	return;
      }
    }

    /* lkchu.981230: 利用 xover 整合 bmw */
    if (up)
      strcpy(bmw->userid, up->userid);	
				/* lkchu.990103: 若是自己送出的 bmw, 存對方的 userid */
    else
      *bmw->userid = '\0';	/* lkchu.990206: 好友廣撥設為 NULL */
      
    time(&bmw->btime);
    usr_fpath(fpath, userid, FN_BMW);
    rec_add(fpath, bmw, sizeof(BMW));
    strcpy(bmw->userid, userid);

#if 0
    /* Thor.0722: msg file加上自己說的話 */

    usr_fpath(fpath, userid, FN_BMW);
    if (fp = fopen(fpath, "a"))
    {

#ifndef BMW_TIME
      fprintf(fp, "☆%s：%s\n", up ? up->userid : "眾家好友", str);
#else  
      /* Thor.980821: 熱訊記錄加上時間 */
      fprintf(fp, "☆%s%s：%s\n", up ? up->userid : "眾家好友", bmw_timemsg(),  str);
#endif

      fclose(fp);
    }
#endif
  }

  if (!cc)
    restore_foot(sl);
}

#if 0
/* lkchu.981201: 選擇線上使用者傳訊息 */
static int
bmw_choose()
{
  UTMP *up, *ubase, *uceil;
  int self, seecloak;
  char userid[IDLEN + 1];

  ll_new();

  self = cuser.userno;
  seecloak = HAS_PERM(PERM_SEECLOAK);
  ubase = up = ushm->uslot;
  uceil = ubase + ushm->count;
  do
  {
    if (up->pid && up->userno != self && str_cmp(up->userid, STR_GUEST) &&
      ((seecloak || !(up->ufo & UFO_CLOAK)) || (can_see(up)==2 && !HAS_PERM(PERM_SYSOP))))
      ll_add(up->userid);
  } while (++up <= uceil);
#if 0
  if (!vget(1, 0, "請輸入代號(按空白鍵列出站上使用者)：", userid, IDLEN + 1, GET_LIST))
  {
    vmsg(err_uid);
    return 0;
  }
#endif  
  
  up = ubase;
  do
  {
    if (!str_cmp(userid, up->userid))
    {
     if (HAS_PERM(PERM_PAGE) && can_message(up))
     {
       BMW bmw;
       char buf[20];

       sprintf(buf, "★[%s]", up->userid);
       bmw_edit(up, buf, &bmw, 0);
     }
     
     return 0;
    
    }    
  } while (++up <= uceil);

  return 0;
}
#endif

/*超炫回顧*/
static void bmw_display(int max,int pos)
{
  int i=9, j, sent;
  BMW bmw,bmw2;
  char buf[128],color[10];

//  bmw_pos = pos;
  move(i, 0);
  clrtobot();
  i++;
  move(i,0);
  prints(" \033[1;36m�鵂w─────�瀵033[43;37m              夢大超炫水球回顧              \033[40;36m�礜w─────�鎪033[m");
  
  i++;
  for (max=0;max<8 ;max++)
  {    
    bmw = bmw_lslot[max];
    if (max == pos)
      bmw2 = bmw;
        

    if(max == pos)
      sprintf(color,"1;45");
    else
      sprintf(color,"0");
      
    if (strstr(bmw.msg,"★廣播"))
      sprintf(buf, "   \033[1;45;37m[%-12s]\033[%sm %-58s\033[m", bmw.userid,color,(bmw.msg+8));
    else
      sprintf(buf, "   \033[37;%sm[\033[33m%-12s\033[37m] %-58s\033[m",color, bmw.userid,bmw.msg);
    move(i,0);
    outs(buf);
    i++;
  }
  
  move(i, 0);
  outs(" \033[1;36m├────────────────────────────────────┤\033[m\n");
  sent = 0;
  for (j=0; j<BMW_LOCAL_MAX; j++)
  {
    bmw = bmw_sentlot[j];
    if (!strncmp(bmw.userid, bmw2.userid, IDLEN))
    {
      sent = 1;
      bmw = bmw_sentlot[j];
      sprintf(buf, "  [1;32mTo %-12s\033[m: \033[32m%-57s\033[m", bmw.userid, bmw.msg);
      outs(buf);
      break;
    }
  }
  if (!sent) 
    prints("  \033[32m尚未傳訊給 %s\033[m",bmw2.userid);
  move(i+2, 0);
  prints(" \033[1;36m�鶔w─────�瀵033[43;37m 操作 (←、Enter)離開 (↑↓)選擇 (其他)回訊 \033[40;36m�礜w─────�贕033[m ");
  
}


void bmw_reply(int replymode)/* 0:一次ctrl+r 1:兩次ctrl+r */
{
  int userno, max, pos, cc, mode;
  UTMP *up, *uhead;
  BMW bmw;
  screenline sl[2],slt[b_lines + 1];
  char buf[128];

  max = bmw_locus - 1;

  save_foot(sl); /* Thor.981222: 想清掉message */

  if (max < 0)
  {
    vmsg("先前並無熱訊呼叫");
    restore_foot(sl); /* Thor.981222: 想清掉message */
    return;
  }

  if (cuser.ufo2 & UFO2_REPLY || replymode)
    vs_save(slt);         /* 記錄 bmd_display 之前的 screen */
        
  mode = bbsmode;               /* lkchu.981201: save current mode */
  utmp_mode(M_BMW_REPLY);

#if 0  /* Thor.981222: 想清掉message */
  save_foot(sl);
#endif

  if (!(cuser.ufo2 & UFO2_REPLY) && !replymode)
  {
    move(b_lines - 1, 0);
    clrtoeol();
//    outs("\033[34;46m 熱訊回應 \033[31;47m (←)\033[30m離開 \033[31m(↑↓→)\033[30m瀏覽 \033[31m(Enter)\033[30m選擇線上使用者扣應 \033[31m(其他)\033[30m回應 \033[m");
    outs("\033[34;46m 熱訊回應 \033[31;47m (← Enter)\033[30m離開 \033[31m(↑↓→)\033[30m瀏覽 \033[31m(其他)\033[30m回應 \033[m");
  }

  cc = 12345;
  pos = max;

  uhead = ushm->uslot;

  for (;;)
  {
    if (cc == 12345)
    {
      bmw = bmw_lslot[pos];
      if (cuser.ufo2 & UFO2_REPLY || replymode)
        bmw_display(max, pos);
      else 
      {
        if (strstr(bmw.msg,"★廣播"))
          sprintf(buf, BMW_FORMAT_BC, bmw.userid, (bmw.msg)+8);
        else
          sprintf(buf, BMW_FORMAT, bmw.userid, bmw.msg);
        outz(buf);
      }
    }

    cc = vkey();

#if 0
    if (cc == '\n')	/* lkchu.981201: 按 Enter 選擇線上 user */
    {
      screenline slp[b_lines + 1];
      
      vs_save(slp);
      bmw_choose();
      memcpy(&slp[b_lines - 1], &sl, sizeof(screenline) * 2);
                         /* lkchu.981201: 讓 foot 不用重覆 restore */
      vs_restore(slp);
      if (cuser.ufo2 & UFO2_REPLY || replymode)
        vs_restore(slt);      /* 還原 bmw_display 之前的 screen */
      utmp_mode(mode);      /* lkchu.981201: restore bbsmode */
      return;
    }
#else
    if (cc == '\n')
    {
      if (cuser.ufo2 & UFO2_REPLY || replymode)
        vs_restore(slt);      /* 還原 bmw_display 之前的 screen */
      else
        restore_foot(sl);
      utmp_mode(mode);
      return;
    }
#endif

    if (cc == Ctrl('R') && !replymode)
    {
      bmw_reply(1);
      break;
    }

    if (cc == KEY_UP)
    {
      if (pos > 0)
      {
	pos--;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_DOWN)
    {
      if (pos < max)
      {
	pos++;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_RIGHT)
    {
      if (pos != max)
      {
	pos = max;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_LEFT)
    {
      if (cuser.ufo2 & UFO2_REPLY || replymode)
        vs_restore(slt);      /* 還原 bmw_display 之前的 screen */
      break;
    }

    if (isprint2(cc))
    {
      userno = bmw.sender; /* Thor.980805: 防止系統協尋回扣 */
      if(!userno)
      {
        vmsg("系統訊息無法回扣"); 
//        vmsg("您並不是對方的好友，無法回扣上站通知");
				/* lkchu.981201: 好友可回扣 */
                                          
        break;
      }
      if(bmw.caller->ufo & UFO_REJECT)
      {
        vmsg("對方有事，請稍待一會兒....");
        break;
      }

      up = bmw.caller;
#if 1    
      if ((up < uhead) || (up > uhead + MAXACTIVE /*ushm->offset*/))
				/* lkchu.981201: comparison of distinct pointer types */
      {
	vmsg(MSG_USR_LEFT);
	break;
      }
#endif
      /* userno = bmw.sender; */ /* Thor.980805: 防止系統回扣 */
      if (up->userno != userno)
      {
	up = utmp_find(userno);
	if (!up)
	{
	  vmsg(MSG_USR_LEFT);
	  break;
	}
      }

#ifdef  HAVE_SHOWNUMMSG
      if(up->num_msg > 9 && (up->ufo & UFO_MAXMSG) && !HAS_PERM(PERM_SYSOP))
      {
        vmsg("對方已經被水球灌爆了!!");
        break;
      }
#endif

#ifdef  HAVE_BANMSG
      if(!(up->ufo & UFO_MESSAGE) && can_banmsg(up))
      {
        vmsg("對方不想聽到您的聲音!!");
        break;
      }
#endif

      /* bmw_edit(up, "★回應：", &bmw, cc); */
      /* Thor.981214: 為使回扣不致弄混 */
      sprintf(buf,"★[%s]", up->userid);
      bmw_edit(up, buf, &bmw, cc); 
      break;
    }
  }

  if (cuser.ufo2 & UFO2_REPLY || replymode)
    vs_restore(slt);
  else
    restore_foot(sl);

  utmp_mode(mode);      /* lkchu.981201: restore bbsmode */
}





#if 0
void
bmw_reply()
{
  int userno, max, pos, cc, mode;
  UTMP *up, *uhead;
  BMW bmw;
  screenline sl[2];
  char buf[128];

  max = bmw_locus - 1;

  save_foot(sl); /* Thor.981222: 想清掉message */

  if (max < 0)
  {
    vmsg("先前並無熱訊呼叫");
    restore_foot(sl); /* Thor.981222: 想清掉message */
    return;
  }
  
  mode = bbsmode;               /* lkchu.981201: save current mode */
  utmp_mode(M_BMW_REPLY);

#if 0  /* Thor.981222: 想清掉message */
  save_foot(sl);
#endif

  move(b_lines - 1, 0);
  outs("\033[34;46m 熱訊回應 \033[31;47m (←)\033[30m離開 \033[31m(↑↓→)\033[30m瀏覽 \033[31m(Enter)\033[30m選擇線上使用者扣應 \033[31m(其他)\033[30m回應 \033[m");

  cc = 12345;
  pos = max;

  uhead = ushm->uslot;

  for (;;)
  {
    if (cc == 12345)
    {
      bmw = bmw_lslot[pos];
      if (strstr(bmw.msg,"★廣播"))
        sprintf(buf, BMW_FORMAT_BC, bmw.userid, (bmw.msg)+8);
      else
        sprintf(buf, BMW_FORMAT, bmw.userid, bmw.msg);
      outz(buf);
    }

    cc = vkey();

#if 0
    if (cc == '\n')	/* lkchu.981201: 按 Enter 選擇線上 user */
    {
      screenline slp[b_lines + 1];
      
      vs_save(slp);
      bmw_choose();
      memcpy(&slp[b_lines - 1], &sl, sizeof(screenline) * 2);
                         /* lkchu.981201: 讓 foot 不用重覆 restore */
      vs_restore(slp);
      utmp_mode(mode);      /* lkchu.981201: restore bbsmode */
      return;
    }
#else
    if (cc == '\n')
    {
      restore_foot(sl);
      utmp_mode(mode);
      return;
    }
#endif

    if (cc == KEY_UP)
    {
      if (pos > 0)
      {
	pos--;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_DOWN)
    {
      if (pos < max)
      {
	pos++;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_RIGHT)
    {
      if (pos != max)
      {
	pos = max;
	cc = 12345;
      }
      continue;
    }

    if (cc == KEY_LEFT)
      break;

    if (isprint2(cc))
    {
      userno = bmw.sender; /* Thor.980805: 防止系統協尋回扣 */
      if(!userno)
      {
        vmsg("系統訊息無法回扣"); 
/*        vmsg("您並不是對方的好友，無法回扣上站通知");*/
				/* lkchu.981201: 好友可回扣 */
                                          
        break;
      }
      if(bmw.caller->ufo & UFO_REJECT)
      {
        vmsg("對方有事，請稍待一會兒....");
        break;
      }

      up = bmw.caller;
#if 1    
      if ((up < uhead) || (up > uhead + MAXACTIVE /*ushm->offset*/))
				/* lkchu.981201: comparison of distinct pointer types */
      {
	vmsg(MSG_USR_LEFT);
	break;
      }
#endif
      /* userno = bmw.sender; */ /* Thor.980805: 防止系統回扣 */
      if (up->userno != userno)
      {
	up = utmp_find(userno);
	if (!up)
	{
	  vmsg(MSG_USR_LEFT);
	  break;
	}
      }

#ifdef  HAVE_SHOWNUMMSG
      if(up->num_msg > 9 && (up->ufo & UFO_MAXMSG) && !HAS_PERM(PERM_SYSOP))
      {
        vmsg("對方已經被水球灌爆了!!");
        break;
      }
#endif

#ifdef  HAVE_BANMSG
      if(!(up->ufo & UFO_MESSAGE) && can_banmsg(up))
      {
        vmsg("對方不想聽到您的聲音!!");
        break;
      }
#endif

      /* bmw_edit(up, "★回應：", &bmw, cc); */
      /* Thor.981214: 為使回扣不致弄混 */
      sprintf(buf,"★[%s]", up->userid);
      bmw_edit(up, buf, &bmw, cc); 
      break;
    }
  }

  restore_foot(sl);
  utmp_mode(mode);      /* lkchu.981201: restore bbsmode */
}
#endif

/* ----------------------------------------------------- */
/* Thor.0607: system assist user login notify		 */
/* ----------------------------------------------------- */


#define MSG_CC "\033[32m[群組名單]\033[m\n"


int
pal_list(reciper)
  int reciper;
{
  LIST list;
  int userno, fd;
  char buf[32], fpath[64],msg[128],temp[30];
  ACCT acct;
  sprintf(msg,"A)增加 D)刪除 F)好友 G)條件 [1~%d]群組名單 M)定案 Q)取消？[M]",MAX_LIST);

  userno = 0;

  for (;;)
  {
    switch (vget(1, 0, msg , buf, 2, LCECHO))
    {
    case '1': case '2': case '3': case '4': case '5': case '6': case '7':
    case '8': case '9':
      sprintf(temp,"list.%c",*buf);
      usr_fpath(fpath,cuser.userid,temp);
      fd = open(fpath,O_RDONLY);
      while (fd)
      {
        if(read(fd,&list,sizeof(LIST)) == sizeof(LIST))
        {
          if (!ll_has(list.userid))
          {
            ll_add(list.userid);
            reciper++;
            ll_out(3, 0, MSG_CC);
          }
        }
        else
        {
          close(fd);
          break;
        }
      }
    
      break;
    case 'a':
      while (acct_get("請輸入代號(只按 ENTER 結束新增): ", &acct) > 0)
      {
	if (!ll_has(acct.userid))
	{
	  ll_add(acct.userid);
	  reciper++;
	  ll_out(3, 0, MSG_CC);
	}
      }
      break;

    case 'd':

      while (reciper)
      {
	if (!vget(1, 0, "請輸入代號(只按 ENTER 結束刪除): ",
	    buf, IDLEN + 1, GET_LIST))
	  break;
	if (ll_del(buf))
	  reciper--;
	ll_out(3, 0, MSG_CC);
      }
      break;
#if 1
    case 'g':
      if ((userno = vget(b_lines, 0, "群組條件：", buf, 16, DOECHO)))
	str_lower(buf, buf);
#endif
    case 'f':
      usr_fpath(fpath, cuser.userid, FN_PAL);
      if ((fd = open(fpath, O_RDONLY)) >= 0)
      {
	PAL *pal;
	char *userid;

	mgets(-1);
	while ((pal = mread(fd, sizeof(PAL))))
	{
	  userid = pal->userid;
	  if (!ll_has(userid) && (pal->userno != cuser.userno) &&
	    !(pal->ftype & PAL_BAD) &&
	    (!userno || str_str(pal->ship, buf)))
	  {
	    ll_add(userid);
	    reciper++;
	  }
	}
	close(fd);
      }
      ll_out(3, 0, MSG_CC);
      userno = 0;
      break;

    case 'q':
      return 0;

    default:
      return reciper;
    }
  }
}


#ifdef HAVE_ALOHA
void
aloha()
{
  UTMP *up, *ubase, *uceil;
  int fd;
  char fpath[64];
  BMW *bmw, benz;
  int userno;
  struct stat st;
  
  /* lkchu.981201: 好友通知 */

  usr_fpath(fpath, cuser.userid, FN_FRIEND_BENZ);
  if (((fd = open(fpath, O_RDONLY)) >= 0) && !fstat(fd, &st))
  {
    if(st.st_size <= 0)
    {
      close(fd);
      return;
    }
    benz.caller = cutmp;
    /* benz.sender = cuser.userno; */
    benz.sender = 0; /* Thor.980805: 系統協尋為單向 call in */
    strcpy(benz.userid, cuser.userid);
    sprintf(benz.msg, "◎ 進 入 (%s) 囉!! ◎", BOARDNAME);

    ubase = ushm->uslot;
    uceil = (void *) ubase + ushm->offset;

    mgets(-1);
    while ((bmw = mread(fd, sizeof(BMW))))
    {
      /* Thor.1030:只通知朋友 */

      userno = bmw->recver;
      /* up = bmw->caller; */
      up = utmp_find(userno);	/* lkchu.981201: frienz 中的 utmp 無效 */
      
      if (up >= ubase && up <= uceil &&
	up->userno == userno && !(cuser.ufo & UFO_CLOAK) && can_message(up))
                                 /* Thor.980804: 隱身上站不通知, 站長特權 */
                                 /* lkchu.981201: 對方要打開「好友上站通知」才通知 */
      {
        /*if (is_pal(userno))*/           /* lkchu.980806: 好友才可以 reply */
        /*  benz.sender = cuser.userno;
        else*/
        if(!is_bad(userno) || (up->userlevel & PERM_SYSOP))
        {
          benz.sender = 0;

  	  benz.recver = userno;
	  bmw_send(up, &benz);
	}
      }
    }
    close(fd);
  }
}
#endif


#ifdef LOGIN_NOTIFY
extern LinkList *ll_head;


int
t_loginNotify()
{
  LinkList *wp;
  BMW bmw;
  char fpath[64];

  /* 設定 list 的名單 */

  vs_bar("系統協尋網友");

  ll_new();

  if (pal_list(0))
  {
    wp = ll_head;
    bmw.caller = cutmp;
    bmw.recver = cuser.userno;
    strcpy(bmw.userid, cuser.userid);

    /* Thor.980629: 小bug, 可以協尋自己, 進而call-in自己:P */

    do
    {
      usr_fpath(fpath, wp->data, FN_BENZ);
      rec_add(fpath, &bmw, sizeof(BMW));
    } while ((wp = wp->next));

    vmsg("協尋設定完成, 朋友上站時" SYSOPNICK "會通知您,請勿重覆設定!");

    /* Thor.1030 : 若不是朋友就不通知啦...... 以免callin 不對稱 */
  }
  return 0;
}


void
loginNotify()
{
  UTMP *up, *ubase, *uceil;
  int fd;
  char fpath[64];
  BMW *bmw, benz;
  int userno;

  usr_fpath(fpath, cuser.userid, FN_BENZ);

  if ((fd = open(fpath, O_RDONLY)) >= 0)
  {
    vs_bar("系統協尋網友");

#if 0
    move(10, 0);
    clrtobot();
    outs("★ " SYSOPNICK "：『曾經有人想找您哦……\n");
#endif

    benz.caller = cutmp;
    /* benz.sender = cuser.userno; */
    benz.sender = 0; /* Thor.980805: 系統協尋為單向 call in */
    strcpy(benz.userid, cuser.userid);
    sprintf(benz.msg, "◎ 剛剛踏進%s的門 [系統協尋] ◎", BOARDNAME);

    ubase = ushm->uslot;
    uceil = (void *) ubase + ushm->offset;

    mgets(-1);
    while ((bmw = mread(fd, sizeof(BMW))))
    {
      /* Thor.1030:只通知朋友 */

      up = bmw->caller;
      userno = bmw->recver;

      if (up >= ubase && up <= uceil &&
	up->userno == userno && !(cuser.ufo & UFO_CLOAK) && userno != cuser.userno  && can_see(up) !=2)
                                 /* Thor.980804: 隱身上站不通知, 站長特權 */
#if 0  
        /* Thor.980805: 不會回扣了, 單向 call-in */
        && is_pal(userno) && 
        userno != cuser.userno && !(cuser.ufo & UFO_QUIET))
        /* Thor.980707: 不尋自己 */ /* Thor.980804: 遠離塵囂時不通知,以防回扣 */
#endif
      {
/*        benz.sender = is_pal(userno) ? cuser.userno : 0; */
                      /* lkchu.980806: 好友才可以 reply */
        if(!is_bad(userno) || (up->userlevel & PERM_SYSOP))
        {              
	  benz.sender = 0;
	  benz.recver = userno;
	  bmw_send(up, &benz);

          prints("*");  /* Thor.980707: 有通知到的有所不同 */
        }
      }

      prints("%-13s", bmw->userid);

    }
    close(fd);
    unlink(fpath);
    vmsg(NULL);
  }
}
#endif


/* Thor: for ask last call-in messages */

/* lkchu: 熱訊回顧新介面 */
int
t_recall()
{
  xover(XZ_BMW);
  return 0;
}


#ifdef LOG_TALK
void
talk_save()
{
  char fpath[64];
  struct stat st;
  
  usr_fpath(fpath, cuser.userid, FN_TALK_LOG);
  stat(fpath, &st);

  if (!(cuser.ufo2 & UFO2_NTLOG) && st.st_size > 0)
  {
    char buf[64];
    HDR fhdr;
    
    usr_fpath(buf, cuser.userid, fn_dir);
    hdr_stamp(buf, HDR_LINK, &fhdr, fpath);
    strcpy(fhdr.title, "[備 忘 錄] 聊天紀錄");
    strcpy(fhdr.owner, cuser.userid);
    fhdr.xmode = MAIL_READ | MAIL_NOREPLY;
    rec_add(buf, &fhdr, sizeof(fhdr)); 

  } 
  unlink(fpath);
  return;


#if 0
  /* lkchu.981201: 放進私人信箱內/清除/保留 */
  if (dashf(fpath))
  {
    
    switch (vans("本次聊天紀錄處理 (M)備忘錄 (C)清除？[M] "))
    {
    case 'c':
      unlink(fpath);
      break;
      
    default:
      {
        char buf[64];
        HDR fhdr;
        
        usr_fpath(buf, cuser.userid, fn_dir);
        hdr_stamp(buf, HDR_LINK, &fhdr, fpath);
        strcpy(fhdr.title, "[備 忘 錄] 聊天紀錄");
        strcpy(fhdr.owner, cuser.userid);
        fhdr.xmode = MAIL_READ | MAIL_NOREPLY;
        rec_add(buf, &fhdr, sizeof(fhdr)); 

        unlink(fpath);
      }
      break;
    }      
  }
#endif

}
#endif


#ifdef LOG_BMW
void
bmw_save()
{
  int fd,check_max;
  char ans;
  char fpath[64],buf[128];
  struct stat st;
    
  
  usr_fpath(fpath, cuser.userid, FN_BMW);
  fd = f_open(fpath);	/* lkchu.990428: 若 size 為 0 會被 unlink 掉 */
  if (fd >= 0)
  {
    fstat(fd, &st);
    check_max = st.st_size;
  }
  else
    check_max = 0;


  /* lkchu.981201: 放進私人信箱內/清除/保留 */
  if (fd >= 0 && check_max > sizeof(BMW))
  {
    if(check_max >= BMW_MAX_SIZE * 1024)
      ans = 'm';
    else if(cuser.ufo2 & UFO2_NWLOG)
      ans = 'r';
    else
    {
      sprintf(buf,"本次上站熱訊處理 (M)備忘錄 (R)保留 (C)清除 [%dk/%dk] ？[R] ",check_max/1024,BMW_MAX_SIZE);
      ans = vans(buf);
    }
    switch (ans)
    {
    case 'c':
      close(fd);
      unlink(fpath);
      break;

    case 'r':
      close(fd);
      break;
      
    case 'm':
      {
        FILE *fout;
        char buf[80], folder[80];
        HDR fhdr;
        
        usr_fpath(folder, cuser.userid, fn_dir);
        if ((fout = fdopen(hdr_stamp(folder, 0, &fhdr, buf), "w")))
        {        
          BMW bmw;
          
          while (read(fd, &bmw, sizeof(BMW)) == sizeof(BMW)) 
          {
            struct tm *ptime = localtime(&bmw.btime);
            
            fprintf(fout, "%s%s(%02d:%02d)：%s\033[m\n", 
              bmw.sender == cuser.userno ? "☆" : "\033[32m★",
              bmw.userid, ptime->tm_hour, ptime->tm_min, bmw.msg);
          }
          fclose(fout);
        }
        close(fd);
        
        fhdr.xmode = MAIL_READ | MAIL_NOREPLY;
	strcpy(fhdr.title, "[備 忘 錄] 熱訊紀錄");
	strcpy(fhdr.owner, cuser.userid);
        rec_add(folder, &fhdr, sizeof(fhdr));

        unlink(fpath);
      }
      break;
      
    default:
      close(fd);
      break;
    }      
  }

}
#endif


void
bmw_rqst()
{
  int i, j, userno, locus;
  BMW bmw[BMW_PER_USER], *mptr, **mslot;

  /* download BMW slot first */

  i = j = 0;
  userno = cuser.userno;
  mslot = cutmp->mslot;

  while ((mptr = mslot[i]))
  {
    mslot[i] = NULL;
    if (mptr->recver == userno)
    {
      bmw[j++] = *mptr;
    }
    mptr->btime = 0;

    if (++i >= BMW_PER_USER)
      break;
  }

  /* process the request */

  if (j)
  {
    char buf[128];

    locus = bmw_locus;
    i = locus + j - BMW_LOCAL_MAX;
    if (i >= 0)
    {
      locus -= i;
      memcpy(bmw_lslot, bmw_lslot + i, locus * sizeof(BMW));
    }

    i = 0;
    do
    {
      mptr = &bmw[i];

      /* lkchu.981230: 利用 xover 整合 bmw */
      usr_fpath(buf, cuser.userid, FN_BMW);
      rec_add(buf, &bmw[i], sizeof(BMW)); 
                  
      bmw_lslot[locus++] = *mptr;
    } while (++i < j);

    bmw_locus = locus;

    if(!(cutmp->ufo & (UFO_REJECT | UFO_NET)))
    {
      if (strstr(mptr->msg,"★廣播"))
         sprintf(buf, BMW_FORMAT_BC, mptr->userid, (mptr->msg)+8);
      else
         sprintf(buf, BMW_FORMAT, mptr->userid, mptr->msg);

      /* Thor.980827: 為了防止列印一半(more)時熱訊而後列印超過範圍踢人, 
                      故存下游標位置 */
      cursor_save(); 

      outz(buf);
      /* Thor.980827: 為了防止列印一半(more)時熱訊而後列印超過範圍踢人, 
                      故還原游標位置 */
      cursor_restore();

      refresh();
      bell();
#ifdef	HAVE_SHOWNUMMSG      
      cutmp->num_msg++;
#endif      
    }
  }
}


/* ----------------------------------------------------- */
/* talk sub-routines					 */
/* ----------------------------------------------------- */


static void
talk_nextline(twin)
  talk_win *twin;
{
  int curln;

  curln = twin->curln + 1;
  if (curln > twin->eline)
    curln = twin->sline;
  if (curln != twin->eline)
  {
    move(curln + 1, 0);
    clrtoeol();
  }
  move(curln, 0);
  clrtoeol();
  twin->curcol = 0;
  twin->curln = curln;
}


static void
talk_char(twin, ch)
  talk_win *twin;
  int ch;
{
  int col, ln;

  col = twin->curcol;
  ln = twin->curln;

  if (isprint2(ch))
  {
    if (col < 79)
    {
      move(ln, col);
      twin->curcol = ++col;
    }
    else
    {
      talk_nextline(twin);
      twin->curcol = 1;
    }
    outc(ch);
  }
  else if (ch == '\n')
  {
    talk_nextline(twin);
  }
  else if (ch == Ctrl('H'))
  {
    if (col)
    {
      twin->curcol = --col;
      move(ln, col);
      outc(' ');
      move(ln, col);
    }
  }
  else if (ch == Ctrl('G'))
  {
    bell();
  }
}


static void
talk_string(twin, str)
  talk_win *twin;
  uschar *str;
{
  int ch;

  while ((ch = *str))
  {
    talk_char(twin, ch);
    str++;
  }
}


static void
talk_speak(fd)
  int fd;
{
  talk_win mywin, itswin;
  uschar data[80];
  char buf[80];
  int i, ch;
#ifdef  LOG_TALK
  char mywords[80], itswords[80], itsuserid[40];
  FILE *fp;
    
  /* lkchu: make sure that's empty */
  mywords[0] = itswords[0] = '\0';
  
  strcpy(itsuserid, page_requestor);
  strtok(itsuserid, " (");
#endif

  utmp_mode(M_TALK);

  ch = 58 - strlen(page_requestor);

  sprintf(buf, "%s【%s", cuser.userid, cuser.username);

  i = ch - strlen(buf);
  if (i >= 0)
  {
    i = (i >> 1) + 1;
  }
  else
  {
    buf[ch] = '\0';
    i = 1;
  }
  memset(data, ' ', i);
  data[i] = '\0';

  memset(&mywin, 0, sizeof(mywin));
  memset(&itswin, 0, sizeof(itswin));

  i = b_lines >> 1;
  mywin.eline = i - 1;
  itswin.curln = itswin.sline = i + 1;
  itswin.eline = b_lines - 1;

  clear();
  move(i, 0);
  prints("\033[1;46;37m  談天說地  \033[45m%s%s】 ◆  %s%s\033[m",
    data, buf, page_requestor, data);
#if 1
  outz("\033[34;46m 交談模式 \033[31;47m (^A)\033[30m對奕模式 \033[31m(^B)\033[30m象棋模式 \033[31m(^C,^D)\033[30m結束交談 \033[31m(^Z)\033[30m快捷列表 \033[31m(^G)\033[30m嗶嗶 \033[m");
#endif
  move(0, 0);

#ifdef LOG_TALK                            /* lkchu.981201: 聊天記錄 */
  usr_fpath(buf, cuser.userid, FN_TALK_LOG);
  if ((fp = fopen(buf, "a+")))
    fprintf(fp, "【 %s 與 %s 之聊天記錄 】\n", cuser.userid, page_requestor);
#endif

  add_io(fd, 60);

  for (;;)
  {
    ch = igetch();

    if (ch == KEY_ESC)
    {
      igetch();
      igetch();
      continue;
    }

#ifdef EVERY_Z
    /* Thor.0725: talk中, ctrl-z */
    if (ch == Ctrl('Z'))
    {
      screenline sl[b_lines + 1];
      char buf[IDLEN + 1];
      /* Thor.0731: 暫存 mateid, 因為有可能query別人時會用掉mateid */
      strcpy(buf, cutmp->mateid);

      /* Thor.0727: 暫存 vio_fd */
      holdon_fd = vio_fd;
      vio_fd = 0;
      vs_save(sl);
      every_Z();
      vs_restore(sl);
      /* Thor.0727: 還原 vio_fd */
      vio_fd = holdon_fd;
      holdon_fd = 0;

      /* Thor.0731: 還原 mateid, 因為有可能query別人時會用掉mateid */
      strcpy(cutmp->mateid, buf);
      continue;
    }
#endif
    if (ch == Ctrl('U'))
    {
      char buf[IDLEN + 1];
      strcpy(buf, cutmp->mateid);

      holdon_fd = vio_fd;
      vio_fd = 0;
      every_U();
      vio_fd = holdon_fd;
      holdon_fd = 0;

      strcpy(cutmp->mateid, buf);
      continue;
    }
    if (ch == Ctrl('D') || ch == Ctrl('C'))
      break;

    if (ch == I_OTHERDATA)
    {
      ch = recv(fd, data, 80, 0);
      if (ch <= 0)
	break;
#if 1
      if (data[0] == Ctrl('A'))
      { /* Thor.990219: 呼叫外掛棋盤 */
        if(DL_func("bin/bwboard.so:vaBWboard",fd,1)==-2)
          break;
        continue;
      }
      if (data[0] == Ctrl('B'))
      {
        if(DL_func("bin/chess.so:vaChess",fd,1)==-2)
          break;
        continue;
      }
#endif
      for (i = 0; i < ch; i++)
      {
	talk_char(&itswin, data[i]);
#ifdef	LOG_TALK
        switch (data[i])
        {
        case '\n':
	  /* lkchu.981201: 有換行就把 itswords 印出清掉 */
	  if (itswords[0] != '\0')
  	  {
  	    fprintf(fp, "\033[32m%s：%s\033[m\n", itsuserid, itswords);
	    itswords[0] = '\0';
	  }
	  break;

        case Ctrl('H'):	/* lkchu.981201: backspace */
          itswords[str_len(itswords) - 1] = '\0';
          break;
          
        default:
	  if (str_len(itswords) < sizeof(itswords))
  	  {
  	    strncat(itswords, (char *)&data[i], 1);
	  }
	  else	/* lkchu.981201: itswords 裝滿了 */
	  {
  	    fprintf(fp, "\033[32m%s：%s%c\033[m\n", itsuserid, itswords, data[i]);
	    itswords[0] = '\0';
	  }
	  break;
	}
#endif

      }
    }
#if 1
    else if (ch == Ctrl('A'))
    { /* Thor.990219: 呼叫外掛棋盤 */
      /* extern int BWboard(); */
      data[0] = ch;
      if (send(fd, data, 1, 0) != 1)
	break;
      /* if (BWboard(fd,0)==-2) */
      if(DL_func("bin/bwboard.so:vaBWboard",fd,0)==-2)
        break;
    }
    else if (ch == Ctrl('B'))
    { /* Thor.990219: 呼叫外掛棋盤 */
      /* extern int BWboard(); */
      data[0] = ch;
      if (send(fd, data, 1, 0) != 1)
        break;
      /* if (BWboard(fd,0)==-2) */
      if(DL_func("bin/chess.so:vaChess",fd,0)==-2)
        break;
    }
#endif
    else if (isprint2(ch) || ch == '\n' || ch == Ctrl('H') || ch == Ctrl('G'))
    {
      data[0] = ch;
      if (send(fd, data, 1, 0) != 1)
	break;

#ifdef LOG_TALK				/* lkchu: 自己說的話 */
      switch (ch)
      {
      case '\n':
        if (mywords[0] != '\0')
        {
          fprintf(fp, "%s：%s\n", cuser.userid, mywords);
  	  mywords[0] = '\0';
        }
        break;
      
      case Ctrl('H'):
        mywords[str_len(mywords) - 1] = '\0';
        break;
      
      default:
        if (str_len(mywords) < sizeof(mywords))
	{
          strncat(mywords, (char *)&ch, 1);
        }
        else
        {
          fprintf(fp, "%s：%s%c\n", cuser.userid, mywords, ch);
	  mywords[0] = '\0';
	}
        break;
      }
#endif

      talk_char(&mywin, ch);

#ifdef EVERY_BIFF 
      /* Thor.980805: 有人在旁邊按enter才需要check biff */ 
      if(ch=='\n')
      {
        static int old_biff,old_biffn; 
        int biff = cutmp->ufo & UFO_BIFF; 
        if (biff && !old_biff) 
          talk_string(&mywin, "◆ 噹! 郵差衝進來了!\n"); 
        old_biff = biff;
        biff = cutmp->ufo & UFO_BIFFN;                                       
        if (biff && !old_biffn)
          talk_string(&mywin, "◆ 噹! 您有神秘留言!\n");
        old_biffn = biff; 
      }
#endif
    }

  }

#ifdef LOG_TALK
  fclose(fp);
#endif

  add_io(0, 60);
}


#if 0
static int
xsocket()
{
  int sock, val;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock >= 0)
  {
    val = 64;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
  }
  return sock;
}
#endif


static void
talk_hangup(sock)
  int sock;
{
  cutmp->sockport = 0;
  add_io(0, 60);
  close(sock);
}


static char *talk_reason[] =
{
  "對不起，我有事情不能跟你 talk",
  "我現在很忙，請等一會兒再 call 我",
  "現在忙不過來，等一下我會主動 page 你",
  "我現在不想 talk 啦 ...",
  "你真的很煩，我實在不想跟你 talk"

#ifdef EVERY_Z
  ,"我的嘴巴正忙著和別人講話呢，沒有空的嘴巴了"
  /* Thor.0725: for chat&talk 用^z 作準備 */
#endif
};


/* return 0: 沒有 talk, 1: 有 talk, -1: 其他 */


static int
talk_page(up)
  UTMP *up;
{
  int sock, msgsock;
  struct sockaddr_in sin;
  pid_t pid;
  int ans, length,myans;
  char buf[60];
#if     defined(__OpenBSD__)
  struct hostent *h;
#endif

#if 0
  /* Thor.0523: 測試 500人以上, 禁止未認證user talk */
  if (ushm->number > 500 && !HAS_PERM(PERM_VALID))
  {
    vmsg("目前線上人數太多，您未通過認證，無法使用此功\能");
    return 0;
  }
#endif

#ifdef EVERY_Z
  /* Thor.0725: 為 talk & chat 可用 ^z 作準備 */
  if (holdon_fd)
  {
    vmsg("您講話講一半還沒講完耶");
    return 0;
  }
#endif

  pid = up->mode;

  if (pid >= M_BBTP && pid <= M_CHAT)
  {
    vmsg("對方無暇聊天");
    return 0;
  }

  if (!(pid = up->pid) || kill(pid, 0))
  {
    vmsg(MSG_USR_LEFT);
    return 0;
  }

  /* showplans(up->userid); */
#ifdef  HAVE_PIP_FIGHT
  myans = vans("要和他/她談天(Y)或對戰小雞(C)嗎 (Y/N/C)?[N] ");
#else
  myans = vans("確定要和他/她談天嗎 (Y/N)?[N] ");
#endif

#ifdef  HAVE_PIP_FIGHT
  if (myans != 'y' && myans != 'c')
#else
  if (myans != 'y')
#endif
    return 0;

#ifdef  HAVE_PIP_FIGHT
  if (myans == 'c')
  {
    usr_fpath(buf,up->userid,"chicken");
    if(access(buf,0))
    {
      vmsg("對方沒有養小雞！");
      return 1;
    }
  }
#endif

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return 0;

#if     defined(__OpenBSD__)                    /* lkchu */

  if (!(h = gethostbyname(MYHOSTNAME)))
    return -1;  
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  memcpy(&sin.sin_addr, h->h_addr, h->h_length);                

#else

  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  sin.sin_addr.s_addr = INADDR_ANY;
  memset(sin.sin_zero, 0, sizeof(sin.sin_zero));

#endif

  length = sizeof(sin);
  if (bind(sock, (struct sockaddr *) &sin, length) < 0 || getsockname(sock, (struct sockaddr *) &sin, &length) < 0)
  {
    close(sock);
    return 0;
  }

  cutmp->sockport = sin.sin_port;
  strcpy(cutmp->mateid, up->userid);
  up->talker = cutmp;
#ifdef  HAVE_PIP_FIGHT
  if(myans == 'c')
    utmp_mode(M_CHICKEN);
  else
#endif
    utmp_mode(M_PAGE);
  kill(pid, SIGUSR1);

  clear();
  prints("首度呼叫 %s ...\n可按 Ctrl-D 中止", up->userid);

  listen(sock, 1);
  add_io(sock, 20);
  do
  {
    msgsock = igetch();

    if (msgsock == Ctrl('D'))
    {
      talk_hangup(sock);
      return -1;
    }

    if (msgsock == I_TIMEOUT)
    {
      move(0, 0);
      outs("再");
      bell();

      if (kill(pid, SIGUSR1)) 
      /* Thor.990201:註解:其實這的kill,也只是看看對方是不是還在線上而已:p
                          重發signal其實似乎 talk_rqst不會再被叫 */
      {
	talk_hangup(sock);
	vmsg(MSG_USR_LEFT);
	return -1;
      }
    }
  } while (msgsock != I_OTHERDATA);

  msgsock = accept(sock, NULL, NULL);
  talk_hangup(sock);
  if (msgsock == -1)
    return -1;

  length = read(msgsock, buf, sizeof(buf));
  ans = buf[0];
  if (ans == 'y')
  {
    sprintf(page_requestor, "%s (%s)", up->userid, up->username);

    /*
     * Thor.0814: 注意! 在此有一個雞同鴨講的可能情況, 如果 askia 先page
     * starlight, 但在 starlight 回應前卻馬上離開, 換 page backspace,
     * backspace尚未回應前, 如果 starlight 回應了, starlight 就會被 accept,
     * 而不是 backspace.
     * 
     * 此時在螢幕中央, 看到的 page_requestor會是 backspace, 可是事實上,
     * talk的對象是 starlight, 造成雞同鴨講!
     * 
     * 暫時不予修正, 以作為花心者的懲罰:P
     */

    talk_speak(msgsock);
  }
#ifdef  HAVE_PIP_FIGHT
  else if(ans == 'c')
  {
    if(!p)
      p = DL_get("bin/pip.so:pip_vf_fight");
    if(p)
	{
	  up->pip = NULL;
      (*p)(msgsock,2);
	  cutmp->pip = NULL;
	}
    add_io(0,60);
/*    pip_vf_fight(msgsock,1);*/
  }
  else if(ans == 'C')
  {
    outs("【回音】對方不想 PK 小雞！");
  }
#endif
  else
  {
    char *reply;

    if (ans == ' ')
    {
      reply = buf;
      reply[length] = '\0';
    }
    else
      reply = talk_reason[ans - '1'];

    move(4, 0);
    outs("【回音】");
    outs(reply);
  }

  close(msgsock);
  cutmp->talker = NULL;
#ifdef  LOG_TALK
  if (ans == 'y' && cutmp->mode != M_CHICKEN)    /* mat.991011: 防止Talk被拒絕時，產生聊天記錄的record */
    talk_save();          /* lkchu.981201: talk 記錄處理 */
#endif
  vmsg("聊天結束");
  return 1;
}


/*-------------------------------------------------------*/
/* 選單式聊天介面					 */
/*-------------------------------------------------------*/


static PICKUP ulist_pool[MAXACTIVE];
#ifdef	FRIEND_FIRST
static int friend_num,ofriend_num,pfriend_num,bfriend_num;/* visor.991103: 記錄目前站上好友數 */
#endif
static int ulist_head();
static int ulist_init();
static XO ulist_xo;


static char *msg_pickup_way[PICKUP_WAYS] =
{
  "任意",
  "代號",
  "故鄉",
  "動態",
  "暱稱",
  "閒置"
#ifdef	HAVE_BOARD_PAL  
  ,"板友"
#endif
};

static char 
ck_state(in1,in2,up,mode)
  int in1;
  int in2;
  UTMP *up;
  int mode;
{
	if (up->ufo & in2)
	  return '#';
	else if (up->ufo & in1)
	{
	  if(mode)
	    return can_override(up) ? 'o' : '*';
	  else
	    return can_message(up) ? 'o' : '*';
	}
	else
	  return ' ';
}	



static int
ulist_body(xo)
  XO *xo;
{
  PICKUP *pp;
  UTMP *up;
  int paltmp;
  int n, cnt, max, ufo, self, userno, sysop, diff, diffmsg, fcolor,colortmp;
  char buf[8],color[20],ship[80],*wcolor[7] = {"\033[m",COLOR_PAL,COLOR_BAD,COLOR_BOTH,COLOR_OPAL,COLOR_CLOAK,COLOR_BOARDPAL};

#ifdef HAVE_BOARD_PAL
  int isbpal;
  
  isbpal = (cutmp->board_pal != -1);
#endif
   
  max = xo->max;
  if (max <= 0)
  {
    return XO_QUIT;
  }

  cnt = xo->top;
  pp = &ulist_pool[cnt];
  self = cuser.userno;
  sysop = HAS_PERM(PERM_SYSOP | PERM_ACCOUNTS);

  n = 2;
  while (++n < b_lines)
  {
    move(n, 0);
    if (cnt++ < max)
    {
      up = pp->utmp;

#if 0      
      if(supervisor)
      {
        sprintf(ship,"%s,%d,%d",up->userid,pp->type,up->userno);
        vmsg(ship);
      }
#endif
	  
      if ((userno = up->userno) && (up->userid[0]) && !((up->ufo & UFO_CLOAK) && !HAS_PERM(PERM_SEECLOAK) && (up->userno != cuser.userno)) )
      {
	if ((diff = up->idle_time))
	  sprintf(buf, "%2d", diff);
	else
	  buf[0] = '\0';
	
	paltmp = (pp->type == 1 || pp->type == 2);
	fcolor = (userno == self) ? 3 : paltmp;

#ifdef	HAVE_BOARD_PAL	
	if(isbpal && is_boardpal(up) && userno != self) 
	  fcolor = 6;
#else
	if(userno != self) 
	  fcolor = 6;
#endif

        colortmp = (pp->type == 1 || pp->type == 3);

	if(paltmp && colortmp == 1) 
	  fcolor = 3;
        else if (!paltmp && colortmp == 1) 
          fcolor = 4;
	
        if (is_bad(userno)) 
          fcolor = 2;

        if(cuser.ufo2 & UFO2_SHIP)
        {
          strcpy(ship," ");
          copyship(ship,userno);
        }
        
	ufo = up->ufo;


        diff = ck_state(UFO_PAGER,UFO_PAGER1,up,1);
        diffmsg = ck_state(UFO_QUIET,UFO_MESSAGE,up,0);

        colortmp = 1;

        if(ufo & UFO_CLOAK) 
          fcolor = 5;
        else if(fcolor == 0)
          colortmp = 0;  

        strcpy(color,wcolor[fcolor]);

	prints("%5d%c%s%-13s%-22.21s%s%-16.15s%c%c %-16.16s%s",
	  cnt,(up->ufo & UFO_WEB)?'*':' ',
	  color, up->userid,
	  (HAS_PERM(PERM_SYSOP) && (cuser.ufo2 & UFO2_REALNAME))? up->realname : up->username , 
	  colortmp > 0 ? "\033[m" : "",
	  (cuser.ufo2 & UFO2_SHIP) ? ship : ((up->ufo & UFO_HIDEDN)&&!HAS_PERM(PERM_SYSOP)) ? 
	  HIDEDN_SRC : up->from , diff,diffmsg,
	  bmode(up, 0), buf);
      }
      else
      {
		outs("      < 此位網友正巧離開 >");
      }
      pp++;
    }
    clrtoeol();
  }

  return XO_NONE;
}


static int
ulist_cmp_userid(i, j)
  PICKUP *i, *j;
{
  if(i->type == j->type)
	return str_cmp(i->utmp->userid, j->utmp->userid);
  else
    return i->type - j->type;
}

static int
ulist_cmp_host(i, j)
  PICKUP *i, *j;
{
  return str_cmp(i->utmp->from, j->utmp->from);
}

static int
ulist_cmp_idle(i, j)
  PICKUP *i, *j;
{
  return i->utmp->idle_time - j->utmp->idle_time;
}

static int
ulist_cmp_mode(i, j)
  PICKUP *i, *j;
{
  return i->utmp->mode - j->utmp->mode;
}

static int
ulist_cmp_nick(i, j)
  PICKUP *i, *j;
{
  return str_cmp(i->utmp->username, j->utmp->username);
}

#ifdef	HAVE_BOARD_PAL
static int
ulist_cmp_board(i,j)
  PICKUP *i, *j;
{
  return i->utmp->board_pal - j->utmp->board_pal;
}
#endif

static int (*ulist_cmp[]) () =
{
  ulist_cmp_userid,
  ulist_cmp_host,
  ulist_cmp_mode,
  ulist_cmp_nick,
  ulist_cmp_idle
#ifdef	HAVE_BOARD_PAL  
  ,ulist_cmp_board
#endif
};


static int
ulist_init(xo)
  XO *xo;
{
  UTMP *up, *uceil;
  PICKUP *pp;
  int max, filter, seecloak, userno, self, nf_num,tmp;
  int ispal,bad;
#ifdef HAVE_BOARD_PAL
  int isbpal;
#endif

  pp = ulist_pool;

  self = cuser.userno;
  filter = cuser.ufo2 & UFO2_PAL;
  
  if(cutmp->pid <= 0 || cutmp->userno <= 0)
    reset_utmp();

  seecloak = HAS_PERM(PERM_SEECLOAK);

  up = ushm->uslot;
  uceil = (void *) up + ushm->offset;

  max = 0;
  bad = 0;
#ifdef HAVE_BOARD_PAL
  board_pals = 0;
  isbpal = (cutmp->board_pal != -1);
#endif
#ifdef  FRIEND_FIRST	/* by visor : 重寫好友置前 */

  friend_num = ofriend_num = pfriend_num = nf_num = bfriend_num = 0;

  do					/* 先找好友 */
  {
    userno = up->userno;
    if (userno <= 0 || (up->pid <= 0 && !HAS_PERM(PERM_SYSOP|PERM_SEECLOAK)))
      continue;
    if (!seecloak && (up->ufo & UFO_CLOAK))
      continue;      
    tmp = can_see(up);
    if(is_bad(userno)) 
    {
      bfriend_num++;
      bad = 1;
    }
    else
      bad = 0;
      
    if (((seecloak || !(up->ufo & UFO_CLOAK)) && (tmp != 2)) || HAS_PERM(PERM_SYSOP|PERM_SEECLOAK) || up->userno == cuser.userno)
    {
#ifdef HAVE_BOARD_PAL
      if(isbpal && is_boardpal(up)) 
        board_pals++;     
#endif
      ispal = is_pal(userno);

      if (!bad && (ispal && (tmp == 1)) || (userno == self))
      {
         pp->utmp = up;
         pp->type = 1;
         friend_num++;
         pp++;
      }
      else if(ispal && !(tmp == 1) && !filter && !bad)
      {
         pp->utmp = up;
         pp->type = 2;
         pfriend_num++;
         pp++;
      }
      else if(!ispal && (tmp == 1) && !filter && !bad)
      {
         pp->utmp = up;
         pp->type = 3;
         ofriend_num++;
         pp++;
      }
      else if(!filter)
      {
         pp->utmp = up;
         pp->type = 4;
         nf_num++;
         pp++;
      }
    }
  } while (++up <= uceil);
#else
  do
  {
    userno = up->userno;
    if (userno <= 0)
      continue;
    if ((userno == self) || ((seecloak || !(up->ufo & UFO_CLOAK))&&(can_see(up)!=2 || HAS_PERM(PERM_SYSOP)) && (!filter || is_pal(userno))))
    {    
      *pp++ = up;
    }
  } while (++up <= uceil);

#endif

  xo->max = max = pp - ulist_pool;

  if (xo->pos >= max)
    xo->pos = xo->top = 0;

  if ((max > 1) && (pickup_way))
  {
     xsort(ulist_pool, max, sizeof(PICKUP), ulist_cmp[pickup_way - 1]);
  }
  total_num = max;
  
/* cache.101023: shm爆炸造成人數亂掉後的自動修正 */
#ifdef AUTO_FIX_INFO
  ushm->count = total_num;
  
  extern BCACHE *bshm;
  if ((fd = brd_bno(currboard)) >= 0)
    bshm->mantime[fd] =   board_pals;       /* 最後看的那個板人數更新 */
#endif  
  
  return ulist_head(xo);
}



static int
ulist_neck(xo)
  XO *xo;
{
  move(1, 0);
#ifdef HAVE_BOARD_PAL
  prints("  排列方式：[\033[1m%s\033[m] 上站人數：%d %s我的朋友：%d %s與我為友：%d %s壞人：%d \033[0;36m板友：%d\033[m\n"
    "\033[30;47m No.  代號         %-22s%-13s   PM %-14s閒置\033[m",
    msg_pickup_way[pickup_way],total_num,COLOR_PAL,friend_num+pfriend_num,COLOR_OPAL,friend_num+ofriend_num,COLOR_BAD,bfriend_num,board_pals,
    (HAS_PERM(PERM_SYSOP) && (cuser.ufo2 & UFO2_REALNAME)) ? "真實姓名" : "暱  稱",
    (cuser.ufo2 & UFO2_SHIP) ? "好友描述" :"故鄉", "動態");
#else
  prints("  排列方式：[\033[1m%s\033[m] 上站人數：%d %s我的朋友：%d %s與我為友：%d %s壞人：%d\033[m\n"
    "\033[30;47m No.  代號         %-22s%-13s   PM %-14s閒置 \033[m",
    msg_pickup_way[pickup_way],total_num,COLOR_PAL,friend_num+pfriend_num,COLOR_OPAL,friend_num+ofriend_num,COLOR_BAD,bfriend_num,
    (HAS_PERM(PERM_SYSOP) && (cuser.ufo & UFO_REALNAME)) ? "真實姓名" : "暱  稱",
    (cuser.ufo2 & UFO2_SHIP) ? "好友描述" :"故鄉", "動態");
#endif

  return ulist_body(xo);
}


static int
ulist_head(xo)
  XO *xo;
{
  vs_head((cuser.ufo2 & UFO2_PAL)?"好友列表":"網友列表", str_site);
  return ulist_neck(xo);
}


static int
ulist_toggle(xo)
  XO *xo;
{
  int ans, max;
  ans = pickup_way + 1;
  ans %= PICKUP_WAYS;
  
  pickup_way = ans;
  max = xo->max;
  if (max <= 1)
    return XO_FOOT;
  return ulist_init(xo);
}


static int
ulist_pal(xo)
  XO *xo;
{
  cuser.ufo2 ^= UFO2_PAL;
  /* Thor.980805: 注意 ufo 同步問題 */
  return ulist_init(xo);
}


static int
ulist_search(xo, step)
  XO *xo;
  int step;
{
  int num, pos, max;
  PICKUP *pp;
  static char buf[IDLEN + 1];

  if (vget(b_lines, 0, msg_uid, buf, IDLEN + 1, GCARRY))
  {
    int buflen;
    char bufl[IDLEN + 1];

    str_lower(bufl, buf);
    buflen = strlen(bufl); /* Thor: 必定大於0 */
    
    pos = num = xo->pos;
    max = xo->max;
    pp = ulist_pool;
    do
    {
      pos += step;
      if (pos < 0) /* Thor.990124: 假設 max 不為0 */
        pos = max - 1;
      else if (pos >= max)
	pos = 0;

      /* Thor.990124: id 則從頭 match */
      /* if (str_ncmp(pp[pos]->userid, bufl, buflen)==0 */
      
      if (str_str(pp[pos].utmp->userid, bufl) /* lkchu.990127: 找部份 id 好像比較好用 :p */
       || str_str(pp[pos].utmp->username, bufl)) /* Thor.990124: 可以找 部分 nickname */
      {
	move(b_lines, 0);
	clrtoeol();
	return pos + XO_MOVE;
      }

    } while (pos != num);
  }

  return XO_FOOT;
}

static int
ulist_search_forward(xo)
  XO *xo;
{
  return ulist_search(xo, 1); /* step = +1 */
}

static int
ulist_search_backward(xo)
  XO *xo;
{
  return ulist_search(xo, -1); /* step = -1 */
}



static int
ulist_makepal(xo)
  XO *xo;
{
  if (cuser.userlevel)
  {
    UTMP *up;
    int userno;

    up = ulist_pool[xo->pos].utmp;
    userno = up->userno;
    if (userno > 0 && !is_pal(userno) && !is_bad(userno)   /* 尚未列入好友名單 */
         && (userno != cuser.userno))	/* lkchu.981217: 自己不可為好友 */
					
    {
      PAL pal;
      char buf[80];

      strcpy(buf, up->userid);

      vget(b_lines, 0, "好友描述：", pal.ship, sizeof(pal.ship), DOECHO);
      pal.ftype = 0;
      pal.userno = userno;
      strcpy(pal.userid, buf);
      usr_fpath(buf, cuser.userid, FN_PAL);
      if(rec_num(buf,sizeof(PAL)) < PAL_MAX)
      {
        rec_add(buf, &pal, sizeof(PAL));
        pal_cache();
        return ulist_init(xo);
      }
      else
      {
        vmsg("您的好友名單太多，請善加整理");
      }
    }
  }
  return XO_FOOT;
}

static int
ulist_makebad(xo)
  XO *xo;
{
  if (cuser.userlevel)
  {
    UTMP *up;
    int userno;

    up = ulist_pool[xo->pos].utmp;
    userno = up->userno;
    if (userno > 0 && !is_pal(userno) && !is_bad(userno)  /* 尚未列入好友名單 */
         && (userno != cuser.userno))	/* lkchu.981217: 自己不可為好友 */
					
    {
      PAL pal;
      char buf[80];

      strcpy(buf, up->userid);

      vget(b_lines, 0, "惡行惡狀：", pal.ship, sizeof(pal.ship), DOECHO);
      pal.ftype = PAL_BAD;
      pal.userno = userno;
      strcpy(pal.userid, buf);
      usr_fpath(buf, cuser.userid, FN_PAL);
      if(rec_num(buf,sizeof(PAL)) < PAL_MAX)
      {
        rec_add(buf, &pal, sizeof(PAL));
        pal_cache();
        return ulist_init(xo);
      }
      else
        vmsg("您的好友名單太多，請善加整理");
    }
  }
  return XO_NONE;
}


static int
ulist_mail(xo)
  XO *xo;
{
  char userid[IDLEN + 1];

  /* Thor.981022:不給沒基本權的寄信 */
  if (!HAS_PERM(PERM_INTERNET) || HAS_PERM(PERM_DENYMAIL) || !cuser.userlevel)
    return XO_NONE;

  strcpy(userid, ulist_pool[xo->pos].utmp->userid);
  if (*userid)
  {
    vs_bar("寄  信");
    prints("收信人：%s", userid);
    my_send(userid);
    return ulist_init(xo);
  }

  vmsg(MSG_USR_LEFT);
  return XO_FOOT;
}


static int
ulist_query(xo)
  XO *xo;
{
  move(1, 0);
  clrtobot();
  my_query(ulist_pool[xo->pos].utmp->userid, 0);
  /*ulist_neck(xo);*/
  return XO_INIT;
}


static int
ulist_broadcast(xo)
  XO *xo;
{
  int num;
  PICKUP *pp;
  UTMP *up;
  BMW bmw;
  char buf[80],ans,admin;

  num = cuser.userlevel;
  if (!(num & (PERM_SYSOP)) &&
    (!(num & PERM_PAGE) || !(cuser.ufo2 & UFO2_PAL)))
    return XO_NONE;

  num = xo->max;
  if (num < 1)
    return XO_NONE;

  bmw.caller = 0;
  bmw_edit(NULL, "★廣播：", &bmw, 0);
  sprintf(buf,"★廣播：%s",bmw.msg);
  strcpy(bmw.msg,buf);
  admin = check_admin(cuser.userid);
  if(admin && !(cuser.ufo2 & UFO2_PAL))
  {
    if((ans = vans("◎ 使用 SYSOP 廣播嗎？ [y/N]")) != 'Y' && ans != 'y')
      admin = 0;
    if((ans = vans("◎ 確定全站廣播嗎？ [y/N]")) != 'Y' && ans != 'y')
      return XO_INIT;
  }
  if (!(cuser.ufo2 & UFO2_PAL) && admin)
  {
    strcpy(bmw.userid,"SYSOP");
    /*bmw.sender = 1;*/
  }
  if (bmw.caller)
  {
    pp = ulist_pool;
    while (--num >= 0)
    {
      up = pp[num].utmp;
      if (can_message(up) && (!(up->ufo & UFO_BROADCAST)||
         (HAS_PERM(PERM_SYSOP|PERM_CHATROOM) && !(cuser.ufo2 & UFO2_PAL))))
      {
	bmw.recver = up->userno;
	bmw_send(up, &bmw);
      }
    }
  }
  return XO_INIT;
}


static int
ulist_talk(xo)
  XO *xo;
{
  if (HAS_PERM(PERM_PAGE))
  {
    UTMP *up;

    up = ulist_pool[xo->pos].utmp;
    if (can_override(up))
      return talk_page(up) ? ulist_init(xo) : XO_FOOT;
  }
  return XO_NONE;
}


static int
ulist_write(xo)
  XO *xo;
{
  if (HAS_PERM(PERM_PAGE))
  {
    UTMP *up;

    up = ulist_pool[xo->pos].utmp;
    if (can_message(up))
    {
      BMW bmw;
      char buf[20];

#ifdef	HAVE_SHOWNUMMSG
      if(up->num_msg > 9 && (up->ufo & UFO_MAXMSG) && !HAS_PERM(PERM_SYSOP))
      {
        vmsg("對方已經被水球灌爆了!!");
        return XO_INIT;
      }
#endif

      sprintf(buf, "★[%s]", up->userid);
      bmw_edit(up, buf, &bmw, 0);
    }
#ifdef	HAVE_BANMSG    
    else if(!(up->ufo & UFO_MESSAGE) && can_banmsg(up))
    {
      vmsg("對方不想聽到您的聲音!!");
    }
#endif    
    return XO_INIT;
  }
  return XO_NONE;
}


static int
ulist_edit(xo)			/* Thor: 可線上查看及修改使用者 */
  XO *xo;
{
  ACCT acct;

  if (!HAS_PERM(PERM_SYSOP) ||
    acct_load(&acct, ulist_pool[xo->pos].utmp->userid) < 0)
    return XO_NONE;

  vs_bar("使用者設定");
  acct_setup(&acct, 1);
  return ulist_head(xo);
}

/* BLACK SU */
static int
ulist_su(xo)
  XO *xo;
{
  XO *tmp;
  ACCT acct;
  char path[80];  
  int level,ufo;
  ufo = cuser.ufo;
  level = cuser.userlevel;
  if (!supervisor ||
      acct_load(&acct, ulist_pool[xo->pos].utmp->userid) < 0)
      return XO_NONE;

  memcpy(&cuser,&acct,sizeof(ACCT));
  cuser.userlevel = level;
  cuser.ufo = ufo;
  usr_fpath(path,acct.userid,".DIR");  
  tmp = xz[XZ_MBOX - XO_ZONE].xo;
  xz[XZ_MBOX - XO_ZONE].xo =  xo_new(path);
  free(tmp);
  usr_fpath(path, acct.userid, FN_BMW);
  tmp = xz[XZ_BMW - XO_ZONE].xo;
  xz[XZ_BMW - XO_ZONE].xo =  xo_new(path);
  free(tmp);
  pal_cache();  
  return ulist_init(xo);
}
/* BLACK SU */

static int
ulist_kick(xo)
  XO *xo;
{
  ACCT u;
  acct_load(&u, ulist_pool[xo->pos].utmp->userid);
  if ((HAS_PERM(PERM_SYSOP)&& (!(u.userlevel & PERM_SYSOP) || !strcmp(cuser.userid,u.userid)) )||check_admin(cuser.userid))
  {
    UTMP *up;
    pid_t pid;
    char buf[80];

    up = ulist_pool[xo->pos].utmp;
    if ((pid = up->pid))
    {
      if (vans(msg_sure_ny) != 'y' || pid != up->pid)
	return XO_FOOT;

      sprintf(buf, "%s (%s)", up->userid, up->username);

      if ((kill(pid, SIGTERM) == -1) && (errno == ESRCH))
        memset(up,0,sizeof(UTMP));

      blog("KICK ", buf);
      return ulist_init(xo);
    }
    else
    {
      if (vans(msg_sure_ny) != 'y')
        return XO_FOOT;
      memset(up,0,sizeof(UTMP));
      return ulist_init(xo);
    }
  }
  return XO_NONE;
}


#ifdef	HAVE_CHANGE_FROM
static int
ulist_fromchange(xo)
  XO *xo;
{
  char *str, buf[34];
  
  if (!HAS_PERM(PERM_ADMIN))
    return XO_NONE;
  
  strcpy(buf, str = cutmp->from);
  vget(b_lines, 0, "請輸入新的故鄉：", buf, sizeof(cutmp->from), GCARRY);
  if (strcmp(buf, str))
  {
    strcpy(str, buf);
    strcpy(cutmp->from, buf);
    return XO_INIT;
    /*ulist_body(xo);*/
  }

  return XO_FOOT;
}
#endif


static int
ulist_nickchange(xo)
  XO *xo;
{
  char *str, buf[24];

  if (!HAS_PERM(PERM_VALID) || (HAS_PERM(PERM_DENYNICK)&&!HAS_PERM(PERM_SYSOP)))
    return XO_NONE;

  strcpy(buf, str = cuser.username);
  vget(b_lines, 0, "請輸入新的暱稱：", buf, sizeof(cuser.username), GCARRY);

  if (strcmp(buf, str) && str_len(buf) > 0 )
  {
    strcpy(str, buf);
    strcpy(cutmp->username, buf);
    return XO_INIT;
    /*ulist_body(xo);*/
  }
  return XO_FOOT;
}


static int
ulist_help(xo)
  XO *xo;
{
  film_out(FILM_ULIST, -1);
  return ulist_init(xo);
}


#if 0
static int
ulist_exotic(xo)
  XO *xo;
{
  UTMP *uhead, *utail, *uceil;
  char buf[80];

  if (!HAS_PERM(PERM_SYSOP))
    return XO_NONE;

  uhead = ushm->uslot;
  uceil = ushm->uceil;
  utail = uhead + MAXACTIVE - 1;
  sprintf(buf, "%s %p [%p, %p]\n", Now(), uceil, uhead, utail);
  f_cat("run/ushm.log", buf);

  ushm->uceil = utail;
  return ulist_init(xo);
}
#endif

static int
ulist_pager(xo)
  XO *xo;
{
  if (!HAS_PERM(PERM_PAGE))
      return XO_NONE;
   if((cuser.ufo & UFO_PAGER) && (cuser.ufo & UFO_PAGER1))
   {
   cuser.ufo ^= UFO_PAGER;
   cutmp->ufo ^= UFO_PAGER;
   cuser.ufo ^= UFO_PAGER1;
   cutmp->ufo ^= UFO_PAGER1;
   }
   else if(cuser.ufo & UFO_PAGER)
   {
   cuser.ufo ^= UFO_PAGER1;
   cutmp->ufo ^= UFO_PAGER1;
   }
   else 
   {
   cuser.ufo ^= UFO_PAGER;
   cutmp->ufo ^= UFO_PAGER;
   }
   return ulist_init(xo);
}

static int
ulist_message(xo)
  XO *xo;
{

  if (!HAS_PERM(PERM_PAGE))
        return XO_NONE;
   if((cuser.ufo & UFO_QUIET) && (cuser.ufo & UFO_MESSAGE))
   {
   cuser.ufo ^= UFO_QUIET;
   cutmp->ufo ^= UFO_QUIET;
   cuser.ufo ^= UFO_MESSAGE;
   cutmp->ufo ^= UFO_MESSAGE;
   }
   else if(cuser.ufo & UFO_QUIET)
   {
   cuser.ufo ^= UFO_MESSAGE;
   cutmp->ufo ^= UFO_MESSAGE;
   }
   else 
   {
   cuser.ufo ^= UFO_QUIET;
   cutmp->ufo ^= UFO_QUIET;
   }
   return ulist_init(xo);
}

static int
ulist_recall(xo)
  XO *xo;
{
  t_recall();
  return ulist_init(xo);
}

static int
ulist_realname(xo)
  XO *xo;
{
  if(HAS_PERM(PERM_SYSOP))
  {
     cuser.ufo2 ^= UFO2_REALNAME;
//     cutmp->ufo ^= UFO_REALNAME;
  } 
  return ulist_init(xo);
}

static int
ulist_ship(xo)
  XO *xo;
{
  cuser.ufo2 ^= UFO2_SHIP;
//  cutmp->ufo ^= UFO_SHIP;
  return ulist_init(xo);
}

static int
ulist_mp(xo)
  XO *xo;
{
  int tmp;
  
  if(!HAS_PERM(PERM_VALID))
    return XO_NONE;
  tmp = t_pal();
  return ulist_init(xo);
}

static int
ulist_readmail(xo)
  XO *xo;
{
  if(cuser.userlevel)
  {
    if (HAS_PERM(PERM_DENYMAIL))
      vmsg("您的信箱被鎖了！");
    else 
      xover(XZ_MBOX);
    return ulist_init(xo);
  }
  else
    return XO_NONE;
}

static int
ulist_del(xo)
  XO *xo;
{
  UTMP *up;
  char ans;
  int userno;
  int  fd,tmp=0;
  char fpath[64];
  PAL *pal;

  ans = vans("是否刪除(y/N)：");
  if (ans == 'y' || ans == 'Y')
  { 
    up = ulist_pool[xo->pos].utmp;
    userno = up->userno;

    usr_fpath(fpath, cuser.userid, FN_PAL);
    if ((fd = open(fpath, O_RDONLY)) >= 0)
    {
      mgets(-1);
      while ((pal = mread(fd, sizeof(PAL))))
      {
          if(pal->userno!=userno)
            tmp = tmp + 1;
          else
          { 
            rec_del(fpath,sizeof(PAL),tmp,NULL,NULL);
            break;
          }
      }
      close(fd);
    }
    pal_cache();
    return ulist_init(xo);
  }
  else
    return XO_FOOT;
}

static int
ulist_changeship(xo)
  XO *xo;
{
  UTMP *up;
  int userno;
  int  fd,tmp=0;
  char fpath[64],buf[46];
  PAL *pal;
  int check;

  up = ulist_pool[xo->pos].utmp;
  userno = up->userno;


  check = is_pal(userno) ? 1 : is_bad(userno) ? 2 : 0;
  if(!check)
      return XO_NONE;
  strcpy(buf,"");
  copyship(buf,userno);

  if(vget(b_lines, 0, check == 1 ?"友誼：":"惡行惡狀：", buf, sizeof(buf), GCARRY))
  {
    usr_fpath(fpath, cuser.userid, FN_PAL);
    if ((fd = open(fpath, O_RDONLY)) >= 0)
    {
      mgets(-1);
      while ((pal = mread(fd, sizeof(PAL))))
      {
          if(pal->userno!=userno)
            tmp = tmp + 1;
          else
          {
            strcpy(pal->ship,buf);
            rec_put(fpath, pal, sizeof(PAL), tmp);
            break;
          }
      }
      close(fd);
    }
    pal_cache();
    return ulist_init(xo);
  }
  else
    return XO_HEAD;
}

#if 1
static int
ulist_test(xo)
  XO *xo;
{
  int fd;
  char buf[128];
  fd = open(FN_ETC_SYSOP, O_RDONLY);
  sprintf(buf,"檔案編號 %d:%d:%d",fd,Ctrl('F'),'@');
  if(fd)
    close(fd);
  pmsg(buf);
/*  vget(b_lines,0,"多少人:",buf,6,DOECHO);
  ushm->count = atoi(buf);*/
  return XO_INIT;
}

static int
ulist_state(xo)
  XO *xo;
{
  char buf[128];
  if(!HAS_PERM(PERM_SYSOP))
    return XO_NONE;
  sprintf(buf,"PID : %d",ulist_pool[xo->pos].utmp->pid);
  vmsg(buf);
  return XO_INIT;
}
#endif

#ifdef	APRIL_FIRST
static int
ulist_april1(xo)
  XO *xo;
{
  char buf[256];
  more("gem/brd/Admin/J/A106LL7J",NULL);
  sprintf(buf,"您是第 %d 個被騙的使用者 ^^y ",ushm->avgload);
  if(!aprilfirst)
    ushm->avgload++;
  vmsg(buf);
  aprilfirst = 1;
  return XO_INIT;
}
#endif

KeyFunc ulist_cb[] =
{
  {XO_INIT, ulist_init},
  {XO_LOAD, ulist_body},
  {XO_NONE, ulist_init},
#if 1
  {'V', ulist_test},
  {'S', ulist_state},
#endif
  {'y', ulist_readmail},
/* BLACK SU */
  {'u', ulist_su},
/* BLACK SU */
  {'m', ulist_message},
  {'Z', ulist_ship},
  {'f', ulist_pal},
  {'a', ulist_makepal},
  {'A', ulist_makebad},
  {'t', ulist_talk},
  {'w', ulist_write},
  {'l', ulist_recall},		/* Thor: 熱訊回顧 */
  {'j', ulist_changeship},
  {'q', ulist_query},
  {'b', ulist_broadcast},
  {'s', ulist_init},		/* refresh status Thor: 應user要求 */
  {'c', t_cloak},
  {'R', ulist_realname},
  {'o', ulist_mp},
  {'d', ulist_del},
  {'p', ulist_pager},
  {Ctrl('Q'),ulist_query},
#if 0
  {'x', ulist_exotic},
#endif

  {Ctrl('K'), ulist_kick},
  {Ctrl('X'), ulist_edit},
  {'g', ulist_nickchange},
#ifdef HAVE_CHANGE_FROM
  {Ctrl('F'), ulist_fromchange},
#endif
  
#if 0
  {'/', ulist_search},
#endif
  /* Thor.990125: 可前後搜尋, id or nickname */
  {'/', ulist_search_forward},
  {'?', ulist_search_backward},

#ifdef  APRIL_FIRST
  {'X', ulist_april1},
#endif

  {'M', ulist_mail},
  {KEY_TAB, ulist_toggle},
  {'h', ulist_help}
};


/* ----------------------------------------------------- */
/* talk main-routines					 */
/* ----------------------------------------------------- */

int
t_message()
{
   if((cuser.ufo & UFO_QUIET) && (cuser.ufo & UFO_MESSAGE))
   {
   cuser.ufo ^= UFO_QUIET;
   cutmp->ufo ^= UFO_QUIET;
   cuser.ufo ^= UFO_MESSAGE;
   cutmp->ufo ^= UFO_MESSAGE;
   }
   else if(cuser.ufo & UFO_QUIET)
   {
   cuser.ufo ^= UFO_MESSAGE;
   cutmp->ufo ^= UFO_MESSAGE;
   }
   else 
   {
   cuser.ufo ^= UFO_QUIET;
   cutmp->ufo ^= UFO_QUIET;
   }
  return 0;
}


int
t_pager()
{
  /* cuser.ufo = (cutmp->ufo ^= UFO_PAGER); */
   if((cuser.ufo & UFO_PAGER) && (cuser.ufo & UFO_PAGER1))
   {
   cuser.ufo ^= UFO_PAGER;
   cutmp->ufo ^= UFO_PAGER;
   cuser.ufo ^= UFO_PAGER1;
   cutmp->ufo ^= UFO_PAGER1;
   }
   else if(cuser.ufo & UFO_PAGER)
   {
   cuser.ufo ^= UFO_PAGER1;
   cutmp->ufo ^= UFO_PAGER1;
   }
   else 
   {
   cuser.ufo ^= UFO_PAGER;
   cutmp->ufo ^= UFO_PAGER;
   }
  return 0;
}

int
t_cloak(xo)
  XO *xo;
{
  if(HAS_PERM(PERM_CLOAK))
  {
    cuser.ufo ^= UFO_CLOAK;
    cutmp->ufo ^= UFO_CLOAK;
  } /* Thor.980805: 解決 ufo不同步問題 */
  return ulist_init(xo);
}




int
t_query()
{
  ACCT acct;

  vs_bar("查詢網友");
  if (acct_get(msg_uid, &acct) > 0)
  {
    move(2, 0);
    clrtobot();
    do_query(&acct, 0);
  }

  return 0;
}


#if 0
static int
talk_choose()
{
  UTMP *up, *ubase, *uceil;
  int self, seecloak;
  char userid[IDLEN + 1];

  ll_new();

  self = cuser.userno;
  seecloak = HAS_PERM(PERM_SEECLOAK);
  ubase = up = ushm->uslot;
  uceil = ushm->uceil;
  do
  {
    if (up->pid && up->userno != self &&
      ((seecloak || !(up->ufo & UFO_CLOAK))&&(can_see(up)!=2 || HAS_PERM(PERM_SYSOP))))
      AddLinkList(up->userid);
  } while (++up <= uceil);

  if (!vget(1, 0, "請輸入代號：", userid, IDLEN + 1, GET_LIST))
    return 0;

  up = ubase;
  do
  {
    if (!str_cmp(userid, up->userid))
      return up->userno;
  } while (++up <= uceil);

  return 0;
}


int
t_talk()
{
  int tuid, unum, ucount;
  UTMP *up;
  char ans[4];
  BMW bmw;

  if (ushm->count <= 1)
  {
    outs("目前線上只有您一人，快邀請朋友來光臨【" BOARDNAME "】吧！");
    return XEASY;
  }

  tuid = talk_choose();
  if (!tuid)
    return 0;

  /* ----------------- */
  /* multi-login check */
  /* ----------------- */

  move(3, 0);
  unum = 1;
  while ((ucount = utmp_count(tuid, 0)) > 1)
  {
    outs("(0) 不想 talk 了...\n");
    utmp_count(tuid, 1);
    vget(1, 33, "請選擇一個聊天對象 [0]：", ans, 3, DOECHO);
    unum = atoi(ans);
    if (unum == 0)
      return 0;
    move(3, 0);
    clrtobot();
    if (unum > 0 && unum <= ucount)
      break;
  }

  if (up = utmp_search(tuid, unum))
  {
    if (can_override(up))
    {
      ucount = vget(3, 0, "確定要聊天嗎？ T)alk W)rite Q)uit [T] ",
	ans, 3, LCECHO);

      if (ucount == 'q')
	return 0;

      if (tuid != up->userno)
      {
	vmsg(MSG_USR_LEFT);
	return 0;
      }

      if (ucount == 'w')
      {
	bmw_edit(up, "★熱訊：", &bmw, 0);
      }
      else
      {
	talk_page(up);
      }
    }
    else
    {
      vmsg("對方關掉呼叫器了");
    }
  }

  return 0;
}
#endif


/* ------------------------------------- */
/* 有人來串門子了，回應呼叫器		 */
/* ------------------------------------- */


void
talk_rqst()
{
  UTMP *up;
  int mode, sock, ans, len, port;
  char buf[80];
  struct sockaddr_in sin;
  screenline sl[b_lines + 1];
#if     defined(__OpenBSD__)
  struct hostent *h;
#endif

  up = cutmp->talker;
  if (!up)
    return;
  up->talker = cutmp;

  port = up->sockport;
  if (!port)
    return;

  mode = bbsmode;
#ifdef  HAVE_PIP_FIGHT
  if(up->mode == M_CHICKEN)
    utmp_mode(M_CHICKEN);
  else
#endif
    utmp_mode(M_TRQST);

  vs_save(sl);

  clear();
  sprintf(page_requestor, "%s (%s)", up->userid, up->username);

#ifdef EVERY_Z
  /* Thor.0725: 為 talk & chat 可用 ^z 作準備 */

  if (holdon_fd)
  {
    sprintf(buf, "%s 想和您聊，不過您只有一張嘴", page_requestor);
    vmsg(buf);
    buf[0] = ans = '6';		/* Thor.0725:只有一張嘴 */
    len = 1;
    goto over_for;
  }
#endif

  bell();
#ifdef  HAVE_PIP_FIGHT
  if(up->mode != M_CHICKEN)
#endif
  {
    prints("您想跟 %s 聊天嗎？(來自 %s)", page_requestor, up->from);
    for (;;)
    {
      ans = vget(1, 0, "==> Yes, No, Query？[Y] ", buf, 3, LCECHO);
      if (ans == 'q')
      {
        my_query(up->userid, 0);
      }
      else
        break;
    }
  }
#ifdef  HAVE_PIP_FIGHT
  else
  {
    prints("您想跟 %s PK 小雞嗎？(來自 %s)", page_requestor, up->from);
    ans = vget(1, 0, "==> Yes, No？[Y] ", buf, 3, LCECHO);
  }
#endif

  len = 1;

  if (ans == 'n')
  {
    move(2, 0);
    clrtobot();
#ifdef  HAVE_PIP_FIGHT
    if(up->mode != M_CHICKEN)
#endif
    {
      for (ans = 0; ans < 5; ans++)
        prints("\n (%d) %s", ans + 1, talk_reason[ans]);
      ans = vget(10, 0, "請輸入選項或其他情由 [1]：\n==> ",
        buf + 1, sizeof(buf) - 1, DOECHO);

      if (ans == 0)
        ans = '1';

      if (ans >= '1' && ans <= '5')
      {
        buf[0] = ans;
      }
      else
      {
        buf[0] = ans = ' ';
        len = strlen(buf);
      }
    }
#ifdef  HAVE_PIP_FIGHT
    else
      buf[0] = ans = 'C';
#endif
  }
  else
  {
#ifdef  HAVE_PIP_FIGHT
    if(up->mode != M_CHICKEN)
#endif
      buf[0] = ans = 'y';
#ifdef  HAVE_PIP_FIGHT
    else
      buf[0] = ans = 'c';
#endif
  }

#ifdef EVERY_Z
over_for:
#endif

  sock = socket(AF_INET, SOCK_STREAM, 0);

#if     defined(__OpenBSD__)                    /* lkchu */

  if (!(h = gethostbyname(MYHOSTNAME)))
    return;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = port;
  memcpy(&sin.sin_addr, h->h_addr, h->h_length);
                
#else

  sin.sin_family = AF_INET;
  sin.sin_port = port;
  sin.sin_addr.s_addr = INADDR_ANY /* INADDR_LOOPBACK */;
  memset(sin.sin_zero, 0, sizeof(sin.sin_zero));

#endif

  if (!connect(sock, (struct sockaddr *) & sin, sizeof(sin)))
  {
    send(sock, buf, len, 0);

    if (ans == 'y')
    {
      strcpy(cutmp->mateid, up->userid);
      talk_speak(sock);
    }
#ifdef  HAVE_PIP_FIGHT
    else if(ans == 'c')
    {
      if(!p)
        p = DL_get("bin/pip.so:pip_vf_fight");
      strcpy(cutmp->mateid, up->userid);
      if(p)
	  {
	    cutmp->pip = NULL;
        (*p)(sock,1);
	    cutmp->pip = NULL;

	  }
      add_io(0,60);
/*      pip_vf_fight(sock,2);*/
    }
#endif
  }

  close(sock);
#ifdef  LOG_TALK
  if (ans == 'y' && up->mode != M_CHICKEN) /* mat.991011: 防止Talk拒絕時，產生聊天記錄的record */
    talk_save();          /* lkchu.981201: talk 記錄處理 */
#endif
  vs_restore(sl);
  utmp_mode(mode);
}


void
talk_main()
{
  char fpath[64];
  
  xz[XZ_ULIST - XO_ZONE].xo = &ulist_xo;
  xz[XZ_ULIST - XO_ZONE].cb = ulist_cb;

  xz[XZ_PAL - XO_ZONE].cb = pal_cb;
  
  /* lkchu.981230: 利用 xover 整合 bmw */
  usr_fpath(fpath, cuser.userid, FN_BMW);
  xz[XZ_BMW - XO_ZONE].xo = xo_new(fpath);
  xz[XZ_BMW - XO_ZONE].cb = bmw_cb;
}

int
check_personal_note(newflag,userid)
  int newflag;
  char *userid;
{
 char fpath[256];
 int  fd,total = 0;
 notedata myitem;
 char *fn_note_dat      = "pnote.dat";

 if (userid == NULL)
   usr_fpath(fpath, cuser.userid, fn_note_dat);
 else
   usr_fpath(fpath, userid, fn_note_dat);

 if ((fd = open(fpath, O_RDONLY)) >=0)
 {
   while (read(fd,&myitem, sizeof(myitem)) == sizeof(myitem))
   {
     if (newflag)
     {
       if (myitem.mode == 0) total++;
     }
     else
       total++;
   }
   close(fd);
   return total;
 }
 return 0;
}

#ifdef	HAVE_BANMSG
void
banmsg_cache()
{
  int count, fsize, fd;
  int *plist, *cache;
  BANMSG *phead, *ptail;
  char *fimage, fpath[64];
  UTMP *up;

  up = cutmp;
  cutmp->userno = cuser.userno;

  cache = NULL;
  count = 0;

  fsize = 0;
  usr_fpath(fpath, cuser.userid, FN_BANMSG);
  fimage = f_img(fpath, &fsize);
  if((fsize > (BANMSG_MAX * sizeof(BANMSG))) && (fd = open(fpath,O_RDWR)))
  {
    ftruncate(fd, BANMSG_MAX * sizeof(BANMSG));
    close(fd);
  }
  if (fimage != NULL)
  {
    if (fsize > (BANMSG_MAX * sizeof(BANMSG)))
    {
      fsize = BANMSG_MAX * sizeof(BANMSG);
    }

    count = fsize / sizeof(BANMSG);
    if (count)
    {
      cache = plist = up->banmsg_spool;
      phead = (BANMSG *) fimage;
      ptail = (BANMSG *) (fimage + fsize);
      do
      {
	*plist++ = phead->userno;
      } while (++phead < ptail);

      if (count > 0)
      {
	if (count > 1)
	  xsort(cache, count, sizeof(int), int_cmp);
      }
    }
  }

  up->banmsg_max = count;
  
  if(fimage) 
    free(fimage);
}


void
banmsg_sync(fpath)
  char *fpath;
{
  int fd, size=0;
  struct stat st;
  char buf[64];

  if (!fpath)
  {
    fpath = buf;
    usr_fpath(fpath, cuser.userid, FN_BANMSG);
  }

  if ((fd = open(fpath, O_RDWR, 0600)) < 0)
    return;

  outz("★ 資料整理稽核中，請稍候 \033[5m...\033[m");
  refresh();

  if (!fstat(fd, &st) && (size = st.st_size) > 0)
  {
    BANMSG *pbase, *phead, *ptail;
    int userno;

    pbase = phead = (BANMSG *) malloc(size);
    size = read(fd, pbase, size);
    if (size >= sizeof(BANMSG))
    {
      ptail = (BANMSG *) ((char *) pbase + size);
      while (phead < ptail)
      {
	userno = phead->userno;
	if (userno > 0 && userno == acct_userno(phead->userid))
	{
	  phead++;
	  continue;
	}

	ptail--;
	if (phead >= ptail)
	  break;
	memcpy(phead, ptail, sizeof(BANMSG));
      }

      size = (char *) ptail - (char *) pbase;
      if (size > 0)
      {
	if (size > sizeof(BANMSG))
	{
	  xsort(pbase, size / sizeof(BANMSG), sizeof(BANMSG), (void *)str_cmp);
	}

	lseek(fd, 0, SEEK_SET);
	write(fd, pbase, size);
	ftruncate(fd, size);
      }
    }
    free(pbase);
  }
  close(fd);

  if (size <= 0)
    unlink(fpath);
}


/* ----------------------------------------------------- */
/* 拒收訊息名單：選單式操作界面描述		 	 */
/* ----------------------------------------------------- */


static int banmsg_add();


static void
banmsg_item(num, banmsg)
  int num;
  BANMSG *banmsg;
{
  prints("%6d    %-14s%s\n", num, banmsg->userid, banmsg->ship);
}


static int
banmsg_body(xo)
  XO *xo;
{
  BANMSG *banmsg;
  int num, max, tail;

  move(3, 0);
  clrtobot();
  max = xo->max;
  if (max <= 0)
  {
    if (vans("要新增嗎(Y/N)？[N] ") == 'y')
      return banmsg_add(xo);
    return XO_QUIT;
  }

  banmsg = (BANMSG *) xo_pool;
  num = xo->top;
  tail = num + XO_TALL;
  if (max > tail)
    max = tail;

  do
  {
    banmsg_item(++num, banmsg++);
  } while (num < max);

  return XO_NONE;
}


static int
banmsg_head(xo)
  XO *xo;
{
  vs_head("拒收名單", str_site);
  outs("\
  [←]離開 a)新增 c)修改 d)刪除 m)寄信 s)整理 [q]查詢 [h]elp\n\
\033[30;47m  編號    代 號         描       述                                           \033[m");
  return banmsg_body(xo);
}


static int
banmsg_load(xo)
  XO *xo;
{
  xo_load(xo, sizeof(BANMSG));
  return banmsg_body(xo);
}


static int
banmsg_init(xo)
  XO *xo;
{
  xo_load(xo, sizeof(BANMSG));
  return banmsg_head(xo);
}


static void
banmsg_edit(banmsg, echo)
  BANMSG *banmsg;
  int echo;
{
  if (echo == DOECHO)
    memset(banmsg, 0, sizeof(BANMSG));
  vget(b_lines, 0, "描述：", banmsg->ship, sizeof(banmsg->ship), echo);
}


static int
banmsg_add(xo)
  XO *xo;
{
  ACCT acct;
  int userno;

  if (xo->max >= BANMSG_MAX)
  {
    vmsg("您的拒收訊息名單太多，請善加整理");
    return XO_FOOT;
  }

  userno = acct_get(msg_uid, &acct);

#if 1				/* Thor.0709: 不重覆加入 */
  if ((xo->dir[0] == 'u') && is_banmsg(userno))
  {
    vmsg("名單中已有此人");
    return XO_FOOT;
  }
  else if (userno == cuser.userno)
  {
    vmsg("自己不須加入拒收訊息名單中");
    return XO_FOOT;
  }
#endif

  if (userno > 0)
  {
    BANMSG banmsg;

    banmsg_edit(&banmsg, DOECHO);
    strcpy(banmsg.userid, acct.userid);
    banmsg.userno = userno;
    rec_add(xo->dir, &banmsg, sizeof(BANMSG));
    xo->pos = XO_TAIL;
    xo_load(xo, sizeof(BANMSG));
  }

  banmsg_cache();

  return banmsg_head(xo);
}


static int
banmsg_delete(xo)
  XO *xo;
{
  if (vans(msg_del_ny) == 'y')
  {

    if (!rec_del(xo->dir, sizeof(BANMSG), xo->pos, NULL, NULL))
    {

      banmsg_cache();
      return banmsg_load(xo);
    }
  }
  return XO_FOOT;
}


static int
banmsg_change(xo)
  XO *xo;
{
  BANMSG *banmsg, mate;
  int pos, cur;
  
  pos = xo->pos;
  cur = pos - xo->top;
  banmsg = (BANMSG *) xo_pool + cur;

  mate = *banmsg;
  banmsg_edit(banmsg, GCARRY);
  if (memcmp(banmsg, &mate, sizeof(BANMSG)))
  {
    rec_put(xo->dir, banmsg, sizeof(BANMSG), pos);
    move(3 + cur, 0);
    banmsg_item(++pos, banmsg);
  }

  return XO_FOOT;
}


static int
banmsg_mail(xo)
  XO *xo;
{
  BANMSG *banmsg;
  char *userid;

  banmsg = (BANMSG *) xo_pool + (xo->pos - xo->top);
  userid = banmsg->userid;
  if (*userid)
  {
    vs_bar("寄  信");
    prints("收信人：%s", userid);
    my_send(userid);
  }
  return banmsg_head(xo);
}


static int
banmsg_sort(xo)
  XO *xo;
{
  banmsg_sync(xo->dir);
  return banmsg_load(xo);
}


static int
banmsg_query(xo)
  XO *xo;
{
  BANMSG *banmsg;

  banmsg = (BANMSG *) xo_pool + (xo->pos - xo->top);
  move(1, 0);
  clrtobot();
  my_query(banmsg->userid, 1);
  return banmsg_head(xo);
}


static int
banmsg_help(xo)
  XO *xo;
{
//  film_out(FILM_BANMSG, -1);
  return banmsg_head(xo);
}


KeyFunc banmsg_cb[] =
{
  {XO_INIT, banmsg_init},
  {XO_LOAD, banmsg_load},
  {XO_HEAD, banmsg_head},
  {XO_BODY, banmsg_body},

  {'a', banmsg_add},
  {'c', banmsg_change},
  {'d', banmsg_delete},
  {'m', banmsg_mail},
  {'q', banmsg_query},
  {'s', banmsg_sort},
  {'h', banmsg_help}
};


int
t_banmsg()
{
  XO *xo;
  char fpath[64];

  usr_fpath(fpath, cuser.userid, FN_BANMSG);
  xz[XZ_OTHER - XO_ZONE].xo = xo = xo_new(fpath);
  xz[XZ_OTHER - XO_ZONE].cb = banmsg_cb;
  xover(XZ_OTHER);
  banmsg_cache();
  free(xo);

  return 0;
}
#endif
