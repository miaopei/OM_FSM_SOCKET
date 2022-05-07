#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "om_socket.h"

int setnonblocking(int fd)
{
    int flags;
    if((flags = fcntl(fd, F_GETFD, 0)) < 0)
    {
        printf("get falg error\n");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        printf("set nonblock fail\n");
        return -1;
    }
    return 0;
}

int create_udp_listen(short port)
{
    int sockFd, ret;
    struct sockaddr_in seraddr;

    bzero(&seraddr, sizeof(struct sockaddr_in));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = INADDR_ANY;
    seraddr.sin_port = htons(port);

    sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0)
    {
        perror("socket error");
        return -1;
    }

    if (setnonblocking(sockFd) < 0)
    {
        perror("setnonblock error");
    }

    ret = bind(sockFd, (struct sockaddr*)&seraddr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        perror("bind");
        return -1;
    }

    return sockFd;
}

om_socket_t *om_init(unsigned port)
{
    om_socket_t *oms = NULL;
    int ret = 0;

    oms = calloc(1, sizeof(om_socket_t));
    if (NULL == oms)
    {
        printf("calloc fail.\n");
        goto Error0;
    }

    oms->listen = create_udp_listen(port);
    oms->cliLock = 0;
    oms->writeLock = 0;
    oms->readLock = 0;
    oms->bufLock = 0;
    if (oms->listen < 0)
    {
        printf("listen and bind.\n");
        goto Error1;
    }

    return oms;

Error1:
    free(oms);
Error0:
    return NULL;
}

