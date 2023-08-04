#include <json.hpp>
#include <stdexcept>
#include <utility>

#include "apply-patch.hpp"
#include "utility.hpp"

#include "basedclient.hpp"

using namespace nlohmann::literals;

enum IncomingType {
    FUNCTION_DATA = 0,
    SUBSCRIPTION_DATA = 1,
    SUBSCRIPTION_DIFF_DATA = 2,
    GET_DATA = 3,
    AUTH_DATA = 4,
    ERROR_DATA = 5,
    CHANNEL_REPUBLISH = 6,
    CHANNEL_MESSAGE = 7,
};

BasedClient::BasedClient() : m_request_id(0), m_sub_id(0), m_auth_in_progress(false){};

/////////////////
// Helper functions
/////////////////

inline obs_id_t make_obs_id(std::string& name, std::string& payload) {
    if (payload.empty()) {
        obs_id_t payload_hash = (obs_id_t)std::hash<json>{}("");
        obs_id_t name_hash = (obs_id_t)std::hash<std::string>{}(name);

        obs_id_t obs_id = (payload_hash * 33) ^ name_hash;

        return obs_id;
    }
    json p = json::parse(payload);
    obs_id_t payload_hash = (obs_id_t)std::hash<json>{}(p);
    obs_id_t name_hash = (obs_id_t)std::hash<std::string>{}(name);

    obs_id_t obs_id = (payload_hash * 33) ^ name_hash;
    return obs_id;
}

//////////////////////////////////////////////////////////////////////////
///////////////////////// Client methods /////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string BasedClient::discover_service(std::string cluster,
                                          std::string org,
                                          std::string project,
                                          std::string env,
                                          std::string name,
                                          std::string key,
                                          bool optional_key,
                                          bool html) {
    // TODO: This is currently blocking (no bueno), should be changed.
    // Especially for bad connections
    BasedConnectOpt opts = {
        .cluster = cluster,
        .org = org,
        .project = project,
        .env = env,
        .name = name.empty() ? "@based/env-hub" : name,
        .key = key,
        .optional_key = optional_key,
    };

    std::string url = m_con.discover_service(opts, html);
    return url;
}

void BasedClient::_connect_to_url(std::string url) {
    m_con.set_message_handler([&](std::string msg) { on_message(msg); });
    m_con.set_open_handler([&]() { on_open(); });
    m_con.connect_to_uri(url);
}

void BasedClient::connect(std::string cluster,
                          std::string org,
                          std::string project,
                          std::string env,
                          std::string name,
                          std::string key,
                          bool optional_key) {
    m_con.set_message_handler([&](std::string msg) { on_message(msg); });
    m_con.set_open_handler([&]() { on_open(); });
    m_con.connect(cluster, org, project, env, key, optional_key);
}

void BasedClient::disconnect() {
    m_con.disconnect();
}

int BasedClient::observe(std::string name,
                         std::string payload,
                         /**
                          * Callback that the observable will trigger.
                          */
                         void (*cb)(const char* /*data*/,
                                    checksum_t /*checksum*/,
                                    const char* /*error*/,
                                    int /*sub_id*/)) {
    /**
     * Each observable must be stored in memory, in case the connection drops.
     * So there's a queue, which is emptied on drain, but is refilled with the observables
     * stored in memory in the event of a reconnection.
     *
     * These observables all have a list of listeners. Each listener has a cb callback.
     *
     * When all listeners are removed with .unobserve, the observable should be removed
     * and the unobserve request should be queued, to let the server know.
     */

    auto obs_id = make_obs_id(name, payload);
    auto sub_id = m_sub_id++;

    if (m_active_observables.find(obs_id) == m_active_observables.end()) {
        // first time this query is observed
        // encode request
        checksum_t checksum = 0;

        if (m_cache.find(obs_id) != m_cache.end()) {
            // if cache for this obs exists
            checksum = m_cache.at(obs_id).second;
        }

        std::vector<uint8_t> msg = Utility::encode_observe_message(obs_id, name, payload, checksum);

        // add encoded request to queue
        m_observe_queue.push_back(msg);

        // add encoded request to map of observables
        m_active_observables[obs_id] = new Observable(name, payload);

        // add subscriber to list of subs for this observable
        m_obs_to_subs[obs_id] = std::set<sub_id_t>{sub_id};

        // record what obs this sub is for, to delete it later
        m_sub_to_obs[sub_id] = obs_id;

        // add cb for this sub
        m_sub_callback[sub_id] = cb;

        // add on_error for this sub if on_error is present (overload?)
    } else {
        // this query has already been requested once, only add subscriber,
        // dont send a new request.

        // add subscriber to that observable
        m_obs_to_subs.at(obs_id).insert(sub_id);

        // record what obs this sub is for, to delete it later
        m_sub_to_obs[sub_id] = obs_id;

        // add cb for this new sub
        m_sub_callback[sub_id] = cb;
    }

    drain_queues();

    return sub_id;
}

