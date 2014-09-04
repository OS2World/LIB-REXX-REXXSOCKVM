/*********************************************************************/
/*								     */
/* Module:	RxSockVM				 	     */
/*								     */
/* Author:      Peter Flass <Flass@Leginfo.LBDC.State.NY.US>         */
/*								     */
/* Function:	OS/2 Rexx Sockets library compatible with VM/ESA     */
/*              Version 2 Release 3.0 and higher                     */
/*								     */
/* Date:	14 Jul, 2000				  	     */
/*								     */
/* Version:	1.0a						     */
/*								     */
/* OS Version:	OS/2 Warp 4.0, TCP/IP version 4			     */
/*              EMX 0.9d ** Uses emx sockets, not OS/2 sockets       */
/*								     */
/* Unimplemented Functions return EINVALIDRXSOCKETCALL:              */
/*   (not supported by underlying OS/2 TCP/IP)                       */
/*    GiveSocket                                                     */
/*    TakeSocket                                                     */
/*    Trace                                                          */
/*								     */
/*********************************************************************/

#define  VERSION "RxSockVM 1.00a 07 Aug 2000"

#define  MAX_DIGITS     9          /* maximum digits in numeric arg  */

#define  EINVALIDRXSOCKETCALL 2001
#define  ESUBTASKNOTACTIVE    2005
#define  EMAXSOCKETSREACHED   2007

typedef  struct sockaddr_in *PSOCKADDR;

#define INCL_REXXSAA
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>

/*-------------------------*/
/* Internal Function Defs  */
/*-------------------------*/
INT soaccept();
INT sobind();
INT socls();
INT soconn();
INT sofcntl();
INT sogetclid();
INT sogetdom();
INT sogethba();
INT sogethbn();
INT sogethid();
INT sogethn();
INT sogetpeer();
INT sogetpbynam();
INT sogetpbynum();
INT sogetsbyn();
INT sogetsbyp();
INT sogetsnam();
INT sogetsopt();
INT soinit();
INT soioctl();
INT solisten();
INT soread();
INT sorecv();
INT sorecvfr();
INT soresolv();
INT sosel();
INT sosend();
INT sosendto();
INT sosetsopt();
INT sosocket();
INT soset();
INT sossl();
INT sosss();
INT soshut();
INT soterm();
INT sotrans();
INT sovers();
INT sowrite();
INT sounimpl();

RexxFunctionHandler Socket;

/*-------------------------*/
/* Socket Table - Internal */
/*-------------------------*/
typedef struct {
   INT stab_snum;
   INT stab_satt;
   } STAB, *PSTAB;
/* stab_satt=0 ASCII  */
/* stab_satt=1 EBCDIC */
LONG  maxsockets = 0;
INT   nsock = 0;
PSTAB ps;

CHAR Subtaskid[9] = "";

/*-------------------------*/
/* Command table           */
/*-------------------------*/
static const PCH fns[] = { 
  "ACCEPT",         "BIND",             "CLOSE",           "CONNECT", 
  "FCNTL",          "GETCLIENTID",      "GETDOMAINNAME",   "GETHOSTBYADDR", 
  "GETHOSTBYNAME",  "GETHOSTID",        "GETHOSTNAME",     "GETPEERNAME",
  "GETPROTOBYNAME", "GETPROTOBYNUMBER", "GETSERVBYNAME",   "GETSERVBYPORT",
  "GETSOCKNAME",    "GETSOCKOPT",       "GIVESOCKET",      "INITIALIZE",
  "IOCTL",          "LISTEN",           "READ",            "RECV",
  "RECVFROM",       "RESOLVE",          "SELECT",          "SEND",
  "SENDTO",         "SETSOCKOPT",       "SHUTDOWN",        "SOCKET",
  "SOCKETSET",      "SOCKETSETLIST",    "SOCKETSETSTATUS", "TAKESOCKET",
  "TERMINATE",      "TRACE",            "TRANSLATE",       "VERSION",
  "WRITE",          (PCH)NULL  };

/*-------------------------*/
/* Internal function names */
/*-------------------------*/
static const PFN fne[] = { 
   (PFN)&soaccept,   (PFN)&sobind,      (PFN)&socls,      (PFN)&soconn,
   (PFN)&sofcntl,    (PFN)&sogetclid,   (PFN)&sogetdom,   (PFN)&sogethba,
   (PFN)&sogethbn,   (PFN)&sogethid,    (PFN)&sogethn,    (PFN)&sogetpeer,
   (PFN)&sogetpbynam,(PFN)&sogetpbynum, (PFN)&sogetsbyn,  (PFN)&sogetsbyp,
   (PFN)&sogetsnam,  (PFN)&sogetsopt,   (PFN)&sounimpl,   (PFN)&soinit,
   (PFN)&soioctl,    (PFN)&solisten,    (PFN)&soread,     (PFN)&sorecv,
   (PFN)&sorecvfr,   (PFN)&soresolv,    (PFN)&sosel,      (PFN)&sosend,
   (PFN)&sosendto,   (PFN)&sosetsopt,   (PFN)&soshut,     (PFN)&sosocket,
   (PFN)&soset,      (PFN)&sossl,       (PFN)&sosss,      (PFN)&sounimpl,
   (PFN)&soterm,     (PFN)&sounimpl,    (PFN)&sotrans,    (PFN)&sovers,
   (PFN)&sowrite  };

/*-------------------------*/
/* Minimum # of arguments  */
/*-------------------------*/
static const INT argmin[] = {
   2,			3,		2,		3,
   3,			1,		1,		2,
   2,			1,		1,		2,
   2,			2,		2,		2,
   2,			4,		3,		2,
   3,			2,		2,		2,
   2,			2,		1,		3,
   3,			5,		2,		1,
   1,			1,		1,		3,
   1,			1,		3,		1,
   3,			0 };

/*-------------------------*/
/* Maximum # of arguments  */
/*-------------------------*/
static const INT argmax[] = {
   2,			3,		2,		3,
   4,			2,		1,		2,
   2,			1,		1,		2,
   2,			2,		3,		3,
   2,			4,		3,		4,
   4,			3,		3,		4,
   4,			3,	        3,		4,
   5,			5,		3,		4,
   2,			1,		2,		3,
   2,			2,		3,		1,
   3,			0 };

/*-------------------------*/
/* Address Families        */
/*-------------------------*/
static const PSZ fam[AF_MAX] = { 
  "AF_UNSPEC",      "AF_UNIX",          "AF_INET",	"AF_IMPLINK",
  "AF_PUP",	    "AF_CHAOS",		"AF_NS",	"AF_OSI",
  "AF_ECMA",	    "AF_DATAKIT",	"AF_CCITT",	"AF_SNA",
  "AF_DECNET",	    "AF_DLI",		"AF_LAT",	"AF_HYLINK",
  "AF_APPLETALK",   "AF_NETBIOS" };

/*-------------------------*/
/* Socket Types            */
/*-------------------------*/
#define TYPE_MAX 4
static const PSZ typ[TYPE_MAX] = { 
  "SOCK_STREAM",    "SOCK_DGRAM",       "SOCK_RAW",	"RAW" };    

/*-------------------------*/
/* Socket Options          */
/* (combo of OS/2 and VM)  */
/*-------------------------*/
#define OPT_MAX 20
#define SO_ASCII  -1
#define SO_EBCDIC -2
static const PSZ opt[OPT_MAX] = { 
  "SO_BROADCAST",   "SO_ERROR",         "SO_KEEPALIVE", "SO_LINGER",
  "SO_OOBINLINE",   "SO_REUSEADDR",	"SO_SNDBUF",    "SO_TYPE",
  "SO_ASCII",       "SO_EBCDIC",        "SO_DEBUG",	"SO_DONTROUTE",
  "SO_RCVBUF",	    "SO_RCVLOWAT",	"SO_RCVTIMEO",	"SO_SNDBUF",
  "SO_SNDLOWAT",    "SO_SNDTIMEO",	"SO_TYPE",	"SO_USELOOPBACK"
  };

/*-------------------------*/
/* Descriptive error codes */
/*-------------------------*/
static const PSZ perrx[] = { "", /*000808*/
   "EPERM",   "ENOENT", "ESRCH",   "EINTR",  "EIO",
   "ENXIO",   "E2BIG",  "ENOEXEC", "EBADF",  "ECHILD",
   "EAGAIN",  "ENOMEM", "EACCES",  "EFAULT", "ENOLCK",
   "EBUSY",   "EEXIST", "EXDEV",   "ENODEV", "ENOTDIR",
   "EISDIR",  "EINVAL", "ENFILE",  "EMFILE", "ENOTTY",
   "EDEADLK", "EFBIG",  "ENOSPC",  "ESPIPE", "EROFS",
   "EMLINK",  "EPIPE",  "EDOM",    "ERANGE", "ENOTEMPTY",
   "EINPROGRESS",       "ENOSYS",            "ENAMETOOLONG",
   "EDESTADDRREQ",      "EMSGSIZE",          "EPROTOTYPE",
   "ENOPROTOOPT",       "EPROTONOSUPPORT",   "ESOCKTNOSUPPORT",
   "EOPNOTSUPP",        "EPFNOSUPPORT",      "EAFNOSUPPORT",
   "EADDRINUSE",        "EADDRNOTAVAIL",     "ENETDOWN",
   "ENETUNREACH",       "ENETRESET",         "ECONNABORTED",
   "ECONNRESET",        "ENOBUFS",           "EISCONN",
   "ENOTCONN",          "ESHUTDOWN",         "ETOOMANYREFS",
   "ETIMEDOUT",         "ECONNREFUSED",      "ELOOP",
   "ENOTSOCK",          "EHOSTDOWN",         "EHOSTUNREACH",
   "EALREADY"  
   };

