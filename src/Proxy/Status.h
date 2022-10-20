//
// Created by xzl on 2020/8/21.
//

#ifndef TCPPROXY_STATUS_H
#define TCPPROXY_STATUS_H

#include <stdint.h>
#include <memory>
#include "Util/TimeTicker.h"
using namespace std;
using namespace toolkit;

namespace Proxy {

typedef struct StatusDataTag{
    uint64_t _bytes_signal[2] = {0, 0};
    uint64_t _bytes_bind_server[2] = {0, 0};
    uint64_t _bytes_bind_client[2] = {0, 0};
    uint64_t _bytes_transfer = 0;
    uint64_t _bytes_ack = 0;
} StatusData;

class Status : public std::enable_shared_from_this<Status>{
public:
    ~Status(){}

    static Status &Instance();
    void addCountSignal(bool is_recv, uint64_t bytes);
    void addCountBindServer(bool is_recv, uint64_t bytes);
    void addCountBindClient(bool is_recv, uint64_t bytes);
    void addBytesTransfer(uint64_t bytes);
    void addBytesAck(uint64_t bytes);
    void addBytesAckFailed(uint64_t bytes);
    void clearNoAck();

private:
    Status(){};
    void onCreate();
    void onTick();

private:
    Ticker _ticker;
    StatusData _now;
    uint64_t _bytes_transfer_total = 0;
    uint64_t _bytes_ack_total = 0;
    uint64_t _bytes_ack_failed_total = 0;
};

}//namespace Proxy
#endif //TCPPROXY_STATUS_H
