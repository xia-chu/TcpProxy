package cn.crossnat.proxydemo;
import android.content.Intent;
import android.content.res.Configuration;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.Toast;
import cn.crossnat.proxysdk.ProxySDK;
import java.util.HashMap;

public class MainActivity extends AppCompatActivity implements ProxySDK.ProxySDKEvent {
    public final static  String APP_TAG="APP";
    public final static  String NOTICE_SHUTDOWN="ProxySDK.onShutdown";
    private EditText txt_user ;
    private EditText txt_pwd ;
    private Button bt_login ;
    private ProgressBar progressBar ;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        txt_user = (EditText)findViewById(R.id.editText_userName);
        txt_pwd = (EditText)findViewById(R.id.editText_pwd);
        bt_login = (Button)findViewById(R.id.button_login);
        progressBar = (ProgressBar)findViewById(R.id.progressBar_login);

        updateStatus(ProxySDK.GetStatus());
        ProxySDK.SetDelegate(this);
    }

    private void updateStatus(int status){
        switch (status){
            case 0:{//0：未登录 1：登录中 2: 已登录
                txt_user.setEnabled(true);
                txt_pwd.setEnabled(true);
                bt_login.setEnabled(true);
                bt_login.setText("登录");
                progressBar.setVisibility(View.INVISIBLE);
            }break;
            case 1:{//0：未登录 1：登录中 2: 已登录
                txt_user.setEnabled(false);
                txt_pwd.setEnabled(false);
                bt_login.setEnabled(false);
                bt_login.setText("登录中...");
                progressBar.setVisibility(View.VISIBLE);
            }break;
            case 2:{//0：未登录 1：登录中 2: 已登录
                txt_user.setEnabled(false);
                txt_pwd.setEnabled(false);
                bt_login.setEnabled(false);
                bt_login.setText("登录成功");
                progressBar.setVisibility(View.INVISIBLE);
            }break;
        }
    }

    public void onClick_login(View sender){
        updateStatus(1);
        HashMap<String, String> map = new HashMap<String, String>();
        map.put("token", txt_pwd.getText().toString());
        map.put("appId", "test");
        //TODO 此处修改为真实的服务器ip端口
        ProxySDK.Login("10.138.59.148", 8500, txt_user.getText().toString(), map, false);
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateStatus(ProxySDK.GetStatus());
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateStatus(ProxySDK.GetStatus());
        Log.d(APP_TAG,"onConfigurationChanged " + newConfig.toString());
    }

    /**
     * @brief 登录结果回调
     * @param errCode 错误代码
     * @param errMsg 错误提示
     */
    @Override
    public void onLoginResult(final int errCode,final String errMsg){
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateStatus(ProxySDK.GetStatus());
                if (errCode == 0) {
                    //成功
                    Intent intent = new Intent(MainActivity.this, ActivityBinder.class);
                    MainActivity.this.startActivity(intent);
                    ProxySDK.JoinRoom("room:1", new ProxySDK.ResponseInterface() {
                        @Override
                        public void onResponse(int code, String msg, String res) {
                            Log.e("TAG", "join room " + code + " " + msg);
                        }
                    });
                } else {
                    AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
                    builder.setTitle("登录失败");
                    builder.setMessage("错误码:" + errCode + ",提示:" + errMsg);
                    builder.setNegativeButton("好的", null);
                    builder.show();
                }
            }
        });
    }

    /**
     * @brief 掉线事件
     * @param errCode 错误代码
     * @param errMsg 错误提示或对方ip
     */
    @Override
    public void onShutdown(final int errCode, final String errMsg){
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateStatus(ProxySDK.GetStatus());
                Toast.makeText(MainActivity.this,"掉线," + "错误码:" + errCode + ",提示:" + errMsg,Toast.LENGTH_SHORT).show();
                NoticeCenter.Instance().PostNotice(NOTICE_SHUTDOWN,null);
            }
        });
    }

    /**
     * @brief 被其他用户绑定事件
     * @param binder 对方登录用户ID
     */
    @Override
    public void onBinded(final String binder){
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateStatus(ProxySDK.GetStatus());
                Toast.makeText(MainActivity.this,"被" + binder + "绑定",Toast.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public void onMessage(final String from_uuid, final String data, ProxySDK.ResponseInterface res_handle) {
        res_handle.onResponse(0, "我已经收到该消息", "我已经收到该消息");
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MainActivity.this,"收到来自" + from_uuid + "的消息:" + data,Toast.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public void onJoinRoom(final String from_uuid, final String room_id) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MainActivity.this, from_uuid  + "加入房间:" + room_id, Toast.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public void onExitRoom(final String from_uuid, final String room_id) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MainActivity.this, from_uuid  + "退出房间:" + room_id, Toast.LENGTH_SHORT).show();
            }
        });
    }

    @Override
    public void onRoomBroadcast(final String from_uuid, final String room_id, final String data) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MainActivity.this, "群" +  room_id + "中" + from_uuid  + "广播了消息:" + data, Toast.LENGTH_SHORT).show();
            }
        });
    }

    /**
     * @brief 发送网速统计事件
     * @param dst_uuid 目标用户ID
     * @param bytesPerSec 发送网速，单位B/S
     */
    @Override
    public void onSendSpeed(String dst_uuid, long bytesPerSec){
        Log.d(APP_TAG,"onSendSpeed " + dst_uuid + ":" + bytesPerSec / 1024 + "KB/s");
    }

}
