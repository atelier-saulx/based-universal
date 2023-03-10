#ifndef BASED_OUTGOING_H
#define BASED_OUTGOING_H

#include <string>
#include <vector>

// #define BASED_VERBOSE 1

namespace Utility {
std::string inflate_string(const std::string& str);
std::string deflate_string(const std::string& str);
void append_bytes(std::vector<uint8_t>& buff, uint64_t src, size_t size);
void append_string(std::vector<uint8_t>& buff, std::string payload);
void append_header(std::vector<uint8_t>& buff, int32_t type, int32_t is_deflate, int32_t len);
std::vector<uint8_t> encode_function_message(int32_t id, std::string name, std::string& payload);
std::vector<uint8_t> encode_observe_message(uint32_t id,
                                            std::string name,
                                            std::string& payload,
                                            uint64_t checksum);
std::vector<uint8_t> encode_unobserve_message(uint32_t obs_id);
std::vector<uint8_t> encode_get_message(uint64_t id,
                                        std::string name,
                                        std::string& payload,
                                        uint64_t checksum);
std::vector<uint8_t> encode_auth_message(std::string& auth_state);
int32_t get_payload_type(int32_t header);
int32_t get_payload_len(int32_t header);
int32_t get_payload_is_deflate(int32_t header);
int32_t read_header(std::string buff);
int64_t read_bytes_from_string(std::string& buff, int start, int len);

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