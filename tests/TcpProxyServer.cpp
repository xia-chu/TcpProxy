#include <signal.h>
#include <sys/resource.h>
#include <functional>
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Proxy/Config.h"
#include "Proxy/ProxySession.h"

using namespace std;
using namespace toolkit;
using namespace Proxy;

namespace Config {

namespace Proxy {
#define PROXY_FIELD "proxy."
const char kUrl[] = PROXY_FIELD"url";
const char kPort[] = PROXY_FIELD"port";
const char kSSLPort[] = PROXY_FIELD"port_ssl";

static onceToken token([]() {
    mINI::Instance()[kUrl] = "";//业务服务器url
    mINI::Instance()[kPort] = 5002;//业务服务器端口号
    mINI::Instance()[kSSLPort] = 5003;//业务服务器端口号
}, nullptr);
}//namespace Proxy

}//namespace Config

int main(int argc, char *argv[]) {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().add(std::make_shared<FileChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    Config::Proxy::loadIniConfig();
    SSL_Initor::Instance().loadCertificate((exePath() + ".pem").data());

    InfoL << "ProxyServer";
    TcpServer::Ptr proxyServer(new TcpServer());
    TcpServer::Ptr proxyServer_ssl(new TcpServer());

    uint16_t proxy_port = mINI::Instance()[Config::Proxy::kPort];
    uint16_t proxy_port_ssl = mINI::Instance()[Config::Proxy::kSSLPort];

    if (proxy_port) {
        proxyServer_ssl->start<ProxySession>(proxy_port);
    }

#ifdef ENABLE_OPENSSL
    if (proxy_port_ssl) {
        proxyServer->start<TcpSessionWithSSL<ProxySession> >(proxy_port_ssl);
    }
#endif

    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });
    sem.wait();
    return 0;
}







