#ifndef BASED_WS_CONNECTION_H
#define BASED_WS_CONNECTION_H

#include <iostream>
#include <string>
#include <thread>
#include <websocketpp/client.hpp>

#include <websocketpp/config/asio_client.hpp>         // SSL
#include <websocketpp/config/asio_no_tls_client.hpp>  // No SSL

typedef websocketpp::client<websocketpp::config::asio_tls_client> wss_client;  // SSL
typedef websocketpp::client<websocketpp::config::asio_client> ws_client;       // No SSL

enum ConnectionStatus { OPEN = 0, CONNECTING, CLOSED, FAILED, TERMINATED_BY_USER };

struct BasedConnectOpt {
    std::string cluster = "production";
    std::string org;
    std::string project;
    std::string env;
    std::string name = "@based/env-hub";
    std::string key;
    std::string host;
    bool optional_key;
    std::string url;
    std::string discovery_url;
    std::map<std::string, std::string> headers;
};

class WsConnection {
    // eventually there should be some logic here to handle inactivity.
   public:
    WsConnection(bool enable_tls);
    ~WsConnection();
    void connect(std::string cluster,
                 std::string org,
                 std::string project,
                 std::string env,
                 //  std::string name,
                 std::string key,
                 bool optional_key,
                 std::string host,
                 std::string discovery_url);
    void connect_to_uri(std::string uri);
    void disconnect();
    void set_open_handler(std::function<void()> on_open);
    void set_message_handler(std::function<void(std::string)> on_message);
    void send(std::vector<uint8_t> message);
    ConnectionStatus status();
    std::string discover_service(BasedConnectOpt opts, bool http);

    void set_handlers(ws_client::connection_ptr con);
    void set_handlers(wss_client::connection_ptr con);

    using context_ptr = std::shared_ptr<asio::ssl::context>;
    static context_ptr on_tls_init();

   private:  // Members
    bool m_enable_tls;
    ws_client m_ws_endpoint;
    wss_client m_wss_endpoint;
    websocketpp::connection_hdl m_hdl;
    ConnectionStatus m_status;
    std::string m_uri;
    std::shared_ptr<std::thread> m_thread;
    std::shared_future<void> m_reconnect_future;
    std::function<void()> m_on_open;
    std::function<void(std::string)> m_on_message;
    int m_reconnect_attempts;

    BasedConnectOpt m_opts;

    std::pair<std::string, std::string> make_request(std::string url, BasedConnectOpt opts);
};

#endif