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

#define ERR(a) {perror(a); return EXIT_FAILURE;}
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

string set_header(int content_len, string filetype, string extra){
    string header;
    header = "HTTP/1.1 200 OK\r\nContent-Length: " + to_string(content_len);
    header = header + "\r\nContent-Type: " + filetype + "\r\nConnection: keep-alive\r\n";
    header = header + extra + "\r\n";
    return header;
}

string header404 ="\
HTTP/1.1 404 Bad Request\r\n\
Content-Length: 122\r\n\
Content-Type: text/html\r\n\
Connection: Closed\r\n\r\n\
<html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center></body></html>";
string used = "<html><h2>Username used or illegal, please choose another</h2></html>", user, target;
string shead, boundary;
map<string, string> mmap;

void inline_init(){
    mmap["html"] = "text/html";
    mmap["jpg"] = "image/jpeg";
    mmap["jpeg"] = "image/jpeg";
    mmap["png"] = "image/png";
    mmap["ico"] = "image/png";
    mmap["gif"] = "image/gif";
    mmap["mp4"] = "video/mp4";
    mmap["mp3"] = "audio/mp3";
}

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
int bufsize,headsize,sock_fd,http_fd,cli_fd,logflag;
int filesize;
vector<struct package> space;

int login();
int get(string path, int extra);
int post(string event, int body_size);

