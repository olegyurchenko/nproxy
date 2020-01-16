/*----------------------------------------------------------------------------*/
/*"$Id$"*/
/**
* Proxy server.
*
* (C) T&T, Kiev, Ukraine 2005.
* start 25.01.06 09:34:27
* @pkgdoc proxy
* @author Oleg Yurchenko
* @version $Revision$
*/
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#ifdef WIN32
  #include <windows.h>
  #include <winsock2.h>
//  #include <io.h>
  typedef HANDLE pthread_mutex_t;
  typedef HANDLE pthread_t;
  typedef HANDLE sem_t;
  #define pthread_mutex_lock(h) WaitForSingleObject(*(h),INFINITE)
  #define pthread_mutex_unlock(h) ReleaseMutex(*(h))
  #define usleep(n) Sleep(n)
  typedef int socklen_t;
#endif

#ifdef UNIX
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <sys/wait.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <errno.h>
  #include <netdb.h>
  typedef int SOCKET;
  #include <pthread.h>
  #include <semaphore.h>
  #define closesocket(sd) close(sd)
#endif

/*----------------------------------------------------------------------------*/
#define PRODUCT "Proxy server"
#define VERSION "0.0.1"
#define COPYRIGHT "(C) T&T 2008"
#define BUILD __DATE__
/*----------------------------------------------------------------------------*/