static const INT optx[OPT_MAX] = {
   SO_BROADCAST,     SO_ERROR,           SO_KEEPALIVE,   SO_LINGER, 
   SO_OOBINLINE,     SO_REUSEADDR, 	 SO_SNDBUF,      SO_TYPE,
   SO_ASCII,         SO_EBCDIC,          SO_DEBUG,	 SO_DONTROUTE,
   SO_RCVBUF,	     SO_RCVLOWAT,	 SO_RCVTIMEO,	 SO_SNDBUF,
   SO_SNDLOWAT,	     SO_SNDTIMEO,	 SO_TYPE,	 SO_USELOOPBACK
   };

/*-------------------------------------------------*/
/* STANDARD translate table copied from OS/390 TCP */
/*-------------------------------------------------*/
/* tcplxbin: ascii to ebcdic */
static UCHAR A2E[] = {
  0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F,
  0x16, 0x05, 0x25, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26,
  0x18, 0x19, 0x3F, 0x27, 0x22, 0x1D, 0x35, 0x1F,
  0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D,
  0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
  0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
  0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
  0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,
  0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
  0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F,
  0x16, 0x05, 0x25, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26,
  0x18, 0x19, 0x3F, 0x27, 0x22, 0x1D, 0x35, 0x1F,
  0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D,
  0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
  0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
  0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
  0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
  0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
  0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
  0xE7, 0xE8, 0xE9, 0xAD, 0xE0, 0xBD, 0x5F, 0x6D,
  0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
  0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07 };

/* tcplxbin: ebcdic to ascii */
static UCHAR E2A[] = {
  0x00, 0x01, 0x02, 0x03, 0x1A, 0x09, 0x1A, 0x7F,
  0x1A, 0x1A, 0x1A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x10, 0x11, 0x12, 0x13, 0x1A, 0x0A, 0x08, 0x1A,
  0x18, 0x19, 0x1A, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F,
  0x1A, 0x1A, 0x1C, 0x1A, 0x1A, 0x0A, 0x17, 0x1B,
  0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x05, 0x06, 0x07,
  0x1A, 0x1A, 0x16, 0x1A, 0x1A, 0x1E, 0x1A, 0x04,
  0x1A, 0x1A, 0x1A, 0x1A, 0x14, 0x15, 0x1A, 0x1A,
  0x20, 0xA6, 0xE1, 0x80, 0xEB, 0x90, 0x9F, 0xE2,
  0xAB, 0x8B, 0x9B, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
  0x26, 0xA9, 0xAA, 0x9C, 0xDB, 0xA5, 0x99, 0xE3,
  0xA8, 0x9E, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
  0x2D, 0x2F, 0xDF, 0xDC, 0x9A, 0xDD, 0xDE, 0x98,
  0x9D, 0xAC, 0xBA, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
  0xD7, 0x88, 0x94, 0xB0, 0xB1, 0xB2, 0xFC, 0xD6,
  0xFB, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
  0xF8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  0x68, 0x69, 0x96, 0xA4, 0xF3, 0xAF, 0xAE, 0xC5,
  0x8C, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
  0x71, 0x72, 0x97, 0x87, 0xCE, 0x93, 0xF1, 0xFE,
  0xC8, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7A, 0xEF, 0xC0, 0xDA, 0x5B, 0xF2, 0xF9,
  0xB5, 0xB6, 0xFD, 0xB7, 0xB8, 0xB9, 0xE6, 0xBB,
  0xBC, 0xBD, 0x8D, 0xD9, 0xBF, 0x5D, 0xD8, 0xC4,
  0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0xCB, 0xCA, 0xBE, 0xE8, 0xEC, 0xED,
  0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
  0x51, 0x52, 0xA1, 0xAD, 0xF5, 0xF4, 0xA3, 0x8F,
  0x5C, 0xE7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5A, 0xA0, 0x85, 0x8E, 0xE9, 0xE4, 0xD1,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0xB3, 0xF7, 0xF0, 0xFA, 0xA7, 0xFF };



/*-----------------------------------*/
/* Generate Error message            */
/*-----------------------------------*/
void soerr( PRXSTRING r, ULONG err ) {
   PCH p;
   p = r->strptr;
   p+= sprintf( p, "%d ", err );
   switch(err) {
      case EINVALIDRXSOCKETCALL:
         p+= sprintf( p, "EINVALIDRXSOCKETCALL Syntax error in RXSOCKET parameter list" );
         break;
      case ESUBTASKNOTACTIVE:
         p+= sprintf( p, "ESUBTASKNOTACTIVE Subtask not active" );
         break;
      case EMAXSOCKETSREACHED:
         p+= sprintf( p, "EMAXSOCKETSREACHED Maximum number of sockets reached" );
         break;
      /* NOTE: ===> First string is error code */
      default: 
         if( err<1 || err>EALREADY )
           p+= sprintf( p, "%s %s", "*", strerror(err) );
         else p+= sprintf( p, "%s %s", perrx[err], strerror(err) ); /*000807*/
         break;
      }
   r->strlength = strlen(r->strptr);
   return;
   } /* End soerr */

/*-----------------------------------*/
/* Convert a Rexx string to a LONG   */
/*-----------------------------------*/
BOOL string2long(PRXSTRING str, LONG *num)
{
  ULONG    accumulator;                /* converted number           */
  INT      length;                     /* length of number           */
  INT      sign;                       /* sign of number             */
  PCH      string;		       /* -> String                  */
 
  if( RXNULLSTRING(*str) || RXZEROLENSTRING(*str) ) {
     return FALSE;
     }

  length = str->strlength;
  string = str->strptr;
  sign = 1;                            /* set default sign           */
  if (*string == '-') {                /* negative?                  */
    sign = -1;                         /* change sign                */
    string++;                          /* step past sign             */
    length--;			       /* decrement length	     */
  }
 
  if (length == 0 ||                   /* if null string             */
      length > MAX_DIGITS)             /* or too long                */
    return FALSE;                      /* not valid                  */
 
  accumulator = 0;                     /* start with zero            */
 
  while (length) {                     /* while more digits          */
    if (!isdigit(*string))             /* not a digit?               */
      return FALSE;                    /* tell caller                */
                                       /* add to accumulator         */
    accumulator = accumulator *10 + (*string - '0');
    length--;                          /* reduce length              */
    string++;                          /* step pointer               */
  }
  *num = accumulator * sign;           /* return the value           */
  return TRUE;                         /* good number                */
} /* End string2long */

/*-----------------------------------*/
/* Extract a number from a field     */
/*-----------------------------------*/
INT getint(PCH p, INT l )
{
  INT      accumulator;                /* converted number           */
  INT      sign;                       /* sign of number             */
 
  sign = 1;                            /* set default sign           */
  if (*p == '-') {                     /* negative?                  */
    sign = -1;                         /* change sign                */
    p++;                               /* step past sign             */
    l--;			       /* decrement length	     */
  }
 
  if (l == 0 || l > 5 )                /* if null string or too long */
    return INT_MIN;                    /* not valid                  */
 
  accumulator = 0;                     /* start with zero            */
 
  while (l) {                          /* while more digits          */
    if (!isdigit(*p))                  /* not a digit?               */
      return INT_MIN;                  /* tell caller                */
                                       /* add to accumulator         */
    accumulator = accumulator *10 + (*p - '0');
    l--;                               /* reduce length              */
    p++;                               /* step pointer               */
  }
  return accumulator * sign;           /* good number                */
} /* End getint */        

/*-----------------------------------*/
/* Parse an address parameter        */
/* 'domain portid ipaddress'         */
/*-----------------------------------*/
BOOL parseaddress(PRXSTRING s, PSOCKADDR paddr)
{
  INT l;
  PUCHAR p,q;
  struct hostent *pHE;
 
  if( RXNULLSTRING(*s) || RXZEROLENSTRING(*s) ) {
     return FALSE;
     }

  /*-----------------------*/
  /*  Parse Domain         */
  /*-----------------------*/
  p = s->strptr;
  q = strchr(p,' ');
  if( q==NULL ) return(FALSE);
  l = q-p;
  if( (strnicmp(p,"AF_INET",l)==0) || (strncmp(p,"2",l)==0) )
     paddr->sin_family = AF_INET;
  else return(FALSE);

  /*-----------------------*/
  /*  Parse Port           */
  /*-----------------------*/
  for(;*q==' ';q++) /* continue */ ;
  p = q;
  q = strchr(p,' ');
  l = q-p;
  l = getint(p,l);                     /* Get an integer             */
  if( l==INT_MIN ) return FALSE;       /* Bad number                 */
  paddr->sin_port = htons(l); 

  /*-----------------------*/
  /*  Parse IP Address     */
  /*-----------------------*/
  for(;*q==' ';q++) /* continue */ ;
  p = q;
  q = strchr(p,' ');
  if( q==NULL ) q=p+strlen(p);
  l = q-p;
  if( isdigit(*p) ) {                  /* numeric IP address         */
     paddr->sin_addr.s_addr = inet_addr(p);
     }
  else {                               /* Alphanumeric host name     */
     if( stricmp(p,"INADDR_ANY")==0 ) 
        paddr->sin_addr.s_addr = INADDR_ANY;
     else if( stricmp(p,"INADDR_BROADCAST")==0  )
        paddr->sin_addr.s_addr = INADDR_BROADCAST;
     else {
        pHE = gethostbyname(p);
        if( pHE==NULL ) return FALSE;
        paddr->sin_addr.s_addr = (ULONG) *(pHE->h_addr);
        }
     }

  return TRUE;                         /* good number                */
} /* End parseaddress */

