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

void sig_handler(int signal)
{
	if(unlink("/var/tmp/aesdsocketdata")==-1)
		perror("unlink failed");
		
	syslog(LOG_INFO,"Caught signal,exiting");
	exit(0);
}
void connection_handler(int* arg)
{

        int connfd = *arg,bytes;
        int pkt_length=0;
        unsigned int rd_bytes;
        char buff[512],rd_buff[128],*packet=NULL;
        free(arg);

        int fd,rd_fd;

        fd = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
	
	while((bytes=recv(connfd,buff,sizeof(buff),0))!=0)
	{
		int written=0;
		char *sop=buff,*eop=NULL;		
		while(written < bytes)
		{
			
			if((eop=strchr(sop,'\n'))>0)
			{
				if(packet==NULL)
					packet = malloc(eop-sop+1);
				else
					packet = realloc(packet,pkt_length+eop-sop+1);
					
	
				strncpy(packet + pkt_length,sop,eop-sop+1);
				pkt_length += eop-sop+1;
				written += eop-sop+1;	
				
				sop=eop+1;
				if(write(fd,packet,pkt_length)==-1)
					perror("write() failed");

				
				rd_fd = open("/var/tmp/aesdsocketdata",O_RDONLY);
				while((rd_bytes=read(rd_fd,rd_buff,sizeof(rd_buff)))!=0)
				{
				//	printf("read %d bytes sending %s\n",rd_bytes,rd_buff);
					if(send(connfd,rd_buff,rd_bytes,0)==-1)
					{
						perror("send() failed");
						exit(-1);
					}
				}
				memset(rd_buff,0,sizeof(rd_buff));
				close(rd_fd);
				pkt_length=0;
				free(packet);
				packet=NULL;
			}
			else
			{
				if(packet==NULL)
					packet = malloc(bytes-written);
				else
					packet = realloc(packet,pkt_length+bytes);
				
				if(packet==NULL)
				{
					perror("malloc failed");
					exit(-1);
				}
				
				
				strncpy(packet + pkt_length,sop,bytes-written);
				pkt_length += bytes-written;
				written = bytes;			
			}
		}
		
		memset(buff,0,sizeof(buff));
		
	}
	close(fd);
}

int main(int argc,char *argv[])
{
        int listenfd, *connfdp, port, daemonize=0,optval=1;
        char client[16],c;
        void *ret;
        pid_t pid;

        struct sockaddr_in serveraddr;
        struct sockaddr_in clientaddr;
	socklen_t clientlen=sizeof(struct sockaddr_in);

  	while ((c = getopt (argc, argv, "d")) != -1)
    	switch (c)
      	{
      		case 'd': daemonize=1;
      			break;
      	}
      
        openlog(NULL,LOG_ODELAY,LOG_USER);
        port=9000;

        /* Create a socket descriptor */
        if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

	ret = signal(SIGINT,sig_handler);
	if(ret==SIG_ERR)
	{
		perror("signal call failed");
		return -1;
	}
	
	/* set handler for SIGTERM */

	ret = signal(SIGTERM,sig_handler);
	if(ret==SIG_ERR)
	{
		perror("signal call failed");
		return -1;
	}
        /* begin infinite loop listening for connections */
        while (1) {

                connfdp = malloc(sizeof(int));
                *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
                if(inet_ntop(AF_INET,(void*)&clientaddr.sin_addr,client,sizeof(client))==NULL)
                        perror("inet_ntop failed");
                syslog(LOG_INFO,"Accepted connection from %s\n",client);
                connection_handler(connfdp);
                syslog(LOG_INFO,"Closing connection from %s\n",client);
                close(*connfdp);
        }
 }
