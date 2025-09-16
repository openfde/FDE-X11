package com.fde.fusionwindowmanager;

import android.os.Parcel;

public class XappWindowAttribute extends WindowAttribute {
    public XappWindowAttribute(int offsetX, int offsetY, int width, int height, int index, long windowPtr, long window) {
        super();
    }

    protected XappWindowAttribute(Parcel in) {
        super(in);
    }
}