#ifdef WIN32
static void sock_init()
{
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  wVersionRequested = MAKEWORD( 2, 0 );

  err = WSAStartup(wVersionRequested, &wsaData);
}
#else
static void sock_init(){}
#endif /*WIN32*/
static int resolve(const char *hostname, struct in_addr *addr);
/*----------------------------------------------------------------------------*/
#define BUFFER_SIZE (16 * 1024)
#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif
/*----------------------------------------------------------------------------*/
static int listen_port = 30101, connect_port = 30102;
static char *host = "localhost";
static int terminated = 0, verbose = 0, spy=0;
void* client_thread_fn(void *arg);
static pthread_mutex_t clients_mutex;
static struct sockaddr_in remote;
/*----------------------------------------------------------------------------*/
typedef struct
{
    struct sockaddr_in addr;
    SOCKET sd;
} CLIENT_DATA;
/*----------------------------------------------------------------------------*/
void help(char *argv0)
{
  printf("Use %s options, where options are:\n"
    "-h or --help                              This help\n"
    "-l <port> or --listen-port <port>         Set listen port (default 30101)\n"
    "-c <port> or --connect-port <port>        Set connect port (default 30102)\n"
    "-H <host> or --host <host>                Set connect host (default localhost)\n"
    "-V or --version                           Print version and exit\n"
    "-v or --verbose                           Set verbose mode\n"
    "-s or --spy                               Set spy mode\n"
    , argv0);
}
/*----------------------------------------------------------------------------*/
void version()
{
  printf("%s V%s (build %s)\n%s\n", PRODUCT, VERSION, BUILD, COPYRIGHT);
}
//---------------------------------------------------------------------------
int main(int argc, char **argv)
{
  struct sockaddr_in srvr;// = {AF_INET, 7000, INADDR_ANY};
  int opt = 1, i;
  SOCKET sd;

  /*parse args*/
  for(i = 1; i < argc; i++)
  {
    if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
    {
      help(argv[0]);
      return 0;
    }
    else
    if(!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version"))
    {
      version();
      return 0;
    }
    else
    if( (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--listen-port")) && i < argc - 1)
    {
      listen_port = atoi(argv[++i]);
    }
    else
    if( (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--connect-port")) && i < argc - 1)
    {
      connect_port = atoi(argv[++i]);
    }
    else
    if( (!strcmp(argv[i], "-H") || !strcmp(argv[i], "--host")) && i < argc - 1)
    {
      host = argv[++i];
    }
    else
    if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
    {
      verbose = 1;
    }
    else
    if(!strcmp(argv[i], "-s") || !strcmp(argv[i], "--spy"))
    {
      spy = 1;
    }
    else
    {
      fprintf(stderr, "Invalid option '%s'. Use '%s -h' for help\n", argv[i], argv[0]);
      return 100;
    }
  }

  if(argc == 1)
    verbose = 1;


  sock_init();

  #ifdef WIN32
  clients_mutex = CreateMutex(NULL, 0, NULL);
  #else
  pthread_mutex_init(&clients_mutex, NULL);
  #endif

  srvr.sin_family = AF_INET;
  srvr.sin_addr.s_addr = INADDR_ANY;
  srvr.sin_port = htons(listen_port);

  remote.sin_family = AF_INET;
  if(!resolve(host, &remote.sin_addr))
  {
    fprintf(stderr, "Unknown host '%s'\n", host);
    return 1;
  }
  remote.sin_port = htons(connect_port);

  if(verbose)
  {
    version();
    printf("Listen port: %u\n", ntohs(srvr.sin_port));
    printf("Host to connect: %s:%u\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));


  }

  if( (sd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
  {
    fprintf(stderr, "Socket error\n");
    return 1;
  }

  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR , (char *)&opt, sizeof(opt));

  if( bind(sd, (struct sockaddr *)&srvr, sizeof(srvr)) == -1)
  {
    fprintf(stderr, "Bind error\n");
    closesocket(sd);
    return 2;
  }

  if(listen(sd, 0) == -1)
  {
    fprintf(stderr, "Listen error\n");
    closesocket(sd);
    return 3;
  }

  while(!terminated)
  {
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);
    SOCKET client;
    CLIENT_DATA *data;

    client = accept(sd, (struct sockaddr *)&addr, &size);
    if ( (int)client <= 0 )
    {
      fprintf(stderr, "Problems with accepting a connection\n");
      terminated = 4;
      closesocket(client);
      break;
    }

    if(verbose)
      printf("Accept connection with %s:%u\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    data = (CLIENT_DATA *)malloc(sizeof(CLIENT_DATA));
    if(data == NULL)
    {
      fprintf(stderr, "Malloc error\n");
      terminated = 6;
      closesocket(client);
      break;
    }
    data->addr = addr;
    data->sd = client;

#ifdef WIN32
    {
      DWORD id;
      HANDLE hClientThread;
      hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) client_thread_fn, (void *)data, 0, &id);
      if(hClientThread == INVALID_HANDLE_VALUE)
      {
        fprintf(stderr, "Thread creation error\n");
        terminated = 5;
        closesocket(client);
        break;
      }
    }
#else
    {
      pthread_t hClientThread;
      if ( pthread_create(&hClientThread, NULL, client_thread_fn, (void *)data) != 0 )
      {
        fprintf(stderr, "Thread creation error\n");
        terminated = 5;
        closesocket(client);
        break;
      }
    }
#endif
  }

  #ifdef WIN32
  CloseHandle(clients_mutex);
  #else
  pthread_mutex_destroy(&clients_mutex);
  #endif

  closesocket(sd);
  return terminated;
}
/*----------------------------------------------------------------------------*/
static void log_data(char *data, int size)
{
    int i = 0,j;
    while(i < size)
    {
  for(j = i; j < i + 16 && j < size; j++)
      printf("%02X ", 0xff & (unsigned) data[j]);
  printf(" '");
  for(j = i; j < i + 16 && j < size; j++)
      if(data[j] <= 0x7f && data[j] >= ' ')
    printf("%c", 0xff & (unsigned) data[j]);
      else
    printf(".");
  printf(" '\n");
  i = j;
    }

}
/*----------------------------------------------------------------------------*/
static int sock_cpy(SOCKET dst, SOCKET src) //Socket copy ;-)
{
  int r, s = -1;
  int size = BUFFER_SIZE, sz = sizeof(int);
  char *buf;

  getsockopt(src, SOL_SOCKET, SO_RCVBUF , (char *)&size, &sz);
  //printf("%u\n", size);
  if(size <= 0)
    size = BUFFER_SIZE;
  buf = (char *) malloc(size);
  if(buf == NULL)
    return -1;
  if((r = recv(src, buf, size, 0)) <= 0)
  {
    if(verbose) fprintf(stderr, "Rcv error\n");
    return -1;
  }
  else
  {
    if(spy) log_data(buf, r);
    if((s = send(dst, buf, r, 0)) != r)
    {
     if(verbose) fprintf(stderr, "send error\n");
     return -1;
    }
    free(buf);
    return s;
  }
}
/*----------------------------------------------------------------------------*/
void* client_thread_fn(void *arg)
{
  CLIENT_DATA *hcl;
  SOCKET srv;

  hcl = (CLIENT_DATA *)arg;

  if( (srv = socket(AF_INET, SOCK_STREAM, 0)) == -1 || connect(srv, (struct sockaddr *)&remote, sizeof(remote)) == -1)
  {
    if(verbose) fprintf(stderr, "Error connect to %s:%u\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
    closesocket(srv);
    closesocket(hcl->sd);
    free(hcl);
    return NULL;
  }


  while(!terminated)
  {
    fd_set rd, wr, er;
    struct timeval t;

    FD_ZERO (&rd);
    FD_ZERO (&wr);
    FD_ZERO (&er);

    FD_SET((unsigned)hcl->sd, &rd);
    FD_SET((unsigned)hcl->sd, &er);

    FD_SET((unsigned)srv, &rd);
    FD_SET((unsigned)srv, &er);


    t.tv_sec = 0;
    t.tv_usec = 100000;        //100ms

    if(select(MAX(hcl->sd, srv) + 1, &rd, &wr, &er, &t) < 0)
    {
      if(verbose) fprintf(stderr, "select error\n");
      break;
    }

    if(FD_ISSET(hcl->sd, &er))
    {
      if(verbose) fprintf(stderr, "client error\n");
      break;
    }

    if(FD_ISSET(srv, &er))
    {
      if(verbose) fprintf(stderr, "server error\n");
      break;
    }

    if(FD_ISSET(hcl->sd, &rd))
    {
      if(spy) printf("RX:");
      if(sock_cpy(srv, hcl->sd) < 0)
        break;
    }

    if(FD_ISSET(srv, &rd))
    {
      if(spy) printf("TX:");
      if(sock_cpy(hcl->sd, srv) < 0)
        break;
    }
  }

  closesocket(srv);
  closesocket(hcl->sd);
  free(hcl);
  return NULL;
}
/*----------------------------------------------------------------------------*/
int resolve(const char *hostname, struct in_addr *addr)
{
  struct hostent *hostptr;
  int hp_error;
  long laddr;

  laddr=inet_addr(hostname);
  if (laddr != -1 || !strcmp(hostname,"255.255.255.255"))
  {
    addr->s_addr=laddr;
    return 1;
  }

  hostptr = gethostbyname(hostname);
  hp_error = errno;
  if( hostptr == 0 )
  {
//		TRACE2( "Failed to resolve address '%s', error=%d\n", hostname, hp_error );
    return 0;
  }
/*    throw error( "failed to resolve address '%s', error=%d",
        hostname.c_str(), hp_error );*/
  memcpy(&(addr->s_addr),hostptr->h_addr,sizeof(addr->s_addr));
  return 1;
}
/*--------------------------------------------------------------------------*/
