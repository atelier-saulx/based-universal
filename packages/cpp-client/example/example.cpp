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
    // if (argc < 2) {
    //     std::cerr << "Specify address" << std::endl;
    //     return -1;
    // }

    int client1 = Based__new_client();

    // Based__connect_to_url(client1, (char*)"wss://localhost:9910");
    Based__connect(client1, (char*)"https://d15p61sp2f2oaj.cloudfront.net", (char*)"airhub",
                   (char*)"airhub", (char*)"edge", (char*)"@based/edge", (char*)"", false);

    // Based__connect(client1, (char*)"http://localhost:7022", (char*)"saulx", (char*)"hello",
    //                (char*)"production", (char*)"@based/edge", (char*)"", false);

    // std::string res = Based__get_service(client1, (char*)"https://d15p61sp2f2oaj.cloudfront.net",
    //                                      (char*)"saulx", (char*)"demo", (char*)"production",
    //                                      (char*)"@based/edge", (char*)"", true);

    // Based__auth(client1, "derp", NULL);
    bool done = false;
    // int i = 0;
    std::string cmd;

    std::list<int> obs;

    // int x = Based__get(client1, (char*)"counter", (char*)"", &based_cb);

    // std::cout << "x = " << x << std::endl;

    // obs.push_back(Based__observe(client1, (char*)"counter", (char*)"", &based_observe_cb));

    while (!done) {
        std::getline(std::cin, cmd);
        if (cmd == "q") {
            done = true;
            continue;
        }

        if (cmd.substr(0, 7) == "service") {
            auto url = Based__get_service(client1, (char*)"https://d15p61sp2f2oaj.cloudfront.net",
                                          (char*)"airhub", (char*)"airhub", (char*)"edge",
                                          (char*)"@based/edge", (char*)"", false);
            std::cout << "Service url! = " << url << std::endl;
        } else if (cmd.substr(0, 5) == "login") {
            std::string payload = cmd.substr(6);
            std::cout << "Payload =" << payload << std::endl;

            char* p = &*payload.begin();

            Based__function(client1, (char*)"login", p, &based_cb);
        } else if (cmd.substr(0, 3) == "get") {
            std::string payload = cmd.substr(4);
            std::cout << "Payload =" << payload << std::endl;

            char* p = &*payload.begin();

            Based__function(client1, (char*)"based-db-get", p, &based_cb);
        } else if (cmd.substr(0, 7) == "flights") {
            std::string payload = cmd.substr(8);
            std::cout << "Payload =" << payload << std::endl;

            char* p = &*payload.begin();

            Based__observe(client1, (char*)"flights-observeAll", p, &based_observe_cb);
        } else if (cmd.substr(0, 1) == "r") {
            int rem_id = atoi(cmd.substr(2).c_str());
            Based__unobserve(client1, rem_id);
            obs.remove(rem_id);
            continue;
        } else if (cmd.substr(0, 1) == "o") {
            std::string fn_name = cmd.substr(2);
            char* fn = &*fn_name.begin();

            int id = Based__observe(client1, fn, (char*)"{}", &based_observe_cb);

            obs.push_back(id);
        } else if (cmd.substr(0, 1) == "g") {
            std::string fn_name = cmd.substr(2);
            char* fn = &*fn_name.begin();

            Based__get(client1, fn, (char*)"", &based_cb);
        } else if (cmd.substr(0, 1) == "f") {
            std::string fn_name = cmd.substr(2);
            std::cout << "Function " << fn_name << std::endl;
            char* fn = &*fn_name.begin();

            Based__function(client1, fn, (char*)"", &based_cb);
        } else if (cmd.substr(0, 1) == "s") {
            std::string payload = cmd.substr(2);
            std::cout << "Payload =" << payload << std::endl;

            char* p = &*payload.begin();

            Based__function(client1, (char*)"based-db-set", p, &based_cb);
        } else if (cmd.substr(0, 1) == "d") {
            Based__disconnect(client1);
        } else if (cmd.substr(0, 1) == "a") {
            std::string payload = cmd.substr(2);

            char* p = &*payload.begin();

            Based__auth(client1, p, &based_auth_cb);
        } else if (cmd.substr(0, 1) == "c") {
            // Based__connect(client1, "http://localhost:7022/", "saulx", "demo", "production",
            //                "@based/edge", "", false);
            Based__connect_to_url(client1, (char*)"wss://localhost:9910");
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