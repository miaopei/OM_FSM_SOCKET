
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#define MSGBUF  10240
#define RCVBUF	1200+128

/*

https://github.com/AstarLight/FSM-framework/blob/master/main.c

https://github.com/leijian001/sm/blob/master/sm.h#L103:9

https://github.com/ChengOS1/c_FSM/blob/master/FSM.c

https://blog.csdn.net/jenie/article/details/106484709

*/

typedef struct om_socket_s {
    uint32_t 			listen;
	struct sockaddr_in	cliAddr;
    uint32_t 			cliPort;
    uint8_t 			cliLock;
    uint8_t 			writeLock;
    uint8_t 			readLock;
    uint8_t 			bufLock;
} om_socket_t;

typedef enum {
    IDLE = 0,
    INIT,
    BUSY,
    PENDING,
    SYNC,
    LAST
} State;

typedef enum {
    False   = 0,
    True    = 1
} LastMsgState;

typedef enum {
    ARQ                 = 0,
    TIMEOUT_ARQ         = 1,
    SEGMENT_CONTINUE    = 2,
    MSG_CONTINUE        = 3
} SyncState;

typedef enum {
    DL      = 0,
    UL      = 1,
    PRACH   = 2
} IQType;

typedef struct Req_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint16_t    segLen;
    uint16_t    segBat;
    uint16_t    numSlot;
    uint8_t     cellId;
} Req_t;

typedef struct Sync_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint32_t    segIdx;
    uint8_t     synCode;
} Sync_t;

typedef struct CellCfg_rsp_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint8_t     lastMsg;    // 1--true, 0--false
    uint16_t    rsv0;       // reserved
    uint8_t     rsv1;       // reserved
    uint32_t    numSeg;
    uint32_t    segIdx;
    uint8_t     numCell;
    uint8_t     numTxAnt;
    uint8_t     numRxAnt;
} __attribute__((packed)) CellCfg_rsp_t;

typedef struct RT_rsp_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint8_t     lastMsg;    // 1--true, 0--false
    uint16_t    rsv0;       // reserved
    uint8_t     rsv1;       // reserved
    uint32_t    numSeg;
    uint32_t    segIdx;
} __attribute__((packed)) RT_rsp_t;

typedef struct IQ_rsp_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint8_t     lastMsg;    // 1--true, 0--false
    uint16_t    rsv0;       // reserved
    uint8_t     rsv1;       // reserved
    uint32_t    numSeg;
    uint32_t    segIdx;
    uint8_t     cellId;
    uint8_t     antId;
    uint8_t     iqType;     // 0--DL, 1--UL, 2--PRACH
} __attribute__((packed)) IQ_rsp_t;

typedef struct fsm_s {
    State       state;
    Req_t       req;
    Sync_t      sync;
    uint32_t    alive;
    uint32_t    numMsg;
    uint32_t    msgIdx;
    uint32_t    numSeg;
    uint32_t    segIdx;
    char        rspMsgBuf[MSGBUF];
} fsm_t;


