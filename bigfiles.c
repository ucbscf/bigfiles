/* bigfiles - find big files in a directory
 *
 * Author: Phil Spector (spector@stat.Berkeley.EDU)
 *	    
 * Last update: Ofer Licht (ofer@stat.Berkeley.EDU)
 *
 * Usage: bigfiles [ -nNUMBER ] [ -dDAYS ] [ -q ] [ -v ] [ -x ] [ directory ]
 *
 *	    -nNUMBER	print the names of the largest NUMBER files.
 *	    -dDAYS	mark with asterisk files older than DAYS days.
 *			default is 30.
 *	    -u		count only files owned by user running program
 *	    -uLOGIN	count only files owned by user named LOGIN
 *	    -uUID	count only files owned by user with id UID
 *	    -q		display quota information too.
 *	    -v		verbose: show directory names during recursion
 *	    -x		restricts search to file system of named directory
 */

#ifndef lint
static char sccs_id[] = "@(#)bigfiles.c	1.8	(SCF)	01/12/98";
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>

#ifdef SVR4
# include <sys/fs/ufs_quota.h>
#else
# include <ufs/quota.h>
#endif

#if defined(__STDC__) && defined(SVR4)
    void chkit(int);
#else
    void chkit();
#endif

#define SECPERDAY ((long)(60*60*24))

#define DEF_NFILES  15
#define DEF_DAYS    30


struct FINFO {
    struct FINFO    *next;
    char	    *name;
    off_t	    size;
    time_t	    atime;
} ;

struct stat	statbuf;
dev_t		thisfs;	/* mount point device */
struct FINFO	*head,*tail,*srch;
long		count,nfiles,fseen,fsize;
char		dotdir[]=".";
char		serr[256];

#define myperror(msg)	sprintf(serr,"%s: %s",prog,(msg)),perror(serr)

int	    srchdir();
char	    *cleanname();
static char *prog;
char	    *home;
int	    lhome;
int	    vflag=0;
int	    uflag=0;
int	    xflag=0;
uid_t	    checkuid;
 
#ifdef __STDC__
main(int argc, char **argv)
#else
main(argc,argv)
    int argc;
    char **argv;