void om_fsm_start(om_socket_t* oms)
{
    char recvbuf[RCVBUF] = { 0 };
    int len;
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(struct sockaddr);

    len = recvfrom(oms->listen, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&cliaddr, &clilen);
    if (len > 0)
    {
        //oms->udp_handle(oms->listen, &clientaddr, recvbuf, len);
        printf(">>client address = %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        printf("Recv: %s\n", recvbuf);

        sendto(oms->listen, recvbuf, strlen(recvbuf), 0, (struct sockaddr*)&cliaddr, sizeof(struct sockaddr_in));
    }
}

void om_server_delete(om_socket_t* oms, fsm_t *fsm)
{
    if(NULL == oms && NULL == fsm) return;

    close(oms->listen);
    free(oms);
    free(fsm);
}

fsm_t* fsm_init(void)
{
    fsm_t *fsm = (fsm_t *)malloc(sizeof(fsm_t));
    if(NULL == fsm) return NULL;

    fsm->state 		= IDLE;
    fsm->alive 		= 0;
    fsm->numMsg 	= 0;
    fsm->msgIdx 	= 0;
    fsm->numSeg 	= 0;
    fsm->segIdx 	= 0;
    memset(fsm->rspMsgBuf, 0x00, sizeof(fsm->rspMsgBuf));

    return fsm;
}

uint32_t cur_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

const char *fsm_state_to_string(State s)
{
    switch(s)
    {
    case IDLE:      return "IDLE";
    case INIT:      return "INIT";
    case BUSY:      return "BUSY";
    case PENDING:   return "PENDING";
    case SYNC:      return "SYNC";
    default:        break;
    }
    return "Unknow State";
}

void print_req_msg(fsm_t *fsm)
{
    printf("fsm status: %s\n", fsm_state_to_string(fsm->state));
    if(fsm->req.type == 1000)
    {
        printf("\tReq msg {type:%d msgLen:%d}\n", fsm->req.type, fsm->req.msgLen);
    }
    else if(fsm->req.type == 1001)
    {
        printf("\tReq msg {type:%d msgLen:%d segLen:%d segBat:%d}\n"
               , fsm->req.type
               , fsm->req.msgLen
               , fsm->req.segLen
               , fsm->req.segBat);
    }
    else if(fsm->req.type == 1002)
    {
        printf("\tReq msg {type:%d msgLen:%d segLen:%d segBat:%d numSlot:%d cellId:%d}\n"
               , fsm->req.type
               , fsm->req.msgLen
               , fsm->req.segLen
               , fsm->req.segBat
               , fsm->req.numSlot
               , fsm->req.cellId);
    }
    return ;
}

void print_sync_req_msg(fsm_t *fsm)
{
    printf("fsm status: %s\n", fsm_state_to_string(fsm->state));
    printf("\tReq msg {type:%d msgLen:%d segIdx:%d synCode:%d}\n"
           , fsm->sync.type
           , fsm->sync.msgLen
           , fsm->sync.segIdx
           , fsm->sync.synCode);

    return ;
}

uint32_t req_msg_parse(fsm_t *fsm, const char *msg)
{
    uint16_t type;
    memcpy(&type, msg, sizeof(uint16_t));
    printf("Recv msg type: %d\n", type);

    if(type < 3000)
    {
        fsm->state = INIT;

        Req_t *req_msg = (Req_t *)msg;
        if(req_msg->type == 1000)
        {
            fsm->req.type       = req_msg->type;
            fsm->req.msgLen     = req_msg->msgLen;
        }
        else if(req_msg->type == 1001)
        {
            fsm->req.type       = req_msg->type;
            fsm->req.msgLen     = req_msg->msgLen;
            fsm->req.segLen     = req_msg->segLen;
            fsm->req.segBat     = req_msg->segBat;
        }
        else if(req_msg->type == 1002)
        {
            fsm->req.type       = req_msg->type;
            fsm->req.msgLen     = req_msg->msgLen;
            fsm->req.segLen     = req_msg->segLen;
            fsm->req.segBat     = req_msg->segBat;
            fsm->req.numSlot    = req_msg->numSlot;
            fsm->req.cellId     = req_msg->cellId;
        }

        print_req_msg(fsm);
    }
    else if(type = 3000)
    {
        fsm->state = SYNC;

        Sync_t *sync_msg = (Sync_t *)msg;
        fsm->sync.type      = sync_msg->type;
        fsm->sync.msgLen    = sync_msg->msgLen;
        fsm->sync.segIdx    = sync_msg->segIdx;
        fsm->sync.synCode   = sync_msg->synCode;

        print_sync_req_msg(fsm);
    }

    return 0;
}

uint32_t req_msg_handler(om_socket_t* oms, fsm_t *fsm)
{
    char recvbuf[RCVBUF] = { 0 };
    int len;
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(struct sockaddr);

    len = recvfrom(oms->listen, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&cliaddr, &clilen);
    if (len > 0)
    {
        printf(">>client address = %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        printf(">>Recv msg len %d.\n", len);

        oms->cliAddr = cliaddr;
        oms->cliPort = ntohs(cliaddr.sin_port);
        oms->cliLock = 1;

        req_msg_parse(fsm, recvbuf);
    }

    return 0;
}

uint32_t pack_cell_cfg_rsp_head(fsm_t *fsm
                                , uint16_t dataLen
                                , uint8_t last
                                , uint8_t numCell
                                , uint8_t numTxAnt
                                , uint8_t numRxAnt)
{
    memset(fsm->rspMsgBuf, 0x00, sizeof(fsm->rspMsgBuf));
    CellCfg_rsp_t CellCfg_rsp;
    CellCfg_rsp.type        = 2000;
    CellCfg_rsp.msglen      = 32 + dataLen;
    CellCfg_rsp.lastMsg     = last;
    CellCfg_rsp.rsv0        = 0;
    CellCfg_rsp.rsv1        = 0;
    CellCfg_rsp.numSeg      = fsm->numMsg;
    CellCfg_rsp.segIdx      = fsm->msgIdx;
    CellCfg_rsp.numCell     = numCell;
    CellCfg_rsp.numTxAnt    = numTxAnt;
    CellCfg_rsp.numRxAnt    = numRxAnt;
    memcpy(fsm->rspMsgBuf, &CellCfg_rsp, sizeof(CellCfg_rsp));

    return 0;
}

uint32_t pack_rt_rsp_head(fsm_t *fsm
                          , uint16_t dataLen
                          , uint8_t last)
{
    memset(fsm->rspMsgBuf, 0x00, sizeof(fsm->rspMsgBuf));
    RT_rsp_t rt;
    rt.type         = 2001;
    rt.msglen       = 32 + dataLen;
    rt.lastMsg      = last;
    rt.rsv0         = 0;
    rt.rsv1         = 0;
    rt.numSeg       = fsm->numMsg;
    rt.segIdx       = fsm->msgIdx;
    memcpy(fsm->rspMsgBuf, &rt, sizeof(rt));

    return 0;
}

uint32_t pack_iq_rsp_head(fsm_t *fsm
                          , uint16_t dataLen
                          , uint8_t last
                          , uint8_t cellId
                          , uint8_t antId
                          , uint8_t iqType)
{
    memset(fsm->rspMsgBuf, 0x00, sizeof(fsm->rspMsgBuf));
    IQ_rsp_t iq;
    iq.type         = 2002;
    iq.msgLen       = 32 + dataLen;
    iq.lastMsg      = last;
    iq.rsv0         = 0;
    iq.rsv1         = 0;
    iq.numSeg       = fsm->numMsg;
    iq.segIdx       = fsm->msgIdx;
    iq.cellId       = cellId;
    iq.antId        = antId;
    iq.iqType       = iqType;
    memcpy(fsm->rspMsgBuf, &iq, sizeof(iq));

    return 0;
}

uint32_t pack_rsp_msg(fsm_t *fsm)
{
    if(fsm->state == INIT)
    {
        if(fsm->req.type == 1000)
        {
            /* data process */
            fsm->numMsg = 1;
            fsm->msgIdx = 0;

            pack_cell_cfg_rsp_head(fsm, 0, 1, 2, 4, 8);

            /* body data pack */

        }
        else if(fsm->req.type == 1001)
        {
            /* data process */
            fsm->numMsg = 1;
            fsm->msgIdx = 0;

            pack_rt_rsp_head(fsm, 0, 1);

            /* body data pack */

        }
        else if(fsm->req.type == 1002)
        {
            /* data process */
            fsm->numMsg = 1;
            fsm->msgIdx = 0;

            pack_iq_rsp_head(fsm, 0, 1, 1, 0, 1);

            /* body data pack */

        }

        fsm->state = BUSY;
    }
    else if(fsm->state == SYNC)
    {
        if(MSG_CONTINUE == fsm->sync.synCode)
        {
            /* next msg */
            fsm->msgIdx += 1;
            if(fsm->msgIdx == fsm->numMsg)
            {
                printf("INT: TX end, FSM idle.\n");
                fsm->state = IDLE;
            }
            else
            {
                //printf();
                //fsm->numMsg = ;
                //fsm->msgIdx = 0;

                fsm->state = BUSY;
            }
        }
        else 
        {
            fsm->msgIdx = fsm->sync.segIdx;
            fsm->state = BUSY;
        }
    }
    else if(fsm->state == BUSY)
    {

    }
    else if(fsm->state == PENDING)
    {

    }

    if(fsm->state == BUSY)
    {

    }

    return 0;
}

uint32_t rsp_msg_handler(om_socket_t* oms, fsm_t *fsm)
{
    uint32_t ret, len;

    ret = pack_rsp_msg(fsm);
    if(0 != ret)
    {
        printf("Error: pack rsp msg failed.\n");
        return -1;
    }

again:
    len = sendto(oms->listen, fsm->rspMsgBuf, strlen(fsm->rspMsgBuf), 0,
                 (struct sockaddr*)&oms->cliAddr, sizeof(struct sockaddr_in));
    if(len > 0)
    {
        printf("INT: send msg success, size %d.\n", len);
        //fsm->msgIdx += 1;
    }
    else if(len == -1)
    {
        if(errno == EINTR)
        {
            printf("Error: sendint data interrupted by signal.\n");
            goto again;
        }
    }
    else if(len == 0)
    {
        oms->cliLock = 0;
        printf("Error: client closed.\n");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    om_socket_t *oms;
    fsm_t *fsm;
    uint32_t t1, t2;

    if (argc < 2)
    {
        printf("Usage: %s [port]\n", argv[0]);
        return -1;
    }

    oms = om_init(atoi(argv[1]));
    if (NULL == oms)
    {
        printf("epoll_server_init failed.\n");
        return -2;
    }

    fsm = fsm_init();
    if(NULL == fsm)
    {
        printf("Error: fsm malloc failed.\n");
        return -3;
    }

    t1 = cur_time_ms();
    while(1)
    {
        t2 = cur_time_ms();
        /* tx */
        if((t2 - t1) > 500)
        {
            t1 = t2;
            if(fsm->state != IDLE)
            {
                rsp_msg_handler(oms, fsm);
            }
        }

        /* rx */
        req_msg_handler(oms, fsm);

        //om_fsm_start(oms);
        //usleep(500000); // 500 ms
    }

    om_server_delete(oms, fsm);

    return 0;
}

