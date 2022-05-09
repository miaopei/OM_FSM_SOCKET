#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define MAXDATASIZE 65507

typedef struct thread_send_info_s {
    int sockfd;
    struct sockaddr_in cli;
    socklen_t len;
    char msg[MAXDATASIZE];
} thread_send_info_t;

static thread_send_info_t thread_send_info;

typedef struct Req_s {
    uint16_t    type;
    uint16_t    msgLen;
    uint16_t    segLen;
    uint16_t    segBat;
    uint16_t    numSlot;
    uint8_t     cellId;
} __attribute__((packed)) Req_t;

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
    uint8_t     reserved;
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

int Usage(const char* proc)
{
    printf("Usage:%s[server_ip][server_prot]\n", proc);
    return 0;
}

void send_handler(thread_send_info_t *ts_info, const char *data, uint32_t len)
{
    sendto(ts_info->sockfd, data, len, 0, (struct sockaddr *)&ts_info->cli, ts_info->len);
}

void recv_process_handler(thread_send_info_t *ts_info)
{
    int ret;
    char ver_info[MAXDATASIZE] = {0};
    //char data[1024] = {0};
    uint32_t data_msg;
    uint16_t id;

    memset(&ver_info, 0x00, sizeof(ver_info));
    memset(&data_msg, 0x00, sizeof(data_msg));

    ret = recvfrom(ts_info->sockfd, ver_info, MAXDATASIZE, MSG_DONTWAIT, (struct sockaddr *)&ts_info->cli, &ts_info->len);
    if(ret > 0)
    {
        memcpy(&id, ver_info, 2);
        printf("recvfrom return size=[%d] id=%d.\n", ret, id);
        if(id == 2000)
        {
            CellCfg_rsp_t cellCfg_rsp;
            memcpy(&cellCfg_rsp, ver_info, sizeof(cellCfg_rsp));
            printf("Recv msg info: {type=%d msgLen=%d lastMsg=%d numSeg=%d segIdx=%d numCell=%d numTxAnt=%d numRxAnt=%d}.\n"
                   , cellCfg_rsp.type
                   , cellCfg_rsp.msgLen
                   , cellCfg_rsp.lastMsg
                   , cellCfg_rsp.numSeg
                   , cellCfg_rsp.segIdx
                   , cellCfg_rsp.numCell
                   , cellCfg_rsp.numTxAnt
                   , cellCfg_rsp.numRxAnt);


            Sync_t sync;
            sync.type               = 3000;
            sync.msgLen             = 32;
            sync.segIdx             = 0;
            sync.synCode            = 3;

            send_handler(ts_info, (char *)&sync, 32);
        }
        else if(id == 2001)
        {
            RT_rsp_t rt_rsp;
            memcpy(&rt_rsp, ver_info, sizeof(rt_rsp));
            printf("Recv msg info: {type=%d msgLen=%d lastMsg=%d numSeg=%d segIdx=%d}.\n"
                   , rt_rsp.type
                   , rt_rsp.msgLen
                   , rt_rsp.lastMsg
                   , rt_rsp.numSeg
                   , rt_rsp.segIdx);

            //memcpy(data, ver_info + 32, rt_rsp.msgLen - 32);
            memcpy(&data_msg, ver_info + 32, 2);
            printf("Recv msg data: %d.\n", data_msg);

            Sync_t sync;
            sync.type               = 3000;
            sync.msgLen             = 32;
            sync.segIdx             = 0;
            sync.synCode            = 1;

            send_handler(ts_info, (char *)&sync, 32);
        }
        else if(id == 2002)
        {
            IQ_rsp_t iq_rsp;
            memcpy(&iq_rsp, ver_info, sizeof(iq_rsp));
            printf("Recv msg info: {type=%d msgLen=%d lastMsg=%d numSeg=%d segIdx=%d cellId=%d antId=%d iqType=%d}.\n"
                   , iq_rsp.type
                   , iq_rsp.msgLen
                   , iq_rsp.lastMsg
                   , iq_rsp.numSeg
                   , iq_rsp.segIdx
                   , iq_rsp.cellId
                   , iq_rsp.antId
                   , iq_rsp.iqType);

            memcpy(&data_msg, ver_info + 32, iq_rsp.msgLen - 32);
            printf("Recv msg data: %d.\n", data_msg);	

            Sync_t sync;
            sync.type               = 3000;
            sync.msgLen             = 32;
            sync.segIdx             = 0;
            sync.synCode            = 3;

            send_handler(ts_info, (char *)&sync, 32);
        }
    }
}

int main(int argc, char* argv[])
{
    int sockfd;
    struct sockaddr_in addr;

    char sendbuf[MAXDATASIZE] = {0};

    int msgid = 0;

    if(argc != 3)
    {
        Usage(argv[0]);
        return 1;
    }

    if((sockfd = socket(AF_INET,SOCK_DGRAM,0)) == -1)
    {
        printf("Createing socket failed.\n");
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    //struct timeval tv_out;
    //tv_out.tv_sec = 3; // wait 3s
    //tv_out.tv_usec = 1000; // 1000 微秒
    //setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    thread_send_info.sockfd = sockfd;
    thread_send_info.cli = addr;
    thread_send_info.len = sizeof(addr);

    while(1)
    {
        //printf("input usage: \n\tquit[100] \n\tphy_log_start[101] \n\texit[200] \ninput message: ");
        printf("\ninput message: ");
        fgets(sendbuf, 40, stdin);
        sendbuf[strlen(sendbuf)-1] = '\0';

        Req_t req;

        msgid = atoi(sendbuf);
        switch(msgid)
        {
        case 1000:
            {
                req.type        = 1000;
                req.msgLen      = 32;

                //sendto(sockfd, &req, sizeof(req), 0, (struct sockaddr *)&addr, len);
                send_handler(&thread_send_info, (char *)&req, 32);
                break;
            }
        case 1001:
            {
                req.type        = 1001;
                req.msgLen      = 32;
                req.segLen      = 1400;
                req.segBat      = 3;

                //sendto(sockfd, &req, sizeof(req), 0, (struct sockaddr *)&addr, len);
                send_handler(&thread_send_info, (char *)&req, 32);
                break;

            }
        case 1002:
            {
                req.type        = 1002;
                req.msgLen      = 32;
                req.segLen      = 1400;
                req.segBat      = 3;
                req.numSlot     = 2;
                req.cellId      = 1;

                //sendto(sockfd, &req, sizeof(req), 0, (struct sockaddr *)&addr, len);
                send_handler(&thread_send_info, (char *)&req, 32);
                break;
            }
        }

        recv_process_handler(&thread_send_info);
        memset(sendbuf, 0x00, sizeof(sendbuf));
    }

    close(sockfd);

    return 0;
}

