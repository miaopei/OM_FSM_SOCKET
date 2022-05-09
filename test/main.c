#include <stdio.h>
#include <stdlib.h>

#include "fsm.h"

/*

https://blog.csdn.net/xinghuanmeiying/article/details/89387451

*/

typedef enum {
    IDLE = 0,
    INIT,
    BUSY,
    SYNC,
    PENDING
} fsm_state_e;

#if 0
typedef enum
{
    REQ_CELL_CFG    = 1000,
    REQ_RT          = 1001,
    REQ_IQ          = 1002,
    RSP_CELL_CFG    = 2000,
    RSP_RT          = 2001,
    RSP_IQ          = 2002,
    SYNC_IND        = 3000
} fsm_event_e;
#endif

typedef enum
{
    REQUEST,
    RESPONSE,
    MSG_END,
    SYNC_0, // 丢包重传
    SYNC_1, // 超时重传
    SYNC_2, // segment续传
    SYNC_3, // msg续传
    SYNC_END, // lastMsg = 1
    OVERTIME
} fsm_event_e;

static FsmTable_T g_stFsmTable[] = 
{
    {REQUEST,   IDLE, ActFun_ReqParse,  INIT},
    {RESPONSE,  INIT, ActFun_RspPack,   BUSY},
    {SYNC,      BUSY, }
};

int main()
{
    printf("Hello world\n");
    return 0;
}

