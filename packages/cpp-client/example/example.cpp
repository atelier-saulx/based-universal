#include <iostream>
#include <list>
#include <string>

#include <sstream>

#include "based.h"

void based_auth_cb(const char* data) {
    if (strlen(data) > 0) {
        std::cout << "[AUTH_CB] " << data << std::endl;
    }
}

void based_cb(const char* data, const char* error, int id) {
    auto len_data = strlen(data);
    auto len_error = strlen(error);
    if (len_data > 0) {
        std::cout << "[" << id << "] DATA = " << data << std::endl;
    }
    if (len_error > 0) {
        std::cout << "[" << id << "] ERROR = " << error << std::endl;
    }
}

void based_observe_cb(const char* data, uint64_t checksum, const char* error, int id) {
    auto len_data = strlen(data);
    auto len_error = strlen(error);
    if (len_data > 0) {
        std::cout << "[" << id << "] DATA {" << checksum << "} = " << data << std::endl;
    }
    if (len_error > 0) {
        std::cout << "[" << id << "] ERROR = " << error << std::endl;
    }
}

int main(int argc, char** argv) {
    int client1 = Based__new_client();

    Based__connect(client1, (char*)"test", (char*)"saulx", (char*)"test", (char*)"framme",
                   (char*)"", (char*)"", false);

    // Based__connect_to_url(client1, (char*)"ws://localhost:9999");

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

            int id = Based__observe(client1, f, p, &based_observe_cb);
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
            Based__get(client1, f, p, &based_cb);

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
            Based__call(client1, f, p, &based_cb);

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
            auto id = Based__channel_subscribe(client1, f, p, &based_cb);
            ch_subs.push_back(id);
        } else if (cmd.substr(0, 8) == "ch_unsub") {
            cmd.erase(0, 9);
            int rem_id = atoi(cmd.c_str());
            std::cout << "--> Channel unsubscribe id:'" << rem_id << "\'" << std::endl;
            ch_subs.remove(rem_id);

            Based__channel_unsubscribe(client1, rem_id);
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
            Based__channel_publish(client1, f, p, m);
        } else if (cmd.substr(0, 1) == "r") {
            int rem_id = atoi(cmd.substr(2).c_str());
            Based__unobserve(client1, rem_id);
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

    Based__delete_client(client1);
    // disconnect();
}