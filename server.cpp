#include<arpa/inet.h>
#include<dirent.h>
#include<fcntl.h>
#include<iostream>
#include<memory.h>
#include<netinet/in.h>
#include<set>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<time.h>
#include<unistd.h>
using namespace std;

#define ERR(a) {perror(a): return EXIT_FAILURE;}
#define SEND 10
#define RECV 20
#define CMD 30
#define IMG 100
#define FILES 200
#define MSS 300
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct server{
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct request{
    int conn_fd;  // fd to talk with client
    int status;
    char buf[2048];  // data sent by/to client
    std::string user_name;
} request;

typedef struct package{
    int type, content, buf_size;
    char buf[2048], sender[128], recver[128];
    time_t Time;
    package(){
        type = content = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        memset(sender, 0, sizeof(sender));
        memset(recver, 0, sizeof(recver));
        Time = time(NULL);
    }
} package;

server svr;
request* requestP = NULL;
int maxfd;
int readFD[100000],writeFD[100000];


void init_server(unsigned short port);

void init_request(request* reqP);

void free_request(request* reqP);

// int handle_read(){
//     //...
// }

int main(int argc, char* argv[]){
    if (argc != 2){
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    mkdir("./server_dir", 0777);
    chdir("./server_dir");

    struct sockaddr_in cliaddr; // used by accept()

    // Initialize server
    init_server((unsigned short)atoi(argv[1]));
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set read_OK;
    fd_set write_OK;

    while(1){
        FD_ZERO(&read_OK);
        FD_ZERO(&write_OK);
        FD_SET(svr.listen_fd, &read_OK);
        for(int a=4;a<10000;a++){   // choosing clients ready for write/read
            if(writeFD[a])
                FD_SET(a, &write_OK);
            if(readFD[a])
                FD_SET(a, &read_OK);   
        }
        int co = select(maxfd+1, &read_OK, &write_OK, NULL, NULL);  //doing IO mutiplexing
        if(FD_ISSET(svr.listen_fd, &read_OK)){  // accept new connect client
            int clilen = sizeof(cliaddr);
            int conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE){
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }
            readFD[conn_fd]=1;
            requestP[conn_fd].conn_fd=conn_fd;
            requestP[conn_fd].status=0;
            write(requestP[conn_fd].conn_fd, "input your username:\n", 22);
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, inet_ntoa(cliaddr.sin_addr));
            continue;
        }



    }

}


void init_request(request *reqP){
    reqP->conn_fd = -1;
    reqP->status=-1;
    // reqP->user_name;
    memset(reqP->buf,0,sizeof(reqP->buf));
}

void free_request(request *reqP){
    close(reqP->conn_fd);
    init_request(reqP);
}

void init_server(unsigned short port){
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0)
        ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(svr.listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0){
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request *)malloc(sizeof(request) * maxfd);
    if (requestP == NULL){
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++){
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;

    return;
}

