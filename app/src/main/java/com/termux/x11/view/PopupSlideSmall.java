package com.termux.x11.view;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import com.termux.x11.R;
import com.termux.x11.databinding.PopupSlideSmallBinding;

import razerdp.basepopup.BasePopupWindow;

public class PopupSlideSmall extends BasePopupWindow {
    PopupSlideSmallBinding mBinding;
    private onAppOptionItemClickListener listener;

    public PopupSlideSmall(Context context) {
        super(context);
        setContentView(R.layout.popup_slide_small);
        setViewClickListener(this::click, mBinding.tvOpen, mBinding.tvRefresh, mBinding.tvShortcut, mBinding.tvInfo, mBinding.tvConpatible);
    }

    @Override
    public void onViewCreated(View contentView) {
        mBinding = PopupSlideSmallBinding.bind(contentView);
    }


    void click(View v) {
        this.dismiss();
        if(v == mBinding.tvOpen ){
            listener.onOptionOpenClick();
        }
        if(v == mBinding.tvRefresh ){
            listener.onOptionRefreshClick();
        }
        if(v == mBinding.tvShortcut ){
            listener.onOptionShortcutClick();
        }
        if(v == mBinding.tvInfo ){
            listener.onOptionInfoClick();
        }
        if(v == mBinding.tvConpatible){
            listener.onOptionCompatibleClick();
        }

    }

    public void setOptionItemClickListener(onAppOptionItemClickListener listener){
        this.listener = listener;
    }

    public interface onAppOptionItemClickListener {

        void onOptionOpenClick();
        void onOptionRefreshClick();
        void onOptionShortcutClick();
        void onOptionInfoClick();
        void onOptionCompatibleClick();

    }
}
