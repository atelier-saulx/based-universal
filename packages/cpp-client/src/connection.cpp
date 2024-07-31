#include "connection.hpp"
#include <curl/curl.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "utility.hpp"

#define DEFAULT_CLUSTER_NAME "production"

using namespace nlohmann::literals;
using json = nlohmann::json;

const std::map<std::string, int> SERVICES = {{"@based/env-hub", 0},
                                             {"@based/env-admin-hub", 1},
                                             {"@based/admin-hub", 2},
                                             {"@based/machine-hub", 3}};

///////////////////////////////////////////////////
//////////////// Helper functions /////////////////
///////////////////////////////////////////////////

std::vector<std::string> split_on_char(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

size_t write_function(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t get_request_id_header(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t numbytes = size * nitems;
    std::string str(buffer, numbytes);
    if (str.rfind("x-request-id", 0) == 0) {
        // TODO: Is this data always padded with 2 extra characters, or can it be different?
        ((std::string*)userdata)->assign((char*)buffer, numbytes - 2);
    }
    return numbytes;
}

int random_number() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    return static_cast<int>(std::floor(std::rand() / (RAND_MAX + 1.0) * 10000));
}

std::string gen_cache(BasedConnectOpt opts) {
    std::string name = opts.name.length() > 0 ? opts.name : "@based/env-hub";
    if (SERVICES.find(name) == SERVICES.end()) {
        throw std::runtime_error("This service is not discoverable");
    }
    auto name_index = SERVICES.at(name);
    auto random_path = random_number();
    auto prefix = std::to_string(name_index) + std::to_string(random_path);
    if (opts.key.length() > 0) {
        return prefix + "/" + (opts.optional_key ? opts.key + "$" : opts.key);
    }
    return prefix;
}

unsigned int portFromString(const std::string& str) {
    unsigned long hash = 5381;
    int i = str.length();
    while (i) {
        hash = (hash * 33) ^ str[--i];
    }
    unsigned int n = hash & 0xFFFFFFFF;
    while (n > 65535) {
        n = n / 10;
    }
    return std::round(n);
}

unsigned int getServicePort(const std::string& org,
                            const std::string& project,
                            const std::string& env,
                            const std::string& pkgName,
                            const std::string& machineTemplate = "allServices",
                            const std::string& index = "0") {
    std::stringstream ss;
    ss << machineTemplate << "-" << org << "-" << project << "-" << env << "-" << pkgName << "-"
       << index;
    std::string concatenatedString = ss.str();

    unsigned int port = portFromString(concatenatedString);
    return port;
}

std::string gen_discovery_url(BasedConnectOpt opts) {
    if (opts.cluster == "local") {
        // If this were to connect to based-platform services,
        // we'd need to add logic to route it to '@based/hub-discovery' instead.
        auto port = getServicePort(opts.org, opts.project, opts.env, "@based/env-hub-discovery",
                                   "allServices");
        std::string host = opts.host.empty() ? "localhost" : opts.host;
        std::string url = "http://" + host + ":" + std::to_string(port);
        return url;
    }

    auto hashed_env = Utility::hash_env(opts.org, opts.project, opts.env, opts.cluster);

    std::string prefix = Utility::base36_encode(hashed_env);
    std::string affix = "-status";

    std::string url = "https://" + prefix + affix + ".based.dev";

    return url;
}

std::pair<std::string, std::string> WsConnection::make_request(std::string url,
                                                               BasedConnectOpt opts) {
    int timeout = m_reconnect_attempts > 15 ? 1500 : m_reconnect_attempts * 100;
    if (m_reconnect_attempts > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    CURL* curl;
    struct curl_slist* list = NULL;
    CURLcode res;
    std::string request_id_header;
    std::string buf;

    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl object failed to initialize");
    }

    // Set up curl
    std::string sequence_id_header = "sequence-id: " + gen_cache(opts);
    list = curl_slist_append(list, sequence_id_header.c_str());

    if (opts.headers.size() > 0) {
        for (auto const& header : opts.headers) {
            std::string header_str = header.first + ": " + header.second;
            list = curl_slist_append(list, header_str.c_str());
        }
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    std::string connect_url = url + "/status/" + gen_cache(opts);
    curl_easy_setopt(curl, CURLOPT_URL, connect_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);  // timeout in seconds
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, get_request_id_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &request_id_header);

    // TODO: fix it
    // This is not good, but it's a pragmatic solution for now.
    // No sensitive data is shared with this request, and the WebSocket connection will still be
    // over TLS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s, trying again...\n",
                curl_easy_strerror(res));
        m_reconnect_attempts++;
        return make_request(url, opts);
    }
    if (!(request_id_header.size() > 0)) {
        m_reconnect_attempts++;
        return make_request(url, opts);
    }
    m_reconnect_attempts = 0;

    auto header_value = Utility::split_string(request_id_header, ": ")[1];
    auto encode_chars = Utility::split_string(header_value.substr(0, 6), "");
    auto encoded_value = header_value.substr(6);

    std::string decoded_value = Utility::decode(encoded_value, encode_chars);

    auto result = split_on_char(decoded_value, ',');
    std::vector<std::pair<std::string, std::string>> pairs;

    for (int i = 0; i < result.size() / 2; i++) {
        auto hub = result.at(i);
        auto key = Utility::encodeURIComponent(result.at(i + (result.size() / 2)));
        pairs.push_back((std::make_pair(hub, key)));
    }

    curl_easy_cleanup(curl);

    return pairs.at(std::rand() % (pairs.size()));
}

