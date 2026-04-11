package com.kryxan.ipcverify;

public final class NativeBridge {
    static {
        System.loadLibrary("ipcverify");
    }

    private NativeBridge() {
    }

    public static native String runNativeChecks();
}
