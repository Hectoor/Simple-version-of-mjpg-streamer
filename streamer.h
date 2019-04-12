/*
 *  name:streamer.h
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#ifndef __STREAMER_H
#define __STREAMER_H
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#define __USE_GNU		//用于解决strcasestr编译警告
#include <string.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include<sys/ioctl.h>
#include <setjmp.h>
#include <unistd.h>

#include<sys/socket.h>
#include<netinet/in.h>    
#include<arpa/inet.h>   
#include<ctype.h>   
 
#include<pthread.h>

#include <errno.h>
#include <signal.h>
// camera
#define  NB_BUFFER  4	//memory block number
struct pic_data 
{
	unsigned char *tmpbuffer[NB_BUFFER];
	unsigned int tmpbytesused[NB_BUFFER];
	unsigned char *buffer;
	unsigned int tmpesize;
	struct timeval tv;
}pic,pic_global;

int netconnflag;	//连接flag，用于判断是否连接上

// pthread
pthread_t cam_tid,net_tid;
pthread_mutex_t m;
pthread_cond_t m_update;

#endif