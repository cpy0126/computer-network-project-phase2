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

#define ERR(a) {perror(a); return EXIT_FAILURE;}
#define IMG 100
#define FILES 200
#define MSS 300
#define CMD 400


struct package{
    int type, buf_size;
    char buf[2048], sender[128], recver[128];
    time_t Time;
    package(){
        type = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        memset(sender, 0, sizeof(sender));
        memset(recver, 0, sizeof(recver));
        Time = time(NULL);
    }
};

int handle_read(){
    //...
}

int main(int argc, char* argv[]){
    string sbuf = (string)argv[1];
    int pos = sbuf.find(':'), res, sock_fd = -1, file_fd, http_fd = -1;
    struct sockaddr_in servaddr, httpaddr, cliaddr;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(stoi(sbuf.substr(pos+1)));
    inet_pton(AF_INET, sbuf.substr(0,pos).c_str(), &servaddr.sin_addr);

    memset(&httpaddr, 0, sizeof(httpaddr));
    httpaddr.sin_family = AF_INET;
    httpaddr.sin_port = htons(80);
    httpaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //TCP client
    /*
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) ERR("socket()")
    if(connect(sock_fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) ERR("connect()")
    */

    //HTTP server
    if((http_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) ERR("socket()")
    if(bind(http_fd, (struct sockaddr*) &httpaddr, sizeof(httpaddr)) < 0) ERR("bind()")
    if(listen(http_fd, SOMAXCONN) < 0) ERR("listen()")


    char buf[2048];
    int bufsize = 2048;
    while(1){
        int cli_fd = -1, clilen = sizeof(cliaddr);
        if((cli_fd = accept(http_fd, (struct sockaddr*) &cliaddr, (socklen_t*) &clilen)) < 0) continue;
        res = read(cli_fd,buf,bufsize);
        cout<<((string)buf).substr(0,res)<<endl;
    }
}