//////////////////////////////////////////////////////////////////////////
///////////////////////// Class methods /////////////////////////////////
//////////////////////////////////////////////////////////////////////////

WsConnection::WsConnection(bool enable_tls)
    : m_status(ConnectionStatus::CLOSED),
      m_on_open(NULL),
      m_on_message(NULL),
      m_reconnect_attempts(0) {
    m_enable_tls = enable_tls;
    BASED_LOG("ENABLE TLS = %d", m_enable_tls);
    if (m_enable_tls) {
        m_wss_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_wss_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        m_wss_endpoint.init_asio();
        m_wss_endpoint.set_tls_init_handler(websocketpp::lib::bind(&WsConnection::on_tls_init));
        m_wss_endpoint.start_perpetual();
        m_thread = std::make_shared<std::thread>(&wss_client::run, &m_wss_endpoint);
    } else {
        m_ws_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_ws_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        m_ws_endpoint.init_asio();
        m_ws_endpoint.start_perpetual();
        m_thread = std::make_shared<std::thread>(&ws_client::run, &m_ws_endpoint);
    }
};

WsConnection::~WsConnection() {
    disconnect();
    if (m_enable_tls) {
        m_wss_endpoint.stop_perpetual();
    } else {
        m_ws_endpoint.stop_perpetual();
    }
    m_thread->join();

    BASED_LOG("Destroyed WsConnection obj");
};

std::string WsConnection::discover_service(BasedConnectOpt opts, bool http) {
    std::string url;
    if (m_opts.url.length() > 0) {
        url = opts.url;
        if (http && (url.rfind("wss://", 0) == 0)) {
            url.replace(0, 3, "https");
        }
        return url;
    }
    std::string discovery_url = gen_discovery_url(opts);
    if (!m_opts.discovery_url.empty()) {
        discovery_url = m_opts.discovery_url;
    }
    auto req = make_request(discovery_url, opts);
    auto service_url = req.first;
    auto access_key = req.second;
    url = service_url;
    if (m_enable_tls) {
        return http ? "https://" + url : "wss://" + url + "/" + access_key;
    } else {
        return http ? "http://" + url : "ws://" + url + "/" + access_key;
    }
}

