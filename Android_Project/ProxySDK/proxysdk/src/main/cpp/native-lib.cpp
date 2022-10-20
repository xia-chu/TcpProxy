#include <jni.h>
#include <string>
#include "Util/logger.h"
#include "Util/MD5.h"
#include "Util/onceToken.h"
#include "Proxy/Proxy.h"

using namespace std;
using namespace toolkit;

#define JNI_API(retType,funName,...) extern "C"  JNIEXPORT retType Java_cn_crossnat_proxysdk_ProxySDK_##funName(JNIEnv* env, jclass cls, ##__VA_ARGS__)
#define JClassSign_Response "cn/crossnat/proxysdk/ProxySDK$Response"
#define JClassSign_ResponseInterface "cn/crossnat/proxysdk/ProxySDK$ResponseInterface"

jobject makeJavaResponse(JNIEnv* env,jlong handle){
    static jclass jclass_obj = (jclass)env->NewGlobalRef(env->FindClass(JClassSign_Response));
    static jmethodID jmethodID_init = env->GetMethodID(jclass_obj, "<init>", "(J)V");
    if(!handle){
        return nullptr;
    }
    jobject ret = env->NewObject(jclass_obj, jmethodID_init,handle);
    return ret;
}

string stringFromJstring(JNIEnv *env,jstring jstr){
    if(!env || !jstr){
        WarnL << "invalid args";
        return "";
    }
    const char *field_char = env->GetStringUTFChars(jstr, 0);
    string ret(field_char,env->GetStringUTFLength(jstr));
    env->ReleaseStringUTFChars(jstr, field_char);
    return ret;
}

string stringFromJbytes(JNIEnv *env,jbyteArray jbytes){
    if(!env || !jbytes){
        WarnL << "invalid args";
        return "";
    }
    jbyte *bytes = env->GetByteArrayElements(jbytes, 0);
    string ret((char *)bytes,env->GetArrayLength(jbytes));
    env->ReleaseByteArrayElements(jbytes,bytes,0);
    return ret;
}

string stringFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jstring field_str = (jstring)env->GetObjectField(jdata,jid);
    auto ret = stringFromJstring(env,field_str);
    env->DeleteLocalRef(field_str);
    return ret;
}

string bytesFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jbyteArray jbufArray = (jbyteArray)env->GetObjectField(jdata, jid);
    string ret = stringFromJbytes(env,jbufArray);
    env->DeleteLocalRef(jbufArray);
    return ret;
}

jstring jstringFromString(JNIEnv* env, const char* pat){
    return (jstring)env->NewStringUTF(pat);
}

jbyteArray jbyteArrayFromString(JNIEnv* env, const char* pat,int len = 0){
    if(len <= 0){
        len = strlen(pat);
    }
    jbyteArray jarray = env->NewByteArray(len);
    env->SetByteArrayRegion(jarray, 0, len, (jbyte *)(pat));
    return jarray;
}

static JavaVM *s_jvm = nullptr;

/*
 * 加载动态库
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_jvm = vm;
    initProxySDK();
    return JNI_VERSION_1_6;
}

template<typename FUNC>
auto doInJavaThread(FUNC &&func) {
    JNIEnv *env;
    int status = s_jvm->GetEnv((void **) &env, JNI_VERSION_1_4);
    if (status != JNI_OK) {
        if (s_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
            throw std::runtime_error("AttachCurrentThread failed");
        }
    }
    onceToken token(nullptr, [status]() {
        if (status != JNI_OK) {
            //Detach线程
            s_jvm->DetachCurrentThread();
        }
    });
    return func(env);
}

#define emitEvent(delegate,autorelease,method,argFmt,...) \
    doInJavaThread([&](JNIEnv* env) { \
        jclass cls = env->GetObjectClass(delegate); \
        jmethodID jmid = env->GetMethodID(cls, method, argFmt); \
        jobject localRef = env->NewLocalRef(delegate); \
        if(localRef){ \
            env->CallVoidMethod(localRef, jmid, ##__VA_ARGS__); \
            /*局部变量其实在函数返回后会自动释放*/ \
            env->DeleteLocalRef(localRef); \
            if(autorelease){ \
                env->DeleteGlobalRef(delegate);\
            }\
        }else{ \
            WarnL << "弱引用已经释放:" << method << " " << argFmt; \
        }\
        /*局部变量其实在函数返回后会自动释放*/ \
        env->DeleteLocalRef(cls); \
    });

