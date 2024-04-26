package com.fde.fusionwindowmanager;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class WindowAttribute implements Parcelable {
    
    float offsetX;
    float offsetY;
    float width;
    float height;
    int index;
    long windowPtr;

    long XID;

    public long getXID() {
        return XID;
    }

    public void setXID(long XID) {
        this.XID = XID;
    }

    public float getOffsetX() {
        return offsetX;
    }

    public void setOffsetX(float offsetX) {
        this.offsetX = offsetX;
    }

    public float getOffsetY() {
        return offsetY;
    }

    public void setOffsetY(float offsetY) {
        this.offsetY = offsetY;
    }

    public float getWidth() {
        return width;
    }

    public void setWidth(float width) {
        this.width = width;
    }

    public float getHeight() {
        return height;
    }

    public void setHeight(float height) {
        this.height = height;
    }

    public int getIndex() {
        return index;
    }

    public void setIndex(int index) {
        this.index = index;
    }

    public long getWindowPtr() {
        return windowPtr;
    }

    public void setWindowPtr(long windowPtr) {
        this.windowPtr = windowPtr;
    }

    public WindowAttribute(int offsetX, int offsetY, int width, int height, int index, long windowPtr, long window){
        this.offsetX = offsetX;
        this.offsetY = offsetY;
        this.width = width;
        this.height = height;
        this.index = index;
        this.windowPtr = windowPtr;
        this.XID = window;
    }


    protected WindowAttribute(Parcel in) {
        offsetX = in.readFloat();
        offsetY = in.readFloat();
        width = in.readFloat();
        height = in.readFloat();
        index = in.readInt();
        windowPtr = in.readLong();
        XID = in.readLong();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
            dest.writeFloat(offsetX);
            dest.writeFloat(offsetY);
            dest.writeFloat(width);
            dest.writeFloat(height);
            dest.writeInt(index);
            dest.writeLong(windowPtr);
            dest.writeLong(XID);
    }

    public static final Creator<WindowAttribute> CREATOR = new Creator<WindowAttribute>() {
        @Override
        public WindowAttribute createFromParcel(Parcel in) {
            return new WindowAttribute(in);
        }

        @Override
        public WindowAttribute[] newArray(int size) {
            return new WindowAttribute[size];
        }
    };

    @Override
    public String toString() {
        return "Coordinate{" +
                "offsetX=" + offsetX +
                ", offsetY=" + offsetY +
                ", width=" + width +
                ", height=" + height +
                ", index=" + index +
                ", windowPtr=" + Long.toHexString(windowPtr) +
                ", XID=" + Long.toHexString(XID) +
                '}';
    }
}
