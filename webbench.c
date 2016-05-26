/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 * 
 */ 
#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired=0;
int speed=0;
int failed=0;
int bytes=0;
/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;
int clients=1;
int force=0;
int force_reload=0;//缓存
int proxyport=80;
char *proxyhost=NULL;
int benchtime=30;
/* internal */
int mypipe[2];
char host[MAXHOSTNAMELEN];
#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];

static const struct option long_options[]=
{
 {"force",no_argument,&force,1},
 {"reload",no_argument,&force_reload,1},
 {"time",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"version",no_argument,NULL,'V'},
 {"proxy",required_argument,NULL,'p'},
 {"clients",required_argument,NULL,'c'},
 {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

static void alarm_handler(int signal)
{
   timerexpired=1;
}	

static void usage(void)
{
   fprintf(stderr,
	"webbench [option]... URL\n"
	"  -f|--force               Don't wait for reply from server.\n"
	"  -r|--reload              Send reload request - Pragma: no-cache.\n"
	"  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
	"  -p|--proxy <server:port> Use proxy server for request.\n"
	"  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
	"  -9|--http09              Use HTTP/0.9 style requests.\n"
	"  -1|--http10              Use HTTP/1.0 protocol.\n"
	"  -2|--http11              Use HTTP/1.1 protocol.\n"
	"  --get                    Use GET request method.\n"
	"  --head                   Use HEAD request method.\n"
	"  --options                Use OPTIONS request method.\n"
	"  --trace                  Use TRACE request method.\n"
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
};
int main(int argc, char *argv[])
{
 int opt=0;
 int options_index=0;
 char *tmp=NULL;

 if(argc==1)
 {
	  usage();
          return 2;
 } 

 
/*
getopt_long用来处理命令行参数, 前两个参数就是main函数传过来的argc,argv。第三个参数
optstring是一个字符串，表示可以接受的参数。例如，"a:b:cd"，表示可以接受的参数是a,b,c,d，其中，a和b参数带冒号，表示后面跟有更多的参数值。(例如：-a host -b name)

比如这个代码里，表示webbench命令可以支持-9,-f -t等命令，其中-p, -c参数后面必须带有参数值，像-p 9000这样。
*/

 while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF )
 {
  switch(opt)
  {
   case  0 : break;
   case 'f': force=1;break;
   case 'r': force_reload=1;break; 
   case '9': http10=0;break;
   case '1': http10=1;break;
   case '2': http10=2;break;
   case 'V': printf(PROGRAM_VERSION"\n");exit(0);
   case 't': benchtime=atoi(optarg);break;	     //optarg表示命令后的参数，例如-c 100，optarg为100。
   case 'p': 
	     /* proxy server parsing server:port */
		 /*
		 找一个字符c在另一个字符串str中末次出现的位置（也就是从str的右侧开始查找字符c首次出现的位置），
		 并返回从字符串中的这个位置起，一直到字符串结束的所有字符。如果未能找到指定字符，那么函数将返回NULL。


		 如果一个选项带参数，比如-p 192.168.0.1:9800, optarg会指向它的参数，也就是"192.168.0.1:9800"
		 那么这种情况下，proxyhost就是192.168.0.1, proxyport就是9800

		 */
	     tmp=strrchr(optarg,':');
	     proxyhost=optarg;
	     if(tmp==NULL)
	     {
		     break;
	     }
	     if(tmp==optarg)
	     {
		     fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
		     return 2;
	     }
	     if(tmp==optarg+strlen(optarg)-1)
	     {
	     	printf("tmp: %s\n", tmp);
		     fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
		     return 2;
	     }
	     *tmp='\0';
	     proxyport=atoi(tmp+1);break;
   case ':':
   case 'h':
   case '?': usage();return 2;break;
   case 'c': clients=atoi(optarg);break;
  }
 }


 printf("optind: %d\n", optind);
 printf("argc: %d\n", argc);
 printf("argv: %s\n\n", argv);


 
 /*
 这一句初看有点难理解，其实是这样的:
 
 getopt_long先将argv中非option的参数移到argv后端，这就可以让option变成位置无关的，optind初值为1，getopt会渐进遍历argv，
 每次调用后都会让optind指向下一个option在argv中索引，每次optind移动多少取决于optstring：
 1. 遇到"x"，选项不带参数，optind += 1
 2. 遇到“x:”，带参数的选项，optarg = argv[optind + 1], optind += 2
 如果一切顺利，最后optind应该指向第一个非option参数，如果optind >= argc，说明没有已经没有参数了

 如果带url，比如这样的，
 webbench -c 30 http://www.baidu.com/
 那么，optind=3, argc=4，然后optind指向就是url的索引.
 */
 if(optind==argc) {
                      fprintf(stderr,"webbench: Missing URL!\n");
		      usage();
		      return 2;
                    }

 if(clients==0) clients=1;
 if(benchtime==0) benchtime=60;
 /* Copyright */
 fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
	 "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
	 );

 build_request(argv[optind]);


	printf("\n after build_request\n ");

	printf("proxyhost: %s\n", proxyhost);
	printf("proxyport: %d\n", proxyport);
	printf("request: %s\n", request);
	printf("host: %s\n", host);

 
 /* print bench info */
 printf("\nBenchmarking: ");
 switch(method)
 {
	 case METHOD_GET:
	 default:
		 printf("GET");break;
	 case METHOD_OPTIONS:
		 printf("OPTIONS");break;
	 case METHOD_HEAD:
		 printf("HEAD");break;
	 case METHOD_TRACE:
		 printf("TRACE");break;
 }
 printf(" %s",argv[optind]);
 switch(http10)
 {
	 case 0: printf(" (using HTTP/0.9)");break;
	 case 1: printf(" (using HTTP/1.0)");break;
	 case 2: printf(" (using HTTP/1.1)");break;
 }
 printf("\n");

 
 if(clients==1) printf("1 client");
 else
   printf("%d clients",clients);

 printf(", running %d sec", benchtime);
 if(force) printf(", early socket close");
 if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
 if(force_reload) printf(", forcing reload");
 printf(".\n");

 
 return bench();
}


//创建http连接请求
void build_request(const char *url)
{
  char tmp[10] = {0};
  int i;

  printf("url:%s\n", url);

  bzero(host,MAXHOSTNAMELEN);
  bzero(request,REQUEST_SIZE);

  //指当使用了缓存和代理，最低要使用http1.0协议。0.9版本，没有代理这个概念，也没有缓存概念??
  //force_reload为1表示没有缓存
  if(force_reload && proxyhost!=NULL && http10<1) http10=1;
  if(method==METHOD_HEAD && http10<1) http10=1;
  if(method==METHOD_OPTIONS && http10<2) http10=2;
  if(method==METHOD_TRACE && http10<2) http10=2;

  printf("method:%d\n", method);

  switch(method)
  {
	  default:
	  case METHOD_GET: strcpy(request,"GET");break;
	  case METHOD_HEAD: strcpy(request,"HEAD");break;
	  case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
	  case METHOD_TRACE: strcpy(request,"TRACE");break;
  }
		  
  strcat(request," ");

  printf("request:%s\n", request);

	//strstr(str1,str2) 函数用于判断字符串str2是否是str1的子串
  if(NULL==strstr(url,"://"))
  {
	  fprintf(stderr, "\n%s: is not a valid URL.\n",url);
	  exit(2);
  }
  if(strlen(url)>1500)
  {
         fprintf(stderr,"URL is too long.\n");
	 exit(2);
  }


  
  if(proxyhost==NULL)// no -p
  {
	   if (0!=strncasecmp("http://",url,7)) //去掉这个判断可以支持https
	   { fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
            exit(2);
       }
  }
	   
  /* protocol/host delimiter */
  i=strstr(url,"://")-url+3;
   printf("%d\n",i);

  if(strchr(url+i,'/')==NULL) {
                                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                                exit(2);
                              }


  /*
  如果参数中没有指明端口,则用80,
  80端口是为HTTP（HyperText Transport Protocol)即超文本传输协议开放的，此为上网冲浪使用次数最多的协议，
  主要用于WWW（World Wide Web）即万维网传输信息的协议。可以通过HTTP地址（即常说的“网址”）加“:80”来访问网站，
  因为浏览网页服务默认的端口号都是80，因此只需输入网址即可，不用输入“:80”了。

  当然也可以指明端口，比如这样:

  webbench -c 30 http://www.baidu.com:9800/
  */
  if(proxyhost==NULL)
  {
	   /* get port from hostname */
	   if(index(url+i,':')!=NULL &&
	      index(url+i,':')<index(url+i,'/'))
	   {
		   strncpy(host,url+i,strchr(url+i,':')-url-i);
		   bzero(tmp,10);
		   strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
		    printf("tmp=%s\n",tmp); 
		   proxyport=atoi(tmp);
		   if(proxyport==0) proxyport=80;
	   } else
	   {
	   	//strcspn返回第一个出现的字符在s1中的下标值，亦即在s1中出现而s2中没有出现的子串的长度
	     strncpy(host,url+i,strcspn(url+i,"/"));
	   }

   
   	 	printf("Host=%s\n",host);
   		strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
		printf("request=%s\n",request);

		
  } else
  {
    printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
   strcat(request,url);
   printf("request=%s\n",request);
  }

  
  if(http10==1)
	  strcat(request," HTTP/1.0");
  else if (http10==2)
	  strcat(request," HTTP/1.1");
  strcat(request,"\r\n");
  if(http10>0)
	  strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");

  
  if(proxyhost==NULL && http10>0)
  {
	  strcat(request,"Host: ");
	  strcat(request,host);
	  strcat(request,"\r\n");
  }
  if(force_reload && proxyhost!=NULL)
  {
	  strcat(request,"Pragma: no-cache\r\n");
  }
  if(http10>1)
	  strcat(request,"Connection: close\r\n");
  /* add empty line at end */
  if(http10>0) strcat(request,"\r\n"); 
  // printf("Req=%s\n",request);
}

/* vraci system rc error kod */
static int bench(void)
{
  int i,j,k;	
  pid_t pid=0;
  FILE *f;


  printf("bench.....\n");

  /* check avaibility of target server */
  i=Socket(proxyhost==NULL?host:proxyhost,proxyport);
  if(i<0) 
  { 
	   fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
           return 1;
  }
  close(i);

  
  /* create pipe */
  /*
  	定义函数： int pipe(int filedes[2]);
 	3) 函数说明： pipe()会建立管道，并将文件描述词由参数filedes数组返回。
               filedes[0]为管道里的读取端
               filedes[1]则为管道的写入端
  */
  if(pipe(mypipe))
  {
	  perror("pipe failed.");
	  return 3;
  }

  /* not needed, since we have alarm() in childrens */
  /* wait 4 next system clock tick */
  /*
  cas=time(NULL);
  while(time(NULL)==cas)
        sched_yield();
  */

  /* fork childs */

  
  for(i=0;i<clients;i++)
  {
	   pid=fork();
	   if(pid <= (pid_t) 0)
	   {
		   /* child process or error*/
		   ////这段代码目的是生成子进程，当fork后有2个进程执行。当fork出错或者fork后执行到子进程，就sleep(1)，让出CPU，让父进程占用CPU继续执行for循环，fork生成子进程。
	           sleep(1); /* make childs faster */
		   break;
	   }
  }

  if( pid< (pid_t) 0)
  {
          fprintf(stderr,"problems forking worker no. %d\n",i);
	  perror("fork failed.");
	  return 3;
  }

  if(pid== (pid_t) 0)
  {
    /* I am a child */
	//子进程向管道写数据，发送结果
    if(proxyhost==NULL)
      benchcore(host,proxyport,request);
         else
      benchcore(proxyhost,proxyport,request);

         /* write results to pipe */
	 f=fdopen(mypipe[1],"w");
	 if(f==NULL)
	 {
		 perror("open pipe for writing failed.");
		 return 3;
	 }
	 /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
	 fprintf(f,"%d %d %d\n",speed,failed,bytes);
	 fclose(f);
	 return 0;
  } else
  {
  	/* I am the father */
	//父进程从管道读数据，显示结果
	  f=fdopen(mypipe[0],"r");
	  if(f==NULL) 
	  {
		  perror("open pipe for reading failed.");
		  return 3;
	  }

	  
	  setvbuf(f,NULL,_IONBF,0);
	  speed=0;
          failed=0;
          bytes=0;

	  while(1)
	  {
		  pid=fscanf(f,"%d %d %d",&i,&j,&k);
		  if(pid<2)
                  {
                       fprintf(stderr,"Some of our childrens died.\n");
                       break;
                  }
		  speed+=i;
		  failed+=j;
		  bytes+=k;
		  /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
		  if(--clients==0) break;
	  }
	  fclose(f);

  printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
		  (int)((speed+failed)/(benchtime/60.0f)),
		  (int)(bytes/(float)benchtime),
		  speed,
		  failed);
  }
  return i;
}

