#include "based.h"
#include "basedclient.hpp"

#include <map>

std::map<based_id, BasedClient*> clients;
based_id idx = 0;
char get_service_buf[1024];
char auth_state_buf[1048576];

extern "C" based_id Based__new_client() {
    BasedClient* cl = new BasedClient;
    idx++;

    if (clients.find(idx) != clients.end()) {
        throw std::runtime_error("Ran out of client indices");
    }

    clients[idx] = cl;

    return idx;
}

extern "C" void Based__delete_client(based_id id) {
    if (clients.find(id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    delete clients.at(id);
    clients.erase(id);
}

extern "C" char* Based__get_service(based_id client_id,
                                    char* cluster,
                                    char* org,
                                    char* project,
                                    char* env,
                                    char* name,
                                    char* key,
                                    bool optional_key,
                                    bool html) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return (char*)"";
    }
    auto cl = clients.at(client_id);
    auto res = cl->discover_service(cluster, org, project, env, name, key, optional_key, html);
    memset(get_service_buf, 0, sizeof get_service_buf);
    strncpy(get_service_buf, res.c_str(), res.length());
    return get_service_buf;
}

extern "C" void Based__connect_to_url(based_id client_id, char* url) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->_connect_to_url(url);
}

extern "C" void Based__connect(based_id client_id,
                               char* cluster,
                               char* org,
                               char* project,
                               char* env,
                               char* name,
                               char* key,
                               bool optional_key) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->connect(cluster, org, project, env, name, key, optional_key);
}

extern "C" void Based__disconnect(based_id client_id) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->disconnect();
}

extern "C" int Based__observe(based_id client_id,
                              char* name,
                              char* payload,
                              /**
                               * Callback that the observable will trigger.
                               */
                              void (*cb)(const char*, uint64_t, const char*, int)) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return -1;
    }
    auto cl = clients.at(client_id);
    return cl->observe(name, payload, cb);
}

extern "C" int Based__get(based_id client_id,
                          char* name,
                          char* payload,
                          void (*cb)(const char*, const char*, int)) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return -1;
    }
    auto cl = clients.at(client_id);
    return cl->get(name, payload, cb);
}

extern "C" void Based__unobserve(based_id client_id, int sub_id) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->unobserve(sub_id);
}

extern "C" int Based__call(based_id client_id,
                           char* name,
                           char* payload,
                           void (*cb)(const char*, const char*, int)) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return -1;
    }
    auto cl = clients.at(client_id);
    return cl->call(name, payload, cb);
}

extern "C" void Based__set_auth_state(based_id client_id, char* state, void (*cb)(const char*)) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->set_auth_state(state, cb);
}

extern "C" char* Based__get_auth_state(based_id client_id) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return (char*)"{}";
    }
    auto cl = clients.at(client_id);
    auto state = cl->get_auth_state();
    memset(auth_state_buf, 0, sizeof auth_state_buf);
    strncpy(auth_state_buf, state.c_str(), state.length());
    return auth_state_buf;
}

extern "C" int Based__channel_subscribe(based_id client_id,
                                        char* name,
                                        char* payload,
                                        void (*cb)(const char* /* Data */,
                                                   const char* /* Error */,
                                                   int /*request_id*/)) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return -1;
    }
    auto cl = clients.at(client_id);
    cl->channel_subscribe(name, payload, cb);
}

extern "C" void Based__channel_unsubscribe(based_id client_id, int id) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->channel_unsubscribe(id);
}

extern "C" void Based__channel_publish(based_id client_id,
                                       char* name,
                                       char* payload,
                                       char* message) {
    if (clients.find(client_id) == clients.end()) {
        std::cerr << "No such id found" << std::endl;
        return;
    }
    auto cl = clients.at(client_id);
    cl->channel_publish(name, payload, message);
}