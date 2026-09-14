// Android HTTP backend. There is no bundled libcurl/OpenSSL for this
// platform (see Standalone/Android/app/src/main/cpp/CMakeLists.txt's link
// list), so this goes through JNI into java.net.HttpURLConnection instead --
// Android's own HTTPS stack, with the OS's auto-updated CA store, the same
// way WinHTTP is "OS-native" on Windows and libctru's httpc is on 3DS.
//
// The Java-side glue (httpOpen/httpReadChunk/httpClose) lives on
// PolyphaseActivity (Standalone/Android/app/src/main/java/com/you/appname/
// PolyphaseActivity.java) rather than a separate class, matching the
// existing iterateDirFiles/setSystemOrientation pattern of adding instance
// methods callable via JNI on system.mActivity->clazz.
//
// Request/response shape: httpOpen() connects (following redirects itself,
// since HttpURLConnection refuses to cross http<->https) and returns
// {HttpConn connOrNull, Integer status, String errorCode, String errorMessage,
// String finalUrl, String[] headerLines}. The body is then pulled with
// repeated httpReadChunk() calls so this code can check the cancellation
// flag between chunks instead of blocking on the whole transfer at once, and
// enforce HttpRequest::GetMaxBodyBytes() itself (mirroring how the Linux
// backend's write callback enforces it).
#if PLATFORM_ANDROID

#include "Network/Http/Backends/HttpBackend.h"
#include "Log.h"
#include "Engine.h"
#include "System/SystemTypes.h"

#include <jni.h>

#include <string>
#include <vector>

namespace
{
    // Mirrors the attach/detach-per-call convention already used throughout
    // System_Android.cpp (e.g. SYS_OpenDirectory) -- safe here because the
    // HTTP worker thread (HttpClient.cpp's dedicated std::thread) is never
    // already attached, unlike the main/UI thread.
    class ScopedJniThread
    {
    public:
        explicit ScopedJniThread(JavaVM* vm) : mVm(vm)
        {
            if (mVm != nullptr)
            {
                mVm->AttachCurrentThread(&mEnv, nullptr);
            }
        }

        ~ScopedJniThread()
        {
            if (mVm != nullptr)
            {
                mVm->DetachCurrentThread();
            }
        }

        JNIEnv* Env() const { return mEnv; }

    private:
        JavaVM* mVm = nullptr;
        JNIEnv* mEnv = nullptr;
    };

    std::string JStringToStd(JNIEnv* env, jstring s)
    {
        if (s == nullptr)
        {
            return std::string();
        }

        const char* chars = env->GetStringUTFChars(s, nullptr);
        std::string ret(chars != nullptr ? chars : "");
        if (chars != nullptr)
        {
            env->ReleaseStringUTFChars(s, chars);
        }
        return ret;
    }

    HttpError MapErrorCode(const std::string& code)
    {
        if (code == "timeout")    return HttpError::Timeout;
        if (code == "tls")        return HttpError::Tls;
        if (code == "badresponse")return HttpError::BadResponse;
        if (code == "network")    return HttpError::Network;
        return HttpError::Unknown;
    }

    // 64 KiB balances JNI call overhead (one CallObjectMethod + array alloc
    // per chunk) against how promptly a cancelled request notices it.
    const jint kReadChunkSize = 64 * 1024;

    class AndroidHttpBackend : public HttpBackend
    {
    public:
        bool Initialize() override
        {
            ANativeActivity* activity = GetEngineState()->mSystem.mActivity;
            mVm = (activity != nullptr) ? activity->vm : nullptr;
            return mVm != nullptr;
        }

        void Shutdown() override
        {
            mVm = nullptr;
        }

        bool IsAvailable() const override
        {
            return mVm != nullptr;
        }

        const char* GetMissingDependencyMessage() const override
        {
            return "Android activity/JavaVM not available.";
        }

