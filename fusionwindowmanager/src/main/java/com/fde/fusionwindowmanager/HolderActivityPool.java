package com.fde.fusionwindowmanager;

import static com.fde.fusionwindowmanager.HolderActivityPool.Holder.TYPE_DECOR;
import static com.fde.fusionwindowmanager.HolderActivityPool.Holder.TYPE_NO_DECOR;

import android.os.Handler;
import android.os.Message;import android.util.ArrayMap;
import android.util.Log;

public class HolderActivityPool {
    private String TAG = "Pool";

    private final int maxSize;
    private final ArrayMap<Integer, HolderActivityPool.Holder> pool = new ArrayMap<>();
    private final int decorSize;
    private final int noDecorSize;
    private int decorCount;
    private int noDecorCount;
    Handler mHandler;
    private int onGoing;

    public HolderActivityPool(int maxSize, Handler handler) {
        this.maxSize = maxSize;
        this.decorSize = maxSize/2 + 1;
        this.noDecorSize = maxSize - decorSize;
        this.mHandler = handler;
        Log.d(TAG, "HolderActivityPool decorSize:" + decorSize + " noDecorSize:" + noDecorSize + " size:" + pool.size()
                + " maxsize:" + maxSize + " onGoing:" + onGoing);
    }

    public synchronized Holder add(int taskId, Holder obj) {
        if (obj == null) return null;
        if (pool.size() < maxSize) {
            if (decorCount < decorSize) {
                obj.type = TYPE_DECOR;
                decorCount++;
            } else if (noDecorCount < noDecorSize) {
                obj.type = TYPE_NO_DECOR;
                noDecorCount++;
            }
            onGoing--;
            return pool.put(taskId, obj);
        }
        return null;
    }


    public synchronized Holder remove(int taskId) {
        if(pool.get(taskId) == null){

        } if(pool.get(taskId).type == TYPE_DECOR){
            decorCount--;
        } else if(pool.get(taskId).type == TYPE_NO_DECOR){
            noDecorCount--;
        }
        return pool.remove(taskId);
    }

    public synchronized int size() {
        return pool.size();
    }

    public synchronized boolean isFullOrNearly(){
        Log.d(TAG, "isFullOrNearly decorSize:" + decorSize + " noDecorSize:" + noDecorSize + " size:" + pool.size()
                + " maxsize:" + maxSize + " onGoing:" + onGoing);
        return pool.size() + onGoing >= maxSize;
    }

    public boolean goingInrease() {
        onGoing ++;
        return decorCount >= decorSize;
    }

    public int offerHolder(boolean nodecor) {
        int type = nodecor ? TYPE_NO_DECOR : TYPE_DECOR;
        for (Holder holder: pool.values()){
            if(holder.type == type){
                return holder.taskId;
            }
        }
        return -1;
    }

    public static class Holder{
        public static final int TYPE_DECOR = 1;
        public static final int TYPE_NO_DECOR = 2;
        int taskId;
        WindowAttribute attr;
        int type = 1;

        Holder(int taskId, WindowAttribute attr){
            this.attr = attr;
            this.taskId = taskId;
        }

        Holder(int taskId, WindowAttribute attr, int type){
            this(taskId, attr);
            this.type = type;
        }

    }



}
