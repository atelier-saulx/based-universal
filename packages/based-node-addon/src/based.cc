#include "based.h"
#include <napi.h>
#include <iostream>
#include <map>

struct AuthCallbackData {
    std::string state;
};
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

std::map<int, Napi::ThreadSafeFunction> fnStore;
Napi::ThreadSafeFunction authTsfn;
std::map<int, Napi::ThreadSafeFunction> obsStore;

void functionCb(const char* data, const char* error, int id) {
    if (fnStore.find(id) == fnStore.end()) {
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

    fnStore.at(id).Napi::ThreadSafeFunction::BlockingCall(d, callback);
    fnStore.at(id).Release();
    fnStore.erase(id);
}

void authCb(const char* state) {
    if (!authTsfn) {
        return;
    }

    auto callback = [](Napi::Env env, Napi::Function jsCallback, AuthCallbackData* data) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.

        jsCallback.Call({
            Napi::String::New(env, data->state),
        });

        delete data;
    };
    AuthCallbackData* d = new AuthCallbackData{state};

    authTsfn.BlockingCall(d, callback);
    authTsfn.Release();
    authTsfn = NULL;
}

void observeCb(const char* data, uint64_t checksum, const char* error, int id) {
    if (obsStore.find(id) == obsStore.end()) {
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

    obsStore.at(id).BlockingCall(d, callback);
}

Napi::Value NewClient(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    auto clientId = Based__new_client();

    return Napi::Number::New(env, clientId);
}

Napi::Value DeleteClient(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int clientId = info[0].As<Napi::Number>().Int32Value();
    Based__delete_client(clientId);

    return env.Undefined();
}

Napi::Value ConnectToUrl(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int clientId = info[0].As<Napi::Number>().Int32Value();
    std::string url = info[1].As<Napi::String>().Utf8Value();

    Based__connect_to_url(clientId, url.data());

    return env.Undefined();
}

Napi::Value Disconnect(const Napi::CallbackInfo& info) {
    /*
      Disconnect: (clientId: number) => void
    */
    Napi::Env env = info.Env();

    int clientId = info[0].As<Napi::Number>().Int32Value();

    Based__disconnect(clientId);

    return env.Undefined();
}

Napi::Value Observe(const Napi::CallbackInfo& info) {
    /*
    Observe: (
      clientId: number,
      name: string,
      payload: any,
      cb: (data: any, checksum: number, err: any, obsId: number) => void
    ) => number
    */

    Napi::Env env = info.Env();

    int clientId = info[0].As<Napi::Number>().Int32Value();
    std::string name = info[1].As<Napi::String>().Utf8Value();
    std::string payload = "";
    if (info[2].IsString()) {
        payload = info[2].As<Napi::String>().Utf8Value();
    } else if (info[2].IsNumber()) {
        payload = info[2].As<Napi::Number>().ToString();
    }

    int id = Based__observe(clientId, name.data(), payload.data(), observeCb);

    auto fn = info[3].As<Napi::Function>();
    obsStore[id] = Napi::ThreadSafeFunction::New(env, fn, "callback-observe", 0, 2);

    return Napi::Number::New(env, id);
}

Napi::Value Unobserve(const Napi::CallbackInfo& info) {
    /*
        Unobserve: (clientId: number, subId: number) => void
    */

    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as first argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as second argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    int clientId = info[0].As<Napi::Number>().Int32Value();
    int subId = info[1].As<Napi::Number>().Int32Value();

    Based__unobserve(clientId, subId);

    obsStore.at(subId).Release();
    obsStore.erase(subId);

    return env.Undefined();
}
Napi::Value Get(const Napi::CallbackInfo& info) {
    /*
    Get: (
      clientId: number,
      name: string,
      payload: any,
      cb: (data: any, err: any, subId: any) => void
    ) => void
    */

    Napi::Env env = info.Env();

    if (info.Length() != 4) {
        Napi::TypeError::New(env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as first argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsString()) {
        Napi::TypeError::New(env, "Expected string as second argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[2].IsString() && !info[2].IsNull() && !info[2].IsUndefined()) {
        Napi::TypeError::New(
            env, "Expected string as third argument (use stringify if passing object)")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[3].IsFunction()) {
        Napi::TypeError::New(env, "Expected function as fourth argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    int clientId = info[0].As<Napi::Number>().Int32Value();
    std::string name = info[1].As<Napi::String>().Utf8Value();
    std::string payload = "";
    if (info[2].IsString()) {
        payload = info[2].As<Napi::String>().Utf8Value();
    } else if (info[2].IsNumber()) {
        payload = info[2].As<Napi::Number>().ToString();
    }

    int id = Based__get(clientId, name.data(), payload.data(), functionCb);

    auto fn = info[3].As<Napi::Function>();
    fnStore[id] = Napi::ThreadSafeFunction::New(env, fn, "callback-get", 0, 2);

    return env.Undefined();
}

Napi::Value Call(const Napi::CallbackInfo& info) {
    /**
    Call: (
        clientId: number,
        name: string,
        payload: any,
        cb: (data: any, err: any, reqId: number) => void
    ) => void
     */

    Napi::Env env = info.Env();

    if (info.Length() != 4) {
        Napi::TypeError::New(env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as first argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsString()) {
        Napi::TypeError::New(env, "Expected string as second argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    // if (!info[2].IsString()) {
    //     Napi::TypeError::New(
    //         env, "Expected string as third argument (use stringify if passing object)")
    //         .ThrowAsJavaScriptException();
    //     return env.Null();
    // }
    if (!info[3].IsFunction()) {
        Napi::TypeError::New(env, "Expected function as fourth argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    int clientId = info[0].As<Napi::Number>().Int32Value();
    std::string name = info[1].As<Napi::String>().Utf8Value();
    std::string payload = "";
    if (info[2].IsString()) {
        payload = info[2].As<Napi::String>().Utf8Value();
    } else if (info[2].IsNumber()) {
        payload = info[2].As<Napi::Number>().ToString();
    }

    int id = Based__call(clientId, name.data(), payload.data(), functionCb);

    auto fn = info[3].As<Napi::Function>();

    fnStore[id] = Napi::ThreadSafeFunction::New(env, fn, "callback", 0, 2);

    return env.Undefined();
}

Napi::Value SetAuthState(const Napi::CallbackInfo& info) {
    /**
    SetAuthState: (
      clientId: number,
      state: AuthState,
      cb: (state: AuthState) => void
    ) => void
     */

    Napi::Env env = info.Env();

    if (info.Length() != 3) {
        Napi::TypeError::New(env, "Wrong number of arguments")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "Expected number as first argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[1].IsString()) {
        Napi::TypeError::New(env, "Expected string as second argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    if (!info[2].IsFunction()) {
        Napi::TypeError::New(env, "Expected function as third argument")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    auto fn = info[2].As<Napi::Function>();

    int clientId = info[0].As<Napi::Number>().Int32Value();
    std::string state = info[1].As<Napi::String>().Utf8Value();

    Based__set_auth_state(clientId, state.data(), authCb);
    authTsfn = Napi::ThreadSafeFunction::New(env, fn, "auth-callback", 0, 2);

    return env.Undefined();
}

Napi::Value GetAuthState(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int clientId = info[0].As<Napi::Number>().Int32Value();
    auto state = Based__get_auth_state(clientId);

    return Napi::String::New(env, state);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // clang-format off
    exports.Set(Napi::String::New(env, "NewClient"), Napi::Function::New(env, NewClient));
    exports.Set(Napi::String::New(env, "ConnectToUrl"), Napi::Function::New(env, ConnectToUrl));
    exports.Set(Napi::String::New(env, "Observe"), Napi::Function::New(env, Observe));
    exports.Set(Napi::String::New(env, "Unobserve"), Napi::Function::New(env, Unobserve));
    exports.Set(Napi::String::New(env, "Get"), Napi::Function::New(env, Get));
    exports.Set(Napi::String::New(env, "Call"), Napi::Function::New(env, Call));
    exports.Set(Napi::String::New(env, "SetAuthState"), Napi::Function::New(env, SetAuthState));
    exports.Set(Napi::String::New(env, "Disconnect"), Napi::Function::New(env, Disconnect));
    exports.Set(Napi::String::New(env, "DeleteClient"), Napi::Function::New(env, DeleteClient));
    exports.Set(Napi::String::New(env, "GetAuthState"), Napi::Function::New(env, GetAuthState));
    // clang-format on
    return exports;
}

NODE_API_MODULE(hello, Init)