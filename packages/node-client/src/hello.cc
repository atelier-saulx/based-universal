#include <based.h>
#include <napi.h>
#include <iostream>
#include <map>

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

using namespace Napi;

std::map<int, Napi::ThreadSafeFunction> fnStore;
std::map<int, Napi::ThreadSafeFunction> obsStore;
int client_id;

void callNapiFunction(const char* data, const char* error, int id) {
    if (fnStore.find(id) == fnStore.end()) {
        return;
    }
    auto callback = [](Napi::Env env, Function jsCallback, FunctionCallbackData* data) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.

        jsCallback.Call({Napi::String::New(env, data->data),
                         Napi::String::New(env, data->error),
                         Napi::Number::New(env, data->id)});

        delete data;
    };
    FunctionCallbackData* d = new FunctionCallbackData{data, error, id};

    fnStore.at(id).BlockingCall(d, callback);
    fnStore.at(id).Release();
    fnStore.erase(id);
}

void callNapiObservableFunction(const char* data,
                                uint64_t checksum,
                                const char* error,
                                int id) {
    if (obsStore.find(id) == obsStore.end()) {
        return;
    }
    auto callback = [](Napi::Env env, Function jsCallback, ObserveCallbackData* data) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.

        jsCallback.Call(
            {Napi::String::New(env, data->data), Napi::Number::New(env, data->checksum),
             Napi::String::New(env, data->error), Napi::Number::New(env, data->id)});

        delete data;
    };
    ObserveCallbackData* d = new ObserveCallbackData{data, checksum, error, id};

    obsStore.at(id).BlockingCall(d, callback);
    // obsStore.at(id).Release();
    // obsStore.erase(id);
}

Napi::Value BasedFunction(const Napi::CallbackInfo& info) {
    // js = based.function("counter", "", (data, error, id) => {})
    Napi::Env napi_env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsFunction()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string payload = info[1].As<Napi::String>().Utf8Value();

    int id = Based__function(client_id, name.data(), payload.data(), callNapiFunction);

    auto fn = info[2].As<Napi::Function>();

    fnStore[id] = Napi::ThreadSafeFunction::New(
        napi_env,
        fn,                 // JavaScript function called asynchronously
        "callback",         // Name
        0,                  // Unlimited queue
        1,                  // Only one thread will use this initially
        [](Napi::Env) {});  // Finalizer used to clean threads up

    return napi_env.Undefined();
}

Napi::Value Observe(const Napi::CallbackInfo& info) {
    // js = based.function("counter", "", (data, error, id) => {})
    Napi::Env napi_env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsFunction()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string payload = info[1].As<Napi::String>().Utf8Value();

    int id = Based__observe(client_id, name.data(), payload.data(),
                            callNapiObservableFunction);

    auto fn = info[2].As<Napi::Function>();

    obsStore[id] = Napi::ThreadSafeFunction::New(
        napi_env,
        fn,                 // JavaScript function called asynchronously
        "callback",         // Name
        0,                  // Unlimited queue
        1,                  // Only one thread will use this initially
        [](Napi::Env) {});  // Finalizer used to clean threads up

    return napi_env.Undefined();
}

Napi::Value Unobserve(const Napi::CallbackInfo& info) {
    // js = based.function("counter", "", (data, error, id) => {})
    Napi::Env napi_env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(napi_env, "Expected number as first argument")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    int id = info[0].As<Napi::Number>().Int32Value();

    Based__unobserve(client_id, id);

    obsStore.at(id).Release();
    obsStore.erase(id);

    return napi_env.Undefined();
}

Napi::Value ConnectToUrl(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    std::string url = info[0].As<Napi::String>().Utf8Value();

    Based__connect_to_url(client_id, url.data());

    return napi_env.Undefined();
}

Napi::Value Get(const Napi::CallbackInfo& info) {
    // js = based.function("counter", "", (data, error, id) => {})
    Napi::Env napi_env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsString() || !info[1].IsString() || !info[2].IsFunction()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string payload = info[1].As<Napi::String>().Utf8Value();

    int id = Based__get(client_id, name.data(), payload.data(), callNapiFunction);

    auto fn = info[2].As<Napi::Function>();

    fnStore[id] = Napi::ThreadSafeFunction::New(
        napi_env,
        fn,                 // JavaScript function called asynchronously
        "callback",         // Name
        0,                  // Unlimited queue
        1,                  // Only one thread will use this initially
        [](Napi::Env) {});  // Finalizer used to clean threads up

    return napi_env.Undefined();
}

Napi::Value Connect(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsObject()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    Napi::Object opts = info[0].As<Napi::Object>();

    if (!opts.Has("cluster") || !opts.Has("org") || !opts.Has("project") ||
        !opts.Has("env")) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
    }

    std::string cluster = opts.Get("cluster").ToString().Utf8Value();
    std::string org = opts.Get("org").ToString().Utf8Value();
    std::string project = opts.Get("project").ToString().Utf8Value();
    std::string env = opts.Get("env").ToString().Utf8Value();

    Based__connect(client_id, cluster.data(), org.data(), project.data(), env.data(),
                   (char*)"@based/edge", (char*)"", false);

    return napi_env.Undefined();
}

Napi::Value GetService(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();

    std::cerr << "Broken atm, fix later" << std::endl;
    return napi_env.Undefined();

    if (info.Length() < 1) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsObject()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    // int client_id = info[0].As<Napi::Number>().Int32Value();
    Napi::Object opts = info[0].As<Napi::Object>();

    if (!opts.Has("cluster") || !opts.Has("org") || !opts.Has("project") ||
        !opts.Has("env") || !opts.Has("name")) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
    }

    std::string cluster = opts.Get("cluster").ToString().Utf8Value();
    std::string org = opts.Get("org").ToString().Utf8Value();
    std::string project = opts.Get("project").ToString().Utf8Value();
    std::string env = opts.Get("env").ToString().Utf8Value();
    std::string name = opts.Get("name").ToString().Utf8Value();

    bool optionalKey = true;
    if (opts.Has("optionalKey")) optionalKey = opts.Get("optionalKey").ToBoolean();
    try {
        // std::string res;
        if (opts.Has("key")) {
            std::string key = opts.Get("key").ToString().Utf8Value();
            std::string res =
                Based__get_service(client_id, cluster.data(), org.data(), project.data(),
                                   env.data(), name.data(), key.data(), optionalKey);
            return Napi::String::New(napi_env, res);
        } else {
            std::cout << "client_id = " << client_id << ", cluster = " << cluster
                      << ", org = " << org << ", project = " << project
                      << ", env = " << env << ", name = " << name << std::endl;

            std::string res =
                Based__get_service(client_id, cluster.data(), org.data(), project.data(),
                                   env.data(), name.data(), (char*)"", optionalKey);
            return Napi::String::New(napi_env, res);
        }
        // return Napi::String::New(napi_env, res);
    } catch (...) {
        std::cerr << "yes errored here" << std::endl;
        return napi_env.Null();
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    client_id = Based__new_client();

    exports.Set("GetService", Napi::Function::New(env, GetService));
    exports.Set("Connect", Napi::Function::New(env, Connect));
    exports.Set("Function", Napi::Function::New(env, BasedFunction));
    exports.Set("ConnectToUrl", Napi::Function::New(env, ConnectToUrl));
    exports.Set("Observe", Napi::Function::New(env, Observe));
    exports.Set("Unobserve", Napi::Function::New(env, Unobserve));
    exports.Set("Get", Napi::Function::New(env, Get));
    return exports;
}

NODE_API_MODULE(addon, Init)
