package com.termux.x11;

import static com.termux.x11.LoriePreferences.ACTION_PREFERENCES_CHANGED;
import static com.termux.x11.MainActivity.ACTION_STOP;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_ATTRIBUTE;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_PROPERTY;
import static com.termux.x11.XWindowService.DESTROY_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.MODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.START_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.STOP_WINDOW_FROM_X;
import static com.termux.x11.XWindowService.UNMODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.Xserver.ACTION_START;
import static com.termux.x11.Xserver.ACTION_UPDATE_ICON;
import static com.termux.x11.data.Constants.BASEURL;
import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;
import static com.termux.x11.data.Constants.URL_GETALLAPP;
import static com.termux.x11.data.Constants.URL_STARTAPP_X;
import static com.termux.x11.utils.Util.showXserverDisconnect;
import static com.termux.x11.utils.Util.showXserverReconnect;
import static com.termux.x11.utils.Util.showXserverStartSuccess;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.content.res.Configuration;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.TranslateAnimation;
import android.widget.EditText;
import android.widget.ProgressBar;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.Util;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.recyclerview.SwipeRecyclerView;
import com.termux.x11.data.AppAdapter;
import com.termux.x11.data.AppListResult;
import com.termux.x11.data.VncResult;
import com.termux.x11.utils.AppUtils;
import com.termux.x11.utils.DimenUtils;
import com.termux.x11.view.PopupSlideSmall;
import com.xiaokun.dialogtiplib.dialog_tip.TipLoadDialog;
import com.xwdz.http.QuietOkHttp;
import com.xwdz.http.callback.JsonCallBack;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import okhttp3.Call;
import razerdp.basepopup.BasePopupWindow;
import razerdp.util.animation.AnimationHelper;
import razerdp.util.animation.ScaleConfig;

public class AppListActivity extends AppCompatActivity {

    private static final String TAG = "AppListActivity";
    private ProgressBar loadingView;
    private SwipeRecyclerView mRecyclerView;
    private SwipeRefreshLayout mRefreshLayout;
    private AppAdapter mAdapter;
    private List<AppListResult.DataBeanX.DataBean> mDataList = new ArrayList<>();
    private int mPage = 1;
    private int pageSize = 100;
    private String shortcutApp;
    private String shortcuPath;
    private boolean fromShortcut;
    private boolean reentry;
    private int globalWidth;
    private int globalHeight;
    private int screenWidth;
    private int screenHeight;
    private int spanCount;
    public TipLoadDialog tipLoadDialog;
    private ServiceConnection connection;
    private boolean isLoading;

    public IBinder service;
    private EditText filterView;

    private Handler handler = new Handler();

