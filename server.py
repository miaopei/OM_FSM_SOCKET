#!/usr/bin/env python
# coding=utf-8

import ctypes
import struct
import time
import os
import math
import socket

def udp_server_up() :
    HOST   = ''
    PORT   = 6800
    buf_sz = 1200+128
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s :
        s.bind((HOST,PORT))
        s.settimeout(0.000001)
        print('UDP server up...')
        stateM = {
            'state'       : 'idle',
            'req'         : {'type':0,'cfg':{}},
            'sync'        : {'type':0,'segIdx':0},
            'alive'       : 0,
            'numMsg'      : 0,
            'msgIdx'      : 0,
            'msgBuf'      : [],
            'segLen'      : 0,
            'segBat'      : 0,
            'numSeg'      : 0,
            'segIdx'      : -1,
        }
        t1 = time.time()
        while True :
            t2 = time.time()
            # tx
            if (t2-t1) > 0.0005 :
                t1 = t2
                if stateM['state'] != 'idle' :
                    rsp = build_rsp_msg(stateM)
                    #print(state)
                    if rsp :
                        #print(rsp[0:])
                        try :
                            i = s.sendto(rsp,addr)
                        except :
                            pass
                        else :
                            if i >= len(rsp) :
                                stateM['segIdx'] += 1
            # rx
            try :
                data,addr = s.recvfrom(buf_sz)
            except socket.error as e :
                pass#print(str(e))
            else :
                req = req_msg_parse(stateM,data)
                print(req)

def req_msg_parse(stateM,data) :
    req  = {}
    offs = 0
    msgType,msgLen = struct.unpack_from('HH',data,offs)
    offs += 4
    req['msgType'] = msgType
    req['msgLen']  = msgLen
    if msgType < 3000 :
        stateM['state']       = 'init'
        stateM['req']['type'] = msgType
        if   1000 == msgType :
            stateM['segLen'] = 0
            stateM['segBat'] = 1
        elif 1001 == msgType :
            segLen,segBat = struct.unpack_from('HH',data,offs)
            offs  += 4
            stateM['segLen'] = segLen
            stateM['segBat'] = segBat

            req['segLen'] = segLen
            req['segBat'] = segBat
        elif 1002 == msgType :
            segLen,segBat,numSlot,cellId = struct.unpack_from('HHHB',data,offs)
            offs += 7
            stateM['segLen'] = segLen
            stateM['segBat'] = segBat
            stateM['req']['cfg'] = {'numSlot' : numSlot,'cellId' : cellId}

            req['segLen']  = segLen
            req['segBat']  = segBat
            req['numSlot'] = numSlot
            req['cellId']  = cellId
    elif 3000 == msgType :
        segIdx,synCode = struct.unpack_from('IB',data,offs)
        offs += 5
        stateM['state']          = 'sync'
        stateM['sync']['type']   = synCode
        stateM['sync']['segIdx'] = segIdx

        req['synCode'] = synCode
        req['segIdx']  = segIdx

    return req

def build_rsp_msg(stateM) :
    if stateM['state'] == 'init' :
        if   1000 == stateM['req']['type'] :
            stateM['numMsg']  = 1
            stateM['msgIdx']  = 0
            stateM['msgBuf']  = [{'ptr':'','rsp':2000}]
            stateM['numSeg']  = 1
            stateM['segIdx']  = 0
        elif 1001 == stateM['req']['type'] :
            stateM['numMsg']  = 1
            stateM['msgIdx']  = 0
            stateM['msgBuf']  = [{'ptr':'F:/matlab/data/console-test/phy_rt_log.bin','rsp':2001}]
            stateM['numSeg']  = math.ceil(v_get_bin_file_len(stateM['msgBuf'][0]['ptr'])/stateM['segLen'])
            stateM['segIdx']  = 0
        elif 1002 == stateM['req']['type'] :
            stateM['numMsg']  = 9
            stateM['msgIdx']  = 0
            stateM['msgBuf']  = [{'ptr':'F:/matlab/data/console-test/phy_rt_log.bin'     ,'rsp':2001},
                                 {'ptr':'F:/matlab/data/console-test/iq_dl_ant0_car0.bin','rsp':2002,'iqType':0,'cellId':0,'antId':0},
                                 {'ptr':'F:/matlab/data/console-test/iq_dl_ant1_car0.bin','rsp':2002,'iqType':0,'cellId':0,'antId':1},
                                 {'ptr':'F:/matlab/data/console-test/iq_dl_ant2_car0.bin','rsp':2002,'iqType':0,'cellId':0,'antId':2},
                                 {'ptr':'F:/matlab/data/console-test/iq_dl_ant3_car0.bin','rsp':2002,'iqType':0,'cellId':0,'antId':3},
                                 {'ptr':'F:/matlab/data/console-test/iq_ul_ant0_car0.bin','rsp':2002,'iqType':1,'cellId':0,'antId':0},
                                 {'ptr':'F:/matlab/data/console-test/iq_ul_ant1_car0.bin','rsp':2002,'iqType':1,'cellId':0,'antId':1},
                                 {'ptr':'F:/matlab/data/console-test/iq_ul_ant2_car0.bin','rsp':2002,'iqType':1,'cellId':0,'antId':2},
                                 {'ptr':'F:/matlab/data/console-test/iq_ul_ant3_car0.bin','rsp':2002,'iqType':1,'cellId':0,'antId':3},
                               ]
            stateM['numSeg']  = math.ceil(v_get_bin_file_len(stateM['msgBuf'][0]['ptr'])/stateM['segLen'])
            stateM['segIdx']  = 0

        stateM['state']  = 'busy'
    elif stateM['state'] == 'sync' :
        if 3 == stateM['sync']['type'] : # next msg
            stateM['msgIdx'] += 1
            if stateM['msgIdx'] == stateM['numMsg'] :
                print('TX end, idle')
                stateM['state'] = 'idle'
            else :
                print(stateM)
                stateM['numSeg'] = math.ceil(v_get_bin_file_len(stateM['msgBuf'][stateM['msgIdx']]['ptr'])/stateM['segLen'])
                stateM['segIdx'] = 0
                stateM['state']  = 'busy'
        else : # ack
            stateM['segIdx'] = stateM['sync']['segIdx']
            stateM['state']  = 'busy'
    elif stateM['state'] == 'busy' :
        if (stateM['segIdx'] == stateM['numSeg']) or (stateM['segIdx'] % stateM['segBat'] == 0) :
            stateM['state'] = 'pending'
            stateM['alive'] = 20000 # 10s
    elif stateM['state'] == 'pending' :
        stateM['alive'] -= 1
        if stateM['alive'] <= 0 :
            print('overtime, idle')
            stateM['state'] = 'idle' # link down, kill

    if stateM['state'] == 'busy' :
        cc  = stateM['msgBuf'][stateM['msgIdx']]['rsp']-2000
        msg = rsp_msg_build_func[cc](stateM)
    else :
        msg = []

    return msg

