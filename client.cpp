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
#define LIST 700

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
string used = "<html><h2>Username used or illegal, please choose another</h2></html>", recver;

struct package{
    int type, buf_size;
    char buf[2048], sender[64], recver[64];
    time_t Time;
    package(){
        type = buf_size = 0;
        memset(buf, 0, sizeof(buf));
        memset(sender, 0, sizeof(sender));
        memset(recver, 0, sizeof(recver));
        Time = time(NULL);
    }
    package(int _type, string& buffer){
        package();
        type = _type;
        buf_size = buffer.length();
        memcpy(buf, buffer.c_str(), sizeof(buffer));
    }
    package(int _type, string& buffer, string& _sender, string& _recver){
        package();
        type = _type;
        buf_size = buffer.length();
        memcpy(buf, buffer.c_str(), sizeof(buffer));
        memcpy(sender, _sender.c_str(), sizeof(_sender));
        memcpy(recver, _recver.c_str(), sizeof(_recver));
    }
};

char buf[1000000],head_buf[1000];
int bufsize,headsize,sock_fd,http_fd,cli_fd,logflag;
long long int filesize;

int index();
int get(string path, int extra=0);
int post(string event, int body_size=0);

int read_package(package &pkg){
    int tmp = 0, res;
    while(tmp!=sizeof(package)){
        while((res = read(sock_fd, &pkg+tmp, sizeof(package)-tmp))<=0){
            if(res<0 && errno==EAGAIN) continue;
            return -1;
        }
        tmp += res;
    }
    return 0;
}

int write_package(package &pkg){
    int res;
    while((res = write(sock_fd, &pkg, sizeof(package)))<0){
        if(res<0 && errno==EAGAIN) continue;
        return -1;
    }
    return 0;
}

int handle_http(){
    int res, body_size = 0;
    memset(head_buf, 0, sizeof(head_buf));
    headsize = bufsize = 0;
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
    shead = shead.substr(res+1);
    res = shead.find(" ");
    reqpath = shead.substr(0,res);
    res = shead.find("?");
    reqpath = reqpath.substr(0,res);
    if(method=="GET"){
        if(reqpath=="/"){
            if(logflag==1) return get("/homepage.html");
            return index();
        }
        else
            return write(cli_fd, header404.c_str(), header404.length())<0? -1 : 0;
    }
    if(method=="POST"){
        res = shead.rfind("Content-Length: ");
        shead = shead.substr(res);
        res = shead.find("\r\n");
        body_size = stoi(shead.substr(16,res));
        if(reqpath=="/send_image" || reqpath=="/send_file") return post(reqpath, body_size);
        memset(buf, 0, sizeof(char)*(body_size+1));
        while(body_size>0){
            //need to check connection with server
            if((res = read(cli_fd, buf+bufsize, body_size))<0) continue;
            bufsize += res;
            body_size -= res;
        }
        return post(reqpath);
    }
    return -1;
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
    if((http_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) ERR("socket()")
    if(bind(http_fd, (struct sockaddr*) &httpaddr, sizeof(httpaddr)) < 0) ERR("bind()")
    if(listen(http_fd, SOMAXCONN) < 0) ERR("listen()")

    fd_set read_OK;

    while(1){
        cli_fd = -1;
        logflag = 0;
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
                if(handle_http()<0){
                    close(cli_fd);
                    break;
                }
        }
    }
}

