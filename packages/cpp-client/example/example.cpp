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
    int len_data = strlen(data);
    int len_error = strlen(error);
    if (len_data > 0) {
        std::cout << "[" << id << "] DATA = " << data << std::endl;
    }
    if (len_error > 0) {
        std::cout << "[" << id << "] ERROR = " << error << std::endl;
    }
}

void based_observe_cb(const char* data, uint64_t checksum, const char* error, int id) {
    int len_data = strlen(data);
    int len_error = strlen(error);
    if (len_data > 0) {
        std::cout << "[" << id << "] DATA {" << checksum << "} = " << data << std::endl;
    }
    if (len_error > 0) {
        std::cout << "[" << id << "] ERROR = " << error << std::endl;
    }
}

int main(int argc, char** argv) {
    int client1 = Based__new_client();

    Based__connect(client1, (char*)"", (char*)"saulx", (char*)"test", (char*)"framme", (char*)"",
                   (char*)"", false);

    bool done = false;
    std::string cmd;

    std::list<int> obs;

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
            Based__function(client1, f, p, &based_cb);

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
    }

    Based__delete_client(client1);
    // disconnect();
}