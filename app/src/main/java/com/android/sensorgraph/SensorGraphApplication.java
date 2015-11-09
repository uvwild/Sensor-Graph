package com.android.sensorgraph;

import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.util.Log;
import android.widget.Toast;

/**
 * Created by uv on 03/11/2015.
 */
public class SensorGraphApplication extends Application {
    private static Context mContext;
    public void onCreate() {
        super.onCreate();
        mContext = this;
        Log.w("native-activity", "onCreate");

        final PackageManager pm = getApplicationContext().getPackageManager();

        ApplicationInfo ai;
        try {
            ai = pm.getApplicationInfo(this.getPackageName(), 0);
        } catch (final PackageManager.NameNotFoundException e) {
            ai = null;
        }
        final String applicationName = (String) (ai != null ? pm.getApplicationLabel(ai) : "(sensorph)");
        Toast.makeText(this, "UV says " + applicationName + " Holdrio!", Toast.LENGTH_LONG).show();
    }

    public static Context getContext(){
        return mContext;
    }
}
