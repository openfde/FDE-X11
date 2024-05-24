package com.termux.x11.data;



import static com.termux.x11.data.Constants.URL_STOPAPP;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;
import com.termux.x11.AppListActivity;
import com.termux.x11.R;
import com.termux.x11.RightClickView;
import com.termux.x11.utils.AppUtils;
import com.xwdz.http.QuietOkHttp;
import com.xwdz.http.callback.JsonCallBack;
import java.util.List;
import okhttp3.Call;

public class AppAdapter extends RecyclerView.Adapter<AppAdapter.ViewHolder> {

    Handler handler =  new Handler(Looper.getMainLooper());

    private List<AppListResult.DataBeanX.DataBean> list;
    private Context context;
    private String TAG = "AppAdapter";
    private boolean isConnecting;

    AppListActivity.ItemClickListener itemClickListener;

    public AppAdapter(@NonNull Context context, List<AppListResult.DataBeanX.DataBean> list, AppListActivity.ItemClickListener listener) {
        this.context = context;
        this.list = list;
        this.itemClickListener = listener;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.app_item, parent, false);
        ViewHolder holder = new ViewHolder(view);
        return holder;
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, @SuppressLint("RecyclerView") int position) {
        final AppListResult.DataBeanX.DataBean app = list.get(position);
        app.id = position;
        TextView textView = holder.textView;
        textView.setText(app.Name);
        textView.setTypeface(null, Typeface.NORMAL);
        textView.setTextColor(ContextCompat.getColor(context, R.color.black));
        ImageView imageView =holder.imageView;
        imageView.setImageDrawable(AppUtils.getImage(app.Icon, app.getIconType(), app.getName(), context));
        holder.itemView.setTag(position);
        holder.itemView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                tryStartVncApp(app);
            }
        });
        holder.entryView.setListener(new RightClickView.RightClickListener() {
            @Override
            public void onRightClick(boolean b, MotionEvent event) {
                itemClickListener.onItemClick(holder.imageView, position, app, b, event);
            }
        });
    }

    private void tryStartVncApp(AppListResult.DataBeanX.DataBean app) {
        // todo mock
        QuietOkHttp.post(Constants.BASEURL + URL_STOPAPP)
                .setCallbackToMainUIThread(true)
                .addParams("App", app.Name)
                .addParams("Path", app.Path)
                .addParams("SysOnly", "false")
                .execute(new JsonCallBack<VncResult.GetPortResult>() {
                    @Override
                    public void onFailure(Call call, Exception e) {
                        Log.d(TAG, "onFailure() called with: call = [" + call + "], e = [" + e + "]");
                        handler.post(new Runnable() {
                            @Override
                            public void run() {
//                                Toast.makeText(context, "无法启动此程序", Toast.LENGTH_LONG).show();
                            }
                        });
                    }

                    @Override
                    public void onSuccess(Call call, VncResult.GetPortResult response) {
                        Log.d(TAG, "onSuccess() called with: call = [" + call + "], response = [" + response + "]");
                        Activity activity = (Activity) context;
                        handler.post(new Runnable() {
                            @Override
                            public void run() {
//                                Toast.makeText(context, String.format("%s 启动中", app.Name), Toast.LENGTH_SHORT).show();
                            }
                        });
//                        tryLunchApp(app, response.Data.Port);
                    }
                });
    }

    private void tryLunchApp(AppListResult.DataBeanX.DataBean app, int port) {

    }


    @Override
    public int getItemCount() {
        return list.size();
    }

    public void notifyDataSetChanged(List<AppListResult.DataBeanX.DataBean> mDataList) {
        this.list = mDataList;
        super.notifyDataSetChanged();
    }

    public class ViewHolder extends RecyclerView.ViewHolder{
        TextView textView;
        ImageView imageView;
        RightClickView entryView;
        public ViewHolder(View view){
            super(view);
            imageView = view.findViewById(R.id.icon);
            textView = view.findViewById(R.id.name);
            entryView = view.findViewById(R.id.entry);
        }
    }
}