int BasedClient::get(std::string name,
                     std::string payload,
                     void (*cb)(const char* /*data*/, const char* /*error*/, int /*sub_id*/)) {
    auto obs_id = make_obs_id(name, payload);
    auto sub_id = m_sub_id++;

    // if obs_id exists in get_subs, add new sub to list
    if (m_obs_to_gets.find(obs_id) != m_obs_to_gets.end()) {
        m_obs_to_gets.at(obs_id).insert(sub_id);
    } else {  // else create it and then add it
        m_obs_to_gets[obs_id] = std::set<sub_id_t>{sub_id};
    }
    m_get_sub_callbacks[sub_id] = cb;
    // is there an active obs? if so, do nothing (get will trigger on next update)
    // if there isnt, queue request
    if (m_active_observables.find(obs_id) == m_active_observables.end()) {
        checksum_t checksum = 0;

        if (m_cache.find(obs_id) != m_cache.end()) {
            // if cache for this obs exists
            checksum = m_cache.at(obs_id).second;
        }
        std::vector<uint8_t> msg = Utility::encode_get_message(obs_id, name, payload, checksum);
        m_get_queue.push_back(msg);
        drain_queues();
    }

    return sub_id;
}

void BasedClient::unobserve(int sub_id) {
    if (m_sub_to_obs.find(sub_id) == m_sub_to_obs.end()) {
        BASED_LOG("No subscription found with sub_id %d", sub_id);
        return;
    }
    auto obs_id = m_sub_to_obs.at(sub_id);

    // remove sub from list of subs for that observable
    m_obs_to_subs.at(obs_id).erase(sub_id);

    // remove on_data callback
    m_sub_callback.erase(sub_id);

    // remove sub to obs mapping for removed sub
    m_sub_to_obs.erase(sub_id);

    // if the list is now empty, add request to unobserve to queue
    if (m_obs_to_subs.at(obs_id).empty()) {
        std::vector<uint8_t> msg = Utility::encode_unobserve_message(obs_id);
        m_unobserve_queue.push_back(msg);
        // and remove the obs from the map of active ones.
        delete m_active_observables.at(obs_id);
        m_active_observables.erase(obs_id);
        // and the vector of listeners, since it's now empty we can free the memory
        m_obs_to_subs.erase(obs_id);
    }
    drain_queues();
}

int BasedClient::call(std::string name,
                      std::string payload,
                      void (*cb)(const char* /*data*/, const char* /*error*/, int /*request_id*/)) {
    m_request_id++;
    if (m_request_id > 16777215) {
        m_request_id = 0;
    }
    auto id = m_request_id;
    m_call_callbacks[id] = cb;
    // encode the message
    std::vector<uint8_t> msg = Utility::encode_function_message(id, name, payload);
    m_function_queue.push_back(msg);
    drain_queues();
    return m_request_id;
}

void BasedClient::set_auth_state(std::string state, void (*cb)(const char*)) {
    if (m_auth_in_progress) return;

    m_auth_request_state = state;
    m_auth_in_progress = true;
    m_auth_callback = cb;

    m_auth_queue = Utility::encode_auth_message(state);
    drain_queues();
}

int BasedClient::channel_subscribe(std::string name,
                                   std::string payload,
                                   void (*cb)(const char* /*data*/,
                                              const char* /*error*/,
                                              int /*sub_id*/)) {
    auto obs_id = make_obs_id(name, payload);
    auto sub_id = m_sub_id++;

    if (m_active_channels.find(obs_id) == m_active_channels.end()) {
        // first time this channel is observed

        std::vector<uint8_t> msg =
            Utility::encode_subscribe_channel_message(obs_id, name, payload, false);

        m_channel_sub_queue.push_back(msg);

        m_active_channels[obs_id] = new Observable(name, payload);

        m_channel_to_subs[obs_id] = std::set<sub_id_t>{sub_id};

        m_sub_to_channel[sub_id] = obs_id;

        m_channel_callback[sub_id] = cb;
    } else {
        // this query has already been requested once, only add subscriber,
        // dont send a new request.

        m_channel_to_subs.at(obs_id).insert(sub_id);

        m_sub_to_channel[sub_id] = obs_id;

        m_channel_callback[sub_id] = cb;
    }

    drain_queues();

    return sub_id;
}

