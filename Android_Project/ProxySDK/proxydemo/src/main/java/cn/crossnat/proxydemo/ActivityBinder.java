package cn.crossnat.proxydemo;

import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import cn.crossnat.proxysdk.ProxySDK;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class ActivityBinder extends AppCompatActivity implements NoticeCenter.NoticeDelegate {

    private ListView listViewBinder;
    private List<HashMap<String,Object> > binderList = new ArrayList<HashMap<String,Object> >();
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_binder);
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setHomeButtonEnabled(true);
            actionBar.setDisplayHomeAsUpEnabled(true);
        }
        NoticeCenter.Instance().AddDelegate(MainActivity.NOTICE_SHUTDOWN, this);
        NoticeCenter.Instance().AddDelegate(ActivityAddBinder.NOTICE_ADDBINDER,this);
        listViewBinder = (ListView) findViewById(R.id.listViewBinder);
        listViewBinder.setAdapter(adapter);
        getLastNonConfigurationInstance();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.d(MainActivity.APP_TAG,"ActivityBinder.onConfigurationChanged " + newConfig.toString());
    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        NoticeCenter.Instance().RemoveDelegate(this);
        for (HashMap<String,Object> map:binderList) {
            ((ProxySDK.ProxyBinder)map.get("binder")).Release();
        }
    }

    @Override
    public void onRecvNotice(String noticeName, Object obj) {
        if (MainActivity.NOTICE_SHUTDOWN.equals(noticeName)){
            ActivityBinder.this.finish();
        }else if(ActivityAddBinder.NOTICE_ADDBINDER.equals(noticeName)){
            binderList.add((HashMap<String, Object>)obj);
            adapter.notifyDataSetChanged();
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                this.onBackPressed(); // back button
                return true;
            case R.id.add:
                showAddDialog();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void showAddDialog(){
        Intent intent=new Intent(this,ActivityAddBinder.class);
        this.startActivity(intent);
    }

    @Override
    public void onBackPressed() {
        Log.d(MainActivity.APP_TAG,"ActivityBinder.onBackPressed");

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("提示");
        builder.setMessage("确定退出登录？");
        builder.setNegativeButton("取消", null);
        builder.setPositiveButton("确定", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                ProxySDK.Logout();
                ActivityBinder.super.onBackPressed();
            }
        });
        builder.show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.binder, menu);
        return true;
    }

    private BaseAdapter adapter = new BaseAdapter() {
        @Override
        public int getCount() {
            return binderList.size();
        }

        @Override
        public Object getItem(int position) {
            return binderList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            LayoutInflater inflater = getLayoutInflater();
            final View view;
            final TextView text;

            if (convertView == null) {
                view = inflater.inflate(R.layout.binder_cell, parent, false);
            } else {
                view = convertView;
            }
            final HashMap<String,Object> map = binderList.get(position);
            ((TextView)view.findViewById(R.id.lb_title)).setText((String) map.get("dstuser"));
            ((TextView)view.findViewById(R.id.lb_detail)).setText("目标端口:" + map.get("dstport") + "  本地端口:" + map.get("localport"));
            Button bt_del = view.findViewById(R.id.bt_del);
            bt_del.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    ((ProxySDK.ProxyBinder)map.get("binder")).Release();
                    binderList.remove(map);
                    adapter.notifyDataSetChanged();
                }
            });

            return view;
        }
    };


}
