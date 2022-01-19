#include <stdio.h>
#include <syslog.h>
#include <string.h>
int main(int argc,char **argv)
{
	openlog(NULL,LOG_ODELAY,LOG_USER);
	if(argc!=3)
	{
		printf("Requires two arguments. \n");
		syslog(LOG_ERR,"Requires two arguments");
		return 1;
	}

	char *filepath = argv[1];
	char *str = argv[2];

	FILE *fp;

	fp = fopen(filepath,"w");
	if(fp==NULL)
	{
		perror("fopen failed");
		syslog(LOG_ERR,"fopen call failed");
		return 1;
	}

	if(fwrite(str,strlen(str),1,fp)==0)
	{
		perror("fwrite failed");
		syslog(LOG_ERR,"fwrite call failed");
		return 1;
	}

	syslog(LOG_DEBUG,"Writing %s to %s",str,filepath);
	closelog();
	return 0;




}
