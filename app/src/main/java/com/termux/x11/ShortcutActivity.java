package com.termux.x11;

import static com.termux.x11.data.Constants.BASEURL;
import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;
import static com.termux.x11.data.Constants.URL_STARTAPP_X;
import static com.termux.x11.utils.Util.showXserverCloseOnDisconnect;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.termux.x11.data.AppListResult;
import com.termux.x11.data.VncResult;
import com.termux.x11.utils.FLog;
import com.xwdz.http.QuietOkHttp;
import com.xwdz.http.callback.JsonCallBack;

import okhttp3.Call;

public class ShortcutActivity extends AppCompatActivity {

    private static final String TAG = "ShortcutActivity";
    private String shortcutApp, shortcuPath;
    private AppListResult.DataBeanX.DataBean shortcutAppBean;
    private boolean fromShortcut;
    private final ServiceConnection connection = new Connection();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(new View(this));
        if(getIntent() != null && getIntent().getExtras() != null){
            shortcutApp = (String)getIntent().getExtras().get("App");
            shortcuPath = (String)getIntent().getExtras().get("Path");
            FLog.l(TAG, "onCreate() called with: shortcutApp = [" + shortcuPath + "]  shortcutApp = [" + shortcuPath + "]");
            fromShortcut = !TextUtils.isEmpty(shortcuPath) && !TextUtils.isEmpty(shortcutApp);
            shortcutAppBean = new AppListResult.DataBeanX.DataBean(shortcutApp, shortcuPath);
        }
        if(fromShortcut){
            bindService(new Intent(this, XWindowService.class), connection, Context.BIND_AUTO_CREATE);
        } else {
            finish();
        }
    }

    public class Connection implements ServiceConnection {

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            FLog.l(TAG,  "onServiceConnected");
            startLinuxApp(shortcutAppBean, true);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            finish();
        }
    }

    private void startLinuxApp(AppListResult.DataBeanX.DataBean app, boolean finish) {
        FLog.l(TAG, "startLinuxApp: app:" + app + ", finish:" + finish + "");
        QuietOkHttp.post(BASEURL + URL_STARTAPP_X)
                .setCallbackToMainUIThread(true)
                .addParams("App", app.Name)
                .addParams("Path", app.Path)
                .addParams("Display", DISPLAY_GLOBAL_PARAM)
                .execute(new JsonCallBack<VncResult.GetPortResult>() {
                    @Override
                    public void onFailure(Call call, Exception e) {
                    }

                    @Override
                    public void onSuccess(Call call, VncResult.GetPortResult response) {
                        if(finish){
                            finish();
                        }
                    }
                });
    }

}
