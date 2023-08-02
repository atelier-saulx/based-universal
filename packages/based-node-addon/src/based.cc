#include "based.h"
#include <napi.h>

/**
  NewClient,
  Connect,
  ConnectToUrl,
  Disconnect,
  Call,
  SetAuthstate,
} = require('../build/Release/based-node-addon')
*/

Napi::Value NewClient(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    auto clientId = Based__new_client();

    return Napi::Number::New(env, clientId);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "NewClient"), Napi::Function::New(env, NewClient));
    return exports;
}

NODE_API_MODULE(hello, Init)