void BasedClient::channel_publish(std::string name, std::string payload, std::string message) {
    auto obs_id = make_obs_id(name, payload);

    if (m_active_publish_channels.find(obs_id) == m_active_publish_channels.end()) {
        m_active_publish_channels[obs_id] = new Observable(name, payload);
    }

    std::vector<uint8_t> msg = Utility::encode_publish_channel_message(obs_id, message);

    m_channel_publish_queue.push_back(msg);

    drain_queues();
}

void BasedClient::channel_unsubscribe(int sub_id) {
    if (m_sub_to_channel.find(sub_id) == m_sub_to_channel.end()) {
        BASED_LOG("No subscription found with sub_id %d", sub_id);
        return;
    }
    auto obs_id = m_sub_to_channel.at(sub_id);

    // remove sub from list of subs for that observable
    m_channel_to_subs.at(obs_id).erase(sub_id);

    // remove on_data callback
    m_sub_callback.erase(sub_id);

    // remove sub to obs mapping for removed sub
    m_sub_to_channel.erase(sub_id);

    // if the list is now empty, add request to unobserve to queue
    if (m_channel_to_subs.at(obs_id).empty()) {
        std::vector<uint8_t> msg = Utility::encode_unobserve_message(obs_id);
        m_unobserve_queue.push_back(msg);
        // and remove the obs from the map of active ones.
        delete m_active_channels.at(obs_id);
        m_active_channels.erase(obs_id);
        // and the vector of listeners, since it's now empty we can free the memory
        m_channel_to_subs.erase(obs_id);
    }
    drain_queues();
}

/////////////////////////////////////////////////////////////
/////////////////// End of client methods ///////////////////
/////////////////////////////////////////////////////////////

