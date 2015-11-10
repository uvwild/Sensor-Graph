/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.sensorgraph;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.NativeActivity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.ViewFlipper;


public class SensorGraphNativeActivity extends NativeActivity {

    private static final String TAG = "SensorGraphNtvActivity";
    int count = 0;
    GLSurfaceView mView;
    SensorGraphNativeActivity _activity;

//    @Override protected void onPause() {
//        super.onPause();
//        mView.onPause();
//        mView.queueEvent(new Runnable() {
//            @Override
//            public void run() {
//                SensorGraphJNI.pause();
//            }
//        });
//    }
PopupWindow _popupWindow;
    TextView _label;

//    @Override protected void onResume() {
//        super.onResume();
//        mView.onResume();
//        mView.queueEvent(new Runnable() {
//            @Override
//            public void run() {
//                SensorGraphJNI.resume();
//            }
//        });
//    }
TextView _bottomLine;
    ViewFlipper _viewFlipper;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        int SDK_INT = android.os.Build.VERSION.SDK_INT;
        if (SDK_INT >= 19) {
            setImmersiveSticky();

            View decorView = getWindow().getDecorView();
            decorView.setOnSystemUiVisibilityChangeListener
                    (new View.OnSystemUiVisibilityChangeListener() {
                        @Override
                        public void onSystemUiVisibilityChange(int visibility) {
                            setImmersiveSticky();
                        }
                    });
        }

//        mView = new GLSurfaceView(getApplication());
//        mView.setEGLContextClientVersion(2);
//        mView.setRenderer(new GLSurfaceView.Renderer() {
//            @Override
//            public void onSurfaceCreated(GL10 gl, EGLConfig config) {
//                SensorGraphJNI.surfaceCreated();
//            }
//
//            @Override
//            public void onSurfaceChanged(GL10 gl, int width, int height) {
//                SensorGraphJNI.surfaceChanged(width, height);
//            }
//
//            @Override
//            public void onDrawFrame(GL10 gl) {
//                SensorGraphJNI.drawFrame();
//            }
//        });
//        mView.queueEvent(new Runnable() {
//            @Override
//            public void run() {
//                AssetManager  am = getAssets();
//                SensorGraphJNI.init(am);
//            }
//        });
//	    setContentView(mView);
    }

    protected void onPause() {
        super.onPause();
        if (_popupWindow != null) {
            _popupWindow.dismiss();
            _popupWindow = null;
        }
    }

    @TargetApi(19)
    protected void onResume() {
        super.onResume();

        //Hide toolbar
        int SDK_INT = android.os.Build.VERSION.SDK_INT;
        if (SDK_INT >= 11 && SDK_INT < 14) {
            getWindow().getDecorView().setSystemUiVisibility(View.STATUS_BAR_HIDDEN);
        } else if (SDK_INT >= 14 && SDK_INT < 19) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else if (SDK_INT >= 19) {
            setImmersiveSticky();
        }

    }

    @TargetApi(19)
    void setImmersiveSticky() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @SuppressLint("InflateParams")
    public void showUI() {
        if (_popupWindow != null)
            return;

        _activity = this;

        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                LayoutInflater layoutInflater
                        = (LayoutInflater) getBaseContext()
                        .getSystemService(LAYOUT_INFLATER_SERVICE);
                View popupView = layoutInflater.inflate(R.layout.widgets, null);
                _popupWindow = new PopupWindow(
                        popupView,
                        WindowManager.LayoutParams.WRAP_CONTENT,
                        WindowManager.LayoutParams.WRAP_CONTENT);
                _activity.handleButtons(popupView);
                _activity.handleSliders(popupView);
                LinearLayout mainLayout = new LinearLayout(_activity);
                ViewGroup.MarginLayoutParams params = new ViewGroup.MarginLayoutParams(WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT);
                params.setMargins(0, 0, 0, 0);
                _activity.setContentView(mainLayout, params);

                // Show our UI over NativeActivity window
                _popupWindow.showAtLocation(mainLayout, Gravity.TOP | Gravity.START, 10, 10);
                _popupWindow.update();

                _label = (TextView) popupView.findViewById(R.id.textViewFPS);
                _bottomLine = (TextView) popupView.findViewById(R.id.textViewColors);
                _viewFlipper = (ViewFlipper) popupView.findViewById(R.id.viewFlipper);
            }
        });
    }

    private void handleSliders(View view) {
        Log.v(TAG, "slider count=" + 1);

        for (Color c : Color.values())
            handleSlider(view, c);
    }

    @TargetApi(21)
    private void handleSlider(final View view, final Color c) {
        SeekBar seekBar = (SeekBar) view.findViewById(c.sliderId);
        assert (seekBar != null);
        final TextView barLabel = (TextView) view.findViewById(c.labelId);
        assert (barLabel != null);

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                Log.v(TAG, "progress of " + c.name() + " : " + progress);
                barLabel.setText(String.format("%d%%", progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                Log.v(TAG, "onStartTrackingTouch " + seekBar.getId());
                seekBar.setElevation(2.0f);
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.v(TAG, "onStopTrackingTouch " + seekBar.getId());
                seekBar.setElevation(0.0f);
            }
        });
    }

    // create and link the button
    private void handleButtons(View view) {
        Log.v(TAG, "button count=" + 1);

        // Get a reference to the Press Me Button
        final Button setParamButton = (Button) view.findViewById(R.id.button);
        assert (setParamButton != null);
        // Set an OnClickListener on this Button
        // Called each time the user clicks the Button
        setParamButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                // Maintain a count of user presses Display count as text on the Button and active child
                int i = _viewFlipper.getDisplayedChild();
                setParamButton.setText("Press " + ++count + " child: " + i);
                _viewFlipper.showNext();
            }
        });

        // Get a reference to the BACK Button
        final Button goBackButton = (Button) view.findViewById(R.id.buttonBack);
        assert (goBackButton != null);
        goBackButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                // Maintain a count of user presses Display count as text on the Button and active child
                int i = _viewFlipper.getDisplayedChild();
                goBackButton.setText("Press " + ++count + " child: " + i);
                _viewFlipper.showPrevious();    // go Back
            }
        });

        // Get a reference to the SHowMAP Button
        final Button goMapButton = (Button) view.findViewById(R.id.buttonShowMAP);
        assert (goMapButton != null);
        goMapButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                goMapButton.setText("MAP " + ++count);
                Log.v(TAG, "switch to mapview ");
                Intent intent = new Intent(SensorGraphNativeActivity.this, MapViewActivity.class);
                startActivity(intent);
            }
        });


    }

    public void updateFPS(final float fFPS) {
        if (_label == null)
            return;

        _activity = this;
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                _label.setText(String.format("%2.2f FPS", fFPS));
            }
        });
    }


    /**
     * @return if view should be redrawn
     */
    public boolean updateViews() {
        if (_viewFlipper == null) return false;
        return _viewFlipper.getDisplayedChild() == 0;
    }


    public void updateBottomLine(final String text) {
        if (_bottomLine == null)
            return;

        if (text == null)
            return;

        _activity = this;
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                _bottomLine.setText(String.format("%s", text));
            }
        });
    }

    // return the seekbar value for the given color
    public int getProgressValue(int which) {
        Color c = Color.getWithId(which);
        if (_popupWindow != null) {
            SeekBar seekBar = (SeekBar) _popupWindow.getContentView().findViewById(c.sliderId);
            return (seekBar != null) ? seekBar.getProgress() : 0;
        }
        return 0;
    }


    public AssetManager getAssetManager() {
        Log.v(TAG, "getAssetManager()");
        return SensorGraphApplication.getContext().getAssets();
    }

}
