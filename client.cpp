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


struct package{
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
};

int handle_read(){
    //...
}

int main(int argc, char* argv[]){
     
}

