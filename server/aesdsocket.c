#include <stdio.h>
#include <strings.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

int stop_server;
void sig_handler(int signal)
{
	stop_server=1;
	syslog(LOG_INFO,"Caught signal,exiting");
}

typedef struct{
	char *file;
	pthread_mutex_t file_lock;
}file_access;

file_access aesdlog = {.file="/var/tmp/aesdsocketdata"};

struct tdata{
	pthread_t tid;
	int connfd;
	int complete;
	struct tdata *next;
};
struct tdata *head=NULL,*tail=NULL;

void* connection_handler(void *arg)
{
	struct tdata *node = (struct tdata*)arg;
        int connfd = node->connfd;
        int pkt_length=0,bytes;
        unsigned int rd_bytes;
        char buff[512],rd_buff[128],*packet=NULL;

        int fd,rd_fd;

        fd = open(aesdlog.file,O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
	if(fd==-1)
	{
		perror("open call failed");
		pthread_exit(NULL);
	}
	
	while((bytes=recv(connfd,buff,sizeof(buff),0))!=0)
	{
		int written=0;
		char *sop=buff,*eop=NULL; // store pointer to beginning of buffer		
		while(written < bytes)	// loop until all bytes are written 
		{
			
			if((eop=strchr(sop,'\n'))>0) // if '\n' found write into file
			{	
				if(packet==NULL)			// create packet if not created
					packet = malloc(eop-sop+1);
				else					// resize packet if already created
					packet = realloc(packet,pkt_length+eop-sop+1);
					
				if(packet==NULL)
				{
					perror("malloc failed");
					pthread_exit(NULL);
				}
				strncpy(packet + pkt_length,sop,eop-sop+1);	// copy into packet
				pkt_length += eop-sop+1;			// update packet length
				written += eop-sop+1;				// update bytes written from buffer
				sop=eop+1;					// update start to next byte after '\n'
				
				pthread_mutex_lock(&aesdlog.file_lock);
				if(write(fd,packet,pkt_length)==-1)
					perror("write() failed");				
				pthread_mutex_unlock(&aesdlog.file_lock);
				
				// read all bytes written into file and send back to sender
				rd_fd = open("/var/tmp/aesdsocketdata",O_RDONLY);
				if(fd==-1)
				{
					perror("open call failed");
					pthread_exit(NULL);
				}
				while((rd_bytes=read(rd_fd,rd_buff,sizeof(rd_buff)))!=0)
				{
					// printf("read %d bytes sending %s\n",rd_bytes,rd_buff);
					if(send(connfd,rd_buff,rd_bytes,0)==-1)
					{
						perror("send() failed");
						pthread_exit(NULL);
					}
				}
				memset(rd_buff,0,sizeof(rd_buff));
				close(rd_fd);
				pkt_length=0;	// reset packet length
				free(packet);
				packet=NULL;
			}
			else	// no '\n' found in the buffer. continue to write into the packet
			{
				if(packet==NULL)				// create packet if not created
					packet = malloc(bytes-written);		// handles case when more bytes follow '\n'
				else						// resize packet if already created	
					packet = realloc(packet,pkt_length+bytes);
				
				if(packet==NULL)
				{
					perror("malloc failed");
					pthread_exit(NULL);
				}			
			
				strncpy(packet + pkt_length,sop,bytes-written);
				pkt_length += bytes-written;
				written = bytes;			
			}
		}		
		memset(buff,0,sizeof(buff));		
	}
	close(fd);
	//syslog(LOG_INFO,"Closing connection from %s\n",client);
	node->complete=1;
	close(connfd);
	pthread_exit(NULL);
	//return (void*)0;
}

void timestamper(union sigval val)
{
 	int fd,len;
 	char buff[100];
        time_t t;
        struct tm *tmp;
        
        fd = open(aesdlog.file,O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
	if(fd==-1)
	{
		perror("open call failed");
		pthread_exit(NULL);
	}
	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL) {
	perror("localtime");
	pthread_exit(NULL);
	}

	if ((len=strftime(buff, sizeof(buff), "timestamp: %a, %d %b %Y %T %z", tmp)) == 0) {
	fprintf(stderr, "strftime returned 0");
	pthread_exit(NULL);
	}
	buff[len]='\n';
	pthread_mutex_lock(&aesdlog.file_lock);

	if(write(fd,buff,len+1)==-1)
		perror("write() failed");

	pthread_mutex_unlock(&aesdlog.file_lock);
	close(fd);
}

void setup_timer()
{
	struct sigevent evp;
	timer_t timer;
	int ret;	
	struct itimerspec ts;

	evp.sigev_value.sival_ptr = &timer;
	evp.sigev_notify = SIGEV_THREAD;
	evp.sigev_notify_attributes = NULL;
	evp.sigev_notify_function = timestamper;
	
	ret = timer_create (CLOCK_REALTIME,&evp,&timer);
	if (ret)
		perror ("timer_create");	

	ts.it_interval.tv_sec = 10;
	ts.it_interval.tv_nsec = 0;
	ts.it_value.tv_sec = 10;
	ts.it_value.tv_nsec = 0;
	
	ret = timer_settime (timer, 0, &ts, NULL);
	if (ret)
		perror ("timer_settime");
}


void push(struct tdata *node)
{
	//printf("adding tid %d %d\n",node->tid,node->complete);
	if(head==NULL)
	{
		head=node;
		tail=node;
		return;
	}
	node->next=head;
	head=node;
}
void free_threads(int all_threads)
{
	struct tdata *curr,*prev;
	
	curr=head;
	prev=head;
	while(curr!=NULL)
	{
		if(all_threads || curr->complete)
		{
			if(curr==head)
				head = curr->next;
			else
			{
				prev->next = curr->next;
			}
			
			pthread_join(curr->tid,NULL);
			free(curr);
		}
		
		prev = curr;
		curr = curr->next;		
	}
}


int main(int argc,char *argv[])
{
        int listenfd,port, daemonize=0,optval=1;
        char client[16];        
	pid_t pid;
	
        struct sockaddr_in serveraddr;
        struct sockaddr_in clientaddr;
	socklen_t clientlen=sizeof(struct sockaddr_in);
	if(argc > 1)
		if(strcmp(argv[1],"-d")==0)
			daemonize=1;
      
        openlog(NULL,LOG_ODELAY,LOG_USER);
        port=9000;

        /* Create a socket descriptor */
        if ((listenfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0)) < 0)
                return -1;

        /* Eliminates "Address already in use" error from bind. */
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int)) < 0)
                return -1;
	
        /* listenfd will be an endpoint for all requests to port on any IP address for this host */
        bzero((char *) &serveraddr, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serveraddr.sin_port = htons((unsigned short)port);
        if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        {
                perror("bind call failed");
                return -1;
        }

	if(daemonize)
	{
		pid = fork();
		if(pid==-1)
		{
			perror("fork failed");
			exit(-1);
		}
		else if(pid>0)
			exit(0);

		if(setsid()==-1)
		{
			perror("setsid failed");
		}
		int fd;
		fd=open("/dev/null",O_RDWR);
		if(dup2(fd,1)==-1) //stout
			perror("dup2 failed");
		if(dup2(fd,2)==-1) //stderr
			perror("dup2 failed");
	}
        /* Make it a listening socket ready to accept connection requests */
        if (listen(listenfd,16) < 0)
        {
                perror("listen call failed");
                return -1;
        }

	/* set handler for SIGINT */

	if(signal(SIGINT,sig_handler)==SIG_ERR)
	
	{
		perror("signal call failed");
		return -1;
	}
	
	/* set handler for SIGTERM */

	if(signal(SIGTERM,sig_handler)==SIG_ERR)
	
	{
		perror("signal call failed");
		return -1;
	}
	setup_timer();
	struct tdata *curr;
        /* begin infinite loop listening for connections */
        while(1){

		pthread_t tid;
		int connfd;

                connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
                if(connfd != -1)
		{              	
		        if(inet_ntop(AF_INET,(void*)&clientaddr.sin_addr,client,sizeof(client))==NULL)
			        perror("inet_ntop failed");
			        
		        curr=malloc(sizeof(struct tdata)); // allocate memory for node    
		        curr->complete=0;
		        curr->connfd=connfd; 
			
		        syslog(LOG_INFO,"Accepted connection from %s\n",client);
		        pthread_create(&tid, NULL, connection_handler, curr);
		        curr->tid=tid;		        
		        push(curr);
		}
		free_threads(0);	// free memory for any threads already finished
                if(stop_server)break;              
        }
        
        free_threads(1); // free memory for all threads 
	if(unlink("/var/tmp/aesdsocketdata")==-1)
	perror("unlink failed");
 }
 
