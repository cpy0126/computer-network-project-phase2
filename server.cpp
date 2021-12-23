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
#include<map>
using namespace std;

#define ERR(a) {perror(a): return EXIT_FAILURE;}
#define IMG 100
#define FILES 200
#define MSS 300
#define LOGIN 400
#define ADD 500
#define DEL 600


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

typedef struct request{
    int conn_fd;  // fd to talk with client
    int filesize;
    // int status;     
    // char buf[2048];  // data sent by/to client
    package now;
    std::string user_name;
} request;

server svr;
request* requestP = NULL;
int maxfd;
int readFD[100000],writeFD[100000];


void init_server(unsigned short port);

void init_request(request* reqP);

void free_request(request* reqP);

int handle_read(request* reqP){
    if(recv(reqP->conn_fd, &(reqP->now), sizeof(package), 0)<=0){
        // need to free request
        
        return 0;
    }
    // reqP->now.buf[reqP->now.buf_size]='\0';
    return 1;
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

int main(int argc, char* argv[]){
    if (argc != 2){
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }
    mkdir("./server_dir", 0777);
    chdir("./server_dir");

    struct sockaddr_in cliaddr; // used by accept()
    char username_used[16]="Username used";
    char succeed[16]="Succeeed";
    char failed[8]="Failed";
    std::map<string,int> user_names;

    // Initialize server
    init_server((unsigned short)atoi(argv[1]));
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);


    fd_set read_OK;
    fd_set write_OK;

    while(1){
        FD_ZERO(&read_OK);
        FD_ZERO(&write_OK);
        FD_SET(svr.listen_fd, &read_OK);
        for(int a=4;a<=maxfd;a++){   // choosing clients ready for write/read
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
            // package response;
            // char command[8]="init";// input your username: (alphabet and digits only)\n
            // set_response(&response,CMD,sizeof(command),command,std::string(),std::string());
            // send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
            
            // handle_read(&requestP[conn_fd]);
            // fprintf(stderr, "%s\n",requestP[conn_fd].now.buf);

            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, inet_ntoa(cliaddr.sin_addr));
            
            continue;
        }

        for(int conn_fd=4;conn_fd<=maxfd;conn_fd++){
            if(!FD_ISSET(conn_fd, &read_OK)&&!FD_ISSET(conn_fd, &write_OK))
                continue;
            handle_read(&requestP[conn_fd]);
            
            if(requestP[conn_fd].now.type==LOGIN){    // inputing username
                package response;
                string new_user=string(requestP[conn_fd].now.buf);
                if(user_names.find(new_user)!=user_names.end()){
                    set_response(&response,LOGIN,sizeof(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                    continue;
                }
                user_names[new_user]=requestP[conn_fd].conn_fd;
                mkdir(new_user.c_str(),0777);
                set_response(&response,LOGIN,sizeof(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                requestP[conn_fd].user_name=new_user;
                continue;
            }
            else if(requestP[conn_fd].now.type==ADD){    // Adding friend
                package response;
                string new_friend = string(requestP[conn_fd].now.buf);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,ADD,sizeof(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                    continue;
                }
                char friend_name[150]="./";
                strcat(friend_name,requestP[conn_fd].user_name.c_str());
                strcat(friend_name,"/");
                strcat(friend_name,new_friend.c_str());
                creat(friend_name,0777);

                char friend_name2[150]="./";
                strcat(friend_name2,new_friend.c_str());
                strcat(friend_name2,"/");
                strcat(friend_name2,requestP[conn_fd].user_name.c_str());
                creat(friend_name2,0777);

                set_response(&response,ADD,sizeof(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                send(user_names[new_friend],&response,sizeof(response),0);
            }
            else if(requestP[conn_fd].now.type==DEL){    // Deleteing friend
                package response;
                string new_friend = string(requestP[conn_fd].now.buf);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,DEL,sizeof(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                    continue;
                }
                char friend_name[150]="./";
                strcat(friend_name,requestP[conn_fd].user_name.c_str());
                strcat(friend_name,"/");
                strcat(friend_name,new_friend.c_str());
                remove(friend_name);

                char friend_name2[150]="./";
                strcat(friend_name2,new_friend.c_str());
                strcat(friend_name2,"/");
                strcat(friend_name2,requestP[conn_fd].user_name.c_str());
                remove(friend_name2);

                set_response(&response,DEL,sizeof(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                send(user_names[new_friend],&response,sizeof(response),0);
            }
            else if(requestP[conn_fd].now.type==MSS){    // receve message
                package response;
                string new_friend = string(requestP[conn_fd].now.recver);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,MSS,sizeof(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
fprintf(stderr, "This should not happen %s send to %s\n",requestP[conn_fd].now.sender,requestP[conn_fd].now.recver);
                    continue;
                }
                char friend_name[150]="./";
                strcat(friend_name,requestP[conn_fd].user_name.c_str());
                strcat(friend_name,"/");
                strcat(friend_name,new_friend.c_str());
                int me=open(friend_name,O_APPEND);
                write(me,&(requestP[conn_fd].now),sizeof(package));
                close(me);

                char friend_name2[150]="./";
                strcat(friend_name2,new_friend.c_str());
                strcat(friend_name2,"/");
                strcat(friend_name2,requestP[conn_fd].user_name.c_str());
                int you=remove(friend_name2);
                write(you,&(requestP[conn_fd].now),sizeof(package));
                close(you);

                set_response(&response,DEL,sizeof(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(response),0);
                send(user_names[new_friend],&response,sizeof(response),0);
            }
            else if(requestP[conn_fd].now.type==IMG){    // receve image
                
            }
            else if(requestP[conn_fd].now.type==FILES){    // receve file
                
            }

        }



    }

}


void init_request(request *reqP){
    reqP->conn_fd=-1;
    reqP->filesize=0;
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

