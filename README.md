# 说明
一个tcp代理工具；借助公网服务器中转，可以把NAT内的远端设备的tcp端口映射至localhost，访问localhost映射端口相当于访问远端设备端口。

# 支持平台
 linux/windows/mac/ios/android
 
# 注意
 该项目依赖[zlmediakit](https://github.com/ZLMediaKit/ZLMediaKit),编译前这样下载完整代码：
 
```bash
git clone https://github.com/ZLMediaKit/ZLMediaKit.git
cd ZLMediaKit
git checkout b3fd74ccf030fc5f0185660c9cf0e2b812ad1561
git submodule update --init

cd ..
git clone https://github.com/xia-chu/TcpProxy.git
#代码依赖下载完毕，后续按照正常流程编译即可
```

# 免责声明
使用本项目不得从事入侵、破坏计算机系统等一切违法犯罪行为；使用本项目导致的安全风险、损失等后果一概与本项目和作者无关；使用本项目意味已同意上述免责声明。
