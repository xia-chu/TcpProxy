#include <signal.h>
#include <iostream>
#include <list>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Proxy/ProxyTerminal.h"
#include "Util/CMD.h"

using namespace std;
using namespace toolkit;
using namespace Proxy;

class CMD_benchmark: public CMD {
public:
	CMD_benchmark() {
		_parser.reset( new OptionParser);
        auto lam = [](const std::shared_ptr<ostream> &stream, const string &arg){
            if(arg.find(":") == string::npos){
                throw std::runtime_error("\t地址必须指明端口号.");
            }
            return true;
        };
		(*_parser) << Option('s', "srv_addr", Option::ArgRequired,"crossnat.cn:10000",false,"中转服务器地址", lam);
		(*_parser) << Option('n', "num", Option::ArgRequired,"1",false, "测试用户个数", nullptr);
		(*_parser) << Option('u', "user", Option::ArgRequired,"0",false,"测试起始用户ID", nullptr);
		(*_parser) << Option('d', "delay", Option::ArgRequired,"100",false,"用户延时启动毫秒数", nullptr);
        (*_parser) << Option('S', "ssl", Option::ArgNone, nullptr,false,"是否使用ssl登录",nullptr);
	}
	virtual ~CMD_benchmark() {}
};


int main(int argc, char *argv[]) {
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));

	CMD_benchmark cmd;
	try {
		cmd(argc, argv);
	} catch (std::exception &e) {
		cout << e.what() << endl;
		return -1;
	}

	auto processCount = thread::hardware_concurrency();
	if (processCount < 4) {
		processCount = 4;
	}
	auto numPerProcess = atoi(cmd["num"].data()) / processCount;
	InfoL << "将创建" << processCount << "个子进程,每个进程将启动" << numPerProcess
			<< "个测试用户." << endl;

	auto parentPid = getpid();
	int processIndex = 0;
	list<pid_t> childPidList;
	for (int i = 0; i < processCount; i++) {
		if (parentPid == getpid()) {
			//父进程
			processIndex = i;
			auto childPid = fork();
			childPidList.push_back(childPid);
		} else {
			//子进程
			break;
		}
	}
	if (parentPid == getpid()) {
		//父进程
		for (auto pid : childPidList) {
			kill(pid, SIGINT);
			waitpid(pid, NULL, 0);
		}
		InfoL << "父进程退出." << endl;
	} else {
		Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
		InfoL << "子进程[" << processIndex << "]已启动." << endl;

		std::list<ProxyTerminal::Ptr> clientList;
		int onlineClientCout = 0;
		int clientUidIndex = processIndex * numPerProcess + atoi(cmd["user"].data());
		bool ssl = cmd.hasKey("ssl");
		DebugL << "是否启用ssl:" << ssl;

		Timer::Ptr timer(new Timer(atoi(cmd["delay"].data())/1000.0,[&](){
			ProxyTerminal::Ptr terminal(new ProxyTerminal);
			terminal->setOnLogin([&](int code,const string &msg) {
				if(code == 0) {
					DebugL << "子进程[" << processIndex << "]在线客户端：" << ++onlineClientCout << endl;
				}
			});
			terminal->setOnShutdown([&](const SockException &ex) {
				DebugL << "子进程[" << processIndex << "]在线客户端：" << --onlineClientCout << endl;
			});
			terminal->login(cmd.splitedVal("srv_addr")[0],atoi(cmd.splitedVal("srv_addr")[1].data()),to_string(clientUidIndex++),Value(objectValue),ssl);
			clientList.push_back(terminal);
			return --numPerProcess;
		}, nullptr));

		static semaphore sem;
		signal(SIGINT, [](int) { sem.post(); });
		sem.wait();

		InfoL << "子进程[" << processIndex << "]退出." << endl;
	}

	return 0;
}

