package com.termux.x11.utils;

import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Log;

import com.termux.x11.MainActivity;

import java.io.Closeable;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;

public class Util {
    private static final String TAG = "Util";
    private static final int BITMAP_SIZE_LIMIT = 360;
    private static Context baseContext;

    public static void setBaseContext(Context context){
        baseContext = context;
    }

    /**
     * Count the number of bits in an integer.
     *
     * @param n The integer containing the bits.
     * @return The number of bits in the integer.
     */
    public static int bitcount(int n) {
        int c = 0;

        while (n != 0) {
            c += n & 1;
            n >>= 1;
        }

        return c;
    }

    public static void startActivityForWindow(long windowPtr) {
        Log.d(TAG, "startActivityForWindow() called with: windowPtr = [" + windowPtr + "]");
        Intent intent = new Intent(baseContext, MainActivity.MainActivity1.class);
        intent.putExtra("KEY_WindowPtr", windowPtr);
        intent.setFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT | Intent.FLAG_ACTIVITY_NEW_TASK);
        baseContext.startActivity(intent);
    }



    public static void copyAssetsToFiles(Context context, String sourceDir, String targetDir) {
        AssetManager assetManager = context.getAssets();
        String[] files = null;
        try {
            files = assetManager.list(sourceDir);
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }

        if (files != null && files.length > 0) {
            File dir = new File(context.getFilesDir(), targetDir);
            if (!dir.exists()) {
                if (!dir.mkdirs()) {
                    Log.e("AssetCopy", "Failed to create directory: " + dir.getAbsolutePath());
                    return;
                }
            }

            for (String filename : files) {
                String sourceFile = sourceDir.equals("") ? filename : sourceDir + File.separator + filename;
                String targetFile = targetDir.equals("") ? filename : targetDir + File.separator + filename;

                if (isAssetDirectory(assetManager, sourceFile)) {
                    copyAssetsToFiles(context, sourceFile, targetFile);
                } else {
                    copyAssetFile(assetManager, context.getFilesDir(), sourceFile, targetFile);
                }
            }
        }
    }

    private static boolean isAssetDirectory(AssetManager assetManager, String path) {
        try {
            String[] list = assetManager.list(path);
            return list != null && list.length > 0;
        } catch (IOException e) {
            return false;
        }
    }

    private static void copyAssetFile(AssetManager assetManager, File targetDir, String sourceFile, String targetFile) {
        InputStream in = null;
        OutputStream out = null;
        File outFile = new File(targetDir, targetFile);
        try {
            in = assetManager.open(sourceFile);
            out = new FileOutputStream(outFile);
            copyFile(in, out);
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            closeStream(in);
            closeStream(out);
        }
    }

    private static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
    }

    private static void closeStream(Closeable stream) {
        if (stream != null) {
            try {
                stream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public static void set(String key, String defaultValue) {
        try {
            final Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            final Method set = systemProperties.getMethod("set", String.class, String.class);
            set.invoke(null, key, defaultValue);
            Log.d(TAG,"set " + key + " " + defaultValue);
        } catch (Exception e) {
            Log.e(TAG, "Exception while setting system property: ", e);
        }
    }

    public static Class<?> getClassByName(String name) {
        Class<?> xwindowActivityClass = null;
        try {
            xwindowActivityClass = Class.forName(name);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return xwindowActivityClass;
    }

    public static Bitmap scaleBitmapIfneed(Bitmap bitmap) {
        if(bitmap == null || bitmap.getWidth() < BITMAP_SIZE_LIMIT || bitmap.getHeight() < BITMAP_SIZE_LIMIT) {
            return bitmap;
        }
        return Bitmap.createScaledBitmap(bitmap, BITMAP_SIZE_LIMIT, BITMAP_SIZE_LIMIT, true);
    }
}