/*-----------------------------------*/
/* Extract the next blank-delimited  */
/* word from a string                */
/*-----------------------------------*/
PSZ next_word(PSZ p)
{
} /* end next_word */

/*-----------------------------------*/
/* Main Entry point for Socket()     */
/*-----------------------------------*/
ULONG APIENTRY Socket( PCSZ name, ULONG numargs, PRXSTRING argp,
             PCSZ queuename, PRXSTRING rp)
   {

   PRXSTRING fp;              	      /* -> Function request         */
   PCH       f;
   INT       i;

   /*--------------------------------*/
   /* Validate command               */
   /*--------------------------------*/
   if( numargs<1 ) {
      soerr( rp, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   fp = argp;		     	      /* -> First argument	     */
   if( RXNULLSTRING(*fp) ) {
      soerr( rp, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   for( i=0; fns[i]!=(PCH)NULL; i++ ) {
      if( fp->strlength != strlen(fns[i]) )  continue;
      if( stricmp(fns[i],fp->strptr)==0 )    break;
      }
   f = fns[i];
   if( f==(PCH)NULL ) {             /* Invalid function                     */
      soerr( rp, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   
   /*--------------------------------*/
   /* Validate number of arguments   */
   /*--------------------------------*/
   if( numargs<argmin[i] ) {
      soerr( rp, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   if( (argmax[i]>0) && (numargs>argmax[i]) ) {
      soerr( rp, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   /*--------------------------------*/
   /* Execute command                */
   /*--------------------------------*/
   fp = (PRXSTRING)( (PUCHAR)fp + sizeof(RXSTRING) );
   (INT)(*fne[i])( numargs, fp, rp );

   return(0);

   } /* End Socket */


/*-----------------------------------*/
/* ACCEPT Function (0)               */
/*-----------------------------------*/
INT soaccept( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct sockaddr_in addr;
   INT    addrl;
   INT    so;
   LONG   s;
   PCH    p;
   PSTAB  pst;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   if( nsock>=maxsockets ) {		/*000807*/
      soerr( r, EMAXSOCKETSREACHED );	/*000807*/
      return(0);			/*000807*/
      }					/*000807*/

   addrl = sizeof(addr);
   so = accept( s, (struct sockaddr *)&addr, &addrl );
   if( so<0 ) {
      soerr( r, errno );
      return 0;
      }
   if( addr.sin_family>=AF_MAX )
      addr.sin_family=AF_UNSPEC;

   pst = (PSTAB)( (PUCHAR)ps + (sizeof(STAB)*(nsock)) ); /*000807*/
   nsock++;						 /*000807*/
   pst->stab_snum = so;					 /*000807*/
   pst->stab_satt = 0;					 /*000807*/

   p = r->strptr; 
   p += sprintf( p, "0 %d ",so );
   strcat(p,fam[addr.sin_family]);
   p += strlen(p);
   if( addr.sin_family==AF_INET ) {
      p += sprintf( p, " %d %s", ntohs(addr.sin_port),
                   inet_ntoa(addr.sin_addr) );
      } /* End INET */
   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End soaccept */

/*-----------------------------------*/
/* BIND Function (1)                 */
/*-----------------------------------*/
INT sobind( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct sockaddr_in addr;
   INT    addrl;
   INT    so;
   LONG   s;
   PUCHAR pAddr;
   PRXSTRING arg3;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   addrl = sizeof(addr);
   if( parseaddress(arg3,&addr)!=TRUE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   so = bind( s, (struct sockaddr *)&addr, addrl );
   if( so<0 ) {
      soerr( r, errno );
      }
   else strcpy( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sobind */

/*-----------------------------------*/
/* CLOSE Function (2)                */
/*-----------------------------------*/
INT socls( ULONG n, PRXSTRING a, PRXSTRING r ) {
   INT    rc;
   LONG   s;
   INT    i,j;
   PSTAB  pst,qst;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   /*----------------------*/
   /* Del socket from list */
   /*----------------------*/
   pst = ps;
   for( i=1; i<=nsock; i++ ) {
      if( pst->stab_snum==s ) break;
      pst = (PSTAB)( (PUCHAR)pst + sizeof(STAB) );
      }
   if( i>nsock ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   for( j=i+1; j<=nsock; j++,i++ ) {
      qst = (PSTAB)( (PUCHAR)pst + sizeof(STAB) );
      qst->stab_snum = pst->stab_snum;
      qst->stab_satt = pst->stab_satt;
      pst = qst;
      }
   nsock--;

   rc = close( s );
   if( rc<0 ) {
      soerr( r, errno );
      }
   else strcpy( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End socls */

/*-----------------------------------*/
/* CONNECT Function (3)              */
/*-----------------------------------*/
INT soconn( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct sockaddr_in addr;
   INT    addrl;
   INT    rc;
   LONG   s;
   PRXSTRING arg3;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   addrl = sizeof(addr);
   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( parseaddress(arg3,&addr)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   rc = connect( s, (struct sockaddr *)&addr, addrl );

   if( rc<0 ) {
      soerr( r, errno );
      }
   else strcpy( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End soconn */

/*-----------------------------------*/
/* FCNTL Function (4)                */
/*-----------------------------------*/
INT sofcntl( ULONG n, PRXSTRING a, PRXSTRING r ) {
   INT    so;
   LONG   s;
   PRXSTRING arg3,arg4;
   INT    fcntl_arg=0;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg3) || RXZEROLENSTRING(*arg3) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   if( stricmp(arg3->strptr,"F_GETFL")==0 ) {
      fcntl_arg = fcntl(s,F_GETFL);
      if( fcntl_arg==-1 ) {
         soerr( r, errno );
         return(0);
         }
      if( (fcntl_arg&O_NONBLOCK)!=0 )
         strcpy( r->strptr, "0 NON-BLOCKING");
      else strcpy( r->strptr, "0 BLOCKING");
      r->strlength = strlen(r->strptr);
      return(0);
      }
   else if( stricmp(arg3->strptr,"F_SETFL")==0 ) {
      if( n<4 ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( RXNULLSTRING(*arg4) || RXZEROLENSTRING(*arg4) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( (stricmp(arg4->strptr,"NON-BLOCKING")==0) ||
          (stricmp(arg4->strptr,"FNDELAY")==0) ) 
         fcntl_arg = O_NONBLOCK;
      else if( (stricmp(arg4->strptr,"BLOCKING")==0) ||
               (stricmp(arg4->strptr,"0")==0) ) 
         fcntl_arg = -1&(~O_NONBLOCK);
      else {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( fcntl(s,F_SETFL,fcntl_arg)==-1 ) {
         soerr( r, errno );
         return(0);
         }
      strcpy( r->strptr, "0" );
      r->strlength = strlen(r->strptr);
      return(0);
      }
   else {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return 0;
      }
   } /* End sofcntl */

/*-----------------------------------*/
/* GETCLIENTID Function (5)          */
/*-----------------------------------*/
INT sogetclid( ULONG n, PRXSTRING a, PRXSTRING r ) {

   if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   if( n>1 ) {
      if( (stricmp(a->strptr,"AF_INET")==0)  ||
          (stricmp(a->strptr,"AF_UNSPEC")==0)||
          (strcmp(a->strptr,"0")==0)         ||
          (strcmp(a->strptr,"2")==0) ) /* continue */ ;
      else {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return 0;
         } 
      } /* end n>1 */
      
   sprintf( r->strptr,"0 AF_INET  %8s %8s","Userid",Subtaskid);
   /* NOTE: TCP/IP "Userid" value */
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetclid */

/*-----------------------------------*/
/* GETDOMAINNAME Function (6)        */
/*-----------------------------------*/
INT sogetdom( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   struct hostent *pHE;
   PCH    p;
 
   host_addr.s_addr = htonl( gethostid() );
   pHE = gethostbyaddr( (PCH)&host_addr, sizeof(struct in_addr) , AF_INET );

   if( pHE==NULL ) {
      soerr( r, h_errno );
      return(0);
      }
   p = strchr(pHE->h_name,'.');
   if( p!=NULL ) p++;
   sprintf( r->strptr, "0 %s",p );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetdom */

/*-----------------------------------*/
/* GETHOSTBYADDR Function (7)        */
/*-----------------------------------*/
INT sogethba( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   struct hostent *pHE;

   if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
     }
   host_addr.s_addr = inet_addr(a->strptr);
   pHE = gethostbyaddr( (PCH)&host_addr, sizeof(struct in_addr) , AF_INET );

   if( pHE==NULL ) {
      soerr( r, h_errno );
      }
   else sprintf( r->strptr, "0 %s",pHE->h_name );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogethba */

/*-----------------------------------*/
/* GETHOSTBYNAME Function (8)        */
/*-----------------------------------*/
INT sogethbn( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   struct hostent *pHE;

   if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
     }
   pHE = gethostbyname( a->strptr );

   if( pHE==NULL ) {
      soerr( r, h_errno );
      return(0);
      }
   memcpy( &host_addr.s_addr, pHE->h_addr, sizeof host_addr.s_addr );
   sprintf( r->strptr, "0 %s", inet_ntoa(host_addr) );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogethbn */

/*-----------------------------------*/
/* GETHOSTID Function (9)            */
/*-----------------------------------*/
INT sogethid( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   PCH    p;
 
   host_addr.s_addr = htonl( gethostid() );
   sprintf( r->strptr, "0 %s", inet_ntoa(host_addr) );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogethid */

/*-----------------------------------*/
/* GETHOSTNAME Function (10)         */
/*-----------------------------------*/
INT sogethn( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   struct hostent *pHE;
   PCH    p;
 
   host_addr.s_addr = htonl( gethostid() );
   pHE = gethostbyaddr( (PCH)&host_addr, sizeof(struct in_addr) , AF_INET );

   if( pHE==NULL ) {
      soerr( r, h_errno );
      return(0);
      }
   p = strchr(pHE->h_name,'.');
   if( p!=NULL ) *p='\0';       
   sprintf( r->strptr, "0 %s",pHE->h_name );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogethn */

/*-----------------------------------*/
/* GETPEERNAME Function (11)         */
/*-----------------------------------*/
INT sogetpeer( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct in_addr host_addr;
   struct sockaddr_in addr;
   INT    addrl;
   PCH    p;
   LONG   s;
   INT    rc;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   addrl = sizeof(addr);
   rc = getpeername( s, (struct sockaddr *)&addr , &addrl );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   if( addr.sin_family>=AF_MAX )
      addr.sin_family=AF_UNSPEC;
   p = r->strptr; 
   p += sprintf( p, "0 %d ",s );
   strcat(p,fam[addr.sin_family]);
   p += strlen(p);
   if( addr.sin_family==AF_INET ) {
      p += sprintf( p, " %d %s", ntohs(addr.sin_port),
                   inet_ntoa(addr.sin_addr) );
      } 
   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetpeer */

/*-----------------------------------*/
/* GETPROTOBYNAME Function (12)      */
/*-----------------------------------*/
INT sogetpbynam( ULONG n, PRXSTRING a, PRXSTRING r ) {
   INT proto;

   if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
     }
   /* This bypasses the getprotobyname() call, */
   /* since only three protocols are valid.    */
   if( stricmp(a->strptr,"TCP")==0 )      proto = IPPROTO_TCP;
   else if( stricmp(a->strptr,"UDP")==0 ) proto = IPPROTO_UDP;
   else if( stricmp(a->strptr,"IP")==0 )  proto = IPPROTO_IP;
   else {
      strcpy( r->strptr, "0" );
      return(0);
      }
   sprintf( r->strptr, "0 %d", proto );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetpbynam */

/*-----------------------------------*/
/* GETPROTOBYNUM Function (13)       */
/*-----------------------------------*/
INT sogetpbynum( ULONG n, PRXSTRING a, PRXSTRING r ) {
   LONG proto;
   struct protoent *pPE;

   if( string2long(a,&proto)==FALSE ) {
      strcpy( r->strptr, "0" );
      return(0);
      }
   pPE = getprotobynumber(proto);
   if( pPE==NULL ) {
      soerr( r, errno );
      return(0);
      }
   sprintf( r->strptr, "0 %s", pPE->p_name );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetpbynum */

/*-----------------------------------*/
/* GETSERVBYNAME Function (14)       */
/*-----------------------------------*/
INT sogetsbyn( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    pProt;
   INT    port;
   struct servent *pSE;
   PRXSTRING arg3;

   if( n<3 ) pProt = "";
   else {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXNULLSTRING(*arg3) || RXZEROLENSTRING(*arg3) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      pProt = arg3->strptr;
      }
   pSE = getservbyname( a->strptr, pProt  );

   if( pSE==NULL ) {
      soerr( r, errno );
      return(0);
      }
   sprintf( r->strptr, "0 %s %d %s", pSE->s_name,
            ntohs(pSE->s_port), pSE->s_proto );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetsbyn */

/*-----------------------------------*/
/* GETSERVBYPORT Function (15)       */
/*-----------------------------------*/
INT sogetsbyp( ULONG n, PRXSTRING a, PRXSTRING r ) {
   LONG   p;
   PCH    pProt;
   INT    port;
   struct servent *pSE;
   PRXSTRING arg3;

   if( string2long(a,&p)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   if( n<3 ) pProt = "";
   else {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      pProt = arg3->strptr;
      }
   pSE = getservbyport( p, pProt  );

   if( pSE==NULL ) {
      soerr( r, errno );
      return(0);
      }
   sprintf( r->strptr, "0 %s %d %s", pSE->s_name,
            ntohs(pSE->s_port), pSE->s_proto );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetsbyp */

/*-----------------------------------*/
/* GETSOCKNAME Function (16)         */
/*-----------------------------------*/
INT sogetsnam( ULONG n, PRXSTRING a, PRXSTRING r ) {
   struct sockaddr_in addr;
   INT    addrl;
   PCH    p;
   LONG   s; 
   INT    rc;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   addrl = sizeof(addr);
   rc = getsockname( s, (struct sockaddr *)&addr , &addrl );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   p = r->strptr; 
   p += sprintf( p, "0 %s", fam[addr.sin_family] );
   if( addr.sin_family==AF_INET ) {
      if( addr.sin_port )
         p += sprintf( p, " %d", ntohs(addr.sin_port) );
      if( addr.sin_addr.s_addr )
         p += sprintf( p, " %s", inet_ntoa(addr.sin_addr) );
      } 
   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sogetsnam */

/*-----------------------------------*/
/* GETSOCKOPT Function (17)          */
/*-----------------------------------*/
INT sogetsopt( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   LONG   s; 
   INT    rc;
   INT    i,j;
   PSTAB  pst;
   PRXSTRING arg3,arg4;
   char   optbuf[80]; /* Max size buffer */
   INT    optlen;
   typedef INT  *POPTINT;
   typedef LONG *POPTLONG;
   typedef struct linger *PLINGER;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   /*----------------------*/
   /* Validate Level       */
   /*----------------------*/
   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg3) || RXZEROLENSTRING(*arg3) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   if( stricmp( arg3->strptr, "SOL_SOCKET" )!=0 ) { 
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   /*----------------------*/
   /* Validate Option      */
   /*----------------------*/
   arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg4) || RXZEROLENSTRING(*arg4) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   for( i=0; i<OPT_MAX; i++ ) {
      if( stricmp( arg4->strptr, opt[i] )==0 ) break;
      }
   if( i>=OPT_MAX ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   i = optx[i];

   /*----------------------*/
   /* Find Socket          */
   /*----------------------*/
   pst = ps;
   for( j=1; j<=nsock; j++ ) {
      if( pst->stab_snum==s ) break;
      pst = (PSTAB)( (PUCHAR)pst + sizeof(STAB) );
      }
   if( j>nsock ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   /*----------------------*/
   /* ASCII and EBCDIC are */
   /* Internal options     */
   /* Translation table is */
   /* always "STANDARD"    */
   /*----------------------*/
   switch( i ) {
      case SO_ASCII:
         p = r->strptr;
         p+= sprintf( p, "0 %s STANDARD", pst->stab_satt? "On": "Off" );
         *p='\0';
         r->strlength = strlen(r->strptr);
         return(0);
      case SO_EBCDIC:
         p = r->strptr;
         p+= sprintf( p, "0 %s STANDARD", pst->stab_satt? "Off": "On" );
         *p='\0';
         r->strlength = strlen(r->strptr);
         return(0);
      default: break; 
      } /* end switch */

   optlen = sizeof optbuf;

   rc = getsockopt( s, SOL_SOCKET, i, (PUCHAR)&optbuf, &optlen );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   p = r->strptr;
   strcpy( p, "0" ); 
   p += strlen(p);

   switch( i ) {
      case SO_BROADCAST:
      case SO_DEBUG:
      case SO_DONTROUTE:
      case SO_KEEPALIVE:
      case SO_OOBINLINE:
      case SO_REUSEADDR:
         p+= sprintf( p, "%s",optbuf[0]? "On": "Off");
         break;
      case SO_ERROR:
      case SO_TYPE:
      case SO_USELOOPBACK:
      case SO_RCVLOWAT:
      case SO_RCVTIMEO:
      case SO_SNDLOWAT:
      case SO_SNDTIMEO:
         p+= sprintf( p, "%d", (POPTINT)&optbuf);
         break;
      case SO_RCVBUF:
      case SO_SNDBUF:
         p+= sprintf( p, "%ld", (POPTLONG)&optbuf);
         break;
      case SO_LINGER:
         p+= sprintf( p, "%s %d", ((PLINGER)&optbuf)->l_onoff? "Off": "On",
                      ((PLINGER)&optbuf)->l_linger);
         break;
      } /* end switch */

   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);

   } /* End segetsopt */

/*-----------------------------------*/
/* GIVESOCKET (18) Unimplemented     */
/*-----------------------------------*/

/*-----------------------------------*/
/* INITIALIZE Function (19)          */
/*-----------------------------------*/
INT soinit( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   INT    so;
   PRXSTRING arg3,arg4;

   if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ||
       (a->strlength>8) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
     }
   strncpy(Subtaskid,a->strptr,8);
   *(Subtaskid+8)='\0';

   maxsockets = 40;
   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg3) ) {
         if( string2long(arg3,&maxsockets)==FALSE ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         }
      if( maxsockets<=0 ) {
         soerr( r, EINVALIDRXSOCKETCALL );
         return(0);
         }
      } /* end n>2 */

   /*----------------------*/
   /* Alloc internal socket*/
   /* structure            */
   /*----------------------*/
   ps = (PSTAB)(malloc( maxsockets * sizeof(STAB) ));


   /* sock_init Not implemented for emx sockets  
    . s = sock_init(); 
    . if( s<0 ) {
    .    soerr( r, errno );
    .    return(0);
    .    }
    */

   p = r->strptr; 
   p += sprintf( p, "0 %s 40 TCPIP", a->strptr );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End soinit */

/*-----------------------------------*/
/* IOCTL Function (20)               */
/*-----------------------------------*/
INT soioctl( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH     p;
   INT     i;
   LONG    so;
   INT     how;
   INT     rc;
   PRXSTRING arg3,arg4;
   static PCH ioc[] = {
      "FIONBIO",		"FIONREAD",	
      "SIOCATMARK",		"SIOFIFADDR",
      "SIOCGIFBRDADDR",		"SIOCGIFCONF",	"SIOCGIFDSTADDR",
      "SIOCGIFFLAGS",		"SIOCGIFMETRIC","SIOCGIFNETMASK"
      "" };
   static INT iox[] = {
      FIONBIO,		FIONREAD,
      0,		0,
      0,		0,	0,
      0,		0,  	0 };

   if( RXVALIDSTRING(*a) ) {
      if( string2long(a,&so)==FALSE ) {
         soerr( r, EINVALIDRXSOCKETCALL );
         return(0);
         }
      }

   /* Validate 'icmd' */
   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( !RXVALIDSTRING(*arg3) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   for( i=0; *(ioc[i]); i++ ) {
      if( stricmp( ioc[i], arg3->strptr )==0 ) break;
      }
   if( !*(ioc[i]) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   i = iox[i]; /* Get value for IOCTL */
   /*----------------------------*/
   /* There is no reason why the */
   /* SIOCGIF* commands can not  */
   /* work, except they are not  */
   /* implemented by the distrib-*/
   /* uted Rexx sockets package  */
   /*----------------------------*/
   if( i==0 ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg4) ) {
         if( i!=FIONBIO ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         if( stricmp(arg4->strptr,"ON")==0 ) how=0;
         else if( stricmp(arg4->strptr,"OFF")==0 ) how=1;
         else {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         }
      } /* end n>3 */

   rc = ioctl( so, i, &how );
   if( rc<0 ) {
      soerr( r, errno );
      return(0);
      }

   p = r->strptr; 
   if( i!=FIONBIO ) 
      strcpy(p,"0");
   else p+= sprintf( p, "0 %d", how );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End soioctl */

/*-----------------------------------*/
/* LISTEN Function (21)              */
/*-----------------------------------*/
INT solisten( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   LONG   s;
   LONG   backlog;
   INT    rc;
   PRXSTRING arg3;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   backlog = 10;
   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg3) ) {
         if( string2long(arg3,&backlog)==FALSE ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         }
      } /* end n>2 */

   if( (backlog<0) || (backlog>10) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   rc = listen( s, backlog );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   strcpy( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End solisten */

/*-----------------------------------*/
/* READ Function (22)                */
/*-----------------------------------*/
INT soread( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   LONG   s;
   LONG   len=10000;
   INT    cnt;
   INT    rc;
   PRXSTRING arg3;
   PVOID  pMem;
   PCH    buf;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg3)) || (RXZEROLENSTRING(*arg3)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( string2long(arg3,&len)==FALSE ) {
         soerr( r, EINVALIDRXSOCKETCALL );
         return(0);
         }
      } /* end n>2 */

   buf = malloc(len);
   if( buf==NULL ) {
      soerr( r, errno );
      return(0);
      }
   cnt = read( s, buf, len );
   if( cnt<=0 ) {
      soerr( r, errno );
      free(buf);
      return(0);
      }
   if( cnt>240 ) {  /* Need to get a new RXSTRING for result */
      rc= DosAllocMem( &pMem, cnt+16, 
                       PAG_COMMIT|PAG_READ|PAG_WRITE );
      if( rc ) {
         soerr( r, ENOMEM );
         free(buf);
         return(0);
         }
      r->strptr = pMem;
      }

   p = r->strptr;
   p += sprintf( p, "0 %d ", cnt );
   memcpy( p, buf, cnt );
   free(buf);
   p+=cnt;
   *p = '\0';
   r->strlength = p - r->strptr;
   return(0);
   } /* End soread */

/*-----------------------------------*/
/* RECV Function (23)                */
/*-----------------------------------*/
INT sorecv( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   PRXSTRING arg3,arg4;
   LONG   s;
   LONG   len=10000; /* Defaults */
   INT    flags=0;
   INT    cnt;
   INT    rc;
   PVOID  pMem;
   PCH    buf;
   PCH    tmp;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg3)) || (RXZEROLENSTRING(*arg3)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( string2long(arg3,&len)==FALSE ) {
         soerr( r, EINVALIDRXSOCKETCALL );
         return(0);
         }
      } /* end n>2 */

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg4)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( !(RXZEROLENSTRING(*arg4)) ) {
         tmp = malloc(arg4->strlength+1); /* Make copy of argument */
         strncpy( tmp, arg4->strptr, arg4->strlength );
         *(tmp+arg4->strlength)='\0';
         p = strtok(tmp," ");
         while( p!=NULL ) {
            if( (stricmp(p,"MSG_OOB")==0) ||
                (stricmp(p,"OOB")==0)     ||
                (stricmp(p,"OUT_OF_BAND")==0) )
                 flags|=MSG_OOB;
            else if( (stricmp(p,"MSG_PEEK")==0) ||
                     (stricmp(p,"PEEK")==0) )
                 flags|=MSG_PEEK;
            else {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            p=strtok( NULL, " " );
            }
         free(tmp);
         }
      } /* end n>3 */

   buf = malloc(len);
   if( buf==NULL ) {
      soerr( r, errno );
      return(0);
      }

   cnt = recv( s, buf, len, flags );

   if( cnt<=0 ) {
      soerr( r, errno );
      free(buf);
      return(0);
      }
   if( cnt>240 ) {  /* Need to get a new RXSTRING for result */
      rc= DosAllocMem( &pMem, cnt+16, 
                       PAG_COMMIT|PAG_READ|PAG_WRITE );
      if( rc ) {
         soerr( r, ENOMEM );
         free(buf);
         return(0);
         }
      r->strptr = pMem;
      }

   p = r->strptr;
   p += sprintf( p, "0 %d ", cnt );
   memcpy( p, buf, cnt );
   free(buf);
   p+=cnt;
   *p = '\0';
   r->strlength = p - r->strptr;
   return(0);
   } /* End sorecv */

/*-----------------------------------*/
/* RECVFROM Function (24)            */
/*-----------------------------------*/
INT sorecvfr( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   PRXSTRING arg3,arg4;
   LONG   s;
   LONG   len=10000; /* Defaults */
   INT    flags=0;
   INT    cnt;
   INT    rc;
   PVOID  pMem;
   PCH    buf;
   PCH    tmp;
   struct sockaddr_in addr;
   INT    addrl;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg3)) || (RXZEROLENSTRING(*arg3)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( string2long(arg3,&len)==FALSE ) {
         soerr( r, EINVALIDRXSOCKETCALL );
         return(0);
         }
      } /* end n>2 */

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg4)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( !(RXZEROLENSTRING(*arg4)) ) {
         tmp = malloc(arg4->strlength+1); /* Make copy of argument */
         strncpy( tmp, arg4->strptr, arg4->strlength );
         *(tmp+arg4->strlength)='\0';
         p = strtok(tmp," ");
         while( p!=NULL ) {
            if( (stricmp(p,"MSG_OOB")==0) ||
                (stricmp(p,"OOB")==0)     ||
                (stricmp(p,"OUT_OF_BAND")==0) )
                 flags|=MSG_OOB;
            else if( (stricmp(p,"MSG_PEEK")==0) ||
                     (stricmp(p,"PEEK")==0) )
                 flags|=MSG_PEEK;
            else {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            p=strtok( NULL, " " );
            }
         free(tmp);
         }
      } /* end n>3 */

   buf = malloc(len);
   if( buf==NULL ) {
      soerr( r, errno );
      return(0);
      }

   addrl = sizeof(addr);
   cnt = recvfrom( s, buf, len, flags, (struct sockaddr *)&addr, &addrl );

   if( cnt<=0 ) {
      soerr( r, errno );
      free(buf);
      return(0);
      }
   if( cnt>240 ) {  /* Need to get a new RXSTRING for result */
      rc= DosAllocMem( &pMem, cnt+16, 
                       PAG_COMMIT|PAG_READ|PAG_WRITE );
      if( rc ) {
         soerr( r, ENOMEM );
         free(buf);
         return(0);
         }
      r->strptr = pMem;
      }

   p = r->strptr;
   p += sprintf( p, "0 %d ", cnt );
   if( addr.sin_family>=AF_MAX )
      addr.sin_family=AF_UNSPEC;
   strcat(p,fam[addr.sin_family]);
   p += strlen(p);
   if( addr.sin_family==AF_INET ) {
      p += sprintf( p, " %d %s ", ntohs(addr.sin_port),
                   inet_ntoa(addr.sin_addr) );
      } /* End INET */
   memcpy( p, buf, cnt );
   free(buf);
   p+=cnt;
   *p = '\0';
   r->strlength = p - r->strptr;
   return(0);
   } /* End sorecvfr */

/*-----------------------------------*/
/* RESOLVE Function (25)             */
/*-----------------------------------*/
INT soresolv( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   LONG   timeout=-1;
   INT    rc;
   PRXSTRING arg3;
   struct in_addr host_addr;
   struct hostent *pHE;

   if( !RXVALIDSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   /* Get 'timeout' if provided */
   if( n>2) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg3) ) {
         if( string2long(arg3,&timeout)==FALSE ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         if( timeout<=0 ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         }
      } /* end n>2 */

   p = r->strptr;
   /* Do we have an IP address or a host/domain name? */
   if( isalpha(*(a->strptr)) ) { /* Host name */
      pHE = gethostbyname( a->strptr );
      if( pHE==NULL ) {
         soerr( r, h_errno );
         return(0);
         }
      memcpy( &host_addr.s_addr, pHE->h_addr, sizeof host_addr.s_addr );
      p+= sprintf( p, "0 %s %s", inet_ntoa(host_addr), pHE->h_name );
      }
   else {  /* IP address */
      host_addr.s_addr = inet_addr(a->strptr);
      pHE = gethostbyaddr( (PCH)&host_addr, sizeof(struct in_addr) , AF_INET );
      if( pHE==NULL ) {
         soerr( r, h_errno );
         return(0);
         }
      p+= sprintf( p, "0 %s %s", a->strptr, pHE->h_name );
      }

   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End soresolv */

/*-----------------------------------*/
/* SELECT Function (26)              */
/*-----------------------------------*/
INT sosel( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   INT    rc;
   INT    nsock=0;
   PRXSTRING arg3;
   struct timeval tv, *ptv;
   fd_set rset,wset,xset;
   fd_set *prset,*pwset,*pxset,*pt;
   PCH    tmp;

   ptv = (struct timeval *)NULL; /* Default - no timeout */
   prset = (fd_set *)NULL;
   pwset = (fd_set *)NULL;
   pxset = (fd_set *)NULL;
   FD_ZERO(&rset);                /* Clear all fd sets    */
   FD_ZERO(&wset);
   FD_ZERO(&xset);

   /* get socket mask if present */
   pt = (fd_set *)NULL;
   if( n>1 ) {
      if( !(RXVALIDSTRING(*a)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( !(RXZEROLENSTRING(*a)) ) {
         tmp = malloc(a->strlength+1); /* Make copy of argument */
         strncpy( tmp, a->strptr, a->strlength );
         *(tmp+a->strlength)='\0';
         p = strtok(tmp," ");
         while( p!=NULL ) {
            if( (stricmp(p,"READ")==0 ) )           pt = (fd_set *)&rset;
            else if( (stricmp(p,"WRITE")==0 ) )     pt = (fd_set *)&wset;
            else if( (stricmp(p,"EXCEPTION")==0 ) ) pt = (fd_set *)&xset;
            else if( !isdigit(*p) ) {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            if( pt==(fd_set *)NULL ) {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            rc = getint(p,strlen(p));
            if( rc<0 || rc>FD_SETSIZE ) {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            FD_SET(rc,pt);
            p=strtok( NULL, " " );
            } /* end while */
         free(tmp);
         } /* end !RXZEROLENSTRING */
      } /* end n>1 */

   /* get timeout if present */
   if( n>2) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXNULLSTRING(*arg3) || RXZEROLENSTRING(*arg3) ) /* continue */ ;
      else { /* Parse Timeout */
         if( stricmp( arg3->strptr, "FOREVER" )==0 ) /* continue */ ; 
         else if( strnicmp( arg3->strptr, "SIGNAL", 6 )==0 ) { 
            soerr( r, errno ); /* Not supported */
            return(0);
            }
         else if( isdigit( *(arg3->strptr)) ) {
            /* get time */
            ptv = &tv;
            }
         else {
            soerr( r, errno );
            return(0);
            }
         }
      } /* end n>2 */

   rc = select( nsock, prset, pwset, pxset, ptv );

   if( rc<0 ) {  /* Error */
      soerr( r, errno );
      return(0);
      }
   /* NOTE: I have timeout returning RC=0 with  */
   /*       number of sockets=0, is this right? */
   if( rc==0 ) { /* Timeout */
      strcpy( r->strptr, "0 0" );
      r->strlength = strlen(r->strptr);
      return(0);
      }

   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sosel */

/*-----------------------------------*/
/* SEND Function (27)                */
/*-----------------------------------*/
INT sosend( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   PRXSTRING arg3,arg4;
   LONG   s;
   INT    flags=0;
   INT    rc;
   PCH    tmp;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( !(RXVALIDSTRING(*arg3)) || (RXZEROLENSTRING(*arg3)) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg4)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( !(RXZEROLENSTRING(*arg4)) ) {
         tmp = malloc(arg4->strlength+1); /* Make copy of argument */
         strncpy( tmp, arg4->strptr, arg4->strlength );
         *(tmp+arg4->strlength)='\0';
         p = strtok(tmp," ");
         while( p!=NULL ) {
            if( (stricmp(p,"MSG_OOB")==0) ||
                (stricmp(p,"OOB")==0)     ||
                (stricmp(p,"OUT_OF_BAND")==0) )
                 flags|=MSG_OOB;
            else if( (stricmp(p,"MSG_PEEK")==0) ||
                     (stricmp(p,"PEEK")==0) )
                 flags|=MSG_PEEK;
            else {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            p=strtok( NULL, " " );
            }
         free(tmp);
         }
      } /* end n>3 */

   rc = send( s, arg3->strptr, arg3->strlength, flags );

   if( rc<0 ) {
      soerr( r, errno );
      return(0);
      }

   p = r->strptr;
   p += sprintf( p, "0 %d", arg3->strlength );
   *p = '\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sosend */

/*-----------------------------------*/
/* SENDTO Function (28)              */
/*-----------------------------------*/
INT sosendto( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   PRXSTRING arg3,arg4,arg5;
   LONG   s;
   INT    flags=0;
   INT    rc;
   INT    cnt;
   PCH    tmp;
   struct sockaddr_in addr, *paddr;
   INT    addrl;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( !(RXVALIDSTRING(*arg3)) || (RXZEROLENSTRING(*arg3)) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( !(RXVALIDSTRING(*arg4)) ) {
         soerr( r, EINVALIDRXSOCKETCALL  );
         return(0);
         }
      if( !(RXZEROLENSTRING(*arg4)) ) {
         tmp = malloc(arg4->strlength+1); /* Make copy of argument */
         strncpy( tmp, arg4->strptr, arg4->strlength );
         *(tmp+arg4->strlength)='\0';
         p = strtok(tmp," ");
         while( p!=NULL ) {
            if( (stricmp(p,"MSG_OOB")==0) ||
                (stricmp(p,"OOB")==0)     ||
                (stricmp(p,"OUT_OF_BAND")==0) )
                 flags|=MSG_OOB;
            else if( (stricmp(p,"MSG_PEEK")==0) ||
                     (stricmp(p,"PEEK")==0) )
                 flags|=MSG_PEEK;
            else {
               soerr( r, EINVALIDRXSOCKETCALL );
               free(tmp);
               return(0);
               }
            p=strtok( NULL, " " );
            } /* end while */
         free(tmp);
         } /* end !RXZEROLENSTRING */
      } /* end n>3 */

   paddr = NULL;

   if( n>4 ) {
      arg5 = (PRXSTRING)( (PUCHAR)arg4 + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg5) && !RXZEROLENSTRING(*arg5) ) {
         paddr = &addr;
         addrl = sizeof(addr);
         if( parseaddress(arg5,paddr)==FALSE ) {
            soerr( r, EINVALIDRXSOCKETCALL  );
            return(0);
            }
         } /* end RXVALIDSTRING */
      } /* end n>4 */

   if( paddr==NULL ) {
      cnt = send( s, arg3->strptr, arg3->strlength, flags );
      if( cnt>=0 ) cnt=arg3->strlength;
      }
   else {
      cnt = sendto( s, arg3->strptr, arg3->strlength, flags, (struct sockaddr *)&addr, addrl );
      }

   if( cnt<=0 ) {
      soerr( r, errno );
      return(0);
      }

   p = r->strptr;
   p += sprintf( p, "0 %d", cnt );
   *p = '\0';
   r->strlength = p - r->strptr;
   return(0);
   } /* End sosendto */

/*-----------------------------------*/
/* SETSOCKOPT Function (29)          */
/*-----------------------------------*/
INT sosetsopt( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;
   LONG   s; 
   LONG   optval;
   INT    rc;
   INT    i,j;
   PSTAB  pst;
   PRXSTRING arg3,arg4,arg5;
   char   optbuf[80]; /* Max size buffer */
   INT    optlen;
   typedef INT  *POPTINT;
   typedef LONG *POPTLONG;
   typedef struct linger *PLINGER;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   /*----------------------*/
   /* Validate Level       */
   /*----------------------*/
   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg3) || RXZEROLENSTRING(*arg3) ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   if( stricmp( arg3->strptr, "SOL_SOCKET" )!=0 ) { 
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   /*----------------------*/
   /* Validate Option      */
   /*----------------------*/
   arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg4) || RXZEROLENSTRING(*arg4) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   for( i=0; i<OPT_MAX; i++ ) {
      if( stricmp( arg4->strptr, opt[i] )==0 ) break;
      }
   if( i>=OPT_MAX ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }
   i = optx[i];

   /*----------------------*/
   /* Validate Value       */
   /*----------------------*/
   arg5 = (PRXSTRING)( (PUCHAR)arg4 + sizeof(RXSTRING) );
   if( RXNULLSTRING(*arg5) || RXZEROLENSTRING(*arg5) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   if( stricmp( arg5->strptr, "ON" )==0 )       optval=1;
   else if( stricmp( arg5->strptr, "OFF" )==0 ) optval=0;
   else { 
         if( string2long(a,&optval)==FALSE ) {
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         if( optval!=0 ) optval=1;
         }

   /*----------------------*/
   /* Find Socket          */
   /*----------------------*/
   pst = ps;
   for( j=1; j<=nsock; j++ ) {
      if( pst->stab_snum==s ) break;
      pst = (PSTAB)( (PUCHAR)pst + sizeof(STAB) );
      }
   if( j>nsock ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   /*----------------------*/
   /* ASCII and EBCDIC are */
   /* Internal options     */
   /* Translation table is */
   /* always "STANDARD"    */
   /*----------------------*/
   switch( i ) {
      case SO_ASCII:
         pst->stab_satt = optval? 0:1;
         p = r->strptr;
         strcpy( p, "0" ); 
         p += strlen(p);
         r->strlength = strlen(r->strptr);
         return(0);
      case SO_EBCDIC:
         pst->stab_satt = optval? 1:0;
         p = r->strptr;
         strcpy( p, "0" ); 
         p += strlen(p);
         r->strlength = strlen(r->strptr);
         return(0);
      default: break; 
      } /* end switch */
   switch( i ) {
      case SO_BROADCAST:
      case SO_DEBUG:
      case SO_DONTROUTE:
      case SO_KEEPALIVE:
      case SO_OOBINLINE:
      case SO_REUSEADDR:
         p+= sprintf( p, "%s",optbuf[0]? "On": "Off");
         break;
      case SO_ERROR:
      case SO_TYPE:
      case SO_USELOOPBACK:
      case SO_RCVLOWAT:
      case SO_RCVTIMEO:
      case SO_SNDLOWAT:
      case SO_SNDTIMEO:
         p+= sprintf( p, "%d", (POPTINT)&optbuf);
         break;
      case SO_RCVBUF:
      case SO_SNDBUF:
         p+= sprintf( p, "%ld", (POPTLONG)&optbuf);
         break;
      case SO_LINGER:
         p+= sprintf( p, "%s %d", ((PLINGER)&optbuf)->l_onoff? "Off": "On",
                      ((PLINGER)&optbuf)->l_linger);
         break;
      } /* end switch */


   optlen = sizeof optbuf;

   rc = setsockopt( s, SOL_SOCKET, i, (PUCHAR)&optbuf, optlen );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   strcpy( r->strptr, "0" ); 
   r->strlength = strlen(r->strptr);
   return(0);

   } /* end sosetsopt */

/*-----------------------------------*/
/* SHUTDOWN Function (30)            */
/*-----------------------------------*/
INT soshut( ULONG n, PRXSTRING a, PRXSTRING r ) {
   LONG   so;
   INT    rc;
   INT    how=2; /* Default='BOTH' */
   PRXSTRING arg3;

   if( string2long(a,&so)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg3) ) { 
         if ( stricmp(arg3->strptr,"BOTH")==0 ) 
            how=2;
         else if( (stricmp(arg3->strptr,"FROM")==0)      ||
                  (stricmp(arg3->strptr,"RECEIVE")==0)   ||
                  (stricmp(arg3->strptr,"RECEIVING")==0) ||
                  (stricmp(arg3->strptr,"READ")==0) ||
                  (stricmp(arg3->strptr,"READING")==0) )
            how=0;
         else if( (stricmp(arg3->strptr,"TO")==0)        ||
                  (stricmp(arg3->strptr,"SEND")==0)      ||
                  (stricmp(arg3->strptr,"SENDING")==0)   ||
                  (stricmp(arg3->strptr,"WRITE")==0)     ||
                  (stricmp(arg3->strptr,"WRITING")==0) )
            how=1;
         else {
            soerr( r, EINVALIDRXSOCKETCALL  );
            return(0);
            }
         }
      } /* end n>2 */

   rc = shutdown( so, how );

   if( rc ) {
      soerr( r, errno );
      return(0);
      }

   strcpy( r->strptr, "0" ); 
   r->strlength = strlen(r->strptr);
   return(0);

   } /* end soshut */

/*-----------------------------------*/
/* SOCKET Function (31)              */
/*-----------------------------------*/
INT sosocket( ULONG n, PRXSTRING a, PRXSTRING r ) {
   INT    domain   = AF_INET;     /* Defaults */
   INT    type     = SOCK_STREAM;
   INT    protocol = IPPROTO_TCP;
   INT    so;
   PCH    p;
   LONG   i;
   PRXSTRING arg3,arg4;
   PSTAB  pst;

   /* Look for 'AF_INET' or '2' */
   if( n>1 ) {
      if( RXVALIDSTRING(*a) ) {
         if( stricmp(a->strptr,"AF_INET")!=0 ) {
   	    if( string2long(a,&i)==FALSE ) {
	      soerr( r, EINVALIDRXSOCKETCALL  );
	      return(0);
              }
   	    if( i!=2 ) {
	      soerr( r, EINVALIDRXSOCKETCALL  );
	      return(0);
              }
            } /* End stricmp */
         } /* End RXVALIDSTRING */
      } /* End n>1 */

   /* Check 'type' argument */
   if( n>2 ) {
      arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg3) ) {
         for( i=0; i<TYPE_MAX; i++ ) {
            if( stricmp( typ[i], arg3->strptr )!=0 ) break;
            }
         if( i>=TYPE_MAX ) {      /* Invalid type       */
            soerr( r, EINVALIDRXSOCKETCALL );
            return(0);
            }
         type = i;
         if( type==TYPE_MAX-1 )  type--;; 
         type = i;
         /* Set up defaults for prptocol */
         if( type==SOCK_RAW )    protocol=0;
         if( type==SOCK_STREAM ) protocol=IPPROTO_TCP;
         if( type==SOCK_DGRAM )  protocol=IPPROTO_UDP;
         } /* End RXVALIDSTRING */
      } /* End n>2 */

   if( n>3 ) {
      arg4 = (PRXSTRING)( (PUCHAR)arg3 + sizeof(RXSTRING) );
      if( RXVALIDSTRING(*arg4) ) {
         if( arg4->strlength==11 ) {
            if ( stricmp(arg4->strptr,"IPPROTO_TCP")==0 ) 
               protocol=IPPROTO_TCP;
            else if( stricmp(arg4->strptr,"IPPROTO_UDP")==0 ) 
               protocol=IPPROTO_UDP;
            else if( stricmp(arg4->strptr,"IPPROTO_RAW")==0 ) 
               protocol=IPPROTO_RAW;
            else protocol=-1; /* (error) */
            } /* End length=11 */
         else if( arg4->strlength==12 ) {
            if ( stricmp(arg4->strptr,"IPPROTO_ICMP")==0 ) 
               protocol=IPPROTO_ICMP;
            else protocol=-1; /* (error) */
            } /* End length=12 */
         else if( arg4->strlength==3 ) {
            if ( stricmp(arg4->strptr,"RAW")==0 ) 
               protocol=IPPROTO_RAW;
            else protocol=-1; /* (error) */
            } /* End length=3 */
         else protocol=-1;
         if( protocol==-1 ) {
            soerr( r, EINVALIDRXSOCKETCALL  );
            return(0);
            }
         } /* End RXVALIDSTRING */
      } /* End n>3 */

   if( nsock>=maxsockets ) {
      soerr( r, EMAXSOCKETSREACHED );
      return(0);
      }

   so = socket( domain, type, protocol );
   pst = (PSTAB)( (PUCHAR)ps + (sizeof(STAB)*(nsock)) );
   nsock++;
   pst->stab_snum = so;
   pst->stab_satt = 0;
   
   p = r->strptr; 
   if( so<0 ) {
      p += sprintf( p, "%d",errno );
      strcpy(p,strerror(errno));
      }
   else {
      p += sprintf( p, "0 %d",so );
      *p='\0';
      }
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sosocket */

/*-----------------------------------*/
/* SOCKETSET Function (32)           */
/*-----------------------------------*/
INT soset( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;

   if( maxsockets<=0 ) {
      soerr( r, ESUBTASKNOTACTIVE );
      return(0);
      }

   if( n>1 ) {
      if( RXVALIDSTRING(*a) ) {
         if( strcmp(Subtaskid,a->strptr)!=0 ) {
            soerr( r, ESUBTASKNOTACTIVE );
            return(0);
            }
         } /* Otherwise use current subtask */
     } /* end n>1 */

   p = r->strptr; 
   p += sprintf( p, "0 %s", Subtaskid );
   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* end soset */

/*-----------------------------------*/
/* SOCKETSETLIST Function (33)       */
/*-----------------------------------*/
INT sossl( ULONG n, PRXSTRING a, PRXSTRING r ) {

   if( maxsockets<=0 ) {
      soerr( r, ESUBTASKNOTACTIVE );
      return(0);
     }

   sprintf( r->strptr, "0 %s", Subtaskid );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* end sossl */

/*-----------------------------------*/
/* SOCKETSETSTATUS Function (34)     */
/*-----------------------------------*/
INT sosss( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;

   if( maxsockets<=0 ) {
      soerr( r, ESUBTASKNOTACTIVE );
      return(0);

   if( n>1 )
      if( RXVALIDSTRING(*a) ) {
         if( strcmp(Subtaskid,a->strptr)!=0 ) {
           soerr( r, ESUBTASKNOTACTIVE );
           return(0);
           }
        } /* Otherwise use current subtask */
     } /* end n>1 */

   /* Always 'Connected'? */
   p = r->strptr; 
   p += sprintf( p, "0 %s Connected Free %d Used %d", Subtaskid,
                    maxsockets-nsock, nsock );
   *p='\0';
   r->strlength = strlen(r->strptr);
   return(0);
   } /* end sosss */

/*-----------------------------------*/
/* TAKESOCKET (35) Unimplemented     */
/*-----------------------------------*/

/*-----------------------------------*/
/* TERMINATE Function (36)           */
/*-----------------------------------*/
INT soterm( ULONG n, PRXSTRING a, PRXSTRING r ) {

   if( n>1 ) {
      if( RXVALIDSTRING(*a) ) {
        if( RXNULLSTRING(*a) || RXZEROLENSTRING(*a) ||
            (a->strlength>8) ) {
           soerr( r, EINVALIDRXSOCKETCALL  );
           return(0);
          }
        if( strcmp(Subtaskid,a->strptr)!=0 ) {
           soerr( r, ESUBTASKNOTACTIVE );
           return(0);
          }
        } /* Otherwise terminate current subtask */
     } /* end n>1 */

   CloseAllSockets();

   *(Subtaskid)='\0';
   sprintf( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* end soterm */

/*-----------------------------------*/
/* TRACE (37) Unimplemented          */
/*-----------------------------------*/

/*-----------------------------------*/
/* TRANSLATE Function (38)           */
/*-----------------------------------*/
INT sotrans( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH       p;
   PRXSTRING arg3;
   INT       cnt;
   INT       rc;
   PVOID    pMem;

   INT       how=0;

   if( !RXVALIDSTRING(*a) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( !RXVALIDSTRING(*arg3) ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   /*----------------------*/
   /* Validate "HOW"       */
   /*----------------------*/
   if ( (stricmp(arg3->strptr,"TO_ASCII")==0) || 
        (stricmp(arg3->strptr,"ASCII")==0) ) {
      cnt = a->strlength;
      how=1; /* EBCDIC->ASCII */ 
      }
   else if ( (stricmp(arg3->strptr,"TO_EBCDIC")==0) || 
             (stricmp(arg3->strptr,"EBCDIC")==0) )  {
      cnt = a->strlength;
      how=2; /* ASCII->EBCDIC */
      }
   else if ( (stricmp(arg3->strptr,"TO_IP_ADDRESS")==0) || 
             (stricmp(arg3->strptr,"TO_IPADDRESS")==0)  ||
             (stricmp(arg3->strptr,"IPADDRESS")==0) ) {
      if( a->strlength==4 ) {
        cnt=-1;
        how=3; /* HEX->Dotted_Decimal */
        }
      else {
        cnt=4;
        how=4; /* Dotted_Decimal->HEX */
        }
      }
   else if ( (stricmp(arg3->strptr,"TO_SOCKADDR_IN")==0) || 
             (stricmp(arg3->strptr,"SOCKADDR_IN")==0) ) {
      if( a->strlength==16 ) {
        cnt=-1;
        how=5; /* HEX->Character */
        }
      else {
        cnt=16;
        how=6; /* Character->HEX */
        }  
      }
   if( !how ) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   p = r->strptr;
   switch( how ) { 
      case 1:
         break;
      case 2:
         break;
      case 3:
         break;
      case 4:
         break;
      case 5:
         break;
      case 6:
         break;
      } /* end switch */

   if( cnt>240 ) {  /* Need to get a new RXSTRING for result */
      rc= DosAllocMem( &pMem, cnt+16, 
                       PAG_COMMIT|PAG_READ|PAG_WRITE );
      if( rc ) {
         soerr( r, ENOMEM );
         return(0);
         }
      r->strptr = pMem;
      }

   r->strlength = strlen(r->strptr);
   return(0);
   } /* end sotrans */


/*-----------------------------------*/
/* VERSION Function (39)             */
/*-----------------------------------*/
INT sovers( ULONG n, PRXSTRING a, PRXSTRING r ) {
   PCH    p;

   p = r->strptr; 
   p += sprintf( p, "0 %s", VERSION );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sovers */

/*-----------------------------------*/
/* WRITE Function (40)               */
/*-----------------------------------*/
INT sowrite( ULONG n, PRXSTRING a, PRXSTRING r ) {
   INT    rc;
   LONG   s;
   PRXSTRING arg3;

   if( string2long(a,&s)==FALSE ) {
      soerr( r, EINVALIDRXSOCKETCALL );
      return(0);
      }

   arg3 = (PRXSTRING)( (PUCHAR)a + sizeof(RXSTRING) );
   if( (RXVALIDSTRING(*arg3)) && !(RXZEROLENSTRING(*arg3)) ) 
      /* continue */ ;
   else {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }
   if( arg3->strlength>INT_MAX) {
      soerr( r, EINVALIDRXSOCKETCALL  );
      return(0);
      }

   rc = write( s, arg3->strptr, arg3->strlength  );

   if( rc<0 ) {
      soerr( r, errno );
      return(0);
      }
   strcpy( r->strptr, "0" );
   r->strlength = strlen(r->strptr);
   return(0);
   } /* End sowrite */

/*-----------------------------------*/
/* Unimplemented Functions:          */
/*    GiveSocket                     */
/*    TakeSocket                     */
/*    Trace                          */
/*-----------------------------------*/
INT sounimpl( ULONG n, PRXSTRING a, PRXSTRING r ) {
   soerr( r, EINVALIDRXSOCKETCALL );
   }

/*-----------------------------------*/
/* DLL Initialization/Termination    */
/* (Modified version of default)     */
/* Most likely never called for Term */
/*-----------------------------------*/
  INT _CRT_init (void);
  VOID _CRT_term (void);
  VOID __ctordtorInit (void);
  VOID __ctordtorTerm (void);
  
ULONG _DLL_InitTerm ( ULONG mod_handle, ULONG flag )
  {
    switch (flag)
      {
      case 0:
        if (_CRT_init () != 0)
          return 0;
        __ctordtorInit ();
       maxsockets = 0;
       *(Subtaskid) = '\0';
       return 1;
      case 1:
       *(Subtaskid) = '\0';
       CloseAllSockets();
        __ctordtorTerm ();
        /* _CRT_term (); <== Not called */
        return 1;
      default:
        return 0;
      }
    return 1;
  }
/*-----------------------------------*/
/* Close all open sockets at DLL     */
/* termination                       */
/*-----------------------------------*/
INT CloseAllSockets( ) {
   INT i;
   PSTAB p;
   INT rc;
   if( maxsockets==0 ) return; /*000807*/
   p = ps;
   for( i=1; i<=nsock; i++ ) {
      rc = close( p->stab_snum );
      p = (PSTAB)( (PUCHAR)p + sizeof(STAB) );
      }
   free(ps);
   maxsockets=0;
   nsock=0;
   }

