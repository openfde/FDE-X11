package com.fde.fusionwindowmanager;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class Property implements Parcelable {

    long XID;
    long transientfor;
    long leader;
    int type;
    String net_name;
    String wm_class;

    int supportDeleteWindow;

    public Property() {
    }

    public Property(long XID, long transientfor, long leader, int type, String net_name, String wm_class, int supportDeleteWindow) {
        this.XID = XID;
        this.transientfor = transientfor;
        this.leader = leader;
        this.type = type;
        this.net_name = net_name;
        this.wm_class = wm_class;
        this.supportDeleteWindow = supportDeleteWindow;
    }

    public Property(long XID, long transientfor, long leader, int type, String net_name, String wm_class) {
        this.XID = XID;
        this.transientfor = transientfor;
        this.leader = leader;
        this.type = type;
        this.net_name = net_name;
        this.wm_class = wm_class;
    }

    protected Property(Parcel in) {
        XID = in.readLong();
        transientfor = in.readLong();
        leader = in.readLong();
        type = in.readInt();
        net_name = in.readString();
        wm_class = in.readString();
        supportDeleteWindow = in.readInt();
    }

    public static final Creator<Property> CREATOR = new Creator<Property>() {
        @Override
        public Property createFromParcel(Parcel in) {
            return new Property(in);
        }

        @Override
        public Property[] newArray(int size) {
            return new Property[size];
        }
    };

    public int getSupportDeleteWindow() {
        return supportDeleteWindow;
    }

    public void setSupportDeleteWindow(int supportDeleteWindow) {
        this.supportDeleteWindow = supportDeleteWindow;
    }

    public long getXID() {
        return XID;
    }

    public void setXID(long XID) {
        this.XID = XID;
    }

    public long getTransientfor() {
        return transientfor;
    }

    public void setTransientfor(long transientfor) {
        this.transientfor = transientfor;
    }

    public long getLeader() {
        return leader;
    }

    public void setLeader(long leader) {
        this.leader = leader;
    }

    public int getType() {
        return type;
    }

    public void setType(int type) {
        this.type = type;
    }

    public String getNet_name() {
        return net_name;
    }

    public void setNet_name(String net_name) {
        this.net_name = net_name;
    }

    public String getWm_class() {
        return wm_class;
    }

    public void setWm_class(String wm_class) {
        this.wm_class = wm_class;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(@NonNull Parcel dest, int flags) {
        dest.writeLong(XID);
        dest.writeLong(transientfor);
        dest.writeLong(leader);
        dest.writeInt(type);
        dest.writeString(net_name);
        dest.writeString(wm_class);
        dest.writeInt(supportDeleteWindow);
    }

    @Override
    public String toString() {
        return "Property{" +
                "XID=" + XID +
                ", transientfor=" + transientfor +
                ", leader=" + leader +
                ", type=" + type +
                ", supportDeleteWindow=" + supportDeleteWindow +
                ", net_name='" + net_name + '\'' +
                ", wm_class='" + wm_class + '\'' +
                '}';
    }
}
