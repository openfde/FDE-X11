package com.fde.x11.activity;

import android.content.res.Configuration;

import androidx.annotation.NonNull;

import com.fde.x11.MainActivity;
import com.fde.x11.utils.FLog;

public class HolderActivity extends MainActivity{


    @Override
    public void onResume() {
        super.onResume();
    }

    @Override
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
    }

    public static class DecorHolderActivity extends HolderActivity{
    }

    public static class NoDecorHolderActivity extends HolderActivity{
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