JNI_API(void,setUserInfo,jstring key,jstring value){
    setUserInfo(stringFromJstring(env,key).c_str(),stringFromJstring(env,value).c_str());
}

JNI_API(void,clearUserInfo){
    clearUserInfo();
}

JNI_API(void,login,jstring srv_url, jint srv_port, jstring user_name,jboolean use_ssl){
    login(stringFromJstring(env,srv_url).c_str(),srv_port,stringFromJstring(env,user_name).c_str(),use_ssl);
}

JNI_API(void,logout){
    logout();
}

JNI_API(jint ,getStatus){
    return getStatus();
}

JNI_API(jlong , createBinder){
    return (jlong) createBinder();
}

JNI_API(void ,releaseBinder,jlong ctx){
    releaseBinder((BinderContext)ctx);
}

JNI_API(jint ,bind,jlong ctx,jstring dst_user,jint dst_port ,jint self_port){
    return binder_bind((BinderContext)ctx,stringFromJstring(env,dst_user).c_str(),dst_port,self_port);
}

JNI_API(jint ,bind2,jlong ctx,jstring dst_user,jint dst_port ,jint self_port,jstring dst_url ,jstring self_ip){
    return binder_bind2((BinderContext)ctx,
                        stringFromJstring(env,dst_user).c_str(),
                        dst_port,
                        self_port,
                        stringFromJstring(env,dst_url).c_str(),
                        stringFromJstring(env,self_ip).c_str());
}

JNI_API(void ,setBinderDelegate,jlong _ctx,jobject obj){
    BinderContext ctx = (BinderContext)_ctx;
    if(obj){
        //弱全局引用可能会导致内存泄露
        jobject globalRef = env->NewWeakGlobalRef(obj);
        binder_setBindResultCB(ctx,[](void *userData, int errCode, const char *errMsg){
            emitEvent((jobject)userData, false, "onBindResult","(ILjava/lang/String;)V",(jint)errCode,env->NewStringUTF(errMsg));
        },globalRef);

        binder_setSocketErrCB(ctx,[](void *userData, int errCode, const char *errMsg){
            emitEvent((jobject)userData, false, "onRemoteSockErr","(ILjava/lang/String;)V",(jint)errCode,env->NewStringUTF(errMsg));
        },globalRef);
    } else{
        binder_setBindResultCB(ctx, nullptr, nullptr);
        binder_setSocketErrCB(ctx, nullptr, nullptr);
    }
}

JNI_API(void ,setProxyDelegate,jobject obj){
    makeJavaResponse(env, (jlong) 0);
    if(obj){
        jobject globalRef = env->NewWeakGlobalRef(obj);
        setLoginCB([](void *userData, int errCode, const char *errMsg){
            emitEvent((jobject) userData, false, "onLoginResult", "(ILjava/lang/String;)V", (jint) errCode, env->NewStringUTF(errMsg));
        },globalRef);

        setShutdownCB([](void *userData, int errCode, const char *errMsg){
            emitEvent((jobject) userData, false, "onShutdown", "(ILjava/lang/String;)V",(jint)errCode,env->NewStringUTF(errMsg));
        },globalRef);

        setBindCB([](void *userData, const char *binder){
            emitEvent((jobject) userData, false, "onBinded", "(Ljava/lang/String;)V",env->NewStringUTF(binder));
        },globalRef);

        setSendSpeedCB([](void *userData, const char *dst_uuid, uint64_t bytesPerSec){
            emitEvent((jobject) userData, false, "onSendSpeed", "(Ljava/lang/String;J)V", env->NewStringUTF(dst_uuid), (jlong) bytesPerSec);
        },globalRef);

        setMessageCB([](void *userData, const char *from, const char *obj, const char *content, int content_len, void *invoker){
            emitEvent((jobject) userData, false, "onMessage","(Ljava/lang/String;Ljava/lang/String;L" JClassSign_ResponseInterface ";)V",
                      env->NewStringUTF(from), env->NewStringUTF(content), makeJavaResponse(env, (jlong) invoker));
        },globalRef);

        setJoinRoomCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            emitEvent((jobject) userData,
                      false,
                      "onJoinRoom",
                      "(Ljava/lang/String;Ljava/lang/String;)V",
                      env->NewStringUTF(from), env->NewStringUTF(room_id));
        },globalRef);

        setExitRoomCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            emitEvent((jobject) userData,
                      false,
                      "onExitRoom",
                      "(Ljava/lang/String;Ljava/lang/String;)V",
                      env->NewStringUTF(from), env->NewStringUTF(room_id));
        },globalRef);

        setRoomBroadcastCB([](void *userData, const char *from, const char *room_id, const char *obj, const char *content, int content_len, void *invoker){
            ProxyResponseDoAndRelease(invoker, 0, "", nullptr, "", 0);
            emitEvent((jobject)userData,
                      false,
                      "onRoomBroadcast",
                      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                      env->NewStringUTF(from),env->NewStringUTF(room_id),env->NewStringUTF(content));
        },globalRef);

    } else{
        setLoginCB(nullptr, nullptr);
        setShutdownCB(nullptr, nullptr);
        setBindCB(nullptr, nullptr);
        setSendSpeedCB(nullptr, nullptr);
        setMessageCB(nullptr, nullptr);
        setJoinRoomCB(nullptr, nullptr);
        setExitRoomCB(nullptr, nullptr);
        setRoomBroadcastCB(nullptr, nullptr);
    }
}

