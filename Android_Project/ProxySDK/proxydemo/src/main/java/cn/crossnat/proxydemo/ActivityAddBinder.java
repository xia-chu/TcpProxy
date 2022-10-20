package cn.crossnat.proxydemo;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import cn.crossnat.proxysdk.ProxySDK;

import java.util.HashMap;

public class ActivityAddBinder extends Activity implements ProxySDK.ProxyBinderEvent {

    public static final String NOTICE_ADDBINDER = "ActivityAddBinder.addBinder";
    private ProxySDK.ProxyBinder binder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_add_binder);

        SharedPreferences userSettings= getSharedPreferences("setting", 0);
        ((TextView)findViewById(R.id.txt_dstuser)).setText(userSettings.getString("dstuser",""));
        ((TextView)findViewById(R.id.txt_dstport)).setText(userSettings.getString("dstport",""));
        ((TextView)findViewById(R.id.txt_localport)).setText(userSettings.getString("localport",""));
    }

    @Override
    public void onAttachedToWindow() {
        View view = getWindow().getDecorView();
        WindowManager.LayoutParams lp = (WindowManager.LayoutParams) view.getLayoutParams();
        lp.gravity = Gravity.FILL;
        lp.height = lp.width = WindowManager.LayoutParams.MATCH_PARENT;
        getWindowManager().updateViewLayout(view, lp);
    }

    public void onClick_cancel(View sender){
        this.finish();
    }

    public void onClick_yes(View sender){
        String dstuser =  ((TextView)findViewById(R.id.txt_dstuser)).getText().toString();
        String dstport =  ((TextView)findViewById(R.id.txt_dstport)).getText().toString();
        String localport =  ((TextView)findViewById(R.id.txt_localport)).getText().toString();

        if (dstuser.isEmpty() || dstport.isEmpty() || localport.isEmpty()){
            Toast.makeText(this,"请输入完整信息!",Toast.LENGTH_SHORT).show();
            return;
        }

        binder = new ProxySDK.ProxyBinder();
        binder.SetDelegate(this);
        if(-1 == binder.Bind(dstuser,Integer.parseInt(dstport),Integer.parseInt(localport),"127.0.0.1","0.0.0.0")){
            binder.Release();
            binder = null;
            Toast.makeText(this,"绑定端口失败!",Toast.LENGTH_SHORT).show();
            return;
        }
        setEnabled(null,false);
        ((Button)findViewById(R.id.bt_yes)).setText("绑定中...");
    }

    @Override
    public void onBindResult(final int errCode, final String errMsg) {
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setEnabled(null,true);
                ((Button)findViewById(R.id.bt_yes)).setText("确定");
                if (errCode != 0 ){
                    Toast.makeText(ActivityAddBinder.this,"绑定失败,错误码:" + errCode + "," + errMsg,Toast.LENGTH_SHORT).show();
                    binder.Release();
                    binder = null;
                    return;
                }
                //成功
                String dstuser =  ((TextView)findViewById(R.id.txt_dstuser)).getText().toString();
                String dstport =  ((TextView)findViewById(R.id.txt_dstport)).getText().toString();
                String localport =  ((TextView)findViewById(R.id.txt_localport)).getText().toString();
                SharedPreferences userSettings = getSharedPreferences("setting", 0);
                SharedPreferences.Editor editor = userSettings.edit();
                editor.putString("dstuser",dstuser);
                editor.putString("dstport",dstport);
                editor.putString("localport",localport);
                editor.commit();

                HashMap<String,Object> mapArgs = new HashMap<String, Object>();
                mapArgs.put("dstuser",dstuser);
                mapArgs.put("dstport",dstport);
                mapArgs.put("localport",localport);
                mapArgs.put("binder",binder);
                NoticeCenter.Instance().PostNotice(NOTICE_ADDBINDER,mapArgs);
                ActivityAddBinder.this.finish();
            }
        });

    }

    @Override
    public void onRemoteSockErr(int errCode, String errMsg) {
    }

    private void setEnabled(ViewGroup viewGroup,boolean flag){
        if(viewGroup == null){
            viewGroup = findViewById(R.id.id_bindDialog);
        }
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View v = viewGroup.getChildAt(i);
            if (v instanceof ViewGroup) {
                if (v instanceof Spinner) {
                    Spinner spinner = (Spinner) v;
                    spinner.setClickable(flag);
                    spinner.setEnabled(flag);
                } else if (v instanceof ListView) {
                    ((ListView) v).setClickable(flag);
                    ((ListView) v).setEnabled(flag);
                } else {
                    setEnabled((ViewGroup) v,flag);
                }
            } else if (v instanceof EditText) {
                ((EditText) v).setEnabled(flag);
                ((EditText) v).setClickable(flag);
            } else if (v instanceof Button) {
                ((Button) v).setEnabled(flag);
            }
        }
    }


}