int read_package(package &pkg){
    int res = 1, tmp = 0;
    memset(buf, 0, sizeof(package));
    while(tmp<sizeof(package)){
        while((res = read(sock_fd, buf+tmp, sizeof(package)-tmp))<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        if(res==0) return -1;
        tmp += res;
    }
    memcpy(&pkg, buf, sizeof(package));
    return 0;
}

int write_package(package &pkg){
    while(send(sock_fd, &pkg, sizeof(package), MSG_NOSIGNAL)<0){
        if(errno==EAGAIN) continue;
        return -1;
    }
    return 0;
}

void get_boundary(){
    int res;
    res = shead.find("boundary=");
    if(res==-1) return;
    boundary = shead.substr(res+9);
    res = boundary.find("\r\n");
    boundary = boundary.substr(0,res);
    boundary = "--" + boundary;
    return;
}

int handle_http(){
    int res, body_size = 0;
    memset(head_buf, 0, sizeof(head_buf));
    headsize = bufsize = 0;
    char cur = '\0', prev = '\0';
    string method, reqpath;
    shead = "";
    while(1){
        if((res = read(cli_fd, head_buf+headsize, 1))<0 && errno==EAGAIN) continue;
        if(res<=0) return res;
        cur = head_buf[headsize];
        ++headsize;
        if(prev=='\r' && cur=='\n' && headsize==2) break;
        if(prev=='\r' && cur=='\n') shead+=((string)head_buf).substr(0,headsize), headsize=0;
        prev = cur;
    }
    res = shead.find(" ");
    method = shead.substr(0,res);
    reqpath = shead.substr(res+1);
    res = reqpath.find(" ");
    reqpath = reqpath.substr(0,res);
    res = shead.rfind("?");
    if(res!=-1)
        reqpath = reqpath.substr(0,res);
    if(method=="GET"){
        if(reqpath=="/"){
            if(logflag==1) return get("/homepage.html", 0);
            return login();
        }
        else
            return get(reqpath, 0);
    }
    if(method=="POST"){
        res = shead.rfind("Content-Length: ");
        method = shead.substr(res);
        res = method.find("\r\n");
        body_size = stoi(method.substr(16,res));
        if(reqpath=="/send_image" || reqpath=="/send_file")
            return post(reqpath, body_size);
        memset(buf, 0, sizeof(char)*(body_size+10));
        while(body_size>0){
            if((res = read(cli_fd, buf+bufsize, body_size))<0 && errno==EAGAIN) continue;
            if(res<=0) return res;
            bufsize += res;
            body_size -= res;
        }
        return post(reqpath, body_size);
    }
    while(send(cli_fd, header404.c_str(), header404.length(), MSG_NOSIGNAL)<0)
        if(errno==EAGAIN) continue;
        else return -1;
    return 0;
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
    inline_init();

    while(1){
        cli_fd = -1;
        int clilen = sizeof(cliaddr);
        if((cli_fd = accept(http_fd, (struct sockaddr*) &cliaddr, (socklen_t*) &clilen)) < 0) continue;
        
        while(1){
            FD_ZERO(&read_OK);
            FD_SET(cli_fd, &read_OK);
            select(cli_fd+1, &read_OK, NULL, NULL, NULL);
            if(FD_ISSET(cli_fd, &read_OK)){
                if(handle_http()==-1){
                    perror("handle_http");
                    package pkg(LOGOUT, user);
                    write_package(pkg);
                    user = "";
                    target = "";
                    logflag = 0;
                }
                close(cli_fd);
                break;
            }
        }
    }
}

int login(){
    int res;
    if(logflag==1) return get("/homepage.html", 0);
    package pkg(GET, "/index.html");
    
    strncpy(pkg.sender, ((string)"..").c_str(), 2);
    strncpy(pkg.recver, ((string)"template").c_str(), 8);
    
    if(write_package(pkg)<0) return -1;
    if(read_package(pkg)<0) return -1;
    if((string)pkg.buf=="Failed"){
        while(send(cli_fd, header404.c_str(), header404.length(), MSG_NOSIGNAL)<0)
            if(errno==EAGAIN) continue;
            else return -1;
        return -1;
    }
    
    int filesize = atoi(pkg.buf);
    string header = "text/html";

    if(logflag==-1) header = set_header(filesize+used.length(), header, "");
    else header = set_header(filesize, header, "");

    while(send(cli_fd, header.c_str(), header.length(), MSG_NOSIGNAL)<0){
        if(errno==EAGAIN) continue;
        return -1;
    }
    
    while(filesize>0){
        if(read_package(pkg)<0) return -1;
        while(send(cli_fd, pkg.buf, pkg.buf_size, MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        filesize -= pkg.buf_size;
    }
    
    if(logflag==-1){
        while(send(cli_fd, used.c_str(), used.length(), MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        logflag=0;
    }
    
    return 0;
}

int get(string path, int extra){
    if(logflag!=1 && path!="/favicon.ico") return login();
    package pkg(GET, path);
    int res;
    
    if(path=="/homepage.html" || path=="/chatroom.html" || path=="/index.html" || path=="/favicon.ico" || user=="" || target==""){
        strncpy(pkg.sender, ((string)"..").c_str(), 2);
        strncpy(pkg.recver, ((string)"template").c_str(), 8);
    }
    else{
        strncpy(pkg.sender, user.c_str(), user.length());
        strncpy(pkg.recver, target.c_str(), target.length());
    }
    cerr << (string)pkg.sender << "/" << (string)pkg.recver << path << endl;

    if(write_package(pkg)<0) return -1;
    if(read_package(pkg)<0) return -1;

    if((string)pkg.buf=="-1"){
        while(send(cli_fd, header404.c_str(), header404.length(), MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        return -1;
    }

    filesize = atoi(pkg.buf);
    res = path.rfind(".");
    string header = "text/plain";
    string extra_header = "Content-Disposition: attachment; filename=\"" + path + "\"\r\n";
    if(res!=-1)
        if(mmap.find(path.substr(res+1))!=mmap.end()){
            header = mmap[path.substr(res+1)];
            extra_header = "";
        }

    header = set_header(filesize + extra, header, extra_header);
    while(send(cli_fd, header.c_str(), header.length(), MSG_NOSIGNAL)<0){
        if(errno==EAGAIN) continue;
        return -1;
    }
    
    while(filesize>0){
        memset(pkg.buf, 0, sizeof(pkg.buf));
        if(read_package(pkg)<0) return -1;
        while(send(cli_fd, pkg.buf, pkg.buf_size, MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        filesize -= pkg.buf_size;
    }
    cerr << "finished" << endl;

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
            return login();
        }

        package pkg(LOGIN, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;

        if(((string)pkg.buf)=="Succeeed") logflag = 1, user = name;
        else logflag = -1;

        return login();
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
        space.clear();
        for(int i=0;i<num;++i){
            if(read_package(pkg)<0) return -1;
            space.push_back(pkg);
        }

        if(get("/homepage.html",extra)<0) return -1;
        
        for(int i=0;i<num;++i){
            tmp = "<p>" + (string)space[i].buf + "</p>";
            while(send(cli_fd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL)<0){
                if(errno==EAGAIN) continue;
                return -1;
            }
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
        
        return get("/homepage.html", 0);
    }
    if(event=="/del_friend"){
        string name = (string) buf;
        res = name.find("=");
        name = name.substr(res+1);

        //check friendship
        package pkg(CHECK, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;

        if(((string)pkg.buf)=="Succeeed"){
            pkg = package(DEL, name);
            if(write_package(pkg)<0) return -1;
            if(read_package(pkg)<0) return -1;
        }

        return get("/homepage.html", 0);
    }
    if(event=="/chat_with"){
        string name = (string) buf;
        res = name.find("=");
        name = name.substr(res+1);

        package pkg(CHECK, name);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;
        
        if(((string)pkg.buf)!="Succeeed")
            return get("/homepage.html", 0);
        
        target = name;
        event = "/update";
    }
    if(event=="/homepage"){
        target = "";
        return get("/homepage.html", 0);
    }
    if(event=="/logout"){
        package pkg(LOGOUT, user);
        user = "";
        target = "";
        logflag = 0;
        if(write_package(pkg)) return -1;
        return login();
    }
    if(event=="/send_message"){
        get_boundary();
        string content = (string) buf;
        res = content.find("\r\n\r\n");
        content = content.substr(res+4);
        res = content.find(boundary);
        content = content.substr(0, res-2);
        
        package pkg(MSS, content, user, target);
        if(write_package(pkg)<0) return -1;
        if(read_package(pkg)<0) return -1;

        event = "/update";
    }
    if(event=="/send_image"){
        get_boundary();
        char cur = '\0', prev = '\0';
        string tmp, filename;
        headsize = 0;
        memset(head_buf, 0, sizeof(head_buf));
        while(1){
            if((res = read(cli_fd, head_buf+headsize, 1))<0 && errno==EAGAIN) continue;
            if(res<=0) return res;
            cur = head_buf[headsize];
            ++headsize;
            if(prev=='\r' && cur=='\n' && headsize==2) break;
            if(prev=='\r' && cur=='\n') tmp+=((string)head_buf).substr(0,headsize), body_size-=headsize, headsize=0;
            prev = cur;
        }
        res = tmp.find("filename=");
        filename = tmp.substr(res+10);
        res = filename.find("\r\n");
        filename = filename.substr(0, res-1);
        res = filename.rfind(".");
        filename = filename.substr(res);
        package pkg;
        time(&pkg.Time);
        filename = to_string(pkg.Time) + filename;
        pkg = package(IMG, filename, user, target);

        body_size -= headsize;
        pkg.buf_size = body_size - boundary.length() - 6;
        if(write_package(pkg)<0) return -1;
        while(body_size>0){
            memset(pkg.buf, 0, sizeof(pkg.buf));
            while((res = read(cli_fd, &pkg.buf, min(2048, body_size)))<=0){
                if(errno==EAGAIN) continue;
                return -1;
            }
            pkg.buf_size = res;
            if(write_package(pkg)<0) return -1;
            body_size -= res;
        }
        pkg = package(IMG, (string)"Succeeed", user, target);
        if(write_package(pkg)<0) return -1;
        //read buf and then send 2 server

        event = "/update";
    }
    if(event=="/send_file"){
        get_boundary();
        char cur = '\0', prev = '\0';
        string tmp, filename;
        headsize = 0;
        memset(head_buf, 0, sizeof(head_buf));
        while(1){
            if((res = read(cli_fd, head_buf+headsize, 1))<0 && errno==EAGAIN) continue;
            if(res<=0) return res;
            cur = head_buf[headsize];
            ++headsize;
            if(prev=='\r' && cur=='\n' && headsize==2) break;
            if(prev=='\r' && cur=='\n') tmp+=((string)head_buf).substr(0,headsize), body_size-=headsize, headsize=0;
            prev = cur;
        }
        res = tmp.find("filename=");
        filename = tmp.substr(res+10);
        res = filename.find("\r\n");
        filename = filename.substr(0, res-1);
        string orgfilename = filename + "\n";
        res = filename.rfind(".");
        filename = filename.substr(res);
        package pkg;
        time(&pkg.Time);
        filename = orgfilename + to_string(pkg.Time) + filename;
        pkg = package(FILES, filename, user, target);

        body_size -= headsize;
        pkg.buf_size = body_size - boundary.length() - 6;
        if(write_package(pkg)<0) return -1;
        while(body_size>0){
            memset(pkg.buf, 0, sizeof(pkg.buf));
            if((res = read(cli_fd, &pkg.buf, min(2048, body_size)))<0) return -1;
            pkg.buf_size = res;
            if(write_package(pkg)<0) return -1;
            body_size -= res;
        }
        pkg = package(FILES, (string)"Succeeed", user, target);
        if(write_package(pkg)<0) return -1;
        //read buf and then send 2 server

        event = "/update";
    }
    if(event=="/view_history" || event=="/update"){
        string tmp = "30";
        if(event=="/view_history"){
            tmp = (string) buf;
            res = tmp.find("=");
            tmp = tmp.substr(res+1);
        }
        
        package pkg(HIS, tmp, user, target);
        if(write_package(pkg)<0) return -1;
        int extra = 0;
        //extra = history_size recv from server

        space.clear();
        while(1){
            if(read_package(pkg)<0) return -1;
            if(pkg.type==HIS) break;
            space.push_back(pkg);
            extra += 80 + ((string)pkg.sender).length();
            if(pkg.type==MSS)
                extra += pkg.buf_size + 24;
            if(pkg.type==IMG)
                extra += ((string)pkg.buf).length() + 83; 
            if(pkg.type==FILES)
                extra += ((string)pkg.buf).length() - 1 + 26;
        }
        extra += 54;
        
        if(get("/chatroom.html",extra)<0) return -1;
        string nname, oname;
        tmp = "<html><body><div class=\"dialogue\">";
        while(send(cli_fd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        
        for(int i=0;i<((int)space.size());++i){
            if(space[i].sender==user){
                tmp = "<div class=\"user local\"><div class=\"avatar\"><div class=\"name\">\0" + (string)space[i].sender + "</div></div>\0";
            }
            else{
                tmp = "<div class=\"user other\"><div class=\"avatar\"><div class=\"name\">\0" + (string)space[i].sender + "</div></div>\0";
            }
            if(space[i].type==MSS)
                tmp = tmp + "<div class=\"text\">" + (string)space[i].buf + "</div></div>\0";
            if(space[i].type==IMG)
                tmp = tmp + "<div class=\"text\"><img src=\"\0" + (string)space[i].buf + "\" width=\"200\" height=\"100\"></div></div>\0";
            if(space[i].type==FILES){
                nname = oname = ((string)space[i].buf);
                res = nname.find("\n");
                nname = nname.substr(res+1), oname = oname.substr(0,res);
                tmp = tmp + "<div class=\"text\"><a href=\"\0" + nname + "\" download>\0" + oname + "</a></div></div>\0";
            }
            while(send(cli_fd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL)<0){
                if(errno==EAGAIN) continue;
                return -1;
            }
        }
        tmp = "</div></body></html>";
        while(send(cli_fd, tmp.c_str(), tmp.length(), MSG_NOSIGNAL)<0){
            if(errno==EAGAIN) continue;
            return -1;
        }
        return 0;
    }
    while(send(cli_fd, header404.c_str(), header404.length(), MSG_NOSIGNAL)<0){
        if(errno==EAGAIN) continue;
        return -1;
    }
    return -1;
}
