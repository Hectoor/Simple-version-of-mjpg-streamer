/*
 *  name:streamer.c
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#include <stdio.h>
#include "streamer.h"
#include "network.h"
#include "v4l2.h"
int main(void)
{
	v4l2_init();	//initialize camera 
	net_init();		//initialize network
	pthread_mutex_init(&m, NULL);	
	v4l2_run();		//create cam pthread,open camera capture and copy image data to global image data.
	while(1)
	{
		net_accept(); //wait client connect.
		net_run();	  //send image data.
	}
}