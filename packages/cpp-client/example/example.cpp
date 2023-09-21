#include <iostream>
#include <list>
#include <map>
#include <string>

#include <sstream>

#include "based.h"

int client = Based__new_client();

std::map<int, std::function<void(std::string, std::string)>> observeFns;
std::map<int, std::function<void(std::string, std::string)>> callFns;

void based_auth_cb(const char* data) {
    if (strlen(data) > 0) {
        std::cout << "[AUTH_CB] " << data << std::endl;
    }
}

void based_cb(const char* data, const char* error, int id) {
    auto len_data = strlen(data);
    auto len_error = strlen(error);
    callFns.at(id)(data, error);
    callFns.erase(id);
}

void based_observe_cb(const char* data, uint64_t checksum, const char* error, int id) {
    auto len_data = strlen(data);
    auto len_error = strlen(error);
    observeFns.at(id)(data, error);
}

void call(std::string name, std::string payload, std::function<void()> cb) {
    auto id = Based__call(client, (char*)name.data(), (char*)payload.data(), &based_cb);
    callFns.emplace(id, cb);
}

void get(std::string name, std::string payload, std::function<void()> cb) {
    auto id = Based__get(client, (char*)name.data(), (char*)payload.data(), &based_cb);
    callFns.emplace(id, cb);
}

std::function<void()> observe(std::string name, std::string payload, std::function<void()> cb) {
    auto id = Based__observe(client, (char*)name.data(), (char*)payload.data(), &based_observe_cb);
    observeFns.emplace(id, cb);
    return [id]() {
        Based__unobserve(client, id);
        observeFns.erase(id);
    };
}

int main(int argc, char** argv) {
    Based__connect(client, (char*)"test", (char*)"saulx", (char*)"test", (char*)"framme", (char*)"",
                   (char*)"", false);

    bool done = false;
    std::string cmd;

    std::list<int> obs;
    std::list<int> ch_subs;

    while (!done) {
        std::getline(std::cin, cmd);
        if (cmd == "q") {
            done = true;
            continue;
        }

        if (cmd.substr(0, 3) == "obs") {
            cmd.erase(0, 4);
            auto idx = cmd.find(" ");
            auto name = cmd.substr(0, idx);
            cmd.erase(0, idx + 1);

            std::string payload;
            if (idx == std::string::npos) {
                payload = "";
            } else {
                payload = cmd;
            }

            char* f = &*name.begin();
            char* p = &*payload.begin();

            int id = Based__observe(client, f, p, &based_observe_cb);
            std::cout << "--> [" << id << "] Observe '" << name << "', payload = \"" << payload
                      << "\"" << std::endl;

            obs.push_back(id);
        } else if (cmd.substr(0, 3) == "get") {
            cmd.erase(0, 4);
            auto idx = cmd.find(" ");
            auto name = cmd.substr(0, idx);
            cmd.erase(0, idx + 1);
            std::string payload;
            if (idx == std::string::npos) {
                payload = "";
            } else {
                payload = cmd;
            }

            char* f = &*name.begin();
            char* p = &*payload.begin();
            std::cout << "--> Get '" << name << "', payload = \"" << payload << "\"" << std::endl;
            Based__get(client, f, p, &based_cb);

        } else if (cmd.substr(0, 4) == "call") {
            cmd.erase(0, 5);
            auto idx = cmd.find(" ");
            auto name = cmd.substr(0, idx);
            cmd.erase(0, idx + 1);
            std::string payload;
            if (idx == std::string::npos) {
                payload = "";
            } else {
                payload = cmd;
            }

            char* f = &*name.begin();
            char* p = &*payload.begin();
            std::cout << "--> Call '" << name << "', payload = \"" << payload << "\"" << std::endl;
            Based__call(client, f, p, &based_cb);

        } else if (cmd.substr(0, 6) == "ch_sub") {
            cmd.erase(0, 7);
            auto idx = cmd.find(" ");
            auto name = cmd.substr(0, idx);
            cmd.erase(0, idx + 1);
            std::string payload;
            if (idx == std::string::npos) {
                payload = "";
            } else {
                payload = cmd;
            }

            char* f = &*name.begin();
            char* p = &*payload.begin();
            std::cout << "--> Channel subscribe '" << name << "', payload = \"" << payload << "\""
                      << std::endl;
            auto id = Based__channel_subscribe(client, f, p, &based_cb);
            ch_subs.push_back(id);
        } else if (cmd.substr(0, 8) == "ch_unsub") {
            cmd.erase(0, 9);
            int rem_id = atoi(cmd.c_str());
            std::cout << "--> Channel unsubscribe id:'" << rem_id << "\'" << std::endl;
            ch_subs.remove(rem_id);

            Based__channel_unsubscribe(client, rem_id);
        } else if (cmd.substr(0, 6) == "ch_pub") {
            cmd.erase(0, 7);
            auto idx = cmd.find(" ");
            auto name = cmd.substr(0, idx);
            cmd.erase(0, idx + 1);

            std::string payload;
            std::string message;
            if (idx == std::string::npos) {
                payload = "";
            } else {
                payload = cmd.substr(0, idx - 3);
                idx = cmd.find(" ");
                cmd.erase(0, idx + 1);
                if (idx == std::string::npos) {
                    message = "";
                } else {
                    message = cmd;
                }
            }

            char* f = &*name.begin();
            char* p = &*payload.begin();
            char* m = &*message.begin();
            std::cout << "--> Channel publish '" << name << "', payload = \"" << payload
                      << "\", message:\"" << message << "\"" << std::endl;
            Based__channel_publish(client, f, p, m);
        } else if (cmd.substr(0, 1) == "r") {
            int rem_id = atoi(cmd.substr(2).c_str());
            Based__unobserve(client, rem_id);
            obs.remove(rem_id);
            continue;
        }

        std::cout << "obs = ";
        for (auto el : obs) {
            std::cout << el << ", ";
        }
        std::cout << "\n";

        std::cout << "ch_subs = ";
        for (auto el : ch_subs) {
            std::cout << el << ", ";
        }
        std::cout << "\n";
    }

    Based__delete_client(client);
    // disconnect();
}