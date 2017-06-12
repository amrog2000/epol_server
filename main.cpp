//*********************************************************************************************//
//https://github.com/linfan/TCP-IP-Network-basic-examples/blob/master/chapter9/EpollServer.c
//
//********************************************************************************************//
// socket interface
#include <sys/socket.h>     
// epoll interface
#include <sys/epoll.h>      
// struct sockaddr_in
#include <netinet/in.h>     
// IP addr convertion
#include <arpa/inet.h>      
// File descriptor controller
#include <fcntl.h>          
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
// bzero()
#include <string.h>         
// malloc(), free()
#include <stdlib.h>         
#include <errno.h>
//extern int errno;

// maximum received data byte
#define MAXBTYE     10      
#define OPEN_MAX    100
//#define LISTENQ     20  >> Changed to MaxEvents

#define SERV_PORT   10012
#define INFTIM      1000
#define LOCAL_ADDR  "127.0.0.1"
#define TIMEOUT     500

// task item in thread pool
struct task                          
{
    // file descriptor or user_data
    epoll_data_t data;        
    // next task
    struct task* next;               
};

// for data transporting
struct user_data {
    int fd;
    // real received data size
    unsigned int n_size;             
    // content received
    char line[MAXBTYE];              
};

typedef struct CEpollCtorList {
  
  int MaxByte;     	// 10
  int Open_Max;    	//100
  int MaxEvents;     	//20
  int Serv_Port;   	//10012
  int _INFTIM;      	//1000
  int Local_addr; 	// "127.0.0.1"
  int iTimeOut;     	//500  
  
  int iNumOFileDescriptors;
  
  int 	iMaxReadThreads;  // Thread Pool for Read
  int 	iMaxWriteThreads; // Thread Pool for Write
  
  int   iServerPort;

  int   iLoadFactor;  // Max load to a given thread until a new thread is added from the pool
  
}EPOLL_CTOR_LIST;

class CEpoll {
  
public:
 CEpoll(EPOLL_CTOR_LIST);
 ~CEpoll();
  
private:  // yes yes it is by default
  
    int i, maxi, m_nfds;               
//    int listenfd;
    int  m_Socket;
    int  connfd;
    
static    int m_efd;
    int m_MaxEvents;
    
    int   m_iTimeOut;
    // task node
    struct task *new_task = NULL;    
    socklen_t clilen;
    
    int m_iServerPort;
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;

static    void *readtask(void *args);
static    void *writetask(void *args);
    // epoll descriptor from epoll_create()
    int m_epfd;                            
    // register epoll_ctl()
static    struct epoll_event ev;               
    // store queued events from epoll_wait()
//    struct epoll_event events[m_MaxEvents];
     struct epoll_event* events;
    
    int   m_iNumOFileDescriptors;
/*
    pthread_t tid1, tid2;            
    pthread_t tid3, tid4;            */


    pthread_t  *pRThread;  // == tid1 or tid2
    pthread_t  *pWThread;  // == tid1 or tid2
    
static    pthread_mutex_t *r_mutex;             
static    pthread_cond_t  *r_condl;

static    pthread_mutex_t *w_mutex;             
static    pthread_cond_t  *w_condl;

static    struct task *readhead, *readtail;
static    struct task *writehead, *writetail;
    
    EPOLL_CTOR_LIST m_CtorList;
    
    int m_iError;
    void setnonblocking(int sock);
    
    int m_iReadMutexIndex;
    int m_iWriteMutexIndex;

    
public:    
    int  PrepListener();
    
    int GetErrorCode();
    int ProcessEpoll();
    
};

pthread_mutex_t *CEpoll::r_mutex;             
pthread_cond_t  *CEpoll::r_condl;

pthread_mutex_t *CEpoll::w_mutex;             
pthread_cond_t  *CEpoll::w_condl;

struct task *CEpoll::readhead = NULL, *CEpoll::readtail = NULL;
struct task *CEpoll::writehead = NULL, *CEpoll::writetail = NULL;

int CEpoll::m_efd;
struct epoll_event CEpoll::ev ;               

