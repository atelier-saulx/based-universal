#include <based.h>
#include <napi.h>
#include <iostream>
#include <map>

using namespace Napi;

std::map<int, Napi::Function> fnStore;
int client_id;

Napi::Value GetService(const Napi::CallbackInfo& info) {
    Napi::Env napi_env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(napi_env, "Wrong number of arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }
    if (!info[1].IsObject()) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
        return napi_env.Null();
    }

    int client_id = info[0].As<Napi::Number>().Int32Value();
    Napi::Object opts = info[1].As<Napi::Object>();

    if (!opts.Has("cluster") || !opts.Has("org") || !opts.Has("project") || !opts.Has("env") ||
        !opts.Has("name")) {
        Napi::TypeError::New(napi_env, "Wrong arguments").ThrowAsJavaScriptException();
    }

    std::string cluster = opts.Get("cluster").ToString().Utf8Value();
    std::string org = opts.Get("org").ToString().Utf8Value();
    std::string project = opts.Get("project").ToString().Utf8Value();
    std::string env = opts.Get("env").ToString().Utf8Value();
    std::string name = opts.Get("name").ToString().Utf8Value();

    bool optionalKey = true;
    if (opts.Has("optionalKey")) optionalKey = opts.Get("optionalKey").ToBoolean();

    char* res;
    if (opts.Has("key")) {
        std::string key = opts.Get("key").ToString().Utf8Value();
        res = Based__get_service(client_id, cluster.data(), org.data(), project.data(), env.data(),
                                 name.data(), key.data(), optionalKey);
    } else {
        res = Based__get_service(client_id, cluster.data(), org.data(), project.data(), env.data(),
                                 name.data(), (char*)"", optionalKey);
    }

    return Napi::String::New(napi_env, res);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    client_id = Based__new_client();

    // exports.Set(Napi::String::New(env, "CreateClient"), Napi::Function::New(env, CreateClient));
    // exports.Set(Napi::String::New(env, "DeleteClient"), Napi::Function::New(env, DeleteClient));
    exports.Set(Napi::String::New(env, "GetService"), Napi::Function::New(env, GetService));
    return exports;
}

NODE_API_MODULE(addon, Init)