#endif
{
    char    *date,*dirn,*fp;
    int	    fd;
    long    i,l,days,qfound,nslash,qflag=0,qq;
    time_t  nowtime;
    uid_t   myuid = geteuid();
    char    fullpath[MAXPATHLEN],tmplate[MAXPATHLEN],qfile[MAXPATHLEN];

    nfiles = DEF_NFILES;
    days = DEF_DAYS;
    fseen = fsize = 0;
    dirn = NULL;

    /* get basename */
    if((prog=strrchr(argv[0],'/')) == NULL) prog=argv[0];
    else prog++;

    for(i=1; i < argc; i++) {
	if(argv[i][0] != '-') {
	    if(dirn)fprintf(stderr,"%s: only one directory expected; %s ignored\n",
			    prog,dirn);
	    dirn = argv[i];
	    continue;
	}
	    
	switch(argv[i][1]){
	    case 'n':
		if(argv[i][2]) nfiles = atoi(argv[i] + 2);
		else if(i == argc - 1) nfiles = DEF_NFILES;
		else nfiles = atoi(argv[++i]);

		if(nfiles < 1) {
		    fprintf(stderr,"%s: Value for -n should be positive. Option ignored.\n", prog);
		    nfiles = DEF_NFILES;
		}
		break;
	    case 'd':
		if(argv[i][2]) days = atoi(argv[i] + 2);
		else if(i == argc - 1) days = DEF_DAYS;
		else days = atoi(argv[++i]);

		if(days < 1) {
		    fprintf(stderr,"%s: Value for -d should be positive. Option ignored.\n", prog);
		    days = DEF_DAYS;
		}
		break;
	    case 'q':
		qflag = 1;
		uflag = 1;
		checkuid = myuid;
		break;
	    case 'u':
		if(qflag) fprintf(stderr,"%s: -q flag already in effect; Option -u ignored.\n",prog);
		else {
		    uflag = 1;
		    if (argv[i][2]) {
			if(isdigit(argv[i][2])) checkuid = atoi(argv[i] + 2);
			else {
			    struct passwd *pwtemp = getpwnam(argv[i] + 2);
			    if (pwtemp) checkuid = pwtemp->pw_uid;
			    else {
				fprintf(stderr,"%s: user \"%s\" unknown.\n",prog,argv[i] + 2);
				exit(1);
			    }
			}
		    } else { /* argv[i][2] == 0 */
			checkuid = myuid;
		    }
		}
		break;
	    case 'v':
		vflag = 1;
		break;
	    case 'x':
		xflag = 1;
		break;
	    default:
		fprintf(stderr,"%s: Unrecognized option \"%s\" ignored.\n",prog,argv[i]);
		break;
		
	} /* switch */
    } /* for */
		
    if(!(home=getenv("HOME"))) {
	fprintf(stderr,"%s: couldn't find home directory in environment.\n",prog);
	exit(1);
    }

    /* use home directory if the qflag is specified */
    if(qflag) {

	if(dirn) {
		fprintf(stderr,"%s: When using the -q flag, searches are made\n",
			prog);
		fprintf(stderr,"\tstarting from your home directory.  %s ignored.\n",dirn);
	}

	realpath(home,fullpath);
	dirn = fp = home = fullpath;

    } /* if qflag */

    else if(!dirn) dirn = dotdir;

    lhome = (int)strlen(home);

    head = tail = NULL;

    if (stat(dirn,&statbuf) < 0) {
	myperror(dirn);
	exit(1);
    }

    if(!(S_ISDIR(statbuf.st_mode))) {
	fprintf(stderr,"%s: %s is not a directory.\n",prog,dirn);
	exit(1);
    }
    if(xflag) {
	thisfs=statbuf.st_dev;
    }

#if 0

/* This part doesn't work correctly, yet. */

# if defined(SVR4) && defined(SA_RESTART)
    /* make interrupted system calls restart automatically */
  {
    struct sigaction act;
    sigset_t nset;
    
    (void) sigemptyset(&nset);
    if (sigaddset(&nset, SIGINT) < 0) {
	myperror("sigaddset");
	exit(1);
    }
    act.sa_handler = chkit;
    act.sa_flags |= SA_RESTART;
    (void) sigemptyset(&act.sa_mask);
    if (sigaction(SIGINT, &act, (struct sigaction*)NULL) < 0) {
	myperror("sigaction");
	exit(1);
    }
    if (sigprocmask(SIG_UNBLOCK, &nset, (sigset_t *)NULL) < 0) {
	myperror("sigprocmask");
	exit(1);
    }
  }
# endif /* SVR4 && SA_RESTART */

#endif /* 0 */


#ifdef SVR4
    sigset(SIGINT,chkit);
#else
    signal(SIGINT,chkit);
#endif

    if (srchdir(dirn) < 0) exit(1);

#ifdef SVR4
    sigset(SIGINT,SIG_DFL);
#else
    signal(SIGINT,SIG_DFL);
#endif

    nowtime = time((time_t*)NULL);
    days *= SECPERDAY;

    printf("%ld files processed: total size = %ld\n", fseen,fsize);

    /* use fullpath to find quotas which are in effect for the user */
    if(qflag) {
	qfound = 0;
	if(!strncmp(fp,"/tmp_mnt/",9)) fp += 8;
	l = strlen(fp);
	for(nslash = i = 0; i < l; i++)
	    if(fp[i] == '/')nslash++;
	strcpy(tmplate,fp);
	while(!qfound && nslash) {
		for(i = l - 1; i > 0; i--){
			if(tmplate[i] == '/') {
				tmplate[i] = 0;
				nslash--;
				break;
			}
		}
		
		sprintf(qfile,"%s/quotas",tmplate);
		if(!access(qfile,R_OK)) qfound = 1;
	}

	if(qfound) {
		if((fd = open(qfile,O_RDONLY)) < 0) {
			myperror(qfile);
			fprintf(stderr,"%s: can't open quota file %s\n",prog,qfile);
			qq = -1;
		}
		else {
			static struct dqblk in = {~((u_long)0),0L,0L,0L,0L,0L,0L,0L};
			lseek(fd,myuid * sizeof(struct dqblk),SEEK_SET);
			read(fd,&in,sizeof(struct dqblk));
			if(in.dqb_bhardlimit == ~((u_long)0))
			    printf("%s: Error reading quota\n",prog);
			qq = in.dqb_bsoftlimit * 512;
			close(fd);
		}
		if(qq >= 0) {
			if(qq == 0)
			    printf("No quota in effect on %s\n",tmplate);
			else
			    printf("Quota on %s is %ld\nCurrent usage represents %5.1lf%%.\n",
				   tmplate,qq,((double)fsize / (double)qq) * 100.);

		} /* qq >= 0 */
	} /* qfound */
	else
	    fprintf(stderr,"%s: Can't establish quota for directory %s\n",prog,fp);

    } /* qflag */


    srch = head;
    while(srch != NULL) {
	date = asctime(localtime(&srch->atime));

	/* skip over name of day */
	for(i = 0; date[i] != 0; i++)
	    if(date[i] == ' ') break;
	date += i;

	/* chop off trailing newline */
 	for(i = 0; date[i] != 0; i++)
	    if(date[i] == '\n') date[i] = 0;

	printf("%8ld  %s %c %s\n",srch->size,date,
          	  nowtime - srch->atime > days ? '*' : ' ' ,
		  myuid ? cleanname(srch->name) : srch->name);
	srch = srch->next;
    }

}

