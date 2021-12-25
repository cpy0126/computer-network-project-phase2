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
#include<string.h>
using namespace std;

#define ERR(a) {perror(a); return EXIT_FAILURE;}
#define IMG 100
#define FILES 200
#define MSS 300
#define LOGIN 400
#define ADD 500
#define DEL 600

string set_header(long long int content_len, string &filetype){
    string header;
    header = "HTTP/1.1 200 OK\r\nServer: jdbhttpd/0.1.0\r\nContent-Length: " + to_string(content_len);
    header = header + "\r\nContent-Type: " + filetype + "\r\nConnection: keep-alive\r\n\r\n";
    return header;
}

string header404 ="\
    HTTP/1.1 404 Bad Request\r\n\
    Server: jdbhttpd/0.1.0\r\n\
    Content-Length: 169\r\n\
    Content-Type: text/html\r\n\
    Connection: Closed\r\n\r\n\
    <html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center>\
    <hr><center>nginx/0.8.54</center></body></html>";

struct package{
    int type, buf_size;
    char buf[2048], sender[128], recver[128];
    time_t Time;
    package(){
        type = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        // memset(sender, 0, sizeof(sender));
        // memset(recver, 0, sizeof(recver));
        Time = time(NULL);
    }
} package;

char buf[1000000],head_buf[1000];
int bufsize,headsize,sock_fd,http_fd,cli_fd;
long long int filesize;

int login(bool flag){
    string used = "<html><h2>Username used, please choose another</h2></html>", header = "text/html";
    int file_fd = open("./template/index.html", O_RDONLY), res;
    if(file_fd<0) return -1;
    filesize = lseek(file_fd, 0, SEEK_END);
    if(filesize<0){
        close(file_fd);
        return -1;
    }
    lseek(file_fd, 0, SEEK_SET);
    if(flag) header = set_header(filesize+used.length(), header);
    else header = set_header(filesize, header);
    if(write(cli_fd, header.c_str(), header.length())<0){
        close(file_fd);
        return -1;
    }
    while((res = read(file_fd, buf, filesize)) > 0){
        if(write(cli_fd, buf, res)<0){
            close(file_fd);
            return -1;
        }
        filesize -= res;
    }
    close(file_fd);
    if(res<0) return -1;
    if(flag) if(write(cli_fd, used.c_str(), used.length())<0) return -1;
    return 0;
}


int handle_http(){
    int res, body_size = 0;
    memset(head_buf, 0, sizeof(head_buf));
    headsize = 0;
    char cur = '\0', prev = '\0';
    string shead, method, reqpath;
    while(1){
        if((res = read(cli_fd, head_buf+headsize, 1))<0) continue;
        if(!res) return -1;
        cur = head_buf[headsize];
        ++headsize;
        if(prev=='\r' && cur=='\n' && headsize==2) break;
        if(prev=='\r' && cur=='\n') shead+=(string)head_buf, headsize=0;
        prev = cur;
    }
    res = shead.find(" ");
    method = shead.substr(0,res);
    if(method=="GET"){
        shead = shead.substr(res+1);
        res = shead.find(" ");
        reqpath = shead.substr(0,res);
        if(reqpath=="/")
            return login(0);
        else{
            if(write(cli_fd, header404.c_str(), header404.length())<0) return -1; 
            return 0;
        }
    }
    if(method=="POST"){
        res = shead.rfind("Content-Length: ");
        shead = shead.substr(res);
        res = shead.find("\r\n");
        body_size = stoi(shead.substr(16,res));
        while(1){
            if((res = read(cli_fd, buf+bufsize, body_size))<0) continue;
            if(!res) return -1;
            bufsize += res;
            body_size -= res;
            if(body_size==0) break;
        }
        cerr << ((string)buf).substr(0,bufsize+1) << endl;
        return 0;
    }
    return -1;
}

int main(int argc, char* argv[]){
    
    if(argc>3||argc<2){
        cout<<"usage: [server] ip:port [browser] port (default=80)"<<endl;
        return 1;
    }

    string sbuf = (string)argv[1];
    int pos = sbuf.find(':'), res;
    struct sockaddr_in servaddr, httpaddr, cliaddr;
    sock_fd = http_fd = -1;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    res = -1;
    if((res = stoi(sbuf.substr(pos+1)))<0 || res>65535){
        cout<<"server port should be a number between 0 to 65535."<<endl;
        return 1;
    }
    servaddr.sin_port = htons(res);
    if(inet_pton(AF_INET, sbuf.substr(0,pos).c_str(), &servaddr.sin_addr) < 0){
        cout<<"server ip should be a valid ip."<<endl;
        return 1;
    }

    memset(&httpaddr, 0, sizeof(httpaddr));
    httpaddr.sin_family = AF_INET;
    res = -1;
    if(argc==2) res = 80;
    else
        if((res = atoi(argv[2]))<0 || res>65535){
            cout<<"browser port should be a number between 0 to 65535."<<endl;
            return 1;
        }
    httpaddr.sin_port = htons(res);
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

    fd_set read_OK;

    while(1){
        cli_fd = -1;
        int clilen = sizeof(cliaddr);
        if((cli_fd = accept(http_fd, (struct sockaddr*) &cliaddr, (socklen_t*) &clilen)) < 0) continue;
        
        while(1){
            FD_ZERO(&read_OK);
            FD_SET(cli_fd, &read_OK);
            //FD_SET(cli_fd, &read_OK), FD_SET(sock_fd, &read_OK);
            select(cli_fd+1, &read_OK, NULL, NULL, NULL);
            /*
            if(FD_ISSET(sock_fd, &read_OK))
                if(handle_server()<0) break;
            */
            if(FD_ISSET(cli_fd, &read_OK))
                if(handle_http()<0) break;
        }
    }
}