//********************************************************************************************//
CEpoll::CEpoll(EPOLL_CTOR_LIST  CtorList):m_CtorList(CtorList)
{
//  m_CtorList = CtorList;
  
//   pThread;  // == tid1 or tid2
   pRThread = new pthread_t [m_CtorList.iMaxReadThreads];
   pWThread = new pthread_t [m_CtorList.iMaxWriteThreads];
   
   r_mutex = new pthread_mutex_t[m_CtorList.iMaxReadThreads];             
   r_condl = new pthread_cond_t[m_CtorList.iMaxReadThreads];

   w_mutex = new pthread_mutex_t[m_CtorList.iMaxWriteThreads ];       
   w_condl = new pthread_cond_t[m_CtorList.iMaxWriteThreads ];
   
   m_iNumOFileDescriptors = m_CtorList.iNumOFileDescriptors;
   m_MaxEvents = m_CtorList.MaxEvents;
   m_iTimeOut = m_CtorList.iTimeOut ;
   
   events = new epoll_event[m_MaxEvents ];
   
   m_iServerPort = m_CtorList.iServerPort;
   
   
  for (int ii = 0; ii < m_CtorList.iMaxReadThreads; ii++) {
    pthread_mutex_init(&r_mutex[i], NULL);
    pthread_cond_init(&r_condl[i], NULL);
    
    pthread_create(&pRThread[ii], NULL, readtask, &ii);    
  }    

  for (int ii = 0; ii < m_CtorList.iMaxWriteThreads; ii++) {
    pthread_mutex_init(&w_mutex[i], NULL);
    pthread_cond_init(&w_condl[i], NULL);
    
    pthread_create(&pWThread[ii], NULL, writetask, &ii);    
  }    
  
      
   m_iReadMutexIndex 	= 0;
   m_iWriteMutexIndex  	= 0;


}
//********************************************************************************************//
CEpoll::~CEpoll()
{
// Terminate all threads
  
   close(m_efd);  // Amro added this one...author did not close the file descriptor
   delete[] events;
   
   delete [] pRThread  ;
   delete []  pWThread ;
   
   delete []  r_mutex ;             
   delete []  r_condl ;

   delete []  w_mutex ;       
   delete []  w_condl ;
   
      
}
//********************************************************************************************//
void CEpoll::setnonblocking(int sock)
{
    int opts;
    if ((opts = fcntl(sock, F_GETFL)) < 0)
        printf("GETFL %d failed", sock);
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
        printf("SETFL %d failed", sock);
}

