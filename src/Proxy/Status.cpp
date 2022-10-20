//
// Created by xzl on 2020/8/21.
//

#include "Status.h"
#include <assert.h>
#include <iomanip>
#include "Poller/EventPoller.h"
#include "Util/onceToken.h"
using namespace toolkit;

namespace Proxy {

void Status::addCountSignal(bool is_recv, uint64_t bytes) {
    assert(EventPoller::Instance().isCurrentThread());
    _now._bytes_signal[is_recv] += bytes;
}

void Status::addCountBindServer(bool is_recv, uint64_t bytes) {
    assert(EventPoller::Instance().isCurrentThread());
    _now._bytes_bind_server[is_recv] += bytes;
}

void Status::addCountBindClient(bool is_recv, uint64_t bytes) {
    assert(EventPoller::Instance().isCurrentThread());
    _now._bytes_bind_client[is_recv] += bytes;
}

void Status::addBytesTransfer( uint64_t bytes) {
    assert(EventPoller::Instance().isCurrentThread());
    _now._bytes_transfer += bytes;
    _bytes_transfer_total += bytes;
}

void Status::addBytesAck( uint64_t bytes) {
    assert(EventPoller::Instance().isCurrentThread());
    _now._bytes_ack += bytes;
    _bytes_ack_total += bytes;
}

void Status::addBytesAckFailed(uint64_t bytes){
    assert(EventPoller::Instance().isCurrentThread());
    _bytes_ack_failed_total += bytes;
}

Status &Status::Instance(){
    static std::shared_ptr<Status> s_instance(new Status);
    static onceToken s_token([](){
        s_instance->onCreate();
    });
    return *s_instance;
}

void Status::onCreate(){
    weak_ptr<Status> weak_self = shared_from_this();
    EventPoller::Instance().doDelayTask(2000, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return 0;
        }
        strong_self->onTick();
        return 2000;
    });
}

void Status::onTick(){
    auto time_ms = _ticker.elapsedTime();
    _ticker.resetTime();
    if (_now._bytes_bind_server[0] != 0 || _now._bytes_bind_server[1] != 0 ||
        _now._bytes_bind_client[0] != 0 || _now._bytes_bind_client[1] != 0 ||
        (_bytes_transfer_total - _bytes_ack_total) > 1024 * 32) {

        InfoL << "\r\n"
              << "data speed(send/recv,KB/s) \r\n"
              << "signal:      " << setw(8) << setfill(' ') << _now._bytes_signal[0] / time_ms << " | " << _now._bytes_signal[1] / time_ms << "\r\n"
              << "bind_server: " << setw(8) << setfill(' ') << _now._bytes_bind_server[0] / time_ms << " | " << _now._bytes_bind_server[1] / time_ms << "\r\n"
              << "bind_client: " << setw(8) << setfill(' ') << _now._bytes_bind_client[0] / time_ms << " | " << _now._bytes_bind_client[1] / time_ms << "\r\n"
              << "transfer:    " << setw(8) << setfill(' ') << _now._bytes_transfer / time_ms << "\r\n"
              << "ack:         " << setw(8) << setfill(' ') << _now._bytes_ack / time_ms<< "\r\n"
              << "no ack:      " << setw(8) << setfill(' ') << (_bytes_transfer_total - _bytes_ack_total) / 1024 << "\r\n"
              << "ack failed:  " << setw(8) << setfill(' ') << (_bytes_ack_failed_total) / 1024 ;
    }
    memset(&_now, 0, sizeof(_now));
}

void Status::clearNoAck(){
    _bytes_ack_failed_total = 0;
    _bytes_transfer_total = 0;
    _bytes_ack_total = 0;
}

}//namespace Proxy