#ifdef __STDC__
struct FINFO *allocf(char *pname, struct stat *statbuf, struct FINFO *next)
#else
struct FINFO *allocf(pname,statbuf,next)
    char *pname;
    struct stat *statbuf;
    struct FINFO *next;
#endif
{
    struct FINFO *fnow;

    fnow = (struct FINFO *)calloc((unsigned)1, (unsigned)sizeof(struct FINFO));
    if(fnow == (struct FINFO *)NULL) {
	fprintf(stderr,"%s: internal error in allocation.  Exiting.\n",prog);
	exit(1);
    }
    
    fnow->name = malloc((unsigned)strlen(pname) + 1);
    strcpy(fnow->name,pname);
    fnow->size = statbuf->st_size;
    fnow->atime = statbuf->st_atime;
    fnow->next = next;

    return(fnow);
}

#ifdef SVR4
# ifdef __STDC__
void chkit(int sig)
# else
void chkit(sig)
int	sig;    /* unused */
# endif /* __STDC__ */
#else
void chkit()
#endif
{
    struct FINFO    *use;
    char	    answer[2];

    printf("\tso far:\n");
    use = head;
    for(use = head; use != NULL; use = use->next)
	printf("%8ld  %s\n",use->size,use->name);
    
    /* get rid of anything else the user typed last time */
#if defined(SYSV) || defined(SVR4)
    /* sysv stdio can flush input streams */
    fflush(stdin);
#else
    /* non-sysv stdio doesn't know how to flush input streams; so cheat */
    if (stdin->_cnt) {
	lseek(stdin->_file,-(stdin->_cnt), SEEK_CUR);
	stdin->_cnt = 0;
	stdin->_ptr = stdin->_base;
    }
#endif
    printf("continue? ");
    if(fgets(answer,2,stdin)) {
	switch(*answer) {
	    case 'v': vflag ^= 1; /* toggle verbose flag */
	    case 'y':
	    case 'Y': return;
	}
    }
    exit(1);
}
		
#ifdef __STDC__
void replacef(char *pname, struct stat *statbuf, struct FINFO *fnow)
#else
void replacef(pname,statbuf,fnow)
    char	    *pname;
    struct stat	    *statbuf;
    struct FINFO    *fnow;
#endif
{
    if((int)strlen(pname) > (int)strlen(fnow->name)) {
	free(fnow->name);
	fnow->name = malloc((unsigned)strlen(pname) + 1);
    }
    strcpy(fnow->name,pname);
    fnow->size = statbuf->st_size;
    fnow->atime = statbuf->st_atime;
}

			
#ifdef __STDC__
void freef(struct FINFO *fnow)
#else
void freef(fnow)
    struct FINFO    *fnow;
