#ifndef BASED_CLIENT_H
#define BASED_CLIENT_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "connection.hpp"
#include "utility.hpp"

using namespace nlohmann::literals;

enum IncomingType {
    FUNCTION_DATA = 0,
    SUBSCRIPTION_DATA = 1,
    SUBSCRIPTION_DIFF_DATA = 2,
    GET_DATA = 3,
    AUTH_DATA = 4,
    ERROR_DATA = 5
};

struct ObservableOpts {
    ObservableOpts(bool ls, int mct) : local_storage(ls), max_cache_time(mct){};

    bool local_storage;
    int max_cache_time;
};

class BasedClient {
   private:
    WsConnection m_con;

    int32_t m_request_id;
    int32_t m_sub_id;

    bool m_auth_in_progress;
    std::string m_auth_state;
    std::string m_auth_request_state;
    std::function<void(std::string)> m_auth_callback;

    std::map<int, std::function<void(std::string, std::string)>> m_function_callbacks;
    // std::map<int, std::function<void(std::string)>> m_error_listeners;

    /////////////////////
    // queues
    /////////////////////

    std::vector<std::vector<uint8_t>> m_observe_queue;
    std::vector<std::vector<uint8_t>> m_function_queue;
    std::vector<std::vector<uint8_t>> m_unobserve_queue;
    std::vector<std::vector<uint8_t>> m_get_queue;

    bool m_draining;

    /////////////////////
    // observables
    /////////////////////

    /**
     * map<obs_hash, encoded_request>
     * The list of all the active observables. These should only be deleted when
     * there are no active subs for it. It's used in the event of a reconnection.
     */
    std::map<int, std::vector<uint8_t>> m_observe_requests;

    /**
     * map<obs_hash, list of sub_ids>
     * The list of subsribers to the observable. These are tied to a on_data function
     * and an optional on_error function, which should be fired appropriately.
     */
    std::map<int, std::set<int>> m_observe_subs;

    /**
     * <sub_id, obs_hash>
     *  Mapping of which observable a sub_id refers to. Necessary for .unobserve.
     */
    std::map<int, int> m_sub_to_obs;

    /**
     * map<sub_id, on_data callback>
     * List of on_data callback to call when receiving the data.
     */
    std::map<int, std::function<void(std::string, int64_t, std::string)>> m_sub_callback;

    ////////////////
    // gets
    ////////////////

    /**
     * map<obs_hash, list of sub_ids>
     * The list of getters to the observable. These should be fired once, when receiving
     * the sub data, and immediatly cleaned up.
     */
    std::map<int, std::set<int>> m_get_subs;

    /**
     * map<sub_id, on_data callback>
     * List of on_data callback to call when receiving the data. Should be deleted after firing.
     */
    std::map<int, std::function<void(std::string /*data*/, std::string /*error*/)>>
        m_get_sub_callbacks;

   public:
    BasedClient() : m_request_id(0), m_sub_id(0), m_draining(false), m_auth_in_progress(false) {
        m_con.set_message_handler([&](std::string msg) { on_message(msg); });
        m_con.set_open_handler([&]() { on_open(); });
    }

    void connect(std::string uri) {
        m_con.connect(uri);
    }

    void disconnect() {
        m_con.disconnect();
    }

    /**
     * Observe a function. This returns the sub_id used to
     * unsubscribe with .unobserve(id)
     */
    int observe(
        std::string name,
        std::string payload,
        /**
         * Callback that the observable will trigger.
         */
        std::function<void(std::string /*data*/, int64_t /*checksum*/, std::string /*error*/)>
            on_data,
        ObservableOpts obs_opts) {
        /**
         * Each observable must be stored in memory, in case the connection drops.
         * So there's a queue, which is emptied on drain, but is refilled with the observables
         * stored in memory in the event of a reconnection.
         *
         * These observables all have a list of listeners. Each listener has a on_data callback
         * and and optional on_error callback.
         *
         * When all listeners are removed with .unobserve, the observable should be removed
         * and the unobserve request should be queued, to let the server know.
         */

        int32_t obs_id = make_obs_id(name, payload);
        int32_t sub_id = m_sub_id++;

        std::cout << "obs_id sent = " << obs_id << std::endl;

        if (m_observe_requests.find(obs_id) == m_observe_requests.end()) {
            // first time this query is observed
            // encode request
            // TODO: remove hardcoded checksum after implementing cache
            std::vector<uint8_t> msg = Utility::encode_observe_message(obs_id, name, payload, 0);

            // add encoded request to queue
            m_observe_queue.push_back(msg);

            // add encoded request to map of observables
            m_observe_requests[obs_id] = msg;

            // add subscriber to list of subs for this observable
            m_observe_subs[obs_id] = std::set<int>{sub_id};

            // record what obs this sub is for, to delete it later
            m_sub_to_obs[sub_id] = obs_id;

            // add on_data for this sub
            m_sub_callback[sub_id] = on_data;

            // add on_error for this sub if on_error is present (overload?)
        } else {
            // this query has already been requested once, only add subscriber,
            // dont send a new request.

            // add subscriber to that observable
            m_observe_subs.at(obs_id).insert(sub_id);

            // record what obs this sub is for, to delete it later
            m_sub_to_obs[sub_id] = obs_id;

            // add on_data for this new sub
            m_sub_callback[sub_id] = on_data;
            // add on_error for this new sub if it exists
        }

        drain_queues();

        return sub_id;
    }

