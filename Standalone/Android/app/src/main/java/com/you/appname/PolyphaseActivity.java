package com.you.appname;

import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.MulticastLock;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

// NOTE: this Java class lives at java/com/you/appname/, so its fully-qualified
// name is com.you.appname.PolyphaseActivity regardless of what gradle's
// `namespace`/`applicationId` is set to. AndroidManifest references it by FQN
// (`android:name="com.you.appname.PolyphaseActivity"`), so projects can change
// applicationId freely without renaming the Java tree.
//
// The path is intentionally unbranded — `com.you.appname` makes it obvious this
// is a template default that must be replaced before any real distribution.
// Override gradle's namespace/applicationId via Build Profile → Target Options
// (the packager rewrites build.gradle at package time).
//
// Earlier this file imported a databinding class (whose package follows gradle's
// `namespace`). When projects override applicationId, namespace changes, the
// databinding package moves, and the import breaks. The binding field was never
// used — removed entirely, along with viewBinding in build.gradle.

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

public class PolyphaseActivity extends NativeActivity {

    // Used to load the 'standalone' library on application startup.
    static {
        System.loadLibrary("standalone");
    }

    private WifiManager wifiManager;
    private MulticastLock multicastLock;

    @Override
    protected void onCreate(Bundle savedInstanceState) {

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

        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onResume() {
        //Hide toolbar
        int SDK_INT = android.os.Build.VERSION.SDK_INT;
        if (SDK_INT >= 11 && SDK_INT < 14) {
            getWindow().getDecorView().setSystemUiVisibility(View.STATUS_BAR_HIDDEN);
        } else if (SDK_INT >= 14 && SDK_INT < 19) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else if (SDK_INT >= 19) {
            setImmersiveSticky();
        }
        super.onResume();

        // Acquire multicast lock. In the future, maybe only do this when searching for LAN sessions.
        // If we don't acquire a multicast lock then we won't be able to receive LAN session broadcasts.
        if (wifiManager == null)
        {
            wifiManager = (WifiManager) getSystemService(Context.WIFI_SERVICE);
        }

        if (multicastLock == null)
        {
            multicastLock = wifiManager.createMulticastLock("Polyphase");
            multicastLock.setReferenceCounted(true);
        }

        if (multicastLock != null && !multicastLock.isHeld())
        {
            multicastLock.acquire();
        }
    }

    @Override
    protected void onPause()
    {
        super.onPause();

        if (multicastLock != null && multicastLock.isHeld())
        {
            multicastLock.release();
        }
    }

    void setImmersiveSticky() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    void setSystemOrientation(int orientation)
    {
        if (orientation == 0) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }
        else if (orientation == 1)
        {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }
        else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER);
        }
    }

    public void showSoftKeyboard()
    {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput( this.getWindow().getDecorView(), InputMethodManager.SHOW_FORCED);
    }

    public void hideSoftKeyboard()
    {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.hideSoftInputFromWindow(this.getWindow().getDecorView().getWindowToken(), 0);
    }

    public boolean isSoftKeyboardShown()
    {
        WindowInsetsCompat insets = ViewCompat.getRootWindowInsets(this.getWindow().getDecorView());
        boolean imeVisible = insets.isVisible(WindowInsetsCompat.Type.ime());

        // In the future, we may want to provide a way to query the keyboard height so the game
        // can position elements better.
        // int imeHeight = insets.getInsets(WindowInsetsCompat.Type.ime()).bottom

        return imeVisible;
    }

    public ArrayList<String> iterateDirFiles(String path)
    {
        ArrayList<String> retList = new ArrayList<>();

        AssetManager assetMgr = getAssets();

        try
        {
            String list[] = assetMgr.list(path);

            if (list != null)
            {
                for (int i = 0; i < list.length; ++i)
                {
                    retList.add(list[i]);
                }
            }

        }
        catch (IOException e)
        {
            Log.d("Polyphase", "Can't iterate directory " + path);
        }

        return retList;
    }

    // ===== HTTP (native HttpBackend_Android.cpp bridge) =====================
    //
    // Native holds no long-lived JNI object refs across the request beyond
    // what these three calls pass back and forth. httpOpen() does the
    // connect (following redirects itself, since HttpURLConnection won't
    // cross http<->https on its own) and returns everything native needs to
    // populate HttpResponse's status/headers/finalUrl; httpReadChunk() is
    // then polled in a loop so native can check its cancellation flag between
    // chunks instead of blocking on the whole body at once; httpClose()
    // releases the stream and connection. See HttpBackend_Android.cpp for the
    // native side of this contract.

    private static class HttpConn {
        HttpURLConnection connection;
        InputStream inputStream;
    }

    private static void installTrustAllForConnection(HttpsURLConnection https)
    {
        // Only ever installed on a single connection instance when the
        // caller explicitly asked to skip verification (HttpRequest::VerifySsl(false))
        // -- never touches the process-wide SSL defaults.
        try
        {
            TrustManager[] trustAll = new TrustManager[] {
                new X509TrustManager() {
                    public void checkClientTrusted(X509Certificate[] chain, String authType) {}
                    public void checkServerTrusted(X509Certificate[] chain, String authType) {}
                    public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
                }
            };
            SSLContext sslContext = SSLContext.getInstance("TLS");
            sslContext.init(null, trustAll, new SecureRandom());
            https.setSSLSocketFactory(sslContext.getSocketFactory());
            https.setHostnameVerifier(new HostnameVerifier() {
                public boolean verify(String hostname, SSLSession session) { return true; }
            });
        }
        catch (Exception e)
        {
            Log.d("Polyphase", "installTrustAllForConnection failed: " + e.getMessage());
        }
    }

    // Returns Object[6]: { HttpConn connOrNull, Integer status, String errorCode,
    //                      String errorMessage, String finalUrl, String[] headerLines }.
    // errorCode is one of "none"/"timeout"/"tls"/"badresponse"/"network"/"unknown";
    // native maps it onto HttpError. connOrNull is null when errorCode != "none".
    public Object[] httpOpen(String verb, String url, String[] headerLines, byte[] body,
                              int timeoutMs, int maxRedirects, boolean verifySsl)
    {
        String currentUrl = url;
        int redirectsLeft = maxRedirects;

        try
        {
            while (true)
            {
                URL u = new URL(currentUrl);
                HttpURLConnection conn = (HttpURLConnection) u.openConnection();
                conn.setRequestMethod(verb);
                conn.setConnectTimeout(timeoutMs);
                conn.setReadTimeout(timeoutMs);
                // Followed manually below -- the built-in follower refuses to
                // cross http<->https, unlike curl's CURLOPT_FOLLOWLOCATION.
                conn.setInstanceFollowRedirects(false);

                if (headerLines != null)
                {
                    for (String line : headerLines)
                    {
                        int idx = line.indexOf(':');
                        if (idx > 0)
                        {
                            String key = line.substring(0, idx);
                            String value = line.substring(Math.min(idx + 2, line.length()));
                            conn.setRequestProperty(key, value);
                        }
                    }
                }

                if (!verifySsl && conn instanceof HttpsURLConnection)
                {
                    installTrustAllForConnection((HttpsURLConnection) conn);
                }

                boolean hasBody = body != null && body.length > 0 &&
                    (verb.equals("POST") || verb.equals("PUT") || verb.equals("PATCH"));

                if (hasBody)
                {
                    conn.setDoOutput(true);
                    OutputStream os = conn.getOutputStream();
                    os.write(body);
                    os.flush();
                    os.close();
                }

                int status = conn.getResponseCode();
                boolean isRedirect = (status == 301 || status == 302 || status == 303 ||
                                       status == 307 || status == 308);

                if (isRedirect && redirectsLeft > 0)
                {
                    String location = conn.getHeaderField("Location");
                    conn.disconnect();

                    if (location == null)
                    {
                        return new Object[] { null, 0, "badresponse",
                            "Redirect with no Location header", currentUrl, null };
                    }

                    currentUrl = new URL(new URL(currentUrl), location).toString();
                    --redirectsLeft;
                    continue;
                }

                ArrayList<String> headerList = new ArrayList<>();
                for (int i = 0; ; ++i)
                {
                    String key = conn.getHeaderFieldKey(i);
                    String value = conn.getHeaderField(i);
                    if (key == null && value == null)
                    {
                        break;
                    }
                    if (key != null)
                    {
                        headerList.add(key + ": " + value);
                    }
                }

                InputStream stream;
                try
                {
                    stream = conn.getInputStream();
                }
                catch (IOException e)
                {
                    // 4xx/5xx responses throw here; the body (if any) is on getErrorStream().
                    stream = conn.getErrorStream();
                }

                HttpConn wrapper = new HttpConn();
                wrapper.connection = conn;
                wrapper.inputStream = stream;

                return new Object[] { wrapper, status, "none", "",
                    conn.getURL().toString(), headerList.toArray(new String[0]) };
            }
        }
        catch (SocketTimeoutException e)
        {
            return new Object[] { null, 0, "timeout", String.valueOf(e.getMessage()), currentUrl, null };
        }
        catch (SSLException e)
        {
            return new Object[] { null, 0, "tls", String.valueOf(e.getMessage()), currentUrl, null };
        }
        catch (MalformedURLException e)
        {
            return new Object[] { null, 0, "badresponse", String.valueOf(e.getMessage()), currentUrl, null };
        }
        catch (IOException e)
        {
            return new Object[] { null, 0, "network", String.valueOf(e.getMessage()), currentUrl, null };
        }
        catch (Exception e)
        {
            return new Object[] { null, 0, "unknown", String.valueOf(e.getMessage()), currentUrl, null };
        }
    }

    // Reads up to maxLen bytes. Empty array = EOF, null = read error.
    public byte[] httpReadChunk(Object connWrapper, int maxLen)
    {
        HttpConn wrapper = (HttpConn) connWrapper;
        if (wrapper.inputStream == null)
        {
            return new byte[0];
        }

        try
        {
            byte[] buffer = new byte[maxLen];
            int read = wrapper.inputStream.read(buffer);
            if (read <= 0)
            {
                return new byte[0];
            }
            if (read == maxLen)
            {
                return buffer;
            }
            byte[] result = new byte[read];
            System.arraycopy(buffer, 0, result, 0, read);
            return result;
        }
        catch (IOException e)
        {
            return null;
        }
    }

    public void httpClose(Object connWrapper)
    {
        HttpConn wrapper = (HttpConn) connWrapper;

        try
        {
            if (wrapper.inputStream != null)
            {
                wrapper.inputStream.close();
            }
        }
        catch (IOException e)
        {
            // Best-effort cleanup.
        }

        if (wrapper.connection != null)
        {
            wrapper.connection.disconnect();
        }
    }
}