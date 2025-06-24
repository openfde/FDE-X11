package com.fde.fusionwindowmanager;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

public class MediaStoreUriHelper {

    public static void copyMediaUriToClipboard(Context context, String filePath) {
        // 1. 获取文件在 MediaStore 中的 URI
        Uri mediaUri = getMediaContentUri(context, filePath);

        if (mediaUri != null) {
            // 2. 授予临时访问权限
            context.grantUriPermission(
                    context.getPackageName(),
                    mediaUri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
            );

            // 3. 存入剪贴板
            ClipboardManager clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clip = ClipData.newUri(context.getContentResolver(), "Media URI", mediaUri);
            clipboard.setPrimaryClip(clip);
        } else {
            // 处理未找到文件的情况
        }
    }

    // 通过文件路径获取 MediaStore URI
    private static Uri getMediaContentUri(Context context, String filePath) {
        ContentResolver resolver = context.getContentResolver();
        Uri externalUri = MediaStore.Files.getContentUri("external");

        // 根据文件类型确定 MIME 类型
        String mimeType = getMimeTypeFromPath(filePath);
        String mediaType = getMediaTypeFromMime(mimeType);

        // 查询条件：文件路径完全匹配
        String selection = MediaStore.MediaColumns.DATA + "=?";
        String[] selectionArgs = new String[]{filePath};

        // 根据文件类型优化查询
        String[] projection = {MediaStore.MediaColumns._ID};

        Cursor cursor = resolver.query(
                externalUri,
                projection,
                selection,
                selectionArgs,
                null
        );

        if (cursor != null && cursor.moveToFirst()) {
            long id = cursor.getLong(cursor.getColumnIndexOrThrow(MediaStore.MediaColumns._ID));
            cursor.close();

            // 构造特定媒体类型的 URI（图片/视频/音频/下载）
            switch (mediaType) {
                case "image":
                    return MediaStore.Images.Media.EXTERNAL_CONTENT_URI.buildUpon()
                            .appendPath(String.valueOf(id)).build();
                case "video":
                    return MediaStore.Video.Media.EXTERNAL_CONTENT_URI.buildUpon()
                            .appendPath(String.valueOf(id)).build();
                case "audio":
                    return MediaStore.Audio.Media.EXTERNAL_CONTENT_URI.buildUpon()
                            .appendPath(String.valueOf(id)).build();
                default: // 包括下载文件
                    return MediaStore.Files.getContentUri("external", id);
            }
        }
        return null; // 未找到文件
    }

    // 获取文件 MIME 类型
    private static String getMimeTypeFromPath(String path) {
        String extension = path.substring(path.lastIndexOf(".") + 1).toLowerCase();
        switch (extension) {
            case "jpg": case "jpeg": return "image/jpeg";
            case "png": return "image/png";
            case "gif": return "image/gif";
            case "mp4": case "mpeg4": return "video/mp4";
            case "3gp": return "video/3gpp";
            case "mkv": return "video/x-matroska";
            case "mp3": return "audio/mpeg";
            case "ogg": return "audio/ogg";
            case "wav": return "audio/wav";
            case "pdf": return "application/pdf";
            case "doc": return "application/msword";
            default: return "*/*";
        }
    }

    // 根据 MIME 类型确定媒体大类
    private static String getMediaTypeFromMime(String mimeType) {
        if (mimeType.startsWith("image/")) return "image";
        if (mimeType.startsWith("video/")) return "video";
        if (mimeType.startsWith("audio/")) return "audio";
        return "download"; // 其他类型视为下载文件
    }
}