void WsConnection::connect(std::string cluster,
                           std::string org,
                           std::string project,
                           std::string env,
                           std::string key,
                           bool optional_key,
                           std::string host,
                           std::string discovery_url) {
    m_opts.cluster = cluster;
    m_opts.org = org;
    m_opts.project = project;
    m_opts.env = env;
    m_opts.name = "@based/env-hub";
    m_opts.key = key;
    m_opts.optional_key = optional_key;
    m_opts.host = host;
    m_opts.discovery_url = discovery_url;

    std::thread con_thr([&]() {
        std::string service_url = discover_service(m_opts, false);
        connect_to_uri(service_url);
    });
    con_thr.detach();
}

void WsConnection::connect_to_uri(std::string uri) {
    // maximum timeout between attempts, in ms
    int timeout = m_reconnect_attempts > 15 ? 1500 : m_reconnect_attempts * 100;
    if (m_reconnect_attempts > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    if (m_status == ConnectionStatus::OPEN) {
        BASED_LOG("Attempting to connect while connection is already open, do nothing...");
        return;
    }
    BASED_LOG("Attempting to connect to \"%s\"", uri.c_str());

    m_uri = uri;
    websocketpp::lib::error_code ec;

    if (m_enable_tls) {
        wss_client::connection_ptr con = m_wss_endpoint.get_connection(m_uri, ec);

        if (ec) {
            BASED_LOG("Error trying to initialize connection, message = \"%s\"",
                      ec.message().c_str());
            m_status = ConnectionStatus::FAILED;
            return;
        }

        m_status = ConnectionStatus::CONNECTING;

        // Add special headers

        con->append_header("sec-websocket-protocol", "{}");

        m_hdl = con->get_handle();

        set_handlers(con);

        m_wss_endpoint.connect(con);

    } else {
        ws_client::connection_ptr con = m_ws_endpoint.get_connection(m_uri, ec);

        if (ec) {
            BASED_LOG("Error trying to initialize connection, message = \"%s\"",
                      ec.message().c_str());
            m_status = ConnectionStatus::FAILED;
            return;
        }

        m_status = ConnectionStatus::CONNECTING;

        // Add special headers

        con->append_header("sec-websocket-protocol", "{}");

        m_hdl = con->get_handle();

        set_handlers(con);

        m_ws_endpoint.connect(con);
    }
};

void WsConnection::set_open_handler(std::function<void()> on_open) {
    m_on_open = on_open;
};
void WsConnection::set_message_handler(std::function<void(std::string)> on_message) {
    m_on_message = on_message;
};

void WsConnection::disconnect() {
    if (m_status != ConnectionStatus::OPEN) {
        return;
    }

    BASED_LOG("Connection terminated by user, closing...");

    m_status = ConnectionStatus::TERMINATED_BY_USER;

    websocketpp::lib::error_code ec;
    if (m_enable_tls) {
        m_wss_endpoint.close(m_hdl, websocketpp::close::status::going_away, "", ec);
    } else {
        m_ws_endpoint.close(m_hdl, websocketpp::close::status::going_away, "", ec);
    }
    if (ec) {
        BASED_LOG("Error trying to close connection, message = \"%s\"", ec.message().c_str());

        return;
    }
};

void WsConnection::send(std::vector<uint8_t> message) {
    websocketpp::lib::error_code ec;

    if (m_status != ConnectionStatus::OPEN) throw(std::runtime_error("Connection is not open."));

    if (m_enable_tls) {
        m_wss_endpoint.send(m_hdl, message.data(), message.size(),
                            websocketpp::frame::opcode::binary, ec);
    } else {
        m_ws_endpoint.send(m_hdl, message.data(), message.size(),
                           websocketpp::frame::opcode::binary, ec);
    }
    if (ec) {
        BASED_LOG("Error trying to send message, message = \"%s\"", ec.message().c_str());
        return;
    }
};

ConnectionStatus WsConnection::status() {
    return m_status;
};

using context_ptr = std::shared_ptr<asio::ssl::context>;
context_ptr WsConnection::on_tls_init() {
    context_ptr ctx = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

    try {
        ctx->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);

    } catch (std::exception& e) {
        std::cerr << "Error in context pointer: " << e.what() << std::endl;
    }
    return ctx;
}

