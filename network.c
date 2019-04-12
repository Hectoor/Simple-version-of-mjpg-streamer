/*
 *  name:network.c
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#include<stdio.h>
#include "streamer.h"
#include "v4l2.h"
#include "network.h"
void *net_pthread(void *arg);

int net_init()
{
	netconnflag = 0;
	//1、Ignore SIGPIPE signal.
	struct sigaction ignore_sign;
	ignore_sign.sa_handler = SIG_IGN;
	sigaction( SIGPIPE, &ignore_sign, 0 );
	//2、Open socket for server.
	if((netfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		perror("Socket set error.");
		return -1;
	}		
	//3、Set socket to ignore "socket already in use" errors.
	int opt = 1;
	if (setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0)
	{           
		perror("Server setsockopt set SO_REUSEADDR failed");
		return 1;
	}
	//4、Set some parameter and bind.
    bzero(&servaddr,sizeof(servaddr));   
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SER_PORT);     
    if(bind(netfd,(struct sockaddr *)&servaddr,sizeof(struct sockaddr)) < 0)
	{
		perror("Bind set error.");
		netfd = -1;
		return -1;
	}		
	//5、Listen
    if(listen(netfd, CONN_NUM) < 0)
	{
		perror("Listen set error.");
		netfd = -1;
		return -1;
	}
    printf("Waiting connections ... \n");
	return 0;
}

int net_accept()
{
	//6、Accept.
	cliaddrSize = sizeof(struct sockaddr_storage);
    if((connfd = accept(netfd,(struct sockaddr *)&cliaddr,&cliaddrSize)) < 0)
	{
		perror("Accept failed.");
	}		
	printf("connect ok!\n");
	return 0;
}
int net_run()
{
	printf("I am net_run\n");
	pthread_create(&net_tid, NULL, net_pthread, &connfd);	
    pthread_detach(net_tid);	//等待线程完成后回收资源
	return 0;
}


void decodeBase64(char *data)
{
    const unsigned char *in = (const unsigned char *)data;
    /* The decoded size will be at most 3/4 the size of the encoded */
    unsigned ch = 0;
    int i = 0;

    while(*in) {
        int t = *in++;

        if(t >= '0' && t <= '9')
            t = t - '0' + 52;
        else if(t >= 'A' && t <= 'Z')
            t = t - 'A';
        else if(t >= 'a' && t <= 'z')
            t = t - 'a' + 26;
        else if(t == '+')
            t = 62;
        else if(t == '/')
            t = 63;
        else if(t == '=')
            t = 0;
        else
            continue;

        ch = (ch << 6) | t;
        i++;
        if(i == 4) {
            *data++ = (char)(ch >> 16);
            *data++ = (char)(ch >> 8);
            *data++ = (char) ch;
            i = 0;
        }
    }
    *data = '\0';
}

void free_request(request *req)
{
    if(req->parameter != NULL) free(req->parameter);
    if(req->client != NULL) free(req->client);
    if(req->credentials != NULL) free(req->credentials);
    if(req->query_string != NULL) free(req->query_string);
}


void init_iobuffer(iobuffer *iobuf)
{
	 memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
	 iobuf->level = 0;
}

void init_request(request *req)
{
    req->type        = A_UNKNOWN;
    req->parameter   = NULL;
    req->client      = NULL;
    req->credentials = NULL;
}

void send_stream(int context_fd, int input_number)
{
    unsigned char *frame = NULL, *tmp = NULL;
    int frame_size = 0, max_frame_size = 0;
    char buffer[BUFFER_SIZE] = {0};
    struct timeval timestamp;
	
    //DBG("preparing header\n");
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
            "Access-Control-Allow-Origin: *\r\n" \
            STD_HEADER \
            "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
            "\r\n" \
            "--" BOUNDARY "\r\n");
    if(write(context_fd, buffer, strlen(buffer)) < 0) 
	{
        free(frame);
        perror("Write error.");
		return ;
    }

	   while(netconnflag) 
	{
		printf("I am netconnflag\n");
        /* wait for fresh frames */
        pthread_mutex_lock(&m);
		/* 等待输入通道发出数据更新的信号 */
        pthread_cond_wait(&m_update, &m);	
		printf("I am waiting for you\n");
        /* read buffer */
        frame_size = pic_global.tmpesize;
		printf("net image size : %d\n",frame_size);
        /* check if framebuffer is large enough, increase it if necessary */
        if(frame_size > max_frame_size) 
		{
            //DBG("increasing buffer size to %d\n", frame_size);

            max_frame_size = frame_size + TEN_K;
            if((tmp = realloc(frame, max_frame_size)) == NULL) {
                free(frame);
                pthread_mutex_unlock(&m);
				perror("Not enough memory\n");
                //send_error(context_fd, 500, "not enough memory");
                return;
            }
            frame = tmp;
        }
		//复制时间过来
        /* copy v4l2_buffer timeval to user space */
        timestamp = pic_global.tv;

        memcpy(frame, pic_global.buffer, frame_size);
        //DBG("got frame (size: %d kB)\n", frame_size / 1024);
		free(pic_global.buffer);
        pthread_mutex_unlock(&m);
	
        /*
         * print the individual mimetype and the length
         * sending the content-length fixes random stream disruption observed
         * with firefox
         */
        sprintf(buffer, "Content-Type: image/jpeg\r\n" \
                "Content-Length: %d\r\n" \
                "X-Timestamp: %d.%06d\r\n" \
                "\r\n", frame_size, (int)timestamp.tv_sec, (int)timestamp.tv_usec);
		//把帧头写进去
        if(write(context_fd, buffer, strlen(buffer)) < 0) 
		{
			netconnflag = 0;
			printf("Write error1.\n");
			break;
		}
		//把一帧图像数据写进去
        if(write(context_fd, frame, frame_size) < 0) 
		{
			netconnflag = 0;
			printf("Write error2.\n");
			break;
		}
		//把帧尾写进去
        sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
        if(write(context_fd, buffer, strlen(buffer)) < 0) 
		{
			netconnflag = 0;
			printf("Write error3.\n");
			break;
		}
    }
    free(frame);
}


