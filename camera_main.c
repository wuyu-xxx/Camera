#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "pthread_fun.h"
#include "camera.h"
#include "ser_start.h"

int flag = 0;
int main()
{
	unsigned int width;
	unsigned height;
	unsigned int size;
	unsigned int ismjpeg;
	unsigned int index;
	char *picbuf;
	char piclen_buf[20];
	int fdTest;
	int ret,count;
	int sockfd,connfd;
	pthread_t p_id;
	
	//摄像头初始化
	char *rgb_buf = NULL;
	char *jpg_buf = NULL;
	int bps = 24;
	width = 640;
	height = 480;
	int fd =  camera_init("/dev/video0", &width, &height, &size, &ismjpeg);
	if(-1 == fd)
	{
		perror("camera_init");
		return -1; 
	}

	//0是yuyv,1是mjpeg
	printf("ismjpeg = %d\n", ismjpeg);
	if (ismjpeg == 0) {
		rgb_buf= (char *)malloc(width*height*3);
		jpg_buf = malloc(100*1024);
		memset(rgb_buf, 0, sizeof(rgb_buf));
		memset(jpg_buf, 0, sizeof(jpg_buf));
		convert_rgb_to_jpg_init();
	}

	ret = camera_start(fd);
	if(ret != 0)
	{
		perror("camera_start");
		close(fd);
		return -1;
	}
	picbuf = (char *)malloc(width*height*2);

	fdTest = open("./1.jpg",O_RDWR | O_CREAT | O_TRUNC, 0777);	
	connfd = ser_start(sockfd);
	
	if(pthread_create(&p_id, NULL, pthread_fun, (void*)connfd) != 0)
	{
		perror("pthread_create");
		exit(1);
	}
	
	while(1)
	{
		int piclen1;
		ret = camera_dqbuf(fd, (void **)&picbuf, &size, &index);
		if(ret != 0)
		{
			perror("camera_dqbuf");
			close(fd);
			return -1;
		}
		if (ismjpeg == 0) {
			/* yuv --- > rgb */
			convert_yuv_to_rgb(picbuf, rgb_buf, width, height, 24);

			/* rgb --- > jpg */
			piclen1 = convert_rgb_to_jpg_work(rgb_buf, jpg_buf, width, height, 24, 80);
			/* save disk*/
		} else {
			jpg_buf = picbuf;
			piclen1 = size;
		}

		
		memset(piclen_buf,0,20);
		sprintf(piclen_buf, "%d", piclen1);

		if(flag)
		{
		write(connfd, piclen_buf, 20);
		write(connfd, jpg_buf, piclen1);
		}
#if 1
		/*图片数据存储在jpg_buf中，把它写到本地文件1.jpg*/
		ret = lseek(fdTest, 0, SEEK_SET);
		if(ret == -1)
		{
			perror("lseek");
			close(fdTest);
			return -1;
		}

		ret = write(fdTest, jpg_buf, piclen1);
		if(ret == -1)
		{
			perror("open");
			close(fdTest);
			return -1;
		}
#endif
		ret = camera_eqbuf(fd, index);
		if(ret != 0)
		{
			perror("camera_eqbuf");
			close(fd);
			return -1;
		}
	}
	pthread_detach(p_id);

	ret = camera_stop(fd);
	if(ret != 0)
	{
		perror("camera_stop");
		close(fd);
		return -1;
	}
	ret = camera_exit(fd);
	if(ret != 0)
	{
		perror("camera_exit");
		close(fd);
		return -1;
	}
	convert_rgb_to_jpg_exit();
	
	close(fd);
	close(fdTest);
	close(connfd);
	close(sockfd);
	return 0;
}
