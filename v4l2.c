/*
 *  name:v4l2.c
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#include <stdio.h>
#include "streamer.h"
#include "v4l2.h"
void *cam_thread(void *arg);
void cam_cleanup(void *);
/*
Description.:Initalize V4L2 driver.
*/
int v4l2_init(void)
{
	int i;
	// 1、Open camera device
	if((cam_fd = open(CAM_DEV,O_RDWR)) == -1)
	{
		perror("ERROR opening V4L interface.");
		return -1;
	}
	
	// 2、Judge if the device is a camera device 
	struct v4l2_capability cam_cap;
	if(ioctl(cam_fd,VIDIOC_QUERYCAP,&cam_cap) == -1)
	{
		perror("Error opening device %s: unable to query device.");
		return -1;
	}
	if((cam_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) 
    {
		perror("ERROR video capture not supported.");
        return -1;
    }

	// 3、Setting output parameter.
	struct v4l2_format v4l2_fmt;
	v4l2_fmt.type = V4L2_CAP_VIDEO_CAPTURE;
	v4l2_fmt.fmt.pix.width = WIDTH;
	v4l2_fmt.fmt.pix.height = HEIGHT;
	v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	if (ioctl (cam_fd, VIDIOC_S_FMT, &v4l2_fmt) == -1) 
	{	
		perror("ERROR camera VIDIOC_S_FMT Failed.");
		return -1;
	}
	// 4、Check whether the parameters are set successfully 
	if (ioctl (cam_fd, VIDIOC_G_FMT, &v4l2_fmt) == -1) 
	{
		perror("ERROR camera VIDIOC_G_FMT Failed.");
		return -1;
	}
	if (v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)
	{
		printf("Set VIDIOC_S_FMT sucessful\n");
	}
	// 5、Require buffer to store image data
	struct v4l2_requestbuffers v4l2_req;
	v4l2_req.count = NB_BUFFER;
	v4l2_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_req.memory = V4L2_MEMORY_MMAP;
	if (ioctl (cam_fd, VIDIOC_REQBUFS, &v4l2_req) == -1) 
	{
		perror("ERROR camera VIDIOC_REQBUFS Failed.");
		return -1;
	} 
	// 6、Start memory map
	struct v4l2_buffer v4l2_buf;
	v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_buf.memory = V4L2_MEMORY_MMAP;
	  for(i = 0; i < NB_BUFFER; i++) 
   {
		v4l2_buf.index = i;
		if(ioctl(cam_fd, VIDIOC_QUERYBUF, &v4l2_buf) < 0)
		{
			perror("Unable to query buffer.");
			return -1;
		}
		
		pic.tmpbuffer[i] = mmap(NULL, v4l2_buf.length, PROT_READ, MAP_SHARED, cam_fd, v4l2_buf.m.offset);
		if(pic.tmpbuffer[i] == MAP_FAILED)
		{
			 perror("Unable to map buffer.");
			 return -1;
		}
        if(ioctl(cam_fd, VIDIOC_QBUF, &v4l2_buf) < 0)
		{
			perror("Unable to queue buffer.");
			return -1;
		}
   }
	//7、Open stream input 
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   	if(ioctl(cam_fd, VIDIOC_STREAMON, &type) < 0)
	{
		perror("Unable to start capture.");
		return -1;
	}
	return 0;
}

/*
Description.:Get a jpeg image and save.
*/
int v4l2Grab(void)
{
	//8、Get a image 
	struct v4l2_buffer buff;
	buff.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buff.memory = V4L2_MEMORY_MMAP;
	if(ioctl(cam_fd, VIDIOC_DQBUF, &buff) < 0)
	{
		printf("camera VIDIOC_DQBUF	Failed.\n");
		return -1;
	}
	
	pic.tmpbytesused[buff.index] = buff.bytesused;//把大小复制过来，用于清理
	pic.tmpesize = buff.bytesused;				  //把大小复制过来，用来传输
	pic.tv = buff.timestamp;					  //把时间戳复制过来
	pic.buffer = (unsigned char *)calloc(1,pic.tmpesize);
	memcpy(pic.buffer, pic.tmpbuffer[buff.index], buff.bytesused);	//拷贝到pic.buffer中.
	printf("size : %d\n",pic.tmpbytesused[buff.index]);			  

	//9、Queue the buffers.
	if(ioctl(cam_fd, VIDIOC_QBUF, &buff) < 0)
	{
		printf("camera VIDIOC_QBUF Failed.");
		return -1;
	}
	return 0;
}

/*
Description.:Release resource
*/
int v4l2_close(void)
{
	// Remove mmap.
	int i;
	for(i=0; i<NB_BUFFER; i++)
		munmap(pic.tmpbuffer[i],pic.tmpbytesused[i]);
	close(cam_fd);
	return 0;
}
/*
Description.:Open camera pthread.
*/
int v4l2_run()
{
	pthread_create(&cam_tid, NULL, cam_thread, NULL);	//创建线程	
    pthread_detach(cam_tid);							//等待线程完成后回收资源
	return 0;
}
/*
Description.:Release resource
*/
void cam_cleanup(void *arg)
{
		printf("Clear memory\n");
        v4l2_close();
}
/*
Description.:Camera pthread,Copy image data to global image data.
*/
void *cam_thread(void *arg)
{
	pthread_cleanup_push(cam_cleanup, NULL);	
	while(1)		
	{
		//pthread_cleanup_push(cam_cleanup, NULL);
		while(netconnflag)
		{
			v4l2Grab();	//获得图像
			//这里可以判断字节大小是否小于最小值，若小于最小值说明图片损坏
			pthread_mutex_lock(&m);								//锁住
			pic_global.buffer = (unsigned char *)calloc(1,pic.tmpesize);//必须要加，不然会出现段错误
			pic_global.tmpesize = pic.tmpesize;					//复制大小			
			pic_global.tv = pic.tv;				        		//复制时间
			memcpy(pic_global.buffer, pic.buffer, pic.tmpesize);//这里复制到全局变量
			free(pic.buffer);									//每次发完就释放掉
			pthread_cond_broadcast(&m_update);					//广播出去，告诉net我已经接收完了
			pthread_mutex_unlock(&m);							//解锁
		}
	}
	pthread_cleanup_pop(1);
}






