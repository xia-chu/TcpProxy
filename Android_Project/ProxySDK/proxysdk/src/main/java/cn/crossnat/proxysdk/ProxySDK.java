package cn.crossnat.proxysdk;

import java.util.Map;

public class ProxySDK {
    public static final int CODE_SUCCESS = 0;
    public static final int CODE_OTHER = -1;
    public static final int CODE_TIMEOUT = -2;
    public static final int CODE_AUTHFAILED = -3;
    public static final int CODE_UNSUPPORT = -4;
    public static final int CODE_NOTFOUND = -5;
    public static final int CODE_JUMP = -6;//服务器跳转
    public static final int CODE_BUSY = -7;//服务器忙
    public static final int CODE_NETWORK = -8;//网络错误
    public static final int CODE_GATE = -9;//网关错误
    public static final int CODE_BAD_REQUEST = -10;//非法的请求
    public static final int CODE_CONFLICT = -100; //被挤占登录
    public static final int CODE_BAD_LOGIN = -101;//正在登陆或者已经登录了

    /**
     * SDK全局事件监听
     */
    public interface ProxySDKEvent {

        /**
         * 登录结果回调
         * @param errCode 错误代码
         * @param errMsg 错误提示
         */
        void onLoginResult(int errCode, String errMsg);

        /**
         * 掉线事件
         * @param errCode 错误代码
         * @param errMsg 错误提示或对方ip
         */
        void onShutdown(int errCode, String errMsg);

        /**
         * 被其他用户绑定事件
         * @param binder 对方登录用户ID
         */
        void onBinded(String binder);

        /**
         * 发送网速统计事件
         * @param dst_uuid 目标用户ID
         * @param bytesPerSec 发送网速,单位B/S
         */
        void onSendSpeed(String dst_uuid, long bytesPerSec);

        /**
         * 收到透传数据事件
         * @param from_uuid 数据来自目标
         * @param data 请求数据
         * @param res_handle 通过该对象回复数据
         */
        void onMessage(String from_uuid,String data,ResponseInterface res_handle);

        /**
         * 其他人进入房间
         * @param from_uuid 用户id
         * @param room_id 房间id
         */
        void onJoinRoom(String from_uuid,String room_id);

        /**
         * 其他人退出房间
         * @param from_uuid
         * @param room_id
         */
        void onExitRoom(String from_uuid,String room_id);

        /**
         * 收到房间信息广播
         * @param from_uuid 数据发送者
         * @param room_id 房间id
         * @param data 数据内容
         */
        void onRoomBroadcast(String from_uuid,String room_id,String data);
    }

    /**
     * 数据回复接口
     */
    public interface ResponseInterface
    {
        /**
         * 主动回复对方数据，或收到对方回复
         * @param code 错误代码
         * @param msg 错误提示
         * @param res 回复内容
         */
        void onResponse(int code,String msg,String res);
    }

    private static class Response implements ResponseInterface{
        public Response(long handle){
            mHandle = handle;
        }
        public void onResponse(int code,String msg,String res){
            if(mHandle != 0){
                invokeResponse(mHandle,code,msg,res);
                mHandle = 0;
            }
        }
        private long mHandle = 0;
    }

    /**
     * 绑定器事件监听
     */
    public interface ProxyBinderEvent {

        /**
         * 绑定结果回调函数定义
         * @param errCode 错误代码
         * @param errMsg 错误提示
         */
        void onBindResult(int errCode, String errMsg);//登录结果回调

        /**
         * 远程套接字异常事件
         * @param errCode 错误代码
         * @param errMsg 错误提示
         */
        void onRemoteSockErr(int errCode, String errMsg);
    }

    /**
     * 创建一个绑定器,所谓绑定指的是把某远程主机某一端口映射到本机某端口
     */
    public static class ProxyBinder {

        /**
         * 构造函数,在native层创建绑定器
         */
        public ProxyBinder(){
            contex = createBinder();
        }

        /**
         * 手动销毁绑定器,防止由于GC滞后导致本机端口迟迟无法释放的问题
         */
        public void Release(){
            if(contex != 0){
                SetDelegate(null);
                releaseBinder(contex);
                contex = 0;
            }
        }

        /**
         * GC触发的绑定器销毁动作
         */
        @Override
        protected void finalize() throws Throwable {
            super.finalize();
            Release();
        }

        /**
         * 触发绑定动作;绑定成功后,访问本机映射的TCP端口犹如访问目标主机
         * @param dst_user 目标主机
         * @param dst_port 目标主机端口号
         * @param self_port 映射至本机的端口号,如果传入0则让系统随机分配
         * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口)
         */
        public int Bind(String dst_user,int dst_port ,int self_port){
            return bind2(contex,dst_user,dst_port,self_port,"127.0.0.1","127.0.0.1");
        }

