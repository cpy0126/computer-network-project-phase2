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
    int conn_fd;  // fd to wait for a new connection
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

typedef struct request{
    int conn_fd;  // fd to talk with client
    int filesize;
    // int status;     
    // char buf[2048];  // data sent by/to client
    package send;
    std::string user_name;
} request;

server svr;
request req;
char username[64]="\0";
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

void set_response(package *now,int type,int bufferSize,char *buffer,char sender[],char recver[]);

void send2server(int type,int buf_size,char *buf,char *sender,char *recver);

void recv_server(int type,int buf_size,char *buf,char *sender,char *recver,package messege);

int file_select(const struct dirent *entry);

void get_data(char *buffer,char *data);

int check_username(char *tmp);

void remove_friend(char *fri);

int handle_server();

int main(int argc, char* argv[]){
    if (argc != 3){
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    mkdir("./client_dir", 0777);
    chdir("./client_dir");
    char ip[16];
    char *cross=strstr(argv[1],":");*cross='\0';cross++;
    strcpy(ip,argv[1]);
    int svr_port=atoi(cross);
    init_server(svr_port,ip);   //build connection with server
    int listen_fd=init_client((unsigned short)atoi(argv[2]));   //create socket to listen to browser
    fprintf(stderr, "Running \n");

    while(1){
        struct sockaddr_in cliaddr;
        int clilen = sizeof(cliaddr);
        // accept a new connection from browser
        int conn_fd = accept(listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
        if (conn_fd < 0){
            ERR_EXIT("accept");
        }
        req.conn_fd=conn_fd;
        fprintf(stderr, "\nstarting on %.80s, port %s connect fd %d\n", svr.hostname, argv[2],conn_fd);
        
        fd_set read_OK;
        while(1){
            FD_ZERO(&read_OK);
            FD_SET(req.conn_fd, &read_OK);FD_SET(svr.conn_fd, &read_OK);
            // select whether browser send request
            select(10,&read_OK,NULL,NULL,NULL);
            if(FD_ISSET(svr.conn_fd,&read_OK)){
                if(!handle_server()){
                    free_request(&req);
                    break;
                }  
            }
            if(FD_ISSET(req.conn_fd,&read_OK)){
                if(!handle_http()){ // handle with http request
                    free_request(&req);
                    break;
                }
            }
        }


    }

    return 0;
}

int handle_server(){
    package got;
    recv(svr.conn_fd,&got,sizeof(package),0);
    if(got.type==ADD){
        mkdir(got.buf,0777);
        char path[128]="./";
        strcat(path,got.buf);
        strcat(path,"/chat");
        creat(path,0777);
    }
    else if(got.type==DEL){
        remove_friend(got.buf);
    }
    return 1;
}

int handle_http(){
    char buffer[100000]="\0";
    int get=recv(req.conn_fd, buffer, 100000, 0);
    int sent=1;
    // fprintf(stderr,"%s\n",buffer);
    if(get<=0){
        return 0;
    }
    char method[8],reqfile[128];
    get_method(buffer,method);
    get_request_file(buffer,reqfile);
    // deal with get request, such as first connection with client
    if(strcmp(method,"GET")==0){    
        if(!strcmp(reqfile,"/")){   // receive 
            if(!send_login(0)){ 
                ERR_EXIT("Wrong sending");
                return 0;
            }
            return 1;
        }   
        else{
            char buf[4096]="\0";
            set_404_header(buf);
            send(req.conn_fd,buf,sizeof(buf),0);
            return 0;
        }
    }
    else if(strcmp(method,"POST")==0){  //deal with post request
        if(!strcmp(reqfile,"/username")){
            char name[64]="\0";
            get_data(buffer,name);
            send2server(LOGIN,strlen(name),name,NULL,NULL);
            package sent;
            recv(svr.conn_fd,&sent,sizeof(package),0);
            if(!strcmp(sent.buf,succeed)&&check_username(name)){
                strcpy(username,name);
                chdir("..");
                rename("./client_dir",username);
                chdir(username);
                if(!send_homepage())
                    return 0;
            }
            else{
                if(!send_login(1))
                    return 0;
            }
        }
        else if(!strcmp(reqfile,"/list_friend")){
            struct dirent **user_file;
            int n=scandir(".", &user_file, file_select, alphasort);
            char response[4096]="\0";
            char friend_list[3072]="\0";
            int index=open("../template/homepage.html",O_RDONLY);
            if(index<=0) perror("open fail");
            int size=read(index,friend_list,2048);
            if(size<=0) perror("read fail");
            for(int i=0;i<n;i++){
                char now[128]="\0";
                strcat(now,"<p>");
                strcat(now, user_file[i]->d_name);
                strcat(now, "</p>");
                strcat(friend_list, now);
                size+=strlen(now);
                free(user_file[i]);
            }
            free(user_file);
            set_http_header(response,size,"text/html");
            strcat(response,friend_list);
            send(req.conn_fd,response,strlen(response),0);
            return 1;
        }
        else if(!strcmp(reqfile,"/add_friend")){
            char name[64]="\0";
            get_data(buffer,name);
            send2server(ADD,strlen(name),name,NULL,NULL);
            package sent;
            recv(svr.conn_fd,&sent,sizeof(package),0);
            if(!send_homepage())
                return 0;
            if(!strcmp(sent.buf,succeed)){
                mkdir(name,0777);
                char path[128]="./";
                strcat(path,name);
                strcat(path,"/chat");
                creat(path,0777);
            }
        }
        else if(!strcmp(reqfile,"/del_friend")){
            char name[64]="\0";
            get_data(buffer,name);
            send2server(DEL,strlen(name),name,NULL,NULL);
            package sent;
            recv(svr.conn_fd,&sent,sizeof(package),0);
            if(!send_homepage())
                return 0;
            if(!strcmp(sent.buf,succeed)){
                remove_friend(name);
            }
        }
        else if(!strcmp(reqfile,"/chat_with")){
            
        }
        else{
            char buf[4096]="\0";
            set_404_header(buf);
            send(req.conn_fd,buf,sizeof(buf),0);
            return 0;
        }
    }
    else{   //deal with undefined behavior
        char buf[4096]="\0";
        set_404_header(buf);
        send(req.conn_fd,buf,sizeof(buf),0);
        return 0;
    }
    return 1;
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
    fprintf(stderr,"%s\n",buf);
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
    fprintf(stderr,"%s\n",buf);
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