void BasedClient::drain_queues() {
    if (m_con.status() != ConnectionStatus::OPEN) {
        // std::cerr << "Connection is unavailable, status = " << m_con.status() << std::endl;
        return;
    }

    std::vector<uint8_t> buff;

    if (!m_auth_queue.empty()) {
        buff.insert(buff.end(), m_auth_queue.begin(), m_auth_queue.end());
        m_auth_queue.clear();
    }

    if (!m_channel_sub_queue.empty()) {
        for (auto msg : m_channel_sub_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_channel_sub_queue.clear();
    }

    if (!m_channel_unsub_queue.empty()) {
        for (auto msg : m_channel_unsub_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_channel_unsub_queue.clear();
    }

    if (!m_channel_publish_queue.empty()) {
        for (auto msg : m_channel_publish_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_channel_publish_queue.clear();
    }

    if (!m_observe_queue.empty()) {
        for (auto msg : m_observe_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_observe_queue.clear();
    }

    if (!m_unobserve_queue.empty()) {
        for (auto msg : m_unobserve_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_unobserve_queue.clear();
    }

    if (!m_function_queue.empty()) {
        for (auto msg : m_function_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_function_queue.clear();
    }

    if (!m_get_queue.empty()) {
        for (auto msg : m_get_queue) {
            buff.insert(buff.end(), msg.begin(), msg.end());
        }
        m_get_queue.clear();
    }

    if (!buff.empty()) {
        if (m_con.status() == ConnectionStatus::OPEN) {
            m_con.send(buff);
        }
    }
}

void BasedClient::request_full_data(obs_id_t obs_id) {
    if (m_active_observables.find(obs_id) == m_active_observables.end()) {
        return;
    }
    auto obs = m_active_observables.at(obs_id);
    auto msg = Utility::encode_observe_message(obs_id, obs->name, obs->payload, 0);
    m_observe_queue.push_back(msg);
    drain_queues();
}

void BasedClient::on_open() {
    // TODO: must reencode the obs request with the latest checksum.
    //       either change the checksum in the encoded request (harder probs) or
    //       just encode it on drain queue rather than on .observe,
    //       changing the data structure a bit
    for (auto el : m_active_observables) {
        Observable* obs = el.second;
        auto msg = Utility::encode_observe_message(el.first, obs->name, obs->payload, 0);
        m_observe_queue.push_back(msg);
    }
    drain_queues();
}

void BasedClient::on_message(std::string message) {
    int32_t header = Utility::read_header(message);
    int32_t type = Utility::get_payload_type(header);
    int32_t len = Utility::get_payload_len(header);
    int32_t is_deflate = Utility::get_payload_is_deflate(header);

    switch (type) {
        case IncomingType::FUNCTION_DATA: {
            req_id_t id = (req_id_t)Utility::read_bytes_from_string(message, 4, 3);

            if (m_call_callbacks.find(id) != m_call_callbacks.end()) {
                auto fn = m_call_callbacks.at(id);
                if (len != 3) {
                    int start = 7;
                    int end = len + 4;
                    std::string payload = is_deflate
                                              ? Utility::inflate_string(message.substr(start, end))
                                              : message.substr(start, end);
                    fn(payload.c_str(), "", id);
                } else {
                    fn("", "", id);
                }
                // Listener has fired, remove it from the map.
                m_call_callbacks.erase(id);
            }
        }
            return;
        case IncomingType::SUBSCRIPTION_DATA: {
            obs_id_t obs_id = (obs_id_t)Utility::read_bytes_from_string(message, 4, 8);
            uint64_t checksum = Utility::read_bytes_from_string(message, 12, 8);

            int start = 20;  // size of header
            int end = len + 4;
            std::string payload = "";
            if (len != 16) {
                payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                     : message.substr(start, end);
            }

            m_cache[obs_id].first = payload;
            m_cache[obs_id].second = checksum;

            if (m_obs_to_subs.find(obs_id) != m_obs_to_subs.end()) {
                for (auto sub_id : m_obs_to_subs.at(obs_id)) {
                    auto fn = m_sub_callback.at(sub_id);
                    fn(payload.c_str(), checksum, "", sub_id);
                }
            }

            if (m_obs_to_gets.find(obs_id) != m_obs_to_gets.end()) {
                for (auto sub_id : m_obs_to_gets.at(obs_id)) {
                    auto fn = m_get_sub_callbacks.at(sub_id);
                    fn(payload.c_str(), "", sub_id);
                    m_get_sub_callbacks.erase(sub_id);
                }
                m_obs_to_gets.at(obs_id).clear();
            }
        }
            return;
        case IncomingType::SUBSCRIPTION_DIFF_DATA: {
            obs_id_t obs_id = (obs_id_t)Utility::read_bytes_from_string(message, 4, 8);
            uint64_t checksum = Utility::read_bytes_from_string(message, 12, 8);
            uint64_t prev_checksum = Utility::read_bytes_from_string(message, 20, 8);

            uint64_t cached_checksum = 0;

            if (m_cache.find(obs_id) != m_cache.end()) {
                cached_checksum = m_cache.at(obs_id).second;
            }

            if (cached_checksum == 0 || (cached_checksum != prev_checksum)) {
                request_full_data(obs_id);
                return;
            }

            int start = 28;  // size of header
            int end = len + 4;
            std::string patch = "";
            if (len != 24) {
                patch = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                   : message.substr(start, end);
            }

            std::string patched_payload = "";

            if (!patch.empty()) {
                json value = json::parse(m_cache.at(obs_id).first);
                json patch_json = json::parse(patch);
                json res = Diff::apply_patch(value, patch_json);
                patched_payload = res.dump();

                m_cache[obs_id].first = patched_payload;
                m_cache[obs_id].second = checksum;
            }

            if (m_obs_to_subs.find(obs_id) != m_obs_to_subs.end()) {
                for (auto sub_id : m_obs_to_subs.at(obs_id)) {
                    auto fn = m_sub_callback.at(sub_id);
                    fn(patched_payload.c_str(), checksum, "", sub_id);
                }
            }

            if (m_obs_to_gets.find(obs_id) != m_obs_to_gets.end()) {
                for (auto sub_id : m_obs_to_gets.at(obs_id)) {
                    auto fn = m_get_sub_callbacks.at(sub_id);
                    fn(patched_payload.c_str(), "", sub_id);
                    m_get_sub_callbacks.erase(sub_id);
                }
                m_obs_to_gets.at(obs_id).clear();
            }

        } break;
        case IncomingType::GET_DATA: {
            uint64_t obs_id = Utility::read_bytes_from_string(message, 4, 8);
            if (m_obs_to_gets.find(obs_id) != m_obs_to_gets.end() &&
                m_cache.find(obs_id) != m_cache.end()) {
                for (auto sub_id : m_obs_to_gets.at(obs_id)) {
                    auto fn = m_get_sub_callbacks.at(sub_id);
                    fn(m_cache.at(obs_id).first.c_str(), "", sub_id);
                    m_get_sub_callbacks.erase(sub_id);
                }
                m_obs_to_gets.at(obs_id).clear();
            }
        } break;
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
            m_auth_in_progress = false;
            if (m_auth_callback) {
                m_auth_callback(m_auth_state.c_str());
                // we remove the callback because we don't want it to fire again if the server
                // updates the client's auth state
                m_auth_callback = NULL;
            }
        }
            return;
        case IncomingType::ERROR_DATA: {
            int32_t start = 4;
            int32_t end = len + 4;
            std::string payload = "{}";
            if (len != 3) {
                payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                     : message.substr(start, end);
            }

            json error = json::parse(payload);
            // fire once
            if (error.find("requestId") != error.end()) {
                auto id = error.at("requestId");
                // std::cout << "id = " << id << std::endl;

                if (m_call_callbacks.find(id) != m_call_callbacks.end()) {
                    auto fn = m_call_callbacks.at(id);
                    fn("", payload.c_str(), id);
                    m_call_callbacks.erase(id);
                }
                if (m_obs_to_gets.find(id) != m_obs_to_gets.end()) {
                    for (auto get_id : m_obs_to_gets.at(id)) {
                        auto fn = m_get_sub_callbacks.at(get_id);
                        fn("", payload.c_str(), id);
                        m_get_sub_callbacks.erase(get_id);
                    }
                    m_obs_to_gets.erase(id);
                }
            } else if (error.find("observableId") != error.end()) {
                // destroy observable
                auto obs_id = error.at("observableId");

                m_active_observables.erase(obs_id);

                if (m_obs_to_subs.find(obs_id) != m_obs_to_subs.end()) {
                    for (auto sub_id : m_obs_to_subs.at(obs_id)) {
                        if (m_sub_callback.find(sub_id) != m_sub_callback.end()) {
                            auto fn = m_sub_callback.at(sub_id);
                            fn("", 0, payload.c_str(), sub_id);
                        }
                        m_obs_to_subs.erase(sub_id);
                        m_sub_to_obs.erase(sub_id);
                    }
                }

                if (m_obs_to_gets.find(obs_id) != m_obs_to_gets.end()) {
                    for (auto sub_id : m_obs_to_gets.at(obs_id)) {
                        auto fn = m_get_sub_callbacks.at(sub_id);
                        fn("", payload.c_str(), sub_id);
                        m_get_sub_callbacks.erase(sub_id);
                    }
                    m_obs_to_gets.at(obs_id).clear();
                }
            } else if (error.find("channelId") != error.end()) {
                auto channel_id = error.at("observableId");

                m_active_channels.erase(channel_id);

                if (m_channel_to_subs.find(channel_id) != m_channel_to_subs.end()) {
                    for (auto sub_id : m_channel_to_subs.at(channel_id)) {
                        if (m_channel_callback.find(sub_id) != m_channel_callback.end()) {
                            auto fn = m_channel_callback.at(sub_id);
                            fn("", payload.c_str(), sub_id);
                        }
                        m_channel_to_subs.erase(sub_id);
                        m_sub_to_channel.erase(sub_id);
                    }
                }
            } else {
                std::cerr << "ERROR MESSAGE WITHOUT ID = " << error << std::endl;
            }
        }
            return;
        case IncomingType::CHANNEL_REPUBLISH: {
            obs_id_t obs_id = Utility::read_bytes_from_string(message, 4, 8);
            // if (m_active_channels.find(obs_id) != m_active_channels.end()) {
            auto obs = m_active_publish_channels.at(obs_id);

            // first time this channel is observed

            std::vector<uint8_t> msg =
                Utility::encode_subscribe_channel_message(obs_id, obs->name, obs->payload, true);

            m_channel_sub_queue.push_back(msg);
            drain_queues();
        }
            return;
        case IncomingType::CHANNEL_MESSAGE: {
            auto sub_type = Utility::read_bytes_from_string(message, 4, 1);
            if (sub_type == 0) {
                obs_id_t obs_id = Utility::read_bytes_from_string(message, 5, 8);
                int start = 13;
                int end = len + 5;

                std::string payload = "";
                if (len != 9) {
                    payload = is_deflate ? Utility::inflate_string(message.substr(start, end))
                                         : message.substr(start, end);
                }
                if (m_channel_to_subs.find(obs_id) != m_channel_to_subs.end()) {
                    for (auto sub_id : m_channel_to_subs.at(obs_id)) {
                        auto fn = m_channel_callback.at(sub_id);
                        fn(payload.c_str(), "", sub_id);
                    }
                } else {
                    BASED_LOG("Channel message received, but no listeners with obs_id %llu found",
                              obs_id);
                }

            } else {
                BASED_LOG("Wrong subtype received... %lld", sub_type);
            }
        }
            return;
        default:
            std::cerr << ">> Unknown payload type \"" << type << "\" received." << std::endl;
            return;
    }
};
