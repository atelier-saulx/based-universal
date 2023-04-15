#ifndef BASED_OUTGOING_H
#define BASED_OUTGOING_H

#include <string>
#include <vector>

// TODO: check for signedness
/**
 * 8 bytes in the protocol
 */
using checksum_t = uint64_t;

/**
 * 8 bytes in the protocol
 */
using obs_id_t = uint64_t;

/**
 * This is represented by 3 bytes in the protocol, so one must check for overflows
 */
using req_id_t = uint32_t;

/**
 * This is an internal counter that is not sent to the server TODO: verify this
 */
using sub_id_t = uint32_t;

// #define BASED_VERBOSE 1

namespace Utility {
std::string inflate_string(const std::string& str);
std::string deflate_string(const std::string& str);

void append_bytes(std::vector<uint8_t>& buff, uint64_t src, size_t size);
void append_string(std::vector<uint8_t>& buff, std::string payload);
void append_header(std::vector<uint8_t>& buff, int32_t type, int32_t is_deflate, int32_t len);

std::vector<uint8_t> encode_function_message(req_id_t id, std::string name, std::string& payload);
std::vector<uint8_t> encode_observe_message(obs_id_t obs_id,
                                            std::string name,
                                            std::string& payload,
                                            checksum_t checksum);
std::vector<uint8_t> encode_unobserve_message(obs_id_t obs_id);
std::vector<uint8_t> encode_get_message(obs_id_t obs_id,
                                        std::string name,
                                        std::string& payload,
                                        checksum_t checksum);
std::vector<uint8_t> encode_subscribe_channel_message(obs_id_t obs_id,
                                                      std::string name,
                                                      std::string& payload,
                                                      bool is_request_subscriber);
std::vector<uint8_t> encode_publish_channel_message(obs_id_t id, std::string& payload);
std::vector<uint8_t> encode_auth_message(std::string& auth_state);

int32_t get_payload_type(int32_t header);
int32_t get_payload_len(int32_t header);
int32_t get_payload_is_deflate(int32_t header);

int32_t read_header(std::string buff);
uint64_t read_bytes_from_string(std::string& buff, int start, int len);

std::string encodeURIComponent(std::string decoded);
std::string encode(std::string str);
std::string decode(std::string str, std::vector<std::string> encode_chars);

std::vector<std::string> split_string(std::string input, std::string delimiter);

}  // namespace Utility

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#if BASED_VERBOSE
#define BASED_LOG(fmt, ...)                                                               \
    do {                                                                                  \
        std::fprintf(stdout, "[%s:%d] " fmt "\n", __FILENAME__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define BASED_LOG(fmt, ...)
#endif

#endif