int index(){
    if(logflag==1) return get("/homepage.html");
    int file_fd = open("./template/index.html", O_RDONLY), res;
    string header = "text/html";
    if(file_fd<0)
        return write(cli_fd, header404.c_str(), header404.length())<0? -1 : 0;
    filesize = lseek(file_fd, 0, SEEK_END);
    if(filesize<0){
        close(file_fd);
        return -1;
    }
    lseek(file_fd, 0, SEEK_SET);
    if(logflag==-1) header = set_header(filesize+used.length(), header);
    else header = set_header(filesize, header);
    while((res = write(cli_fd, header.c_str(), header.length()))<0){
        if(res<0 && errno==EAGAIN) continue;
        close(file_fd);
        return -1;
    }
    while(filesize>0){
        while((res = read(file_fd, buf, filesize))<=0){
            if(res<0 && errno==EAGAIN) continue;
            break;
        }
        while((res = write(cli_fd, buf, res))<0){
            if(res<0 && errno==EAGAIN) continue;
            break;
        }
        filesize -= res;
    }
    close(file_fd);
    if(res<0) return -1;
    if(logflag<0)
        while((res = write(cli_fd, used.c_str(), used.length()))<0){
            if(res<0 && errno==EAGAIN) continue;
            return -1;
        }
    return 0;
}

int get(string path, int extra){
    if(logflag!=1) return index();
    string header = "text/html";
    path = "./template" + path;
    int file_fd = open(path.c_str(), O_RDONLY), res;
    if(file_fd<0)
        return write(cli_fd, header404.c_str(), header404.length())<0? -1 : 0;
    filesize = lseek(file_fd, 0, SEEK_END);
    if(filesize<0){
        close(file_fd);
        return -1;
    }
    lseek(file_fd, 0, SEEK_SET);
    header = set_header(filesize+extra, header);
    if(write(cli_fd, header.c_str(), header.length())<0){
        close(file_fd);
        return -1;
    }
    while(filesize>0){
        while((res = read(file_fd, buf, filesize))<=0){
            if(res<0 && errno==EAGAIN) continue;
            close(file_fd);
            return -1;
        }
        while((res = write(cli_fd, buf, res))<0){
            if(res<0 && errno==EAGAIN) continue;
            close(file_fd);
            return -1;
        }
        filesize -= res;
    }
    close(file_fd);
    return 0;
}

int post(string event, int body_size){
    int res;
    if(event=="/username"){
        string name = (string) buf;
        res = name.find("=");
        name = name.substr(res+1);

        //check username
        for(int i=0;i<name.length();++i){
            if(isalnum(name[i])) continue;
            logflag = -1;
            return index();
        }

        package pkg(LOGIN, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;

        if(((string)pkg.buf)=="Succeeed") logflag = 1;
        else logflag = -1;

        return index();
    }
    if(event=="/list_friend"){
        package pkg;
        pkg.type = LIST;
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;

        string tmp = (string) pkg.buf;
        res = tmp.find(" ");
        int num = stoi(tmp.substr(0,res));
        int extra = stoi(tmp.substr(res+1)) + 7*num;
        get("/homepage.html",extra);

        for(int i=0;i<num;++i){
            if(read_package(pkg)<0) return -1;
            tmp = "<p>" + (string)pkg.buf + "</p>";
            if(write(cli_fd, tmp.c_str(), tmp.length())<0) return -1;
        }

        return 0;
    }
    if(event=="/add_friend"){
        string name = (string) buf;
        res = name.find("=");
        name = name.substr(res+1);

        package pkg(ADD, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;
        
        return get("/homepage.html");
    }
    if(event=="/del_friend"){
        string name = (string) buf;
        res = name.find("=");
        name = name.substr(res+1);

        package pkg(DEL, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;
        
        return get("/homepage.html");
    }
    if(event=="/chat_with"){
        recver = (string) buf;
        return get("/chatroom.html");
    }
    if(event=="/send_message"){
        //send buf 2 server
    }
    if(event=="/send_image"){
        //read buf and then send 2 server
    }
    if(event=="/send_file"){
        //read buf and then send 2 server
    }
    if(event=="/homepage"){
        return get("/homepage.html");
    }
    if(event=="/view_history" || event=="/update"){
        int tmp;
        if(event=="/update") tmp = 20;
        else tmp = atoi(buf);
        //extra = history_size recv from server
        int extra = 0;
        get("/chatroom.html",extra);
        /*
        while(extra>0){
            //need to check connection with server
            res = read(cli_fd, buf, extra);
            if(res<0 || write(cli_fd, buf, res)<0) return -1;
            extra -= res;
        }
        */
        return 0;
    }
}
