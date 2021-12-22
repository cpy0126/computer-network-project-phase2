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

#define ERR(a) {perror(a): return EXIT_FAILURE;}
#define IMG 100
#define FILES 200
#define MSS 300
#define LOGIN 400
#define ADD 500
#define DEL 600
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"


#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct server{
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct package{
    int type, buf_size;
    char buf[2048];
    char sender[64];
    char recver[64];
    time_t Time;
    package(){
        type = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        // memset(sender, 0, sizeof(sender));
        // memset(recver, 0, sizeof(recver));
        Time = time(NULL);
    }
} package;

typedef struct http_package{
    int type, buf_size;
    char buf[100000];
    // time_t Time;
    http_package(){
        type = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        // Time = time(NULL);
    }
} http_package;

typedef struct request{
    int conn_fd;  // fd to talk with client
    int filesize;
    // int status;     
    // char buf[2048];  // data sent by/to client
    http_package get;
    package send;
    std::string user_name;
} request;

server svr;
request req;
char username[64];

void init_client(unsigned short port);

void init_request(request* reqP);

void free_request(request* reqP);

void get_method(char* buffer,char* method);

void get_request_file(char* buffer,char* reqfile);

int set_http_header(char *buf,int content_len,const char *filetype,int status);

void set_404_header(char *buf);

int send_login();

int send_homepage(int status);

int handle_http();

void set_response(package *now,int type,int bufferSize,char *buffer,char sender[],char recver[]);

int main(int argc, char* argv[]){
    if (argc != 2){
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    mkdir("./client_dir", 0777);
    chdir("./client_dir");
    char succeed[16]="Succeeed";
    char failed[8]="Failed";
    fprintf(stderr, "Running\n");
    while(1){
        init_client((unsigned short)atoi(argv[1]));
        fprintf(stderr, "\nstarting on %.80s, port %d\n", svr.hostname, svr.port);
        timeval tm;
        tm.tv_sec=0;
        tm.tv_usec=20;
        fd_set read_OK,write_OK;
        while(1){
            FD_ZERO(&read_OK);FD_ZERO(&write_OK);
            FD_SET(req.conn_fd, &read_OK);FD_SET(req.conn_fd, &write_OK);
            select(5,&read_OK,NULL,NULL,NULL);
            if(FD_ISSET(req.conn_fd,&read_OK)){
                handle_http();
            }
            else if(FD_ISSET(req.conn_fd,&write_OK)){
                
            }
        }


    }

    return 0;
}


int handle_http(){
    char buffer[100000];
    int get=recv(req.conn_fd, buffer, 100000, 0);
    // fprintf(stderr,"%d\n",get);
    if(get<=0){
        // need to free request
        return 0;
    }
    char method[8],reqfile[128];
    get_method(buffer,method);
    get_request_file(buffer,reqfile);
    if(strcmp(method,"GET")==0){
        if(!strcmp(reqfile,"/")){
            if(!send_login()){
                ERR_EXIT("Wrong sending");
            }
            return 1;
        }
        else if(!strcmp(reqfile,"/favicon.ico")){
            char buf[1024];
            set_404_header(buf);
            send(req.conn_fd,buf,1024,0);
        }
        else if(!strcmp(reqfile,"/homepage")){
            
        }    
    }
    else if(strcmp(method,"POST")==0){
        if(!strcmp(reqfile,"/username")){
            char *content=strstr(buffer,"\r\n\r\n");
            if(content==NULL){
                content=strstr(buffer,"\n\n");
                if(content==NULL){
                    fprintf(stderr,"I can't get request content\n");
                }
            }
            content++;
            char *name=strstr(content,"=");name++;
            strcpy(username,name);
            // fprintf(stderr,"username = %s\n",username);
            send_homepage(201);
        }
    }
    return 1;
}

void get_method(char* buffer,char* method){
    strncpy(method,buffer,8);
    char *space=strstr(method," ");
    *space='\0';
}

void get_request_file(char* buffer,char* reqfile){
    int idx=0;
    char *space=strstr(buffer," ");space++;
    while(*space!=' ')
        reqfile[idx++]=*(space++);
    reqfile[idx]='\0';
}

int send_login(){
    char buf[4096]="\0";
    char index_buf[2048]="\0";
    int index,size;
    index=open("../template/index.html",O_RDONLY);
    if(index<=0) perror("open fail");
    size=read(index,index_buf,2048);
    if(size<=0) perror("read fail");
    set_http_header(buf,size,"text/html",200);
    strcat(buf,index_buf);
    send(req.conn_fd,buf,strlen(buf),MSG_NOSIGNAL);
    close(index);
    return 1;
}

int send_homepage(int status){
    char buf[4096]="\0";
    char index_buf[2048]="\0";
    int index,size;
    index=open("../template/homepage.html",O_RDONLY);
    if(index<=0) perror("open fail");
    size=read(index,index_buf,2048);
    if(size<=0) perror("read fail");
    size+=set_http_header(buf,size,"text/html",status);
    strcat(buf,index_buf);
    send(req.conn_fd,buf,size,MSG_NOSIGNAL);
    close(index);
    fprintf(stderr,"%s\n",buf);
    return 1;
}

int set_http_header(char *buf,int content_len,const char *filetype,int status){
    char code[64]="\0";
    if(status==200){
        strcpy(code,"HTTP/1.1 200 OK\r\n");
    }
    else if(status==201){
        strcpy(code,"HTTP/1.1 201 Created\r\n");
    }
    strcat(buf,code);
    strcat(buf,"Server: jdbhttpd/0.1.0\r\n");
    char content[64];
    sprintf(content,"Content-Length: %d\r\n",content_len);
    strcat(buf,content);
    strcat(buf,"Content-Type: ");
    strcat(buf,filetype);
    strcat(buf,"\r\nConnection: keep-alive\r\n\r\n");  
    return strlen(buf);  
}

void set_404_header(char *buf){
    strcat(buf,"HTTP/1.1 404 Bad Request\r\n");
    strcat(buf,"Server: jdbhttpd/0.1.0\r\n");
    strcat(buf,"Content-Length: 169\r\n");
    strcat(buf,"Content-Type: text/html\r\nConnection: Closed\r\n\r\n"); 
    strcat(buf,"<html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center><hr><center>nginx/0.8.54</center></body></html>"); 
}

void set_response(package *now,int type,int bufferSize,char *buffer,char sender[],char recver[]){
    now->type=type;
    now->buf_size=bufferSize;
    memset(now->buf,0,sizeof(now->buf));
    memcpy(now->buf,buffer,bufferSize);
    if(sender!=NULL)
        memcpy(now->sender,sender,64);
    else
        memset(now->sender,0,64);
    if(recver!=NULL)
        memcpy(now->recver,recver,64);
    else
        memset(now->recver,0,64);
}

void init_request(request *reqP){
    reqP->conn_fd=-1;
    reqP->filesize=0;
}

void free_request(request *reqP){
    close(reqP->conn_fd);
    init_request(reqP);
}

void init_client(unsigned short port){
    struct sockaddr_in servaddr;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        ERR_EXIT("socket");
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        ERR_EXIT("bind");
    }
    if (listen(listen_fd, 4) < 0){
        ERR_EXIT("listen");
    }
    init_request(&req);
    struct sockaddr_in cliaddr;
    int clilen = sizeof(cliaddr);
    int conn_fd = accept(listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
    if (conn_fd < 0){
        ERR_EXIT("accept");
    }
    req.conn_fd=conn_fd;
    return;
}