        void PerformRequest(const HttpRequest& request,
                            std::atomic<bool>& cancelFlag,
                            HttpResponse& outResponse) override
        {
            outResponse.SetFinalUrl(request.GetUrl());

            if (mVm == nullptr)
            {
                outResponse.SetError(HttpError::Unavailable, GetMissingDependencyMessage());
                return;
            }

            ScopedJniThread jniThread(mVm);
            JNIEnv* env = jniThread.Env();
            if (env == nullptr)
            {
                outResponse.SetError(HttpError::Unknown, "Failed to attach the HTTP worker thread to the JVM.");
                return;
            }

            ANativeActivity* activity = GetEngineState()->mSystem.mActivity;
            jobject activityObj = activity->clazz;
            jclass activityClass = env->GetObjectClass(activityObj);

            jobjectArray openResult = CallHttpOpen(env, activityObj, activityClass, request);

            if (env->ExceptionCheck())
            {
                env->ExceptionDescribe();
                env->ExceptionClear();
                outResponse.SetError(HttpError::Unknown, "Unhandled Java exception in httpOpen.");
                env->DeleteLocalRef(activityClass);
                return;
            }

            if (openResult == nullptr)
            {
                outResponse.SetError(HttpError::Unknown, "httpOpen returned null.");
                env->DeleteLocalRef(activityClass);
                return;
            }

            jobject connObj    = env->GetObjectArrayElement(openResult, 0);
            jobject statusObj  = env->GetObjectArrayElement(openResult, 1);
            jstring errorCode  = (jstring)env->GetObjectArrayElement(openResult, 2);
            jstring errorMsg   = (jstring)env->GetObjectArrayElement(openResult, 3);
            jstring finalUrl   = (jstring)env->GetObjectArrayElement(openResult, 4);
            jobjectArray headerLines = (jobjectArray)env->GetObjectArrayElement(openResult, 5);
            env->DeleteLocalRef(openResult);

            const std::string errorCodeStd = JStringToStd(env, errorCode);
            const std::string finalUrlStd = JStringToStd(env, finalUrl);
            if (!finalUrlStd.empty())
            {
                outResponse.SetFinalUrl(finalUrlStd);
            }

            if (connObj == nullptr || errorCodeStd != "none")
            {
                outResponse.SetError(MapErrorCode(errorCodeStd), JStringToStd(env, errorMsg));
                DeleteLocalRefsIfNotNull(env, connObj, statusObj, errorCode, errorMsg, finalUrl, headerLines);
                env->DeleteLocalRef(activityClass);
                return;
            }

            outResponse.SetStatus(ReadIntegerAndDelete(env, statusObj));
            CopyHeaders(env, headerLines, outResponse);
            DeleteLocalRefsIfNotNull(env, errorCode, errorMsg, finalUrl, headerLines);

            StreamBody(env, activityObj, activityClass, connObj, request, cancelFlag, outResponse);

            env->DeleteLocalRef(connObj);
            env->DeleteLocalRef(activityClass);
        }

    private:
        static jobjectArray CallHttpOpen(JNIEnv* env, jobject activityObj, jclass activityClass,
                                         const HttpRequest& request)
        {
            jmethodID openMethod = env->GetMethodID(activityClass, "httpOpen",
                "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[BIIZ)[Ljava/lang/Object;");

            jclass stringClass = env->FindClass("java/lang/String");
            const HttpHeaderMap& headers = request.GetHeaders();
            jobjectArray headerArray = env->NewObjectArray((jsize)headers.size(), stringClass, nullptr);
            jsize headerIdx = 0;
            for (const auto& kv : headers)
            {
                jstring line = env->NewStringUTF((kv.first + ": " + kv.second).c_str());
                env->SetObjectArrayElement(headerArray, headerIdx++, line);
                env->DeleteLocalRef(line);
            }

            const std::vector<uint8_t>& body = request.GetBody();
            jbyteArray bodyArray = env->NewByteArray((jsize)body.size());
            if (!body.empty())
            {
                env->SetByteArrayRegion(bodyArray, 0, (jsize)body.size(), (const jbyte*)body.data());
            }

            jstring verb = env->NewStringUTF(HttpVerbToString(request.GetVerb()));
            jstring url = env->NewStringUTF(request.GetUrl().c_str());

            jobjectArray result = (jobjectArray)env->CallObjectMethod(activityObj, openMethod,
                verb, url, headerArray, bodyArray,
                (jint)request.GetTimeoutMs(), (jint)request.GetMaxRedirects(), (jboolean)request.GetVerifySsl());

            env->DeleteLocalRef(verb);
            env->DeleteLocalRef(url);
            env->DeleteLocalRef(headerArray);
            env->DeleteLocalRef(bodyArray);
            env->DeleteLocalRef(stringClass);

            return result;
        }