        /**
         * 触发绑定动作;绑定成功后,访问本机映射的TCP端口将触发目标主机去访问dst_url:dst_port对应的主机
         * @param dst_user 目标主机
         * @param dst_port 目标主机访问的端口号
         * @param self_port 映射至本机的端口号,如果传入0则让系统随机分配
         * @param dst_url 目标主机访问的ip或域名
         * @param self_ip 本机监听ip地址,建议127.0.0.1或0.0.0.0
         * @return 本机端口号,-1代表绑定本机端口失败(可能已占用,也可能没有权限使用该端口)
         */
        public int Bind(String dst_user,int dst_port ,int self_port,String dst_url,String self_ip){
            return bind2(contex,dst_user,dst_port,self_port,dst_url,self_ip);
        }

        /**
         * 设置绑定事件监听
         * @param delegate 监听者,设置null则移除监听
         * @see ProxyBinderEvent
         */
        public void SetDelegate(ProxyBinderEvent delegate){
            setBinderDelegate(contex,delegate);
        }

        //native层绑定者指针地址
        private long contex = 0;
    }

    /**
     * 异步登录
     * @param srv_url 服务器ip或域名
     * @param srv_port 服务器端口
     * @param user_name 登录用户名
     * @param user_info 用户其他信息,后续可能要传入密码、设备信息等
     * @param use_ssl 服务器端口是否为加密服务端口
     */
    public static void Login(String srv_url, int srv_port, String user_name, Map<String,String> user_info, boolean use_ssl){
        clearUserInfo();
        if(user_info != null){
            Object[] keyValuePairs = user_info.entrySet().toArray();
            for (int i = 0; i < user_info.size(); i++){
                Map.Entry entry = (Map.Entry)keyValuePairs[i];
                setUserInfo((String)entry.getKey(),(String)entry.getValue());
            }
        }
        login(srv_url,srv_port,user_name,use_ssl);
    }

    /**
     * 异步登出
     */
    public static void Logout(){
        logout();
    }

    /**
     * 获取当前登录状态
     * @return 登录状态;0:未登录 1:登录中 2: 已登录
     */
    public static int GetStatus(){
        return getStatus();
    }

    /**
     * 设置SDK事件监听
     * @param delegate SDK事件监听者,设置null则移除监听
     * @see ProxySDKEvent
     */
    public static void SetDelegate(ProxySDKEvent delegate){
        setProxyDelegate(delegate);
    }


    /**
     * 设置全局参数配置
     * @param key 配置名
     * @param value 配置值
     */
    public static void SetGlobalOption(String key,String value){
        setGlobalOption(key,value);
    }


    /**
     * 获取全局参数配置
     * @param key 配置名
     * @return 配置值
     */
    public static String GetGlobalOption(String key){
        return getGlobalOption(key);
    }

    /**
     * 以ini格式导出全局配置
     * @return 导出的ini配置
     */
    public static String DumpOptionsIni(){
        return dumpOptionsIni();
    }

    /**
     * 以ini格式加载全局配置
     * @param ini_str ini配置字符串
     */
    public static void LoadOptionsIni(String ini_str){
        loadOptionsIni(ini_str);
    }

    /**
     * 发送消息给对方
     * @param dst_user 目标用户
     * @param data 数据
     * @param onRes 回调
     */
    public static void Message(String dst_user,String data,ResponseInterface onRes){
        message(dst_user,data,onRes);
    }

    /**
     * 加入房间
     * @param room_id 房间id
     * @param onRes 成功与否回调
     */
    public static void JoinRoom(String room_id,ResponseInterface onRes){
        joinRoom(room_id,onRes);
    }

    /**
     * 退出房间
     * @param room_id 房间id
     * @param onRes 成功与否回调
     */
    public static void ExitRoom(String room_id,ResponseInterface onRes){
        exitRoom(room_id,onRes);
    }

    /**
     * 发送群消息
     * @param room_id 房间id
     * @param data 消息内容
     * @param onRes 成功与否回调
     */
    public static void BroadcastRoom(String room_id,String data,ResponseInterface onRes){
        broadcastRoom(room_id,data,onRes);
    }

    /**
     * native层私有接口
     */
    private static native void setUserInfo(String key,String value);
    private static native void clearUserInfo();
    private static native void login(String srv_url, int srv_port, String user_name,boolean use_ssl);
    private static native void logout();
    private static native int  getStatus();
    private static native void setProxyDelegate(Object obj);

    private static native long createBinder();
    private static native void releaseBinder(long ctx);
    private static native int  bind(long ctx,String dst_user,int dst_port ,int self_port);
    private static native int  bind2(long ctx,String dst_user,int dst_port ,int self_port,String dst_url,String self_ip);
    private static native void setBinderDelegate(long ctx,Object obj);

    private static native void setGlobalOption(String key,String value);
    private static native String getGlobalOption(String key);
    private static native String dumpOptionsIni();
    private static native int loadOptionsIni(String ini_str);
    private static native void invokeResponse(long res_handle,int code,String msg,String response);
    private static native void message(String dst_user,String data,ResponseInterface onRes);

    private static native void joinRoom(String room_id,ResponseInterface onRes);
    private static native void exitRoom(String room_id,ResponseInterface onRes);
    private static native void broadcastRoom(String room_id,String data,ResponseInterface onRes);

    /////////////////////////JNI底层接口，请勿直接使用，请使用JAVA对象/////////////////////////////////////////////

    static {
        System.loadLibrary("ProxySDK");
    }

}
