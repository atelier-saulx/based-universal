#pragma once

#include <string>

class NativeBasedObject {
   private:
    int m_client_id;

   public:
    NativeBasedObject();
    std::string get_service(std::string cluster,
                            std::string org,
                            std::string project,
                            std::string env,
                            std::string name,
                            std::string key,
                            bool optional_key);
    void _connect_to_url(std::string url);
    void connect(std::string cluster,
                 std::string org,
                 std::string project,
                 std::string env,
                 std::string name,
                 std::string key,
                 bool optional_key);

    void disconnect();
    int observe(std::string name,
                std::string payload,
                /**
                 * Callback that the observable will trigger.
                 */
                void (*cb)(const char* /*data*/,
                           uint64_t,
                           const char* /*error*/,
                           int /*sub_id*/));
    int get(std::string name,
            std::string payload,
            void (*cb)(const char* /*data*/, const char* /*error*/, int /*sub_id*/));
    void unobserve(int sub_id);
    int function(std::string name,
                 std::string payload,
                 void (*cb)(const char* /*data*/, const char* /*error*/, int /*sub_id*/));
    void auth(std::string state, void (*cb)(const char*));
};
