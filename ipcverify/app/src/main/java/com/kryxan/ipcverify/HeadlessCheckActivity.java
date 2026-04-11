package com.kryxan.ipcverify;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class HeadlessCheckActivity extends Activity {
    private static final String TAG = "IPCVerify";
    private final ExecutorService executor = Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (getIntent() != null && !getIntent().getBooleanExtra("autorun", true)) {
            finish();
            return;
        }

        executor.execute(() -> {
            Log.i(TAG, "Headless IPC verification starting");
            NativeBridge.runNativeChecks();
            Log.i(TAG, "Headless IPC verification finished");
            runOnUiThread(() -> {
                finishAndRemoveTask();
                finish();
            });
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        executor.shutdownNow();
    }
}
