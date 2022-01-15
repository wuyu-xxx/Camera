#include <string.h>
#include <unistd.h>

#include "pthread_fun.h"

void *pthread_fun(void *arg)
{
	char buf[10]={0};
	int clientfd =  (int)arg;
	while(1)
	{

		read(clientfd, buf, 10);
		if(strcmp(buf,"start") == 0)
		{
			flag = 1;
		}
		else
			flag = 0;
	}
	pthread_exit(NULL);
}