def build_cell_cfg_rsp(stateM) :
    msgType  = 2000
    msgLen   = 32
    lastMsg  = 1
    rsv      = 0
    numSeg   = 1
    segIdx   = 0
    numCell  = 2
    numTxAnt = 4
    numRxAnt = 8

    msg  = ctypes.create_string_buffer(4*8)
    offs = 0
    struct.pack_into('HH4BIIBBB',msg,offs,msgType,msgLen,lastMsg,rsv,rsv,rsv,numSeg,segIdx,numCell,numTxAnt,numRxAnt)

    return msg

def build_rt_rsp(stateM) :
    file_bin = stateM['msgBuf'][stateM['msgIdx']]['ptr']
    offs = stateM['segIdx']*stateM['segLen']
    data = v_read_bin_file(file_bin,offs,stateM['segLen'])

    msgType = 2001
    msgLen  = 32+len(data)
    if stateM['msgIdx'] == stateM['numMsg']-1 :
        lastMsg = 1
    else :
        lastMsg = 0
    rsv     = 0
    numSeg  = stateM['numSeg']
    segIdx  = stateM['segIdx']

    msg  = ctypes.create_string_buffer(msgLen)
    offs = 0
    struct.pack_into('HH4BII',msg,offs,msgType,msgLen,lastMsg,rsv,rsv,rsv,numSeg,segIdx)
    offs = 32
    msg[offs:] = data

    return msg

def build_iq_rsp(stateM) :
    file_bin = stateM['msgBuf'][stateM['msgIdx']]['ptr']
    offs = stateM['segIdx']*stateM['segLen']
    data = v_read_bin_file(file_bin,offs,stateM['segLen'])

    msgType = 2002
    msgLen  = 32+len(data)
    if stateM['msgIdx'] == stateM['numMsg']-1 :
        lastMsg = 1
    else :
        lastMsg = 0
    rsv     = 0
    thisMsg = stateM['msgBuf'][stateM['msgIdx']]

    numSeg  = stateM['numSeg']
    segIdx  = stateM['segIdx']
    cellId  = thisMsg['cellId']
    antId   = thisMsg['antId']
    iqType  = thisMsg['iqType']

    msg  = ctypes.create_string_buffer(msgLen)
    offs = 0
    struct.pack_into('HH4BII3B',msg,offs,msgType,msgLen,lastMsg,rsv,rsv,rsv,numSeg,segIdx,cellId,antId,iqType)
    offs = 32
    msg[offs:] = data

    return msg

def v_read_bin_file(file_bin,offs,n_byte) :
    with open(file_bin,'rb') as fo :
        fo.seek(offs,1)
        d = fo.read(n_byte)
    return d

def v_get_bin_file_len(file_bin) :
    with open(file_bin,'rb') as fo :
        fo.seek(0,2)
        n = fo.tell()
    return n

rsp_msg_build_func = [
    build_cell_cfg_rsp,
    build_rt_rsp,
    build_iq_rsp,
]

udp_server_up()


