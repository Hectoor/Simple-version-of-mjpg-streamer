/*
 *  name:network.h
 *  author:Hector Cheung
 *  Time: 4, 9, 2019
 */
#ifndef __NETWORK_H
#define __NETWORK_H

#include "streamer.h"


// network
#define SER_PORT 8080
#define CONN_NUM 5		//allow connect number
#define BUFFER_SIZE 1024 
#define IO_BUFFER 256
#define TEN_K (10*1024)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define STD_HEADER "Connection: close\r\n" \
    "Server: MJPG-Streamer/0.2\r\n" \
    "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
    "Pragma: no-cache\r\n" \
    "Expires: Mon, 3 Jan 2000 12:34:56 GMT\r\n"
#define BOUNDARY "boundarydonotcross" 
#define MAX_SD_LEN 50
struct sockaddr_in servaddr,cliaddr;
socklen_t cliaddrSize;
int netfd,connfd;
int sd[MAX_SD_LEN];
int sd_i;
typedef struct {
    int level;              /* 表示buffer中还剩多少字节的空间how full is the buffer */
    char buffer[IO_BUFFER]; /*用于存放数据 the data */
} iobuffer;

typedef enum {
    A_UNKNOWN,
    A_STREAM,
} answer_t;

typedef struct {
    answer_t type;
    char *parameter;
    char *client;
    char *credentials;
    char *query_string;
} request;

int net_init(void);
int net_accept(void);
int net_run(void);
void decodeBase64(char *data);
void free_request(request *req);
void init_iobuffer(iobuffer *iobuf);
void init_request(request *req);
void send_stream(int context_fd, int input_number);
int _readline(int fd,iobuffer *iobuf, void *buffer, size_t len, int timeout);
int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout);

#endif