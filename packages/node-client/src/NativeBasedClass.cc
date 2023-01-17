#include "NativeBasedClass.h"
#include "based.h"

NativeBasedObject::NativeBasedObject() {
    m_client_id = Based__new_client();
}

NativeBasedObject::~NativeBasedObject() {
    if (m_client_id) {
        Based__delete_client(m_client_id);
    }
}
std::string NativeBasedObject::get_service(std::string cluster,
                                           std::string org,
                                           std::string project,
                                           std::string env,
                                           std::string name,
                                           std::string key,
                                           bool optional_key) {
    char* result =
        Based__get_service(m_client_id, cluster.data(), org.data(), project.data(),
                           env.data(), name.data(), key.data(), optional_key);
    std::string result_str(result);
    free(result);
    return result_str;
}

void NativeBasedObject::_connect_to_url(std::string url) {
    Based__connect_to_url(m_client_id, url.data());
}

void NativeBasedObject::connect(std::string cluster,
                                std::string org,
                                std::string project,
                                std::string env,
                                std::string name,
                                std::string key,
                                bool optional_key) {
    Based__connect(m_client_id, cluster.data(), org.data(), project.data(), env.data(),
                   name.data(), key.data(), optional_key);
}

void NativeBasedObject::disconnect() {
    Based__disconnect(m_client_id);
}

int NativeBasedObject::observe(
    std::string name,
    std::string payload,
    void (*cb)(const char* /*data*/, uint64_t, const char* /*error*/, int /*sub_id*/)) {
    return Based__observe(m_client_id, name.data(), payload.data(), cb);
}

int NativeBasedObject::get(std::string name,
                           std::string payload,
                           void (*cb)(const char* /*data*/,
                                      const char* /*error*/,
                                      int /*sub_id*/)) {
    return Based__get(m_client_id, name.data(), payload.data(), cb);
}

void NativeBasedObject::unobserve(int sub_id) {
    Based__unobserve(m_client_id, sub_id);
}

int NativeBasedObject::function(std::string name,
                                std::string payload,
                                void (*cb)(const char* /*data*/,
                                           const char* /*error*/,
                                           int /*sub_id*/)) {
    return Based__function(m_client_id, name.data(), payload.data(), cb);
}

void NativeBasedObject::auth(std::string state, void (*cb)(const char*)) {
    Based__auth(m_client_id, state.data(), cb);
}
