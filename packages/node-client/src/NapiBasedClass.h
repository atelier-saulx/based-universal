#pragma once

#include <napi.h>
#include <map>

class NapiBasedObject : public Napi::ObjectWrap<NapiBasedObject> {
   public:
    static Napi::Object GetClass(Napi::Env env);

   private:
    Napi::Value GetService(const Napi::CallbackInfo& info);
    Napi::Value ConnectToUrl(const Napi::CallbackInfo& info);
    Napi::Value Connect(const Napi::CallbackInfo& info);
    Napi::Value Disconnect(const Napi::CallbackInfo& info);
    Napi::Value Observe(const Napi::CallbackInfo& info);
    Napi::Value Get(const Napi::CallbackInfo& info);
    Napi::Value Unobserve(const Napi::CallbackInfo& info);
    Napi::Value Function(const Napi::CallbackInfo& info);
    Napi::Value Auth(const Napi::CallbackInfo& info);

    void callNapiFunction(const char* data, const char* error, int sub_id);
    void callNapiObservableFunction(const char* data,
                                    uint64_t checksum,
                                    const char* error,
                                    int id);

    NativeBasedObject m_native_object;
    std::map<int, Napi::ThreadSafeFunction> m_fn_store;
    std::map<int, Napi::ThreadSafeFunction> m_obs_store;
};