    void get(std::string name,
             std::string payload,
             std::function<void(std::string /*data*/, std::string /*error*/)> cb) {
        int32_t obs_id = make_obs_id(name, payload);
        int32_t sub_id = m_sub_id++;

        // if obs_id exists in get_subs, add new sub to list
        if (m_get_subs.find(obs_id) != m_get_subs.end()) {
            m_get_subs.at(obs_id).insert(sub_id);
        } else {  // else create it and then add it
            m_get_subs[obs_id] = std::set<int>{sub_id};
        }
        m_get_sub_callbacks[sub_id] = cb;
        // is there an active obs? if so, do nothing (get will trigger on next update)
        // if there isnt, queue request
        if (m_observe_requests.find(obs_id) == m_observe_requests.end()) {
            // TODO: remove hardcoded checksum when cache is implemented
            std::vector<uint8_t> msg = Utility::encode_get_message(obs_id, name, payload, 0);
            m_get_queue.push_back(msg);
            drain_queues();
        }
    }

    void unobserve(int sub_id) {
        std::cout << "> Removing sub_id " << sub_id << std::endl;
        if (m_sub_to_obs.find(sub_id) == m_sub_to_obs.end()) {
            std::cerr << "No subscription found with sub_id " << sub_id << std::endl;
            return;
        }
        auto obs_id = m_sub_to_obs.at(sub_id);

        // remove sub from list of subs for that observable
        m_observe_subs.at(obs_id).erase(sub_id);

        // remove on_data callback
        m_sub_callback.erase(sub_id);

        // remove sub to obs mapping for removed sub
        m_sub_to_obs.erase(sub_id);

        // if the list is now empty, add request to unobserve to queue
        if (m_observe_subs.at(obs_id).empty()) {
            std::vector<uint8_t> msg = Utility::encode_unobserve_message(obs_id);
            m_unobserve_queue.push_back(msg);
            // and remove the obs from the map of active ones.
            m_observe_requests.erase(obs_id);
            // and the vector of listeners, since it's now empty we can free the memory
            m_observe_subs.erase(obs_id);
        }
        drain_queues();
    }

    void function(std::string name,
                  std::string payload,
                  std::function<void(std::string /*data*/, std::string /*error*/)> cb) {
        m_request_id++;
        if (m_request_id > 16777215) {
            m_request_id = 0;
        }
        int id = m_request_id;
        m_function_callbacks[id] = cb;
        // encode the message
        std::vector<uint8_t> msg = Utility::encode_function_message(id, name, payload);
        m_function_queue.push_back(msg);
        drain_queues();
    }

    void auth(std::string state, std::function<void(std::string)> cb) {
        // std::cout << "auth not implemented yet" << std::endl;
        if (m_auth_in_progress) return;

        m_auth_request_state = state;
        m_auth_in_progress = true;
        m_auth_callback = cb;

        std::vector<uint8_t> msg = Utility::encode_auth_message(state);
        m_con.send(msg);
    }

   private:
    // methods
    int32_t make_obs_id(std::string& name, std::string& payload) {
        if (payload.length() == 0) {
            int32_t payload_hash = (int32_t)std::hash<json>{}("");
            int32_t name_hash = (int32_t)std::hash<std::string>{}(name);

            int32_t obs_id = (payload_hash * 33) ^ name_hash;  // TODO: check with jim

            return obs_id;
        }
        json p = json::parse(payload);
        int32_t payload_hash = (int32_t)std::hash<json>{}(p);
        int32_t name_hash = (int32_t)std::hash<std::string>{}(name);

        int32_t obs_id = (payload_hash * 33) ^ name_hash;  // TODO: check with jim
        return obs_id;
    }

    void drain_queues() {
        if (m_draining || m_con.status() == ConnectionStatus::CLOSED ||
            m_con.status() == ConnectionStatus::FAILED ||
            m_con.status() == ConnectionStatus::CONNECTING) {
            std::cerr << "Connection is unavailable, status = " << m_con.status() << std::endl;
            return;
        }

        std::cout << "> Draining queue" << std::endl;

        m_draining = true;

        std::vector<uint8_t> buff;

        if (m_observe_queue.size() > 0) {
            for (auto msg : m_observe_queue) {
                buff.insert(buff.end(), msg.begin(), msg.end());
            }
            m_observe_queue.clear();
        }

        if (m_unobserve_queue.size() > 0) {
            for (auto msg : m_unobserve_queue) {
                buff.insert(buff.end(), msg.begin(), msg.end());
            }
            m_unobserve_queue.clear();
        }

        if (m_function_queue.size() > 0) {
            for (auto msg : m_function_queue) {
                buff.insert(buff.end(), msg.begin(), msg.end());
            }
            m_function_queue.clear();
        }

        if (m_get_queue.size() > 0) {
            for (auto msg : m_get_queue) {
                buff.insert(buff.end(), msg.begin(), msg.end());
            }
            m_get_queue.clear();
        }

        if (buff.size() > 0) m_con.send(buff);

        m_draining = false;
    }

