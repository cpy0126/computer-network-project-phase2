#include<arpa/inet.h>
#include<dirent.h>
#include<fcntl.h>
#include<iostream>
#include<memory.h>
#include<netinet/in.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<time.h>
#include<unistd.h>
#include<string.h>
#include<vector>
#include<map>
using namespace std;

#define ERR(a) {perror(a); exit(1);}
#define IMG 100
#define FILES 200
#define MSS 300
#define LOGIN 400
#define LOGOUT -400
#define ADD 500
#define DEL 600
#define LIST 700
#define CHECK 800
#define HIS 900
#define GET 999

string shead, boundary;
map<string, string> mmap;

struct package{
    int type, buf_size;
    char buf[2048], sender[64], recver[64];
    time_t Time;
    package(){}
    package(int _type, string buffer){
        memset(buf, 0, sizeof(buf));
        memset(sender, 0, sizeof(sender));
        memset(recver, 0, sizeof(recver));
        Time = time(NULL);
        type = _type;
        buf_size = buffer.length();
        strncpy(buf, buffer.c_str(), buffer.length());
    }
    package(int _type, string buffer, string& _sender, string& _recver){
        memset(buf, 0, sizeof(buf));
        memset(sender, 0, sizeof(sender));
        memset(recver, 0, sizeof(recver));
        Time = time(NULL);
        type = _type;
        buf_size = buffer.length();
        strncpy(buf, buffer.c_str(), buffer.length());
        strncpy(sender, _sender.c_str(), _sender.length());
        strncpy(recver, _recver.c_str(), _recver.length());
    }
};

char buf[100000],head_buf[10000];
char succeed[16]="Succeeed", failed[8]="Failed";
int bufsize,headsize,sock_fd,http_fd,cli_fd;
int filesize;
string username, target;

int login();
int get(string path, int extra);
int post(string event, int body_size);

