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
#define CMD 400


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

typedef struct request{
    int conn_fd;  // fd to talk with client
    int chatfd;
    int filesize;
    char username[64];
    char chat_friend[64];
    // int status;     
    // char buf[2048];  // data sent by/to client
    package send;
} request;

server svr;
request req;
char succeed[16]="Succeeed";
char failed[8]="Failed";

int init_client(unsigned short port);

void init_server(int port, char *ip);

void init_request(request* reqP);

void free_request(request* reqP);

void get_method(char* buffer,char* method);

void get_request_file(char* buffer,char* reqfile);

int set_http_header(char *buf,int content_len,const char *filetype);

void set_404_header(char *buf);

int send_login(int wrong);

int send_homepage();

int handle_http();

void set_response(package *now,int type,int bufferSize,char *buffer,char *sender,char *recver);

void send2server(int type,int buf_size,char *buf,char *sender,char *recver);

void recv_server(int type,int buf_size,char *buf,char *sender,char *recver,package messege);

int file_select(const struct dirent *entry);

void get_data(char *buffer,char *data);

int check_username(char *tmp);

void remove_friend(char *fri);

int handle_server();

void set_chatroom(char *buf,int len);

void set_latest(int fd,int piece,char *buf);

int main(int argc, char* argv[]){
<<<<<<< HEAD
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

// get http request method
void get_method(char* buffer,char* method){
    strncpy(method,buffer,8);
    char *space=strstr(method," ");
    *space='\0';
}

// get http request's file (thing after method)
void get_request_file(char* buffer,char* reqfile){
    int idx=0;
    char *space=strstr(buffer," ");space++;
    while(*space!=' ')
        reqfile[idx++]=*(space++);
    reqfile[idx]='\0';
}

void send2server(int type,int buf_size,char *buf,char *sender,char *recver){
    package sent;
    sent.buf_size=buf_size;
    sent.type=type;
    memcpy(sent.buf,buf,buf_size);
    if(sender!=NULL)
        memcpy(sent.sender,sender,64);
    if(recver!=NULL)
        memcpy(sent.recver,recver,64);
    send(svr.conn_fd,&sent,sizeof(package),0);
}

/* send index.html to browser
 * when wrong is True, ask user to enter name again*/
int send_login(int wrong){
    char buf[4096]="\0";
    char index_buf[2048]="\0";
    char used[128]="<html><h2>Username used, please choose another</h2></html>";
    int index,size;
    index=open("../template/index.html",O_RDONLY);
    if(index<=0) perror("open fail");
    size=read(index,index_buf,2048);
    if(size<=0) perror("read fail");
    
    if(wrong){
        size+=strlen(used);
        strcat(index_buf,used);
    }
    set_http_header(buf,size,"text/html");
    strcat(buf,index_buf);
    int sent=send(req.conn_fd,buf,strlen(buf),MSG_NOSIGNAL);
    close(index);
    return (sent<=0)?0:1;
}

void get_data(char *buffer,char *data){
    char *tmp=strstr(buffer,"\r\n\r\n");
    if(tmp==NULL){
        tmp=strstr(buffer,"\n\n");
        if(tmp==NULL){
            fprintf(stderr,"I can't get request content\n");
        }
    }
    tmp++;
    char *name=strstr(tmp,"=");name++;
    strncpy(data,name,strlen(name));
}

// send homepage.html to browser
int send_homepage(){
    char buf[4096]="\0";
    char index_buf[2048]="\0";
    int index,size;
    index=open("../template/homepage.html",O_RDONLY);
    if(index<=0) perror("open fail");
    size=read(index,index_buf,2048);
    if(size<=0) perror("read fail");
    size+=set_http_header(buf,size,"text/html");
    strcat(buf,index_buf);
    int sent=send(req.conn_fd,buf,size,MSG_NOSIGNAL);
    close(index);
    return (sent<=0)?0:1;
}

// set http header of response, save it to buf
int set_http_header(char *buf,int content_len,const char *filetype){
    char code[64]="\0";
    strcpy(code,"HTTP/1.1 200 OK\r\n");
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

// set http 404 to browser
void set_404_header(char *buf){
    strcat(buf,"HTTP/1.1 404 Bad Request\r\n");
    strcat(buf,"Server: jdbhttpd/0.1.0\r\n");
    strcat(buf,"Content-Length: 169\r\n");
    strcat(buf,"Content-Type: text/html\r\nConnection: Closed\r\n\r\n"); 
    strcat(buf,"<html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center><hr><center>nginx/0.8.54</center></body></html>"); 
}

// set response to server, save it to now
void set_response(package *now,int type,int bufferSize,char *buffer,char *sender,char *recver){
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

// build connection with browser
int init_client(unsigned short port){
    struct sockaddr_in servaddr;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        ERR_EXIT("socket");
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    int tmp=1;
    if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        ERR_EXIT("bind");
    }
    if (listen(listen_fd, 4) < 0){
        ERR_EXIT("listen");
    }
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&tmp, sizeof(tmp)) < 0){
        ERR_EXIT("setsockopt");
    }
    init_request(&req);
    
    return listen_fd;
}

// build connection with server
void init_server(int port, char *ip){
	svr.conn_fd=socket(AF_INET, SOCK_STREAM, 0);
    if (svr.conn_fd < 0)
        ERR_EXIT("socket");
    svr.port=port;
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
	if(connect(svr.conn_fd, (struct sockaddr *)&addr, sizeof(addr))<0){
		perror("socket wrong");
        exit(0);
	}
}

int file_select(const struct dirent *entry){
   return strcmp(entry->d_name, ".")&&strcmp(entry->d_name, "..");
}

int check_username(char *tmp){
    while(*tmp!='\0'){
        char now=*tmp;
        if((now-'a')*(now-'z')>0&&(now-'A')*(now-'Z')>0&&(now-'0')*(now-'9')>0)
            return 0;
        tmp++;
    }
    return 1;
}

void remove_friend(char *fri){
    char path[128]="./";
    strcat(path,fri);
    strcat(path,"/chat");
    remove(path);
    rmdir(fri);
}

void set_chatroom(char *buf,int len){
    char index_buf[2048]="\0";
    int index,size;*buf='\0';
    index=open("../template/chatroom.html",O_RDONLY);
    if(index<=0) perror("open fail");
    size=read(index,index_buf,2048);
    if(size<=0) perror("read fail");
    size+=set_http_header(buf,size+len,"text/html");
    strcat(buf,index_buf);
    close(index);
}

void set_latest(int fd,int piece,char *buf){
    *buf='\0';
    lseek(fd,0,SEEK_SET);
    int fsize=lseek(fd,0,SEEK_END);
    if(fsize<0)
        perror("open failed");
    lseek(fd,-min(fsize,piece*(int)sizeof(package)),SEEK_END);
    package *his=(package *)malloc(piece*sizeof(package));
    long long int rsize=read(fd,his,piece*sizeof(package));
    
    for(int a=0;a<rsize/sizeof(package);a++){
        strcat(buf,"<p>");
        strcat(buf,his[a].sender);
        strcat(buf," : ");
        strcat(buf,his[a].buf);
        strcat(buf,"</p>");
    }
    lseek(fd,0,SEEK_SET);
}
