#include "NapiBasedClass.h"

#include <iostream>
#include "NativeBasedClass.h"

struct FunctionCallbackData {
    std::string data;
    std::string error;
    int id;
};
struct ObserveCallbackData {
    std::string data;
    uint64_t checksum;
    std::string error;
    int id;
};

void NapiBasedObject::callNapiFunction(const char* data, const char* error, int id) {
    if (m_fn_store.find(id) == m_fn_store.end()) {
        return;
    }
    auto callback = [](Napi::Env env, Napi::Function jsCallback,
                       FunctionCallbackData* data) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.

        jsCallback.Call({Napi::String::New(env, data->data),
                         Napi::String::New(env, data->error),
                         Napi::Number::New(env, data->id)});

        delete data;
    };
    FunctionCallbackData* d = new FunctionCallbackData{data, error, id};

    m_fn_store.at(id).BlockingCall(d, callback);
    m_fn_store.at(id).Release();
    m_fn_store.erase(id);
}

void NapiBasedObject::callNapiObservableFunction(const char* data,
                                                 uint64_t checksum,
                                                 const char* error,
                                                 int id) {
    if (m_obs_store.find(id) == m_obs_store.end()) {
        return;
    }
    auto callback = [](Napi::Env env, Napi::Function jsCallback,
                       ObserveCallbackData* data) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.

        jsCallback.Call(
            {Napi::String::New(env, data->data), Napi::Number::New(env, data->checksum),
             Napi::String::New(env, data->error), Napi::Number::New(env, data->id)});

        delete data;
    };
    ObserveCallbackData* d = new ObserveCallbackData{data, checksum, error, id};

    std::cout << "callNapiObservableFunction, id: " << id << std::endl;
    m_obs_store.at(id).BlockingCall(d, callback);
}

Napi::Value NapiBasedObject::GetService(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();
    if (info.Length() != 7) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }
    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsString() ||
        !info[3].IsString() || !info[4].IsString() || !info[5].IsString() ||
        !info[6].IsBoolean()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }
    std::string cluster = info[0].As<Napi::String>();
    std::string org = info[1].As<Napi::String>();
    std::string project = info[2].As<Napi::String>();
    std::string env = info[3].As<Napi::String>();
    std::string name = info[4].As<Napi::String>();
    std::string key = info[5].As<Napi::String>();
    bool optional_key = info[6].As<Napi::Boolean>();
    std::string service =
        m_native_object.get_service(cluster, org, project, env, name, key, optional_key);

    return Napi::String::New(napi_env, service);
}

Napi::Value NapiBasedObject::ConnectToUrl(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();
    if (info.Length() != 1) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    if (!info[0].IsString()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    std::string url = info[0].As<Napi::String>();
    m_native_object._connect_to_url(url);
    return napi_env.Undefined();
}

Napi::Value NapiBasedObject::Connect(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();
    if (info.Length() != 7) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsString() ||
        !info[3].IsString() || !info[4].IsString() || !info[5].IsString() ||
        !info[6].IsBoolean()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    std::string cluster = info[0].As<Napi::String>();
    std::string org = info[1].As<Napi::String>();
    std::string project = info[2].As<Napi::String>();
    std::string env = info[3].As<Napi::String>();
    std::string name = info[4].As<Napi::String>();
    std::string key = info[5].As<Napi::String>();
    bool optional_key = info[6].As<Napi::Boolean>();
    m_native_object.connect(cluster, org, project, env, name, key, optional_key);
    return napi_env.Undefined();
}

Napi::Value NapiBasedObject::Disconnect(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();
    if (info.Length() != 0) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    m_native_object.disconnect();
    return napi_env.Undefined();
}

Napi::Value NapiBasedObject::Observe(const Napi::CallbackInfo& info) {
    // js = based.function("counter", "", (data, error, id) => {})
    Napi::Env napi_env = info.Env();
    if (info.Length() != 3) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsFunction()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Undefined();
    }
    std::string name = info[0].As<Napi::String>();
    std::string payload = info[1].As<Napi::String>();
    Napi::Function callback = info[2].As<Napi::Function>();

    int (NapiBasedObject::*fn)(const char*, uint64_t, const char*, int) = NULL;

    // int id = m_native_object.observe(name, payload, fn);
}

Napi::Object NapiBasedObject::GetClass(Napi::Env env) {
    return DefineClass(env, "NapiBasedObject",
                       {InstanceMethod("GetService", &NapiBasedObject::GetService),
                        InstanceMethod("ConnectToUrl", &NapiBasedObject::ConnectToUrl),
                        InstanceMethod("Connect", &NapiBasedObject::Connect),
                        InstanceMethod("Disconnect", &NapiBasedObject::Disconnect),
                        InstanceMethod("Observe", &NapiBasedObject::Observe),
                        InstanceMethod("Get", &NapiBasedObject::Get),
                        InstanceMethod("Unobserve", &NapiBasedObject::Unobserve),
                        InstanceMethod("Function", &NapiBasedObject::Function),
                        InstanceMethod("Auth", &NapiBasedObject::Auth)});
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("BasedClass", NapiBasedObject::GetClass(env));
    return exports;
}

NODE_API_MODULE(addon, Init)
