package com.fde.x11.activity;

import static com.fde.x11.data.Constants.APP_TITLE_PREFIX;

import android.app.ActivityManager;
import android.content.res.Configuration;
import android.text.TextUtils;
import android.util.Log;
import android.view.Choreographer;
import android.view.View;

import androidx.annotation.NonNull;

import com.fde.fusionwindowmanager.Property;
import com.fde.fusionwindowmanager.WindowAttribute;
import com.fde.x11.LorieView;
import com.fde.x11.MainActivity;
import com.fde.x11.R;
import com.fde.x11.XserviceInterfaceWrapper;
import com.fde.x11.utils.FLog;

public class MainActivityHolderActivity extends MainActivity{

    @Override
    protected void refillActivity(WindowAttribute attr, Property prop) {
        FLog.s(TAG, "refillActivity() called with: attr = [" + attr + "], prop = [" + prop + "]");
        mAttribute = attr;
        if (mAttribute != null) {
            mIndex = mAttribute.getIndex();
            WindowCode = mAttribute.getXID();
            mWindowRect.set(mAttribute.getRect());
            mAttribute.setCaptionHeight(mDecorCaptionViewHeight);
            mAttribute.setTaskId(getTaskId());
        }
        mProperty = prop;
        if (mProperty != null) {
            String wmClass = mProperty.getWm_class();
            String netName = mProperty.getNet_name();
            FLog.a("lifecycle", getWindowId(), "wmclass:" + wmClass + " netName:" + netName);
            this.title = TextUtils.isEmpty(netName) ? (TextUtils.isEmpty(wmClass) ? APP_TITLE_PREFIX : APP_TITLE_PREFIX + ": " + wmClass) : netName;
            if (mProperty.getIcon() != null) {
                ActivityManager.TaskDescription description = new ActivityManager.TaskDescription(title, mProperty.getIcon(), 0);
                setTaskDescription(description);
            }
            if (FLog.SHOW_DEBUG_TITLE) {
                String windowid = Long.toHexString(getWindowId());
                handler.post(()->{
                    setTitle(title + " id:0x" + windowid);
                });
            } else {
                handler.post(()->{
                    setTitle(title);
                });
            }
            FLog.a("lifecycle", getWindowId(), title);
        }
        mXserviceWrapper.mAttribute = mAttribute;
        mXserviceWrapper.updateCoordinate(mAttribute);
        LorieView lorieView = getLorieView();
        lorieView.setZOrderOnTop(false);
        lorieView.updateCoordinate(mAttribute);
        lorieView.setTag(R.id.WINDOW_ARRTRIBUTE, mAttribute);
        reigsterActivityCallback();
    }

    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected void onStopImpl() {
        if( mAttribute != null){
            super.onStopImpl();
        }
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Choreographer.getInstance().postFrameCallback(new Choreographer.FrameCallback() {
            @Override
            public void doFrame(long frameTimeNs) {
                Log.e(TAG, "doFrame() called with: frameTimeNs = [" + frameTimeNs + "]");
                Choreographer.getInstance().removeFrameCallback(this);
            }
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
    }

    public static class DecorHolderActivity extends MainActivityHolderActivity{
    }

    public static class NoDecorHolderActivity extends MainActivityHolderActivity{
        protected boolean hideDecorCaptionView() {
            if (FLog.SHOW_DEBUG_TITLE) {
                return false;
            }
            FLog.a("TAG", "hideDecorCaptionView");
            if (mFrameworkOperations != null) {
                mFrameworkOperations.hideDecorCaptionView(this);
                captionShowing = false;
            }
            return true;
        }
    }

}
