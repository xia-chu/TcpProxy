/*
 * ProxyJumpSession.cpp
 *
 *  Created on: 2017年6月12日
 *      Author: xzl
 */

#include "ProxyJumpSession.h"
#include "ProxySession.h"
#include "Config.h"
#include "Util/onceToken.h"
#include "Network/sockutil.h"
#include "Network/TcpServer.h"
#include "Util/NoticeCenter.h"

using namespace toolkit;

namespace Proxy {

ProxyJumpSession::ProxyJumpSession(const Socket::Ptr &sock):TcpSession(sock),ProxyProtocol(TYPE_SERVER){
    DebugL << get_peer_ip();
}

ProxyJumpSession::~ProxyJumpSession() {
    DebugL << get_peer_ip();
}

void ProxyJumpSession::onRecv(const Buffer::Ptr &pBuf) {
	try{
		onRecvData(pBuf->data(),pBuf->size());
	}catch (std::exception &ex) {
		WarnL << ex.what();
		shutdown();
	}
}

int ProxyJumpSession::onSendData(const string &data) {
	return SockSender::send(data);
}

void ProxyJumpSession::onManager() {
	if(_aliveTicker.createdTime() > 10 * 1000){
		//设备必须在10秒内完成跳转操作
		WarnL << "规定时间未跳转：" << get_peer_ip();
		shutdown();
		return;
	}
}


bool ProxyJumpSession::onProcessRequest(const string &cmd,uint64_t seq, const Value &obj,const string &body) {
	Value resObj(objectValue);
	string jumpSrvUrl;
	uint16_t jumpSrvPort = 0;
	NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastSessionJump,(string &)jumpSrvUrl,(uint16_t &)jumpSrvPort,get_local_port());
	if(!jumpSrvUrl.empty() && jumpSrvPort != 0){
		resObj["jumpSrvUrl"] = jumpSrvUrl;
		resObj["jumpSrvPort"] = jumpSrvPort;
		sendResponse(seq,CODE_JUMP,"jump success",resObj,"");
	}else{
        sendResponse(seq,CODE_BUSY,"jump failed",resObj,"");
    }
    shutdown();
	return true;
}


} /* namespace Proxy */