int read_package(package &pkg){
    int tmp = 0, res;
    memset(buf, 0, sizeof(package));
    while(tmp<sizeof(package)){
        while((res = read(sock_fd, buf+tmp, sizeof(package)-tmp))<=0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        tmp += res;
    }
    memcpy(&pkg, buf, sizeof(package));
    return 0;
}

int write_package(package &pkg){
    int res;
    while((res = send(sock_fd, &pkg, sizeof(package), MSG_NOSIGNAL))<0){
        if(errno==EAGAIN) continue;
        return -1;
    }
    return 0;
}

void show_home(){
    cout<<"Home\n\
    (1) List all friends\n\
    (2) Add friend\n\
    (3) Delete friend\n\
    (4) Chat with one friend\n\
    (5) exit\n\
    Please enter your commend like Ex: \"4 Bob\" or \"1\" or \"2 Alice\"\n";
}
void show_chat(string name){
    cout<<"================================\nChatroom with "<<name<<"\n================================\n";
    cout<<"\
    (1) Send text message (max size: 2048)\n\
    (2) Send image message\n\
    (3) Send file message\n\
    (4) Update chat history\n\
    (5) Back to Homepage\n\
    (6) exit\n\
    Please enter your commend like Ex: \"4\" or \"5\" or \"1 hello\" or \"2 kkk.pdf\"\n";
}

void show_his(string size){
    int latest = stoi(size);
    package pkg(HIS, size, username, target);
    if(write_package(pkg)<0) ERR("HIS wr error: ")
    int extra = 0;
    show_chat(target);
    vector<package> downloadlist;
    while(1){
        if(read_package(pkg)<0) ERR("HIS rd error: ")
        if(pkg.type==HIS) break;
        cout<<(string)pkg.sender<<" : ";
        if(pkg.type==MSS)
            cout<<(string)pkg.buf<<endl;
        if(pkg.type==IMG){
            cout<<"[IMG  "<<(string)pkg.buf<<"]"<<endl;
            downloadlist.push_back(pkg);
        }
        if(pkg.type==FILES){
            string tmp = (string)pkg.buf;
            int res = tmp.find("\n");
            tmp = tmp.substr(res+1);
            memset(pkg.buf, 0, sizeof(pkg.buf));
            strncpy(pkg.buf, tmp.c_str(), tmp.length());
            cout<<"[FILES  "<<(string)pkg.buf<<"]"<<endl;
            downloadlist.push_back(pkg);
        }
    }
    for(int i=0;i<((int)downloadlist.size());++i){
        cerr << "downloading" << downloadlist[i].buf << endl;
        package pkg(GET, (string)downloadlist[i].buf);
        int res;
                    
        strncpy(pkg.sender, username.c_str(), username.length());
        strncpy(pkg.recver, target.c_str(), target.length());

        if(write_package(pkg)<0) ERR("download wr error: ")
        if(read_package(pkg)<0) ERR("download rd error: ")

        if((string)pkg.buf=="-1"){
            cout<<"error occur when taking file\n";
        }

        filesize = atoi(pkg.buf);
        int fd=open(downloadlist[i].buf,O_CREAT|O_WRONLY, 0777);
                    
        while(filesize>0){
            memset(pkg.buf, 0, sizeof(pkg.buf));
            if(read_package(pkg)<0) ERR("Getting package error: ")
            write(fd,pkg.buf,pkg.buf_size);
            filesize -= pkg.buf_size;
        }
    }
}

void bad_commend(){
    cout<<"Wrong commend format please input again\n";
}

int main(int argc, char* argv[]){
    
    if(argc!=3){
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
    if((res = atoi(argv[2]))<0 || res>65535){
        cout<<"browser port should be a number between 0 to 65535."<<endl;
        return 1;
    }
    httpaddr.sin_port = htons(res);
    httpaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //TCP client
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) ERR("socket()")
    if(connect(sock_fd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) ERR("connect()")

    //HTTP server
    mkdir("client_dir",0777);
    chdir("./client_dir");
    fd_set read_OK;
    cout<<"Welcome to cHatrOoM\nPlase enter your username:\n";
    while(1){
        getline(cin,username);
        package login(LOGIN,username),tmp;
        int good_name=1;
        for(int a=0;a<username.size();a++){
            if(isalnum(username[a])) continue;
            good_name=0;
            break;
        }
        if(!good_name){
            cout<<"Welcome to cHatrOoM\nPlase enter your username:\nUsernaem contains alpha and integer only\n";
            continue;
        }
        write_package(login);
        read_package(tmp);
        if(!strcmp(tmp.buf,succeed))
            break;
        cout<<"Welcome to cHatrOoM\nPlase enter your username:\nUsername used please enter again\n";
    }

    show_home();
    int status=1;   // 1 at homepage 2 at chatroom;
    while(1){
        string commend;
        getline(cin,commend);
        if(status==1){
            if(commend=="1"){
                package pkg; pkg.type = LIST;
                if(write_package(pkg)<0) ERR("LIST wr error: ")
                if(read_package(pkg)<0) ERR("LIST rd error: ")

                string tmp = (string) pkg.buf;
                res = tmp.find(" ");
                int num = stoi(tmp.substr(0,res));
                show_home();
                cout<<"Friend name list: \n";
                for(int i=0;i<num;++i){
                    if(read_package(pkg)<0) return -1;
                    cout<<"    "<<(string)pkg.buf<<endl;
                }
                
            }
            else if(commend=="5"){
                package pkg(LOGOUT, "");
                write_package(pkg);
                exit(0);
            }
            else{
                int space=commend.find(" ");
                if(space!=1){
                    bad_commend();
                    continue;
                }
                int act=atoi(&(commend.c_str()[0]));
                if(act==2){
                    string name=commend.substr(2);
                    package pkg(ADD, name);
                    if(write_package(pkg)<0) ERR("ADD wr error: ")
                    if(read_package(pkg)<0) ERR("ADD rd error: ")
                    show_home();
                }
                else if(act==3){
                    string name=commend.substr(2);
                    package pkg(CHECK, name);
                    if(write_package(pkg)<0) ERR("DEL che wr error: ")
                    if(read_package(pkg)<0) ERR("DEL che rd error: ")

                    if(((string)pkg.buf)=="Succeeed"){
                        pkg = package(DEL, name);
                        if(write_package(pkg)<0) ERR("DEL wr error: ")
                        if(read_package(pkg)<0) ERR("DEL rd error: ")
                    }
                    show_home();
                }
                else if(act==4){
                    string name=commend.substr(2);
                    package pkg(CHECK, name);
                    if(write_package(pkg)<0) ERR("CHECK write error: ")
                    if(read_package(pkg)<0) ERR("CHECK read error: ")
                    if(((string)pkg.buf)=="Succeeed"){
                        target = name;
                        status=2;
                        show_chat(name);
                        show_his("30");
                    }
                    else{
                        show_home();
                        cout<<"No such friend\n";
                    }
                }
                else{
                    bad_commend();
                    continue;

                }
            }
        
        }
        else if(status==2){
            if(commend=="4"){
                show_his("30");
            }
            else if(commend=="5"){
                target="";
                status=1;
                show_home();
            }
            else if(commend=="6"){
                package pkg(LOGOUT, "");
                write_package(pkg);
                exit(0);
            }
            else{
                int space=commend.find(" ");
                if(space!=1){
                    bad_commend();
                    continue;
                }
                int act=atoi(&(commend.c_str()[0]));
                if(act==1){
                    string content = commend.substr(2);
                    package pkg(MSS, content, username, target);
                    if(write_package(pkg)<0) ERR("send mss wr error: ")
                    if(read_package(pkg)<0) ERR("send mss rd error: ")
                }
                else if(act==2){
                    string filename = commend.substr(2);
                    int file=open(filename.c_str(),O_RDONLY);
                    if(file<0){
                        cout<<"No such image "<<endl;
                        continue;
                    }
                    int file_size=lseek(file,0,SEEK_END);
                    lseek(file,0,SEEK_SET);

                    res = filename.rfind(".");
                    filename = filename.substr(res);
                    package pkg;
                    time(&pkg.Time);
                    filename = to_string(pkg.Time) + filename;
                    pkg = package(IMG, filename, username, target);
                    pkg.buf_size=file_size;
                    if(write_package(pkg)<0) return -1;
                    while(file_size>0){
                        memset(pkg.buf, 0, sizeof(pkg.buf));
                        while((res = read(file, &pkg.buf, min(2048, file_size)))<=0){
                            if(errno==EAGAIN) continue;
                            ERR("reading file error: ")
                        }
                        pkg.buf_size = res;
                        if(write_package(pkg)<0) ERR("writing file error: ")
                        file_size -= res;
                    }
                    pkg = package(IMG, (string)"Succeeed", username, target);
                    if(write_package(pkg)<0) ERR("writing file error: ")
                }
                else if(act==3){
                    string filename = commend.substr(2);
                    int file=open(filename.c_str(),O_RDONLY);
                    if(file<0){
                        cout<<"No such image "<<endl;
                        continue;
                    }
                    int file_size=lseek(file,0,SEEK_END);
                    lseek(file,0,SEEK_SET);

                    string orgfilename = filename + "\n";
                    res = filename.rfind(".");
                    filename = filename.substr(res);
                    package pkg;
                    time(&pkg.Time);
                    filename = orgfilename + to_string(pkg.Time) + filename;
                    pkg = package(FILES, filename, username, target);
                    pkg.buf_size=file_size;
                    if(write_package(pkg)<0) return -1;
                    while(file_size>0){
                        memset(pkg.buf, 0, sizeof(pkg.buf));
                        while((res = read(file, &pkg.buf, min(2048, file_size)))<=0){
                            if(errno==EAGAIN) continue;
                            ERR("reading file error: ")
                        }
                        pkg.buf_size = res;
                        if(write_package(pkg)<0) ERR("writing file error: ")
                        file_size -= res;
                    }
                    pkg = package(FILES, (string)"Succeeed", username, target);
                    if(write_package(pkg)<0) ERR("writing file error: ")
                }
                else{
                    bad_commend();
                    continue;
                }
                show_chat(target);
                show_his("30");
            }

        }
        
    }
}
