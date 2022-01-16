# Computer-Network-Project-Phase2

### Server

```bash
make
./server [port1]
```

### Client(console mode)

```bash
make
./console_client [server ip]:[port1] [port2]
```

### Client(web mode)

```bash
make
./client [server ip]:[port1] [port3]
#macOS
open http://[client ip]:[port3]/
#linux
xdg-open http://[client ip]:[port3]/
```

make後照著上面提示輸入command就能run起來

如果要使用browser連到client的話，就到網址列輸入client run起來的電腦的[ip]:[port]，就可以接到網站了