#endif
{
    free(fnow->name);
    free(fnow);
}


#ifdef __STDC__
int srchdir(char *dirn)
#else
int srchdir(dirn)
    char    *dirn;
#endif
{
    DIR		    *dirp;
    char	    pname[MAXPATHLEN];
    struct FINFO    *fnow;
    struct FINFO    *allocf();
    void	    replacef(),freef();
    struct dirent   *dp;

    if(vflag) puts(dirn);

  getdir: 
    if((dirp = opendir(dirn)) == (DIR *)NULL){
#ifdef EINTR
	if(errno==EINTR) goto getdir;
#endif
	myperror(dirn);
	return -1;
    }

    while(1) {
      getentry:
	if((dp = readdir(dirp)) == (struct dirent *)NULL) {
#ifdef EINTR
	    if(errno==EINTR) goto getentry;
	    else
#endif
		break;
	}

	if(dp->d_name && dp->d_name[0] == '.' &&
	(dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		continue;
		
	sprintf(pname,"%s/%s\0",dirn,dp->d_name); 

      getstat:
	errno = 0;
	if(lstat(pname,&statbuf) < 0) {
#ifdef EINTR
		if(errno==EINTR) goto getstat;
#endif
		myperror(pname);
		continue;
	}

	/* Did we cross a mount point? */
	if (xflag && statbuf.st_dev != thisfs) {
		if(vflag) {
		    printf("%s crosses mount points; pruned.\n",pname);
		}
		continue;
	}

	if(S_ISDIR(statbuf.st_mode)) {
		(void)srchdir(pname);
	} else {
		fseen++;

		if (!S_ISREG(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
		    if(vflag) {
			printf("%s not a regular file; ignored.\n", pname);
		    }
		    continue;
		}

		if (uflag && checkuid != statbuf.st_uid)
		    continue;

		fsize += statbuf.st_size;
		if(head == NULL) {
			head = tail = allocf(pname,&statbuf,(struct FINFO*)NULL);
			count = 1;
		} else if(count < nfiles) {
			srch = head;
			while(srch->next != NULL) {
                  		if(srch->next->size < statbuf.st_size) break;
				srch = srch->next;
			}
			if(srch == head) {
				if(statbuf.st_size > head->size)
	  				head = allocf(pname, &statbuf,head);
				else {
					head->next = fnow = 
					   allocf(pname,&statbuf,head->next);
					if(head == tail) tail = fnow;
				}
			} else if(srch->next == NULL) {
				tail = (tail->next =
				  allocf(pname,&statbuf,(struct FINFO*)NULL));
			} else {
				srch->next = allocf(pname,&statbuf,srch->next);
			}
			count++;
		} else if(statbuf.st_size > tail->size) {
			srch = head;
			while(srch->next != NULL) {
				if(srch->next->size < statbuf.st_size)
				    break;
				srch = srch->next;
			}
			if(srch->next == NULL) {
				replacef(pname,&statbuf,tail);
				continue;
			} else if(srch == head) {
				if(statbuf.st_size > head->size)
				 	head = allocf(pname,&statbuf,head);
				else head->next = allocf(pname,
						  &statbuf,head->next);
			} else srch->next = allocf(pname,&statbuf,srch->next);

			while(srch != NULL) {
				if(srch->next == tail) break;
				srch = srch->next;
			}
			freef(tail);
			tail = srch;
			tail->next = NULL;
		} /* st_size > tail->size */
	} /* else non-directory */
    } /* while */

    closedir(dirp);
    return 0;
}

/* if home is the first substring of name, then
 * replace last char of match with ~ and return pointer to that
 */
#ifdef __STDC__
char *cleanname(char *name)
#else
char *cleanname(name)
    char    *name;
#endif
{
    if(!name) return name;

    if(!strncmp(name,home,lhome)) {
	name += lhome - 1;
	*name = '~';
    }

    return name;
}
