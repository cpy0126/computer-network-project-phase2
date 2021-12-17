# Computer-Network-Project-Phase2

```cpp
#define SEND 10
#define RECV 20
#define CMD 30
#define IMG 100
#define FILES 200
#define MSS 300

struct package{
    int type: SEND, RECV, CMD
    time_t Time
    int content: IMG, FILE, MSS
    char buf[2048]
    char sender[128]
    char recver[128]
    int buf_size
}
```



```cpp
#include<time.h>
time_t time; //seconds from 1970/1/1 00:00
struct tm date //yyyy-mm-dd ...
time(NULL) //get cur time_t
local_time(*time_t) //change time_t to struct tm
ctime(*time_t) //change time_t to char*
asctime(struct *tm) //change struct tm to char*
```