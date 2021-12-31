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
#define LIST 700
#define CHECK 800
#define HIS 900
#define GET 999


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
    int myfile_fd;
    int friend_fd;
    package now;
    string user_name;
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
        return 0;
    }
    return 1;
}

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

int file_select(const struct dirent *entry){
   return strcmp(entry->d_name, ".")&&strcmp(entry->d_name, "..");
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
    char failed[16]="Failed";
    map<string,int> user_names;

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
            requestP[conn_fd].myfile_fd=-1;
            requestP[conn_fd].friend_fd=-1;
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, inet_ntoa(cliaddr.sin_addr));
            
            continue;
        }

        for(int conn_fd=4;conn_fd<=maxfd;conn_fd++){
            if(!FD_ISSET(conn_fd, &read_OK))
                continue;
            handle_read(&requestP[conn_fd]);
            
            if(requestP[conn_fd].now.type==LOGIN){    // inputing username
                package response;
                string new_user=string(requestP[conn_fd].now.buf);
                if(user_names.find(new_user)!=user_names.end()){
                    set_response(&response,LOGIN,strlen(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                    continue;
                }
                user_names[new_user]=requestP[conn_fd].conn_fd;
                mkdir(new_user.c_str(),0777);
                set_response(&response,LOGIN,strlen(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                requestP[conn_fd].user_name=new_user;
                continue;
            }
            else if(requestP[conn_fd].now.type==ADD){    // Adding friend
                package response;
                string new_friend = string(requestP[conn_fd].now.buf);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,ADD,strlen(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                    continue;
                }
                
                string friend_name="./"+requestP[conn_fd].user_name+"/"+new_friend;
                mkdir(friend_name.c_str(),0777);
                friend_name=friend_name+"/chat";
                creat(friend_name.c_str(),0777);

                string friend_name2="./"+new_friend+"/"+requestP[conn_fd].user_name;
                mkdir(friend_name2.c_str(),0777);
                friend_name2=friend_name2+"/chat";
                creat(friend_name2.c_str(),0777);

                set_response(&response,ADD,strlen(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
            }
            else if(requestP[conn_fd].now.type==DEL){    // Deleteing friend
                package response;
                string new_friend = string(requestP[conn_fd].now.buf);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,DEL,strlen(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                    continue;
                }
                
                string friend_name="./"+requestP[conn_fd].user_name+"/"+new_friend;
                string tmp=friend_name+"/chat";
                remove(tmp.c_str());
                rmdir(friend_name.c_str());

                string friend_name2="./"+new_friend+"/"+requestP[conn_fd].user_name;
                string tmp2=friend_name2+"/chat";
                remove(tmp2.c_str());
                rmdir(friend_name2.c_str());

                set_response(&response,DEL,strlen(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
            }
            else if(requestP[conn_fd].now.type==MSS){    // receve message
                package response;
                string new_friend = string(requestP[conn_fd].now.recver);
                if(user_names.find(new_friend)==user_names.end()){
                    set_response(&response,MSS,strlen(failed),failed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                    continue;
                }
                string friend_name="./"+requestP[conn_fd].user_name+"/"+new_friend+"/chat";
                int me=open(friend_name.c_str(),O_WRONLY|O_APPEND);
                write(me,&(requestP[conn_fd].now),sizeof(package));
                close(me);
                cerr << (string)requestP[conn_fd].now.buf << endl;
                if(requestP[conn_fd].user_name==new_friend){
                    set_response(&response,MSS,strlen(succeed),succeed,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                    continue;
                }
                string friend_name2="./"+new_friend+"/"+requestP[conn_fd].user_name+"/chat";
                int you=open(friend_name2.c_str(),O_WRONLY|O_APPEND);
                write(you,&(requestP[conn_fd].now),sizeof(package));
                close(you);

                set_response(&response,MSS,strlen(succeed),succeed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
                // send(user_names[new_friend],&(requestP[conn_fd].now),sizeof(package),MSG_NOSIGNAL);
            }
            else if(requestP[conn_fd].now.type==IMG){   // receve image
                if(requestP[conn_fd].friend_fd==-1&&requestP[conn_fd].myfile_fd==-1){
                    string file_name=(string)(requestP[conn_fd].now.buf);
                    string friend_name=(string)(requestP[conn_fd].now.recver);
                    string friend_chat="./"+requestP[conn_fd].user_name+"/"+friend_name+"/chat";
                    string my_chat="./"+friend_name+"/"+requestP[conn_fd].user_name+"/chat";
                    string friend_file="./"+requestP[conn_fd].user_name+"/"+friend_name+"/"+file_name;
                    string my_file="./"+friend_name+"/"+requestP[conn_fd].user_name+"/"+file_name;
                    
                    int me=open(friend_chat.c_str(),O_WRONLY|O_APPEND);
                    write(me,&(requestP[conn_fd].now),sizeof(package));
                    close(me);
                    if((requestP[conn_fd].friend_fd=open(my_file.c_str(),O_WRONLY|O_CREAT|O_APPEND))<0)
                        perror("open file error: ");
                    if(requestP[conn_fd].user_name==friend_name)
                        continue;

                    int you=open(my_chat.c_str(),O_WRONLY|O_APPEND);
                    write(you,&(requestP[conn_fd].now),sizeof(package));
                    close(you);
                    if((requestP[conn_fd].myfile_fd=open(friend_file.c_str(),O_WRONLY|O_CREAT|O_APPEND))<0)
                        perror("open file error: ");
                    continue;
                }
                
                if(!strcmp(requestP[conn_fd].now.buf,succeed)){
                    if(requestP[conn_fd].friend_fd!=-1)
                        close(requestP[conn_fd].friend_fd);
                    if(requestP[conn_fd].myfile_fd!=-1)
                        close(requestP[conn_fd].myfile_fd);
                    requestP[conn_fd].myfile_fd=-1;
                    requestP[conn_fd].friend_fd=-1;
                    continue;
                }

                if(requestP[conn_fd].friend_fd!=-1){
                    write(requestP[conn_fd].friend_fd,requestP[conn_fd].now.buf,requestP[conn_fd].now.buf_size);
                }
                if(requestP[conn_fd].myfile_fd!=-1){
                    write(requestP[conn_fd].myfile_fd,requestP[conn_fd].now.buf,requestP[conn_fd].now.buf_size);
                }

            }
            else if(requestP[conn_fd].now.type==FILES){    // receve file
                if(requestP[conn_fd].friend_fd==-1&&requestP[conn_fd].myfile_fd==-1){
                    string file_name=(string)(requestP[conn_fd].now.buf);
                    string friend_name=(string)(requestP[conn_fd].now.recver);
                    string friend_chat="./"+requestP[conn_fd].user_name+"/"+friend_name+"/chat";
                    string my_chat="./"+friend_name+"/"+requestP[conn_fd].user_name+"/chat";
                    string friend_file="./"+requestP[conn_fd].user_name+"/"+friend_name+"/"+file_name;
                    string my_file="./"+friend_name+"/"+requestP[conn_fd].user_name+"/"+file_name;
                    
                    int me=open(friend_chat.c_str(),O_WRONLY|O_APPEND);
                    write(me,&(requestP[conn_fd].now),sizeof(package));
                    close(me);
                    if((requestP[conn_fd].friend_fd=open(my_file.c_str(),O_WRONLY|O_CREAT|O_APPEND))<0)
                        perror("open file error: ");
                    if(requestP[conn_fd].user_name==friend_name)
                        continue;

                    int you=open(my_chat.c_str(),O_WRONLY|O_APPEND);
                    write(you,&(requestP[conn_fd].now),sizeof(package));
                    close(you);
                    if((requestP[conn_fd].myfile_fd=open(friend_file.c_str(),O_WRONLY|O_CREAT|O_APPEND))<0)
                        perror("open file error: ");
                    continue;
                }
                
                if(!strcmp(requestP[conn_fd].now.buf,succeed)){
                    if(requestP[conn_fd].friend_fd!=-1)
                        close(requestP[conn_fd].friend_fd);
                    if(requestP[conn_fd].myfile_fd!=-1)
                        close(requestP[conn_fd].myfile_fd);
                    requestP[conn_fd].myfile_fd=-1;
                    requestP[conn_fd].friend_fd=-1;
                    continue;
                }

                if(requestP[conn_fd].friend_fd!=-1){
                    write(requestP[conn_fd].friend_fd,requestP[conn_fd].now.buf,requestP[conn_fd].now.buf_size);
                }
                if(requestP[conn_fd].myfile_fd!=-1){
                    write(requestP[conn_fd].myfile_fd,requestP[conn_fd].now.buf,requestP[conn_fd].now.buf_size);
                }
                
            }
            else if(requestP[conn_fd].now.type==LIST){
                struct dirent **user_file;
                string now_user="./"+requestP[conn_fd].user_name;
                int n=scandir(now_user.c_str(), &user_file, file_select, alphasort);
                string friend_num=to_string(n);
                package num;
                int size=0;
                for(int i=0;i<n;i++){
                    string f_name=(string)(user_file[i]->d_name);
                    size+=f_name.size();
                }
                string data_size=to_string(size);
                friend_num=friend_num+" "+data_size;
                set_response(&num,LIST,friend_num.size(),(char *)friend_num.c_str(),NULL,NULL);
                send(requestP[conn_fd].conn_fd,&num,sizeof(package),MSG_NOSIGNAL);
                for(int i=0;i<n;i++){
                    string f_name=(string)(user_file[i]->d_name);
                    package tmp;
                    set_response(&tmp,LIST,f_name.size(),(char *)f_name.c_str(),NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&tmp,sizeof(package),MSG_NOSIGNAL);
                    free(user_file[i]);
                }
                free(user_file);
            }
            else if(requestP[conn_fd].now.type==CHECK){
                string friend_name=(string)(requestP[conn_fd].now.buf);
                struct dirent **user_file;
                string now_user="./"+requestP[conn_fd].user_name;
                int n=scandir(now_user.c_str(), &user_file, file_select, alphasort);
                int got=0;
                for(int i=0;i<n;i++){
                    string f_name=(string)(user_file[i]->d_name);
                    if(f_name==friend_name) got=1;
                    free(user_file[i]);
                }
                free(user_file);
                package response;
                if(got)
                    set_response(&response,CHECK,strlen(succeed),succeed,NULL,NULL);
                else
                    set_response(&response,CHECK,strlen(failed),failed,NULL,NULL);
                send(requestP[conn_fd].conn_fd,&response,sizeof(package),MSG_NOSIGNAL);
            }
            else if(requestP[conn_fd].now.type==HIS){
                int n=atoi(requestP[conn_fd].now.buf);
                string friend_path="./"+requestP[conn_fd].user_name+"/"+string(requestP[conn_fd].now.recver)+"/chat";
                int fd=open(friend_path.c_str(),O_RDONLY);
                int his_size=lseek(fd,0,SEEK_END)/sizeof(package);
                lseek(fd,-min(his_size,n)*sizeof(package),SEEK_END);
                for(int a=0;a<min(his_size,n);a++){
                    package tmp;
                    read(fd,&tmp,sizeof(package));
                    send(requestP[conn_fd].conn_fd,&tmp,sizeof(package),MSG_NOSIGNAL);
                }
                close(fd);
                send(requestP[conn_fd].conn_fd,&(requestP[conn_fd].now),sizeof(package),MSG_NOSIGNAL);
            }
            else if(requestP[conn_fd].now.type==GET){
                string friend_name=(string)(requestP[conn_fd].now.recver);
                string my_name=(string)(requestP[conn_fd].now.sender);
                string file_name=(string)(requestP[conn_fd].now.buf);
                string file_path="./"+my_name+"/"+friend_name+"/"+file_name;
                int file_fd=open(file_path.c_str(),O_RDONLY);
                long long int file_size=lseek(file_fd,0,SEEK_END);
                lseek(file_fd,0,SEEK_SET);
                string size=to_string(file_size);
                package res_size;
                set_response(&res_size,GET,size.size(),(char *)size.c_str(),NULL,NULL);
                send(requestP[conn_fd].conn_fd,&res_size,sizeof(package),MSG_NOSIGNAL);
                while(file_size>0){
                    package tmp;
                    char buf[2048];memset(buf,0,2048);
                    int read_size=read(file_fd,buf,2048);
                    set_response(&tmp,GET,read_size,buf,NULL,NULL);
                    send(requestP[conn_fd].conn_fd,&tmp,sizeof(package),MSG_NOSIGNAL);
                    file_size-=read_size;
                }
                close(file_fd);
            }
        }

    }

}


void init_request(request *reqP){
    reqP->conn_fd=-1;
    reqP->filesize=0;
    reqP->myfile_fd=-1;
    reqP->friend_fd=-1;
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