        static int ReadIntegerAndDelete(JNIEnv* env, jobject integerObj)
        {
            jclass integerClass = env->GetObjectClass(integerObj);
            jmethodID intValue = env->GetMethodID(integerClass, "intValue", "()I");
            int value = env->CallIntMethod(integerObj, intValue);
            env->DeleteLocalRef(integerClass);
            env->DeleteLocalRef(integerObj);
            return value;
        }

        static void CopyHeaders(JNIEnv* env, jobjectArray headerLines, HttpResponse& outResponse)
        {
            if (headerLines == nullptr)
            {
                return;
            }

            HttpHeaderMap& outHeaders = outResponse.MutableHeaders();
            const jsize count = env->GetArrayLength(headerLines);
            for (jsize i = 0; i < count; ++i)
            {
                jstring lineObj = (jstring)env->GetObjectArrayElement(headerLines, i);
                const std::string line = JStringToStd(env, lineObj);
                env->DeleteLocalRef(lineObj);

                const size_t colon = line.find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }

                const std::string key = line.substr(0, colon);
                size_t valueStart = colon + 1;
                if (valueStart < line.size() && line[valueStart] == ' ')
                {
                    ++valueStart;
                }
                const std::string value = line.substr(valueStart);

                // Merge duplicate header names with ", " -- matches the Linux
                // backend's HeaderCb (curl reports repeated headers the same
                // way, e.g. multiple Set-Cookie lines).
                auto it = outHeaders.find(key);
                if (it != outHeaders.end())
                {
                    it->second += ", " + value;
                }
                else
                {
                    outHeaders[key] = value;
                }
            }
        }

        static void StreamBody(JNIEnv* env, jobject activityObj, jclass activityClass, jobject connObj,
                               const HttpRequest& request, std::atomic<bool>& cancelFlag,
                               HttpResponse& outResponse)
        {
            jmethodID readChunkMethod = env->GetMethodID(activityClass, "httpReadChunk", "(Ljava/lang/Object;I)[B");
            jmethodID closeMethod = env->GetMethodID(activityClass, "httpClose", "(Ljava/lang/Object;)V");

            const int64_t maxBodyBytes = request.GetMaxBodyBytes();
            int64_t totalRead = 0;
            bool tooLarge = false;
            bool cancelled = false;
            bool readError = false;

            while (true)
            {
                if (cancelFlag.load(std::memory_order_acquire))
                {
                    cancelled = true;
                    break;
                }

                jbyteArray chunk = (jbyteArray)env->CallObjectMethod(activityObj, readChunkMethod, connObj, kReadChunkSize);

                if (env->ExceptionCheck())
                {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    readError = true;
                    break;
                }

                if (chunk == nullptr)
                {
                    readError = true;
                    break;
                }

                const jsize chunkLen = env->GetArrayLength(chunk);
                if (chunkLen == 0)
                {
                    env->DeleteLocalRef(chunk);
                    break;
                }

                if (maxBodyBytes > 0 && totalRead + chunkLen > maxBodyBytes)
                {
                    tooLarge = true;
                    env->DeleteLocalRef(chunk);
                    break;
                }

                std::vector<uint8_t>& outBody = outResponse.MutableBody();
                const size_t oldSize = outBody.size();
                outBody.resize(oldSize + chunkLen);
                env->GetByteArrayRegion(chunk, 0, chunkLen, (jbyte*)(outBody.data() + oldSize));
                env->DeleteLocalRef(chunk);

                totalRead += chunkLen;
            }

            env->CallVoidMethod(activityObj, closeMethod, connObj);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
            }

            if (tooLarge)
            {
                outResponse.SetError(HttpError::TooLarge, "Response body exceeded MaxBodyBytes.");
            }
            else if (cancelled)
            {
                outResponse.SetError(HttpError::Cancelled, "Request cancelled.");
            }
            else if (readError)
            {
                outResponse.SetError(HttpError::Network, "Error reading response body.");
            }
        }

        template<typename... Refs>
        static void DeleteLocalRefsIfNotNull(JNIEnv* env, Refs... refs)
        {
            (DeleteOne(env, refs), ...);
        }

        static void DeleteOne(JNIEnv* env, jobject ref)
        {
            if (ref != nullptr)
            {
                env->DeleteLocalRef(ref);
            }
        }

        JavaVM* mVm = nullptr;
    };
}

std::unique_ptr<HttpBackend> CreatePlatformHttpBackend()
{
    return std::unique_ptr<HttpBackend>(new AndroidHttpBackend());
}

#endif // PLATFORM_ANDROID