JNI_API(void, invokeResponse, jlong ptr, jint code, jstring msg, jstring res) {
    ProxyResponseDoAndRelease((void*)ptr, code, stringFromJstring(env, msg).data(), nullptr, stringFromJstring(env, res).data(), 0);
}

JNI_API(void,message,jstring to_uuid,jstring data,jobject onRes){
    auto data_str = stringFromJstring(env, data);
    jobject globalRef = onRes ? env->NewGlobalRef(onRes) : nullptr;
    sendMessage(stringFromJstring(env, to_uuid).data(), data_str.data(), data_str.size(), globalRef,[](void *userData, int code, const char *message, const char *obj, const char *content, int content_len) {
        jobject globalRef = (jobject) userData;
        if (globalRef) {
            emitEvent((jobject) globalRef, true, "onResponse", "(ILjava/lang/String;Ljava/lang/String;)V", (jint) code, env->NewStringUTF(message), env->NewStringUTF(content));
        }
    });
}

JNI_API(void,joinRoom,jstring room_id,jobject onRes){
    jobject globalRef = onRes ? env->NewGlobalRef(onRes) : nullptr;
    joinRoom(stringFromJstring(env,room_id).data(), globalRef, [](void *userData, int code, const char *message,const char *obj,const char *content,int content_len){
        jobject globalRef = (jobject) userData;
        if (globalRef) {
            emitEvent((jobject)globalRef,true, "onResponse","(ILjava/lang/String;Ljava/lang/String;)V",(jint)code,env->NewStringUTF(message),env->NewStringUTF(content));
        }
    });
}

JNI_API(void,exitRoom,jstring room_id,jobject onRes){
    jobject globalRef = onRes ? env->NewGlobalRef(onRes) : nullptr;
    exitRoom(stringFromJstring(env,room_id).data(), globalRef, [](void *userData, int code, const char *message,const char *obj,const char *content,int content_len){
        jobject globalRef = (jobject) userData;
        if (globalRef) {
            emitEvent((jobject)globalRef,true, "onResponse","(ILjava/lang/String;Ljava/lang/String;)V",(jint)code,env->NewStringUTF(message),env->NewStringUTF(content));
        }
    });
}

JNI_API(void,broadcastRoom,jstring room_id,jstring data,jobject onRes){
    jobject globalRef = onRes ? env->NewGlobalRef(onRes) : nullptr;
    auto data_str = stringFromJstring(env, data);
    broadcastRoom(stringFromJstring(env,room_id).data(), data_str.data(), data_str.size(), globalRef, [](void *userData, int code, const char *message,const char *obj,const char *content,int content_len){
        jobject globalRef = (jobject) userData;
        if (globalRef) {
            emitEvent((jobject)globalRef,true, "onResponse","(ILjava/lang/String;Ljava/lang/String;)V",(jint)code,env->NewStringUTF(message),env->NewStringUTF(content));
        }
    });
}

JNI_API(void,setGlobalOption,jstring key, jstring value){
    setGlobalOption(stringFromJstring(env,key).c_str(),stringFromJstring(env,value).c_str());
}

JNI_API(jstring,getGlobalOption,jstring key){
    char value[512];
    getGlobalOption(stringFromJstring(env,key).c_str(),value);
    return env->NewStringUTF(value);
}

JNI_API(jstring,dumpOptionsIni){
    char out[1024 * 4];
    int size = sizeof(out);
    dumpOptionsIni(out,&size);
    return env->NewStringUTF(out);
}

JNI_API(jint,loadOptionsIni,jstring ini_str){
    return loadOptionsIni(stringFromJstring(env,ini_str).c_str());
}
