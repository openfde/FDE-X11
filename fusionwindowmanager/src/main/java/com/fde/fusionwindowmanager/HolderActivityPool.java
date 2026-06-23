package com.fde.fusionwindowmanager;

import static com.fde.fusionwindowmanager.HolderActivityPool.Holder.TYPE_DECOR;
import static com.fde.fusionwindowmanager.HolderActivityPool.Holder.TYPE_NO_DECOR;

import android.os.Handler;
import android.os.Message;import android.util.ArrayMap;

public class HolderActivityPool {

    private final int maxSize;
    private final ArrayMap<Long, HolderActivityPool.Holder> pool = new ArrayMap<>();
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
    }

    public synchronized Holder add(long taskId, Holder obj) {
        if (obj == null) return null;
        if (pool.size() < maxSize) {
            if (decorCount < decorSize) {
                obj.type = TYPE_DECOR;
                decorCount++;
            } else if (noDecorCount < noDecorSize) {
                obj.type = TYPE_NO_DECOR;
                noDecorCount++;
            }
            return pool.put(taskId, obj);
        }
        return null;
    }


    public synchronized Holder remove(long taskId) {
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
        return pool.size() + onGoing >= maxSize;
    }

    public boolean goingInrease() {
        onGoing ++;
        return decorCount >= decorSize;
    }

    public static class Holder{
        public static final int TYPE_DECOR = 1;
        public static final int TYPE_NO_DECOR = 2;
        long taskId;
        WindowAttribute attr;
        int type = 1;

        Holder(long taskId, WindowAttribute attr){
            this.attr = attr;
            this.taskId = taskId;
        }

        Holder(long taskId, WindowAttribute attr, int type){
            this(taskId, attr);
            this.type = type;
        }

    }



}
