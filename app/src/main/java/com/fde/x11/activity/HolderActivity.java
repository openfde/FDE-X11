package com.fde.x11.activity;

import com.fde.x11.MainActivity;
import com.fde.x11.utils.FLog;

public class HolderActivity extends MainActivity{























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
