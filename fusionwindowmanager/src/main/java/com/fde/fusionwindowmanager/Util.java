package com.fde.fusionwindowmanager;

import android.annotation.SuppressLint;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.PersistableBundle;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.util.Log;


import androidx.core.content.FileProvider;

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

    public static void setBaseContext(Context context) {
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
            if (dir.exists()) {
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
                for (int i = 0; i < itemCount; i++) {
                    ClipData.Item item = clipData.getItemAt(i);
                    Uri uri = item.getUri();
                    if (uri != null) {
                        try {
                            String pathSuffix = uri.getPath().split(":")[1];
                            return "file:///home/"+  LinuxRootFileUtils.getUserName(context)  + "/openfde/" + pathSuffix;
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
                for (int i = 0; i < itemCount; i++) {
                    ClipData.Item item = clipData.getItemAt(i);
                    CharSequence content = item.getText();
                    if (content != null) {
                        return content.toString();
                    }
                }
            }
        }
        return null;
    }

    public static void copyFileUriToClipboard(Context context, String fileUrl) {
        Log.d(TAG, "copyFileUriToClipboard: " + fileUrl);
        String filePath = fileUrl.substring(7);
        String sdCardPath = convertToSdCardPath(filePath, context);
//        copyFileToClipboard(context, sdCardPath);
        MediaStoreUriHelper.copyMediaUriToClipboard(context, sdCardPath);
    }

    private static String convertToSdCardPath(String originalPath, Context context) {
        String internalStoragePath = Environment.getExternalStorageDirectory().getAbsolutePath();
        String userName = LinuxRootFileUtils.getUserName(context);
        Log.d(TAG, "convertToSdCardPath() originalPath = [" + originalPath + "], userName = [" + userName + "]");
        if (originalPath.startsWith("/home/" + userName + "/openfde")) {
            Log.d(TAG, "convertToSdCardPath: contains");
            return originalPath.replace("/home/" + userName  + "/openfde", internalStoragePath);
        }
        Log.d(TAG, "convertToSdCardPath: not contains");
        return originalPath;
    }

    public static void copyFileToClipboard(Context context, String path) {
        Log.d(TAG, "copyFileToClipboard: path:" + path);
        // 获取文件的 URI
        Uri fileUri;
        File file = new File(path.trim());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // 使用 FileProvider 获取安全的 URI
            fileUri = FileProvider.getUriForFile(
                    context,
                    context.getApplicationContext().getPackageName() + ".provider",
                    file
            );
        } else {
            fileUri = Uri.fromFile(file);
        }
        PersistableBundle bundle = new PersistableBundle();
        bundle.putInt("clipper:opType", 1);
        ClipDescription description = new ClipDescription("", new String[]{"image/png"});
        description.setExtras(bundle);
        ClipData clipData = new ClipData(description, new ClipData.Item(fileUri));
                // 创建 ClipData
//        ClipData clipData = ClipData.newUri(context.getContentResolver(), "File", fileUri);
//        ClipData clipData = new ClipData("Label", new String[]{"image/png"}, new ClipData.Item(fileUri));
        // 设置 ClipData 的 Intent（可选，提供更多信息）
//        Intent clipIntent = new Intent();
//        clipIntent.setData(fileUri);
//        clipIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
//        clipData.getDescription().setExtras(clipIntent.getExtras());

        // 获取剪贴板服务并设置 ClipData
        ClipboardManager clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(clipData);

            // 或者授予所有应用临时权限（不推荐）
            context.grantUriPermission(
                    "*",
                    fileUri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
            );
        }
    }
}