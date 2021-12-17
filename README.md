# Computer-Network-Project-Phase2

```cpp
#define SEND 000
#define RECV 100
#define CMD 200
#define IMG 010
#define FILE 020
#define MSS 030

struct package{
    int type: SEND, RECV, CMD
    string sender,recver
    struct time time: yyyy-mm-dd
    int content: IMG, FILE, MSS
    char buf[2048]
    int buf_size
}
```