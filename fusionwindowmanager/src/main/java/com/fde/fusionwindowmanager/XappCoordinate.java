package com.fde.fusionwindowmanager;

import android.os.Parcel;

public class XappCoordinate extends Coordinate{
    public XappCoordinate(int offsetX, int offsetY, int width, int height, int index, long windowPtr) {
        super(offsetX, offsetY, width, height, index, windowPtr);
    }

    protected XappCoordinate(Parcel in) {
        super(in);
    }
}