void WsConnection::set_handlers(ws_client::connection_ptr con) {
    // bind must be used if the function we're binding to doest have the right number of
    // arguments (hence the placeholders) these handlers must be set before calling connect, and
    // can't be changed after (i think)
    con->set_open_handler([this](websocketpp::connection_hdl) {
        BASED_LOG("Connection opened");
        m_status = ConnectionStatus::OPEN;
        m_reconnect_attempts = 0;
        if (m_on_open) {
            m_on_open();
        }
    });

    con->set_message_handler([this](websocketpp::connection_hdl hdl, ws_client::message_ptr msg) {
        // here we will pass the message to the decoder, which, based on the header, will
        // call the appropriate callback

        std::string payload = msg->get_payload();
        if (m_on_message) {
            m_on_message(payload);
        }
    });

    con->set_close_handler([this](websocketpp::connection_hdl) {
        if (m_status != ConnectionStatus::TERMINATED_BY_USER) {
            m_status = ConnectionStatus::CLOSED;
            m_reconnect_attempts++;

            if (!m_opts.name.empty()) {
                connect(m_opts.cluster, m_opts.org, m_opts.project, m_opts.env, m_opts.key,
                        m_opts.optional_key, m_opts.host, m_opts.discovery_url);
            } else {
                connect_to_uri(m_uri);
            }
        }
    });

    con->set_fail_handler([this](websocketpp::connection_hdl) {
        BASED_LOG("Received FAIL event");
        m_status = ConnectionStatus::FAILED;
        m_reconnect_attempts++;

        if (m_uri.size() == 0) {
            connect(m_opts.cluster, m_opts.org, m_opts.project, m_opts.env, m_opts.key,
                    m_opts.optional_key, m_opts.host, m_opts.discovery_url);
        } else {
            connect_to_uri(m_uri);
        }
    });
}

void WsConnection::set_handlers(wss_client::connection_ptr con) {
    // bind must be used if the function we're binding to doest have the right number of
    // arguments (hence the placeholders) these handlers must be set before calling connect, and
    // can't be changed after (i think)
    con->set_open_handler([this](websocketpp::connection_hdl) {
        BASED_LOG("Connection opened");
        m_status = ConnectionStatus::OPEN;
        m_reconnect_attempts = 0;
        if (m_on_open) {
            m_on_open();
        }
    });

    con->set_message_handler([this](websocketpp::connection_hdl hdl, wss_client::message_ptr msg) {
        // here we will pass the message to the decoder, which, based on the header, will
        // call the appropriate callback

        std::string payload = msg->get_payload();
        if (m_on_message) {
            m_on_message(payload);
        }
    });

    con->set_close_handler([this](websocketpp::connection_hdl) {
        if (m_status != ConnectionStatus::TERMINATED_BY_USER) {
            m_status = ConnectionStatus::CLOSED;
            m_reconnect_attempts++;

            if (!m_opts.name.empty()) {
                connect(m_opts.cluster, m_opts.org, m_opts.project, m_opts.env, m_opts.key,
                        m_opts.optional_key, m_opts.host, m_opts.discovery_url);
            } else {
                connect_to_uri(m_uri);
            }
        }
    });

    con->set_fail_handler([this](websocketpp::connection_hdl) {
        BASED_LOG("Received FAIL event");
        m_status = ConnectionStatus::FAILED;
        m_reconnect_attempts++;

        if (m_uri.size() == 0) {
            connect(m_opts.cluster, m_opts.org, m_opts.project, m_opts.env, m_opts.key,
                    m_opts.optional_key, m_opts.host, m_opts.discovery_url);
        } else {
            connect_to_uri(m_uri);
        }
    });
}
