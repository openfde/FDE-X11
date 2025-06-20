package com.fde.fusionwindowmanager;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;


import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;

public class Util {

    public static final String LINUX_WINDOW_ATTRIBUTE = "linux_window_attribute";

    public static final String WINDOW_ATTRIBUTE = "window_attribute";

    private static final String TAG = "Util";
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

    public static void copyAssetsToFilesIfNedd(Context context, String sourceDir, String targetDir) {
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
            if(dir.exists()){
                Log.d(TAG, "copyAssetsToFiles: exists, return");
                return;
            }

            if (!dir.exists()) {
                if (!dir.mkdirs()) {
                    Log.e("AssetCopy", "Failed to create directory: " + dir.getAbsolutePath());
                    return;
                }
            }

            for (String filename : files) {
                String sourceFile = sourceDir.isEmpty() ? filename : sourceDir + File.separator + filename;
                String targetFile = targetDir.isEmpty() ? filename : targetDir + File.separator + filename;

                if (isAssetDirectory(assetManager, sourceFile)) {
                    copyAssetsToFilesIfNedd(context, sourceFile, targetFile);
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

    public static Class<?> getClassByName(String name) {
        Class<?> xwindowActivityClass = null;
        try {
            xwindowActivityClass = Class.forName(name);
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return xwindowActivityClass;
    }


    public static String getClipFilePath(ClipboardManager mClipboardManager, Context context) {
        if (mClipboardManager != null && mClipboardManager.hasPrimaryClip()) {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            if (clipData != null && clipData.getItemCount() > 0
                    && clipData.getDescription().getLabel() != null) {
                int itemCount = clipData.getItemCount();
                for(int i = 0 ; i < itemCount ; i++){
                    ClipData.Item item = clipData.getItemAt(i);
                    Uri uri = item.getUri();
                    if(uri != null ){
                        try {
                            String pathSuffix = uri.getPath().split(":")[1];
                            return "file:///home/huyang/openfde/" + pathSuffix;
                        } catch (Exception e) {
                            Log.e(TAG, "getClipText: " + e.getMessage());
                        }
                    }
                }
            }
        }
        return null;
    }

    public static String getClipText(ClipboardManager mClipboardManager, Context context) {
        if (mClipboardManager != null && mClipboardManager.hasPrimaryClip()) {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            if (clipData != null && clipData.getItemCount() > 0
                    && clipData.getDescription().getLabel() != null) {
                int itemCount = clipData.getItemCount();
                for(int i = 0 ; i < itemCount ; i++){
                    ClipData.Item item = clipData.getItemAt(i);
                    CharSequence content = item.getText();
                    if(content != null){
                        return content.toString();
                    }
                }
            }
        }
        return null;
    }
}