    void on_open() {
        for (auto obs : m_observe_requests) {
            m_observe_queue.push_back(obs.second);
        }
        drain_queues();
    }

    void on_message(std::string message) {
        if (message.length() <= 7) {
            std::cerr << ">> Payload is too small, wrong data: " << message << std::endl;
            return;
        }
        int32_t header = Utility::read_header(message);
        int32_t type = Utility::get_payload_type(header);
        int32_t len = Utility::get_payload_len(header);
        int32_t is_deflate = Utility::get_payload_is_deflate(header);

        // std::cout << "type = " << type << std::endl;
        // std::cout << "len = " << len << std::endl;
        // std::cout << "is_deflate = " << is_deflate << std::endl;

        switch (type) {
            case IncomingType::FUNCTION_DATA: {
                int id = Utility::read_bytes_from_string(message, 4, 3);

                if (m_function_callbacks.find(id) != m_function_callbacks.end()) {
                    auto fn = m_function_callbacks.at(id);
                    if (len != 3) {
                        int start = 7;
                        int end = len + 4;
                        std::string payload =
                            is_deflate ? Utility::inflate_string(message.substr(start, end))
                                       : message.substr(start, end);
                        fn(payload, "");
                    } else {
                        fn("", "");
                    }
                    // Listener has fired, remove it from the map.
                    m_function_callbacks.erase(id);
                }
            }
                return;
            case IncomingType::SUBSCRIPTION_DATA: {
                uint64_t obs_id = Utility::read_bytes_from_string(message, 4, 8);
                int checksum = Utility::read_bytes_from_string(message, 12, 8);

                int start = 20;  // size of header
                int end = len + 4;
                std::string payload = "";
                if (len != 16) {
                    payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                         : message.substr(start, end);
                }

                if (m_observe_subs.find(obs_id) != m_observe_subs.end()) {
                    for (auto sub_id : m_observe_subs.at(obs_id)) {
                        auto fn = m_sub_callback.at(sub_id);
                        fn(payload, checksum, "");
                    }
                }

                if (m_get_subs.find(obs_id) != m_get_subs.end()) {
                    for (auto sub_id : m_get_subs.at(obs_id)) {
                        auto fn = m_get_sub_callbacks.at(sub_id);
                        fn(payload, "");
                        m_get_sub_callbacks.erase(sub_id);
                    }
                    m_get_subs.at(obs_id).clear();
                }
            }
                return;
            case IncomingType::SUBSCRIPTION_DIFF_DATA: {
                std::cerr << "Diffing not implemented yet, should never happen." << std::endl;
            } break;
            case IncomingType::GET_DATA:
                std::cout << "GET DATA CACHE NOT IMPLEMENTED YET" << std::endl;
                break;
            case IncomingType::AUTH_DATA: {
                int32_t start = 4;
                int32_t end = len + 4;
                std::string payload = "";
                if (len != 3) {
                    payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                         : message.substr(start, end);
                }
                if (payload == "true") {
                    m_auth_state = m_auth_request_state;
                    m_auth_request_state = "";
                } else {
                    m_auth_state = payload;
                }
                m_auth_callback(payload);

                m_auth_in_progress = false;
            }
                return;
            case IncomingType::ERROR_DATA: {
                // TODO: test this when errors get implemented in the server

                // std::cout << "Error received. Error handling not implemented yet" << std::endl;
                int32_t start = 4;
                int32_t end = len + 4;
                std::string payload = "{}";
                if (len != 3) {
                    payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                         : message.substr(start, end);
                }

                json error(payload);
                // fire once
                if (error.find("requestId") != error.end()) {
                    auto id = error.at("requestId");
                    if (m_function_callbacks.find(id) != m_function_callbacks.end()) {
                        auto fn = m_function_callbacks.at(id);
                        fn("", payload);
                        m_function_callbacks.erase(id);
                    }
                    if (m_get_subs.find(id) != m_get_subs.end()) {
                        for (auto get_id : m_get_subs.at(id)) {
                            auto fn = m_get_sub_callbacks.at(id);
                            fn("", payload);
                            m_get_sub_callbacks.erase(id);
                        }
                        m_get_subs.erase(id);
                    }
                }
                // keep alive
                if (error.find("observableId") != error.end()) {
                    auto obs_id = error.at("observableId");
                    if (m_observe_subs.find(obs_id) != m_observe_subs.end()) {
                        for (auto sub_id : m_observe_subs.at(obs_id)) {
                            if (m_sub_callback.find(sub_id) != m_sub_callback.end()) {
                                auto fn = m_sub_callback.at(sub_id);
                                fn("", 0, payload);
                            }
                        }
                    }
                }
            }
                return;
            default:
                std::cerr << ">> Unknown payload type \"" << type << "\" received." << std::endl;
                return;
        }
    };
};

#endif
