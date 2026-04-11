package com.kryxan.ipcverify;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private TextView logWindow;
    private Button checkButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(R.string.app_name);
        setContentView(R.layout.activity_main);

        logWindow = findViewById(R.id.logWindow);
        checkButton = findViewById(R.id.checkButton);

        checkButton.setOnClickListener(view -> runChecks());

        if (getIntent() != null && getIntent().getBooleanExtra("autorun", false)) {
            runChecks();
        }
    }

    private void runChecks() {
        checkButton.setEnabled(false);
        logWindow.setText("IPC Verification\n\nRunning checks...");

        executor.execute(() -> {
            final String result = NativeBridge.runNativeChecks();
            runOnUiThread(() -> {
                logWindow.setText(result);
                checkButton.setEnabled(true);
            });
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        executor.shutdownNow();
    }
}
