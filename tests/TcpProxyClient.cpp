
#include <signal.h>
#include <iostream>
#include "Poller/Timer.h"
#include "Proxy/Config.h"
#include "Proxy/Proxy.h"
#include "Util/CMD.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b) )
#endif //MAX

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b) )
#endif //MIN

using namespace std;
using namespace toolkit;


class CMD_client : public CMD {
public:
    CMD_client() {
        _parser.reset(new OptionParser);
        auto lam = [](const std::shared_ptr<ostream> &stream, const string &arg) {
            if (arg.find(":") == string::npos) {
                throw std::runtime_error("\t地址必须指明端口号.");
            }
            return true;
        };
        (*_parser) << Option('s', "srv_addr", Option::ArgRequired, "p2p.crossnat.cn:5002", false, "中转服务器地址", lam);
        (*_parser) << Option('u', "user", Option::ArgRequired, makeRandStr(6).data(), false, "登录用户名", nullptr);
        (*_parser) << Option('b', "bind_user", Option::ArgRequired, nullptr, false, "绑定目标用户名", nullptr);
        (*_parser) << Option('a', "bind_addr", Option::ArgRequired, "127.0.0.1:80", false, "绑定远程地址", lam);
        (*_parser) << Option('l', "local_ip", Option::ArgRequired, "127.0.0.1:80", false, "映射本机地址", lam);
        (*_parser) << Option('S', "ssl", Option::ArgNone, nullptr, false, "是否使用ssl登录", nullptr);
    }

    virtual ~CMD_client() {}
};

static CMD_client s_cmd;
static bool s_bExitFlag = false;
//重试登录计数
static uint64_t s_iTryCount = 0;
//可以重新登录
static bool s_bCanRelogin = false;

static Timer::Ptr s_timer;

void reLogin() {
    //重试延时最小2秒，最大60秒。每失败一次，延时累计3秒
    auto iDelay = MAX((uint64_t) 2 * 1000, MIN(s_iTryCount * 3000, (uint64_t) 60 * 1000));
    s_timer.reset(new Timer(iDelay / 1000.0, []() {
        WarnL << "重试登录:" << s_iTryCount;
        login(s_cmd.splitedVal("srv_addr")[0].data(),
              atoi(s_cmd.splitedVal("srv_addr")[1].data()),
              s_cmd["user"].data(),
              s_cmd.hasKey("ssl"));
        return false;
    }, nullptr));
}

int main(int argc, char *argv[]) {
    signal(SIGINT, [](int) { s_bExitFlag = true; });

    try {
        s_cmd(argc, argv);
    } catch (std::exception &e) {
        cout << e.what() << endl;
        return -1;
    }

    initProxySDK();
    Config::Proxy::loadIniConfig();

    semaphore login_ret_sem;
    setLoginCB([](void *ptr, int code, const char *msg) {
        WarnL << "onLogin:" << code << " " << msg;
        semaphore *sem = (semaphore *) ptr;
        sem->post();
        //如果不是因为鉴权问题，可以重试登录
        s_bCanRelogin = (code != 2);
        if (code == 0) {
            //登录成功后，重试登录计数清0
            s_iTryCount = 0;
        } else if (s_bCanRelogin) {
            //登录失败，并且可以重新登录
            ++s_iTryCount;
            reLogin();
        }
    }, &login_ret_sem);

    setShutdownCB([](void *ptr, int code, const char *msg) {
        WarnL << "onShutdown:" << code << " " << msg;
        //不是挤占掉线
        s_bCanRelogin = (code != 100);
        if (s_bCanRelogin) {
            reLogin();
        }
    }, nullptr);

    setBindCB([](void *ptr, const char *binder) {
        WarnL << "onBind:" << binder;
    }, nullptr);

    setSendSpeedCB([](void *userData, const char *dst_uuid, uint64_t bytesPerSec) {
        WarnL << "sendSpeed:" << dst_uuid << " " << bytesPerSec/1024 << " KB/s";
    }, nullptr);

    setUserInfo("appId", "test");
    setUserInfo("token", "test");

    InfoL << "user name:" << s_cmd["user"];

    login(s_cmd.splitedVal("srv_addr")[0].data(),
          atoi(s_cmd.splitedVal("srv_addr")[1].data()),
          s_cmd["user"].data(),
          s_cmd.hasKey("ssl"));
    login_ret_sem.wait();//等待登录结果

    BinderContext ctx = nullptr;
    if (s_cmd.hasKey("bind_user")) {
        ctx = createBinder();
        binder_setBindResultCB(ctx, [](void *userData, int errCode, const char *errMsg) {
            WarnL << "bind result:" << errCode << " " << errMsg;
        }, ctx);
        binder_setSocketErrCB(ctx, [](void *userData, int errCode, const char *errMsg) {
            WarnL << "connect slave failed:" << errCode << " " << errMsg;
        }, ctx);
        auto local_port = binder_bind2(ctx,
                                       s_cmd["bind_user"].data(),
                                       atoi(s_cmd.splitedVal("bind_addr")[1].data()),
                                       atoi(s_cmd.splitedVal("local_ip")[1].data()),
                                       s_cmd.splitedVal("bind_addr")[0].data(),
                                       s_cmd.splitedVal("local_ip")[0].data());
        InfoL << "绑定本地端口:" << local_port;
    }

    //保持主线程不退出
    while (!s_bExitFlag) {
        sleep(1);
    }

    if (ctx) {
        releaseBinder(ctx);
    }
    return 0;
}