//********************************************************************************************//
int main(int argc,char* argv[])
{
    // nfds is number of events (number of returned fd)
/*
    int i, maxi, nfds;               
    int listenfd, connfd;
    // read task threads
    pthread_t tid1, tid2;            
    // write back threads
    pthread_t tid3, tid4;            
    // task node
    struct task *new_task = NULL;    
    socklen_t clilen;
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;

  
    
    pthread_mutex_init(&r_mutex, NULL);
    pthread_cond_init(&r_condl, NULL);
    pthread_mutex_init(&w_mutex, NULL);
    pthread_cond_init(&w_condl, NULL);

    // threads for reading thread pool
    pthread_create(&tid1, NULL, readtask, NULL);
    pthread_create(&tid2, NULL, readtask, NULL);
    // threads for respond to client
    pthread_create(&tid3, NULL, writetask, NULL);
    pthread_create(&tid4, NULL, writetask, NULL);
*/

  EPOLL_CTOR_LIST SEpoll_Ctor;
  /*  initialize the structure here to construct the server
   
  SEpoll_Ctor.iLoadFactor = 
  SEpoll_Ctor.iMaxReadThreads
  SEpoll_Ctor.iMaxWriteThreads
  SEpoll_Ctor.iMaxWriteThreads
  SEpoll_Ctor.iNumOFileDescriptors
  SEpoll_Ctor.iServerPort
  SEpoll_Ctor.iTimeOut
  SEpoll_Ctor.Local_addr
  SEpoll_Ctor.MaxByte
  SEpoll_Ctor.Open_Max
  
  */

 CEpoll* pCEpoll = nullptr;
 
 pCEpoll = new CEpoll(SEpoll_Ctor);

 if (pCEpoll->GetErrorCode() > 100)
 {
   
 }
 
 pCEpoll->PrepListener();
 if (pCEpoll->GetErrorCode() > 100)
 {
   
 }
 
 pCEpoll->ProcessEpoll();
 if (pCEpoll->GetErrorCode() > 100)
 {
   
 }
 delete pCEpoll;
}
////////////////////////////////////////////////////////////////////////////////////////////
int CEpoll::PrepListener()
{
    // epoll descriptor, for handling accept
    m_efd = epoll_create(m_MaxEvents);        
    m_Socket = socket(PF_INET, SOCK_STREAM, 0);
    // set the descriptor as non-blocking
    setnonblocking(m_Socket);        

    // event related descriptor
    ev.data.fd = m_Socket;           

    // monitor in message, edge trigger
    ev.events = EPOLLIN | EPOLLET;   

    // register epoll event
    epoll_ctl(m_efd, EPOLL_CTL_ADD, m_Socket, &ev);    

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    char *local_addr = LOCAL_ADDR;
    inet_aton(local_addr, &(serveraddr.sin_addr));
    serveraddr.sin_port = htons(m_iServerPort);
    bind(m_Socket, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    listen(m_Socket, m_MaxEvents);
    
}
////////////////////////////////////////////////////////////////////////////////////////////
int CEpoll::GetErrorCode()
{
  return m_iError;

}
////////////////////////////////////////////////////////////////////////////////////////////
int CEpoll::ProcessEpoll()
{
  
  for(;;)
    {
        // waiting for epoll event
        m_nfds = epoll_wait(m_efd, events, m_MaxEvents, m_iTimeOut);     
        // In case of edge trigger, must go over each event
        for (i = 0; i < m_nfds; ++i)   
        {
            // Get new connection
            if (events[i].data.fd == m_Socket)   
            {
                // accept the client connection
                connfd = accept(m_Socket, (struct sockaddr*)&clientaddr, &clilen);
                if (connfd < 0)
                    printf("connfd < 0");
                setnonblocking(connfd);
                printf("[SERVER] connect from %s \n", inet_ntoa(clientaddr.sin_addr));
                ev.data.fd = connfd;
                // monitor in message, edge trigger
                ev.events = EPOLLIN | EPOLLET;   
                // add fd to epoll queue
                epoll_ctl(m_efd, EPOLL_CTL_ADD, connfd, &ev);   
            }
            // Received data
            else if (events[i].events & EPOLLIN) 
            {
                if (events[i].data.fd < 0)
                    continue;
                printf("[SERVER] put task %d to read queue\n", events[i].data.fd);
//                new_task = (task*) malloc(sizeof(struct task));
	        new_task = new task;
		
                new_task->data.fd = events[i].data.fd;
                new_task->next = NULL;
                //printf("[SERVER] thread %d epollin before lock\n", pthread_self());
                // protect task queue (readhead/readtail)
                pthread_mutex_lock(&r_mutex[m_iReadMutexIndex]);      
                //printf("[SERVER] thread %d epollin after lock\n", pthread_self());
                // the queue is empty
                if (readhead == NULL)
                {
                    readhead = new_task;
                    readtail = new_task;
                }
                // queue is not empty
                else                 
                {
                    readtail->next = new_task;
                    readtail = new_task;
                }
                // trigger readtask thread
//                pthread_cond_broadcast(&r_condl);
                pthread_cond_broadcast(&r_condl[m_iReadMutexIndex]);  
                //printf("[SERVER] thread %d epollin before unlock\n", pthread_self());
//                pthread_mutex_unlock(&r_mutex);
                pthread_mutex_unlock(&r_mutex[m_iReadMutexIndex]);		
                //printf("[SERVER] thread %d epollin after unlock\n", pthread_self());
            }
            // Have data to send
            else if (events[i].events & EPOLLOUT)
            {
                if (events[i].data.ptr == NULL)
                    continue;
		
		if (m_iWriteMutexIndex++ > m_CtorList.iMaxWriteThreads)
		  m_iWriteMutexIndex = 0;
		
		
                printf("[SERVER] put task %d to write queue\n", ((struct task*)events[i].data.ptr)->data.fd);
                new_task = new task;
                new_task->data.ptr = (struct user_data*)events[i].data.ptr;
                new_task->next = NULL;
                //printf("[SERVER] thread %d epollout before lock\n", pthread_self());
//                pthread_mutex_lock(&w_mutex);
                pthread_mutex_lock(&w_mutex[m_iWriteMutexIndex]);		
                //printf("[SERVER] thread %d epollout after lock\n", pthread_self());
                // the queue is empty
                if (writehead == NULL)
                {
                    writehead = new_task;
                    writetail = new_task;
                }
                // queue is not empty
                else                 
                {
                    writetail->next = new_task;
                    writetail = new_task;
                }
                // trigger writetask thread
//                pthread_cond_broadcast(&w_condl);
                pthread_cond_broadcast(&w_condl[m_iWriteMutexIndex]);  		
                //printf("[SERVER] thread %d epollout before unlock\n", pthread_self());
                pthread_mutex_unlock(&w_mutex[m_iWriteMutexIndex]);
                //printf("[SERVER] thread %d epollout after unlock\n", pthread_self());
            }
            else
            {
                printf("[SERVER] Error: unknown epoll event");
            }
        }
    }

    return 0;
}
//********************************************************************************************//
void *CEpoll::readtask(void *args)
{
    int iThreadIndex = *((int*) &args); 
    
    int fd = -1;
    int n, i;
    
    
    struct user_data* data = NULL;
    while(1)
    {
        //printf("[SERVER] thread %d readtask before lock\n", pthread_self());
        // protect task queue (readhead/readtail)
        pthread_mutex_lock(&r_mutex[iThreadIndex ]); 
        //printf("[SERVER] thread %d readtask after lock\n", pthread_self());
        while(readhead == NULL)
            // if condl false, will unlock mutex
            pthread_cond_wait(&r_condl[iThreadIndex ], &r_mutex[iThreadIndex ]); 

        fd = readhead->data.fd;
        struct task* tmp = readhead;
        readhead = readhead->next;
        free(tmp);

        //printf("[SERVER] thread %d readtask before unlock\n", pthread_self());
        pthread_mutex_unlock(&r_mutex[iThreadIndex ]);
        //printf("[SERVER] thread %d readtask after unlock\n", pthread_self());

        printf("[SERVER] readtask %d handling %d\n", pthread_self(), fd);
        data = (user_data*)malloc(sizeof(struct user_data));
        data->fd = fd;
        if ((n = recv(fd, data->line, MAXBTYE, 0)) < 0)
        {
            if (errno == ECONNRESET)
                close(fd);
            printf("[SERVER] Error: readline failed: %s\n", strerror(errno));
            if (data != NULL)
                free(data);
        }
        else if (n == 0)
        {
            close(fd);
            printf("[SERVER] Error: client closed connection.\n");
            if (data != NULL)
                free(data);
        }
        else
        {
            data->n_size = n;
            for (i = 0; i < n; ++i)
            {
                if (data->line[i] == '\n' || data->line[i] > 128)
                {
                    data->line[i] = '\0';
                    data->n_size = i + 1;
                }
            }
            printf("[SERVER] readtask %d received %d : [%d] %s\n", pthread_self(), fd, data->n_size, data->line);
            if (data->line[0] != '\0')
            {
                // modify monitored event to EPOLLOUT,  wait next loop to send respond
                ev.data.ptr = data;
                // Modify event to EPOLLOUT
                ev.events = EPOLLOUT | EPOLLET;    
                // modify moditored fd event
                epoll_ctl(m_efd, EPOLL_CTL_MOD, fd, &ev);       
            }
        }
    }
    
}

//********************************************************************************************//
void *CEpoll::writetask(void *args)
{

    int iThreadIndex = *((int*) &args); 
     unsigned int n;
    // data to wirte back to client
    struct user_data *rdata = NULL;  
    while(1)
    {
        //printf("[SERVER] thread %d writetask before lock\n", pthread_self());
        pthread_mutex_lock(&w_mutex[iThreadIndex ]);
        //printf("[SERVER] thread %d writetask after lock\n", pthread_self());
        while(writehead == NULL)
            // if condl false, will unlock mutex
            pthread_cond_wait(&w_condl[iThreadIndex ], &w_mutex[iThreadIndex ]); 

        rdata = (struct user_data*)writehead->data.ptr;
        struct task* tmp = writehead;
        writehead = writehead->next;
        free(tmp);

        //printf("[SERVER] thread %d writetask before unlock\n", pthread_self());
        pthread_mutex_unlock(&w_mutex[iThreadIndex ]);
        //printf("[SERVER] thread %d writetask after unlock\n", pthread_self());
        
        printf("[SERVER] writetask %d sending %d : [%d] %s\n", pthread_self(), rdata->fd, rdata->n_size, rdata->line);
        // send responce to client
        if ((n = send(rdata->fd, rdata->line, rdata->n_size, 0)) < 0)
        {
            if (errno == ECONNRESET)
                close(rdata->fd);
            printf("[SERVER] Error: send responce failed: %s\n", strerror(errno));
        }
        else if (n == 0)
        {
            close(rdata->fd);
            printf("[SERVER] Error: client closed connection.");
        }
        else
        {
            // modify monitored event to EPOLLIN, wait next loop to receive data
            ev.data.fd = rdata->fd;
            // monitor in message, edge trigger
            ev.events = EPOLLIN | EPOLLET;   
            // modify moditored fd event
            epoll_ctl(m_efd, EPOLL_CTL_MOD, rdata->fd, &ev);   
        }
        // delete data
        free(rdata);        
    }
    return nullptr;
}
//********************************************************************************************//