void *net_pthread(void *arg)
{
	printf("i am net_pthread\n");
	int cnt;
    char query_suffixed = 0;
    int input_number = 0;
    char buffer[BUFFER_SIZE] = {0};
	int net_fd;
	iobuffer iobuf;
    request req;
	
	if(arg != NULL) 	
    {
        memcpy(&net_fd, arg, sizeof(connfd));
        //free(arg);	//会出现错误？未解决
    } else
		return NULL;
	
	/*初始化iobuf和req的请求*/
	init_iobuffer(&iobuf);//把iobuf清为0
	init_request(&req);	  //http协议，需要客服端给服务器发送一个请求，而resquest就是这个请求
	
	/*从客户端接受一些数据，用来表示客户端发来的请求，才知道给客户发送什么数据*/
	memset(buffer, 0, sizeof(buffer));
	if((cnt = _readline(net_fd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) 
    {
        close(net_fd);
        return NULL;
    }
	/* determine what to deliver */
	//解析buf中的字符串
	if(strstr(buffer, "GET /?action=stream") != NULL)
	{
        query_suffixed = 255;
        req.type = A_STREAM;
    }  
	/*
     * Since when we are working with multiple input plugins
     * there are some url which could have a _[plugin number suffix]
     * For compatibility reasons it could be left in that case the output will be
     * generated from the 0. input plugin
     */
	if(query_suffixed) 
    {
        char *sch = strchr(buffer, '_');
        if(sch != NULL) 
		{  // there is an _ in the url so the input number should be present
            char numStr[3];
            memset(numStr, 0, 3);
            strncpy(numStr, sch + 1, 1);
            input_number = atoi(numStr);
		}
    }
	
    do {
        memset(buffer, 0, sizeof(buffer));

        if((cnt = _readline(net_fd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
            free_request(&req);
            close(net_fd);
            return NULL;
        }

        if(strcasestr(buffer, "User-Agent: ") != NULL) {
            req.client = strdup(buffer + strlen("User-Agent: "));
        } else if(strcasestr(buffer, "Authorization: Basic ") != NULL) {
            req.credentials = strdup(buffer + strlen("Authorization: Basic "));
            decodeBase64(req.credentials);
        }

    } while(cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n'));
	
	if(req.type == A_STREAM)
	{
		netconnflag = 1;					//标志位为1，说明连接上了
		send_stream(net_fd, input_number);	//发送视频流
	}
	return 0;
}

int _readline(int fd,iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    char c = '\0', *out = (char *)buffer;
    int i;
	/*
		_readline:
			从iobuf.buf[]中一个字节一个字节的将数据取出存放到buffer中农，只到遇见换行符号\n，或者长度达到了
	*/
    memset(buffer, 0, len);

    for(i = 0; i < len && c != '\n'; i++) {
        if(_read(fd, iobuf, &c, 1, timeout) <= 0) {
            /* timeout or error occured */
            return -1;
        }
        *out++ = c;
    }

    return i;
}

int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
    int copied = 0, rc, i;
    fd_set fds;
    struct timeval tv;

    memset(buffer, 0, len);
	
    while((copied < len)) 
    {
        i = MIN(iobuf->level, len - copied);			//第一次i = 0，以后i = 1
        memcpy(buffer + copied, iobuf->buffer + IO_BUFFER - iobuf->level, i);

        iobuf->level -= i;
        copied += i;
        if(copied >= len)
            return copied;

        /* select will return in case of timeout or new data arrived */
		//当客户端发有数据或者超时的时候，select函数就返回
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if((rc = select(fd + 1, &fds, NULL, NULL, &tv)) <= 0) 
	 {
            if(rc < 0)
                exit(EXIT_FAILURE);

            /* this must be a timeout */
            return copied;
        }

        init_iobuffer(iobuf);	//将iobuf清0

        /*
         * there should be at least one byte, because select signalled it.
         * But: It may happen (very seldomly), that the socket gets closed remotly between
         * the select() and the following read. That is the reason for not relying
         * on reading at least one byte.
         */
         //调用read函数，从客户端读取最多256字节的数据
        if((iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER)) <= 0) 
	 {
            /* an error occured */
            return -1;
        }
        /* align data to the end of the buffer if less than IO_BUFFER bytes were read */
		//拷贝iobuf->buffer中的数据
        memmove(iobuf->buffer + (IO_BUFFER - iobuf->level), iobuf->buffer, iobuf->level);
    }

    return 0;
}