void benchcore(const char *host,const int port,const char *req)
{
 int rlen;
 char buf[1500] = {0};
 int s,i;


 /*

sigaction函数用于改变进程接收到特定信号后的行为。该函数的第一个参数为信号的值，SIGALRM 14 A 由alarm(2)发出的信号

第二个参数是指向结构sigaction的一个实例的指针，在结构sigaction的实例中，指定了对特定信号的处理，
第三个参数oldact指向的对象用来保存返回的原来对相应信号的处理，可指定oldact为NULL。

系统调用alarm安排内核为调用进程在指定的seconds秒后发出一个SIGALRM的信号。如果指定的参数seconds为0，
则不再发送 SIGALRM信号。后一次设定将取消前一次的设定。该调用返回值为上次定时调用到发送之间剩余的时间，或者因为没有前一次定时调用而返回0。

注意，在使用时，alarm只设定为发送一次信号，如果要多次发送，就要多次使用alarm调用。

 */
 struct sigaction sa;

 /* setup alarm signal handler */
 sa.sa_handler=alarm_handler;
 sa.sa_flags=0;
 if(sigaction(SIGALRM,&sa,NULL))
    exit(3);

 //这里相当于设置一个benchtime时间的闹钟，限定socket访问的时间
 alarm(benchtime);

 rlen=strlen(req);
 nexttry:while(1)
 {
    if(timerexpired)
    {
       if(failed>0)
       {
          /* fprintf(stderr,"Correcting failed by signal\n"); */
          failed--;
       }
       return;
    }
    s=Socket(host,port);                          
    if(s<0) { failed++;continue;} 

	//发送http请求报文
    if(rlen!=write(s,req,rlen)) {failed++;close(s);continue;}

	
    if(http10==0) 
	    if(shutdown(s,1)) { failed++;close(s);continue;}
		
    if(force==0) 
    {
            /* read all available data from socket */
	    while(1)
	    {
              if(timerexpired) break; 
	      i=read(s,buf,1500);
              /* fprintf(stderr,"%d\n",i); */
	      if(i<0) 
              { 
                 failed++;
                 close(s);
                 goto nexttry;
              }
	       else
		       if(i==0) break;
		       else
			       bytes+=i;
	    }
    }
    if(close(s)) {failed++;continue;}
    speed++;
 }
}
