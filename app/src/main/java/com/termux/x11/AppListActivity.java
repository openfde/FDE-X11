package com.termux.x11;

import static android.view.WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
import static com.termux.x11.LoriePreferences.ACTION_PREFERENCES_CHANGED;
import static com.termux.x11.MainActivity.ACTION_STOP;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_ATTRIBUTE;
import static com.termux.x11.XWindowService.ACTION_X_WINDOW_PROPERTY;
import static com.termux.x11.XWindowService.DESTROY_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.MODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.START_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.STOP_WINDOW_FROM_X;
import static com.termux.x11.XWindowService.UNMODALED_ACTION_ACTIVITY_FROM_X;
import static com.termux.x11.XWindowService.X_WINDOW_ATTRIBUTE;
import static com.termux.x11.Xserver.ACTION_START;
import static com.termux.x11.Xserver.ACTION_UPDATE_ICON;
import static com.termux.x11.data.Constants.BASEURL;
import static com.termux.x11.data.Constants.DISPLAY_GLOBAL_PARAM;
import static com.termux.x11.data.Constants.URL_GETALLAPP;
import static com.termux.x11.data.Constants.URL_STARTAPP_X;
import static com.termux.x11.utils.Util.showXserverConnectSuccess;
import static com.termux.x11.utils.Util.showXserverDisconnect;
import static com.termux.x11.utils.Util.showXserverReconnect;
import static com.termux.x11.utils.Util.showXserverStartSuccess;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
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
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.TranslateAnimation;
import android.widget.ProgressBar;
import android.widget.Toast;

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
import java.util.Random;
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
    //todo mock addr
    public static boolean MOCK_ADDR = false;
    private String shortcutApp;
    private String shortcuPath;
    private boolean fromShortcut;
    private boolean reentry;
    private int globalWidth;
    private int globalHeight;
    private int screenWidth;
    private int screenHeight;
    private int spanCount;
    private boolean mAppListInit = false;
    public TipLoadDialog tipLoadDialog;
    private ServiceConnection connection;
    private boolean isLoading;

    public IBinder service;

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
        initAppList();
//        WindowMetrics maximumWindowMetrics = getWindow().getWindowManager().getMaximumWindowMetrics();
//        Rect bounds = null;
//        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
//            bounds = maximumWindowMetrics.getBounds();
//        }
//        Log.d(TAG, "onCreate: bounds:" + bounds + "");
//        AppUtils.GLOBAL_SCREEN_WIDTH = bounds.right;
//        AppUtils.GLOBAL_SCREEN_HEIGHT = bounds.bottom;
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
    }


    @Override
    protected void onResume() {
        super.onResume();
        if (MOCK_ADDR) {
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    tryLunchApp(null);
                }
            }, 3000);
        }
        mayGetApps();
        Log.d(TAG, "onResume");
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
//            if (isRight) {
//                showOptionView(itemView, app, event);
//            } else {
            load2Start(app);
//            }
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
                load2Start(app);
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

        fromY = globalHeight - event.getY() > DimenUtils.dpToPx(240.0f)? -1 : 1;
        gravity = globalHeight - event.getY() > DimenUtils.dpToPx(240.0f)? Gravity.BOTTOM : Gravity.TOP;
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
        animation.setDuration(200);
        animation.setInterpolator(new DecelerateInterpolator());
        return animation;
    }


    public class GetAppsCallback extends JsonCallBack<AppListResult>{

        private boolean forceRefresh;
        private int page;
        public GetAppsCallback(boolean forceRefresh, int page){
            this.forceRefresh = forceRefresh;
            this.page = page;
        }

        @Override
        public void onFailure(Call call, Exception e) {
            mRecyclerView.loadMoreFinish(false, false);
            mRefreshLayout.setRefreshing(false);
        }

        @Override
        public void onSuccess(Call call, AppListResult response) {
            Log.d(TAG, "onSuccess() called with: call = [" + call + "], response = [" + response + "]");
            List<AppListResult.DataBeanX.DataBean> data = response.getData().getData();
            if (fromShortcut) {
                gotoShortcutApp(data);
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

            if(forceRefresh){
                mRecyclerView.scrollToPosition(0);
            }
        }
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
                        mAppListInit = mDataList.size() != 0;
                    }

                    @Override
                    public void onSuccess(Call call, AppListResult response) {
                        if(tipLoadDialog != null){
                            tipLoadDialog.dismiss();
                        }
                        isLoading = false;
//                        Log.d(TAG, "onSuccess() called with: call = [" + call + "], response = [" + response + "]");
                        List<AppListResult.DataBeanX.DataBean> data = response.getData().getData();
                        if (fromShortcut) {
                            gotoShortcutApp(data);
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
//
//                        if(forceRefresh){
//                            mRecyclerView.scrollToPosition(0);
//                        }
//                        mAppListInit = mDataList.size() != 0;
                    }
                });
    }

    private void createShortcut(AppListResult.DataBeanX.DataBean app) {
        Log.d(TAG, "createShortcut() called with: app = [" + app + "]");
        byte[] decode = Base64.decode(app.getIcon(), Base64.DEFAULT);
//        Bitmap bitmap = BitmapFactory.decodeByteArray(decode, 0, decode.length);
        Icon icon = Icon.createWithBitmap(AppUtils.getScaledBitmap(decode, this));
        ShortcutManager shortcutManager = getSystemService(ShortcutManager.class);
        if (shortcutManager != null && shortcutManager.isRequestPinShortcutSupported()) {
//            Intent launchIntentForPackage = getPackageManager().getLaunchIntentForPackage(getPackageName());
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

    private void gotoShortcutApp(List<AppListResult.DataBeanX.DataBean> data) {
        for (AppListResult.DataBeanX.DataBean bean : data){
            Log.d(TAG, "gotoShortcutApp() called with: bean = [" + bean.getName() + "]");
            if (TextUtils.equals(shortcutApp, bean.getName())){
                load2Start(bean);
            }
        }
    }

    private void load2Start(AppListResult.DataBeanX.DataBean app) {
        if (service == null || !service.isBinderAlive()) {
            showXserverReconnect(this);
            startXWindowService();
        } else {
            tipLoadDialog.setBackground(R.drawable.custom_dialog_bg_corner)
                    .setNoShadowTheme()
                    .setMsgAndType(getString(R.string.lunching_tip) + app.getName(), TipLoadDialog.ICON_TYPE_LOADING)
                    .setTipTime(5000)
                    .show();
            tryStartVncApp(app);
        }
    }


    private void tryStartVncApp(AppListResult.DataBeanX.DataBean app) {
        // todo mock
        QuietOkHttp.post(BASEURL + URL_STARTAPP_X)
                .setCallbackToMainUIThread(true)
                .addParams("App", app.Name)
                .addParams("Path", app.Path)
                .addParams("Display", DISPLAY_GLOBAL_PARAM)
                .execute(new JsonCallBack<VncResult.GetPortResult>() {
                    @Override
                    public void onFailure(Call call, Exception e) {
                        Log.d(TAG, "onFailure() called with: call = [" + call + "], e = [" + e + "]");
                        tipLoadDialog.dismiss();
                    }

                    @Override
                    public void onSuccess(Call call, VncResult.GetPortResult response) {
                        tipLoadDialog.dismiss();
                        Log.i(TAG, "onSuccess() called with: call = [" + call + "], response = [" + response + "]");
//                        tryLunchApp(app);
                    }
                });
    }

    private void tryLunchApp(AppListResult.DataBeanX.DataBean app) {
        loadingView.setVisibility(View.GONE);
        tipLoadDialog.dismiss();
    }

}