    public interface ItemClickListener {
        void onItemClick(View itemView, int position, AppListResult.DataBeanX.DataBean app, boolean isRight, MotionEvent event);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.applist_activity);
        com.xiaokun.dialogtiplib.util.AppUtils.init(this);
        loadingView = (ProgressBar) findViewById(R.id.loadingView);
        tipLoadDialog = new TipLoadDialog(this);
        filterView = (EditText)findViewById(R.id.et_appname);
        initAppList();
        Util.copyAssetsToFilesIfNedd(this, "xkb", "xkb");
        startXWindowService();
        registerReceiver(receiver, new IntentFilter(ACTION_START) {{
            addAction(ACTION_PREFERENCES_CHANGED);
            addAction(ACTION_STOP);
            addAction(DESTROY_ACTIVITY_FROM_X);
            addAction(START_ACTIVITY_FROM_X);
            addAction(STOP_WINDOW_FROM_X);
            addAction(MODALED_ACTION_ACTIVITY_FROM_X);
            addAction(UNMODALED_ACTION_ACTIVITY_FROM_X);
            addAction(ACTION_UPDATE_ICON);
        }},  0);
        Log.d(TAG, "onCreate: savedInstanceState:" + savedInstanceState + "");
        filterView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                Log.d(TAG, "onTextChanged: s:" + s + ", start:" + start + ", before:" + before + ", count:" + count + "");
            }

            @Override
            public void afterTextChanged(Editable s) {
                Log.d(TAG, "afterTextChanged: s:" + s + "");
                handler.removeCallbacks(null);
                handler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        if(mAdapter != null){
                            mAdapter.filterAppName(s.toString());
                        }
                    }
                }, 100);
            }
        });

        if(getIntent() != null && getIntent().getExtras() != null){
            shortcutApp = (String)getIntent().getExtras().get("App");
            shortcuPath = (String)getIntent().getExtras().get("Path");
            Log.d(TAG, "onCreate() called with: shortcutApp = [" + shortcuPath + "]  shortcutApp = [" + shortcuPath + "]");
            fromShortcut = !TextUtils.isEmpty(shortcuPath) && !TextUtils.isEmpty(shortcutApp);
        }
    }

    private final BroadcastReceiver receiver = new BroadcastReceiver() {
        @SuppressLint("UnspecifiedRegisterReceiverFlag")
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_START.equals(intent.getAction())) {
            } else if (ACTION_STOP.equals(intent.getAction())) {
            } else if (ACTION_PREFERENCES_CHANGED.equals(intent.getAction())) {
            } else if (DESTROY_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
            } else if (START_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
            } else if(STOP_WINDOW_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                App.getApp().stopingActivityWindow.add(attr.getXID());
            } else if(MODALED_ACTION_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
                Property property = intent.getParcelableExtra(ACTION_X_WINDOW_PROPERTY);
            } else if(UNMODALED_ACTION_ACTIVITY_FROM_X.equals(intent.getAction())){
                WindowAttribute attr = intent.getParcelableExtra(ACTION_X_WINDOW_ATTRIBUTE);
            } else if(ACTION_UPDATE_ICON.equals(intent.getAction())){
                long windowId = intent.getLongExtra("window_id", 0);
            }
        }
    };


    private void startXWindowService() {
        if(connection == null){
            connection = new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    AppListActivity.this.service = service;
                    try {
                        Objects.requireNonNull(AppListActivity.this.service).linkToDeath(() -> {
                            AppListActivity.this.service = null;
                            Log.v(TAG, "Disconnected");
//                            showXserverDisconnect(AppListActivity.this);
                        }, 0);
                    } catch (RemoteException e) {
                        e.printStackTrace();
                    }
                    showXserverStartSuccess(AppListActivity.this);
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    AppListActivity.this.service = null;
                    showXserverDisconnect(AppListActivity.this);
                }
            };
        }
        bindService(new Intent(this, XWindowService.class), connection, BIND_AUTO_CREATE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if( connection != null ){
            unbindService(connection);
        }
        if(tipLoadDialog != null){
            tipLoadDialog = null;
        }
        unregisterReceiver(receiver);
        handler.removeCallbacks(null);
    }


    @Override
    protected void onResume() {
        super.onResume();
        mayGetApps();
        Log.d(TAG, "onResume");
        findViewById(R.id.et_appname).requestFocus();
    }

    private void mayGetApps() {
        if(screenHeight == 0 | screenWidth == 0){
            screenWidth = DimenUtils.getScreenWidth();
            screenHeight = DimenUtils.getScreenHeight();
        } else if( screenWidth != globalWidth){
            screenWidth = globalWidth;
            screenHeight = globalHeight;
            Log.d(TAG, "mayGetApps");
            int count = Math.max(globalWidth / (int) DimenUtils.dpToPx(160.0f), 3);
            if(spanCount != count){
                initAppList();
                spanCount = count;
            }
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        checkConfig(newConfig);
        Log.d(TAG, "onConfigurationChanged: newConfig:" + newConfig + "");
    }

    private void checkConfig(Configuration configuration){
        if(configuration == null){
            return;
        }
        Pattern pattern = Pattern.compile("mBounds=Rect\\((-?\\d+), (-?\\d+) - (-?\\d+), (-?\\d+)\\)");
        Matcher matcher = pattern.matcher(configuration.toString());
        if(matcher.find()) {
            int left = Integer.parseInt(Objects.requireNonNull(matcher.group(1)));
            int top = Integer.parseInt(Objects.requireNonNull(matcher.group(2)));
            int right = Integer.parseInt(Objects.requireNonNull(matcher.group(3)));
            int bottom = Integer.parseInt(Objects.requireNonNull(matcher.group(4)));
            globalWidth = right - left;
            globalHeight = bottom - top;
        }
        mayGetApps();
    }

    private void initAppList() {
        isLoading = true;
        if(globalWidth != 0 && globalHeight != 0){
            spanCount = globalWidth / (int) DimenUtils.dpToPx(160.0f);
        } else {
            spanCount = DimenUtils.getScreenWidth() / (int) DimenUtils.dpToPx(160.0f);
        }
        spanCount = Math.max(spanCount, 3);
        mRefreshLayout = findViewById(R.id.refresh_layout);
        mRefreshLayout.setOnRefreshListener(mRefreshListener); //
        mRecyclerView = findViewById(R.id.recycler_view);
        mRecyclerView.setLayoutManager(new GridLayoutManager(this, spanCount));
        mRecyclerView.useDefaultLoadMore(); //
        mRecyclerView.setLoadMoreListener(mLoadMoreListener); //
        mRecyclerView.setAutoLoadMore(true);
        mAdapter = new AppAdapter(this, mDataList, mItemClickListener);
        mRecyclerView.setAdapter(mAdapter);
        getAllLinuxApp(true, 1);
        mRefreshLayout.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                globalWidth = mRefreshLayout.getMeasuredWidth();
                globalHeight = mRefreshLayout.getMeasuredHeight();
            }
        });
    }


    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        AppUtils.set("fde.click_as_touch", "false");
        Log.d(TAG, "onWindowFocusChanged: hasFocus:" + hasFocus + "");
        if(hasFocus){
            mayGetApps();
        }
    }

    private SwipeRefreshLayout.OnRefreshListener mRefreshListener = new SwipeRefreshLayout.OnRefreshListener() {
        @Override
        public void onRefresh() {
            getAllLinuxApp(true, 1);
        }
    };

    private SwipeRecyclerView.LoadMoreListener mLoadMoreListener = new SwipeRecyclerView.LoadMoreListener() {
        @Override
        public void onLoadMore() {
            getAllLinuxApp(false, mPage + 1);
        }
    };

    private long mLastClickTime = 0;
    public static final long TIME_INTERVAL = 300L;

    ItemClickListener mItemClickListener = new ItemClickListener() {
        @Override
        public void onItemClick(View itemView, int position, AppListResult.DataBeanX.DataBean app, boolean isRight, MotionEvent event) {
            long nowTime = System.currentTimeMillis();
            if (nowTime - mLastClickTime < TIME_INTERVAL) {
                // do something
                Log.d(TAG, "onItemClick() click too quickly");
                mLastClickTime = nowTime;
                return;
            }
            mLastClickTime = nowTime;
            Log.d(TAG, "onItemClick() called with: itemView = [" + itemView + "], position = [" + position + "], app = [" + app + "], isRight = [" + isRight + "]");
            if (isRight) {
                showOptionView(itemView, app, event);
            } else {
                load2Start(app, false);
            }
        }
    };

    private void showOptionView(View v, AppListResult.DataBeanX.DataBean app, MotionEvent event) {
        PopupSlideSmall mPopupSlideSmall;
        BasePopupWindow popupWindow;
        int gravity = Gravity.TOP;
        boolean blur =false;
        BasePopupWindow.GravityMode horizontalGravityMode = BasePopupWindow.GravityMode.RELATIVE_TO_ANCHOR;
        BasePopupWindow.GravityMode verticalGravityMode = BasePopupWindow.GravityMode.RELATIVE_TO_ANCHOR;
        float fromX = 0;
        float fromY = 0;
        float toX = 0;
        float toY = 0;
        Animation showAnimation = AnimationHelper.asAnimation()
                .withScale(ScaleConfig.CENTER)
                .toShow();
        Animation dismissAnimation = AnimationHelper.asAnimation()
                .withScale(ScaleConfig.CENTER)
                .toDismiss();
        mPopupSlideSmall = new PopupSlideSmall(v.getContext());
        mPopupSlideSmall.setOptionItemClickListener(new PopupSlideSmall.onAppOptionItemClickListener() {
            @Override
            public void onOptionOpenClick() {
                load2Start(app, false);
            }

            @Override
            public void onOptionRefreshClick() {
                mRefreshLayout.setRefreshing(true);
                getAllLinuxApp(true, 1);
            }

            @Override
            public void onOptionShortcutClick() {
                createShortcut(app);
            }

            @Override
            public void onOptionInfoClick() {

            }

            @Override
            public void onOptionCompatibleClick() {
                String showAppName = getRealAppName(app);
                Intent intent = new Intent();
                ComponentName cn = ComponentName.unflattenFromString("com.android.settings/.Settings$SetCompatibleActivity");
                intent.setComponent(cn);
                intent.putExtra("appName", "VNC_"+showAppName);
                intent.putExtra("packageName", showAppName);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
            }

        });

        popupWindow = mPopupSlideSmall;
        boolean withAnchor = true;
        switch (gravity & Gravity.HORIZONTAL_GRAVITY_MASK) {
            case Gravity.LEFT:
                fromX = withAnchor ? 1f : -1f;
                break;
            case Gravity.RIGHT:
                fromX = withAnchor ? -1f : 1f;
                break;
        }

        fromY = globalHeight - event.getY() > DimenUtils.dpToPx(250.0f)? -1 : 1;
        gravity = globalHeight - event.getY() > DimenUtils.dpToPx(250.0f)? Gravity.BOTTOM : Gravity.TOP;
        if (fromX != 0 || fromY != 0) {
            showAnimation = createTranslateAnimation(fromX, toX, fromY, toY);
            dismissAnimation = createTranslateAnimation(toX, fromX, toY, fromY);
        }
        popupWindow.setBlurBackgroundEnable(blur);
        popupWindow.setBackground(null);
        popupWindow.setPopupGravityMode(horizontalGravityMode, verticalGravityMode);
        popupWindow.setPopupGravity(gravity);
        popupWindow.setShowAnimation(showAnimation);
        popupWindow.setDismissAnimation(dismissAnimation);
        if (withAnchor) {
            popupWindow.showPopupWindow(v);
        } else {
            popupWindow.showPopupWindow();
        }
    }

    private String getRealAppName(AppListResult.DataBeanX.DataBean app){
        String showAppName = app.getName();
        if(app.getName().contains("~")){
            String[] arrName = app.getName().split("~");
            showAppName = arrName[0]  ;
        }
        return  showAppName;
    }

    private Animation createTranslateAnimation(float fromX, float toX, float fromY, float toY) {
        Animation animation = new TranslateAnimation(Animation.RELATIVE_TO_SELF,
                fromX,
                Animation.RELATIVE_TO_SELF,
                toX,
                Animation.RELATIVE_TO_SELF,
                fromY,
                Animation.RELATIVE_TO_SELF,
                toY);
        animation.setDuration(100);
        animation.setInterpolator(new DecelerateInterpolator());
        return animation;
    }


    private void getAllLinuxApp(boolean forceRefresh, int page) {
        QuietOkHttp.get(BASEURL + URL_GETALLAPP)
                .addParams("page", new Integer(page).toString())
                .addParams("page_size", new Integer(pageSize).toString())
                .addParams("refresh", String.valueOf(forceRefresh))
                .addParams("page_enable", "true")
                .setCallbackToMainUIThread(true)
                .execute(new JsonCallBack<AppListResult>() {
                    @Override
                    public void onFailure(Call call, Exception e) {
                        isLoading = false;
                        Log.d(TAG, "onFailure() called with: call = [" + call + "], e = [" + e + "]");
                        mRecyclerView.loadMoreFinish(false, false);
                        mRefreshLayout.setRefreshing(false);
                    }

                    @Override
                    public void onSuccess(Call call, AppListResult response) {
                        if(tipLoadDialog != null){
                            tipLoadDialog.dismiss();
                        }
                        isLoading = false;
                        Log.d(TAG, "onSuccess() called with: call = [" + call + "], response = [" + response + "]");
                        List<AppListResult.DataBeanX.DataBean> data = response.getData().getData();
                        if (fromShortcut) {
                            fromShortcut = false;
                            gotoShortcutApp(data, true);
                            return;
                        }
                        if (forceRefresh) {
                            mDataList.clear();
                        }
                        mDataList.addAll(data);
                        mAdapter.notifyDataSetChanged();
                        if (data.size() > 0) {
                            AppListActivity.this.mPage = page;
                        }
                        mRecyclerView.loadMoreFinish(mDataList.size() == 0, response.getData().getPage().getTotal() > mDataList.size());
                        mRefreshLayout.setRefreshing(false);
                    }
                });
    }

    private void createShortcut(AppListResult.DataBeanX.DataBean app) {
        byte[] decode = Base64.decode(app.getIcon(), Base64.DEFAULT);
        Icon icon = Icon.createWithBitmap(AppUtils.getScaledBitmap(decode, this));
        ShortcutManager shortcutManager = getSystemService(ShortcutManager.class);
        if (shortcutManager != null && shortcutManager.isRequestPinShortcutSupported()) {
            Intent launchIntentForPackage = new Intent(this, AppListActivity.class);
            launchIntentForPackage.setAction(Intent.ACTION_MAIN);
            launchIntentForPackage.putExtra("App", app.getName());
            launchIntentForPackage.putExtra("Path", app.getName());
            ShortcutInfo pinShortcutInfo = new ShortcutInfo.Builder(this, app.getName())
                    .setShortLabel(app.getName())
                    .setLongLabel(app.getName())
                    .setIcon(icon)
                    .setIntent(launchIntentForPackage)
                    .build();
            Intent pinnedShortcutCallbackIntent = shortcutManager.createShortcutResultIntent(pinShortcutInfo);
            PendingIntent successCallback = PendingIntent.getBroadcast(this, 0,
                    pinnedShortcutCallbackIntent, PendingIntent.FLAG_IMMUTABLE);
            shortcutManager.requestPinShortcut(pinShortcutInfo, successCallback.getIntentSender());
        }
    }

    private void gotoShortcutApp(List<AppListResult.DataBeanX.DataBean> data, boolean finish) {
        for (AppListResult.DataBeanX.DataBean bean : data){
            if (TextUtils.equals(shortcutApp, bean.getName())){
                load2Start(bean, finish);
            }
        }
    }

    private void load2Start(AppListResult.DataBeanX.DataBean app, boolean finish) {
//        Log.d(TAG, "load2Start: app:" + app + "");
        if (service == null || !service.isBinderAlive()) {
            showXserverReconnect(this);
            startXWindowService();
        } else {
            tipLoadDialog.setBackground(R.drawable.custom_dialog_bg_corner)
                    .setNoShadowTheme()
                    .setMsgAndType(getString(R.string.lunching_tip) + app.getName(), TipLoadDialog.ICON_TYPE_LOADING)
                    .setTipTime(5000)
                    .show();
            startLinuxApp(app, finish);
        }
    }


    private void startLinuxApp(AppListResult.DataBeanX.DataBean app, boolean finish) {
        QuietOkHttp.post(BASEURL + URL_STARTAPP_X)
                .setCallbackToMainUIThread(true)
                .addParams("App", app.Name)
                .addParams("Path", app.Path)
                .addParams("Display", DISPLAY_GLOBAL_PARAM)
                .execute(new JsonCallBack<VncResult.GetPortResult>() {
                    @Override
                    public void onFailure(Call call, Exception e) {
                        tipLoadDialog.dismiss();
                    }

                    @Override
                    public void onSuccess(Call call, VncResult.GetPortResult response) {
                        tipLoadDialog.dismiss();
                        if(finish){
                            moveTaskToBack(false);
                        }
                    }
                });
    }

}
