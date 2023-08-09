#include "utility.hpp"

#include <zlib.h>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

std::string string_from_char_code(uint8_t c) {
    std::string res(1, (char)c);
    return res;
}

int Utility::string_hash(const std::string& str, unsigned int hash) {
    int i = str.length();
    while (i) {
        const char c = str[--i];
        hash = (hash * 33) ^ c;
    }
    return hash;
}

unsigned long int Utility::hash_env(std::string org,
                                    std::string project,
                                    std::string env,
                                    std::string cluster) {
    std::vector<std::string> keys{"cluster", "env", "org", "project"};
    std::vector<std::string> values{cluster, env, org, project};

    int32_t hash = 5381;
    int32_t hash2 = 52711;
    std::string fl = "__len:41";
    hash = string_hash(fl, hash);
    hash2 = string_hash(fl, hash2);

    for (int i = 0; i < 4; i++) {
        auto key = keys.at(i);
        auto value = values.at(i);
        auto f = key + ":" + value;
        hash = string_hash(f, hash);
        hash2 = string_hash(f, hash2);
    }

    uint32_t f = hash;
    uint64_t first = ((uint64_t)f * (uint64_t)4096);
    uint32_t second = hash2;
    uint64_t res = first + second;
    return res;
}

std::string Utility::base36_encode(uint64_t value) {
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::stringstream ss;
    do {
        ss << charset[value % 36];
        value /= 36;
    } while (value != 0);
    std::string result = ss.str();
    std::reverse(result.begin(), result.end());
    return result;
}

// std::string Utility::friendly_id(std::string& cluster,
//                                  std::string& org,
//                                  std::string& project,
//                                  std::string& env) {
//     std::string str = cluster + ":" + org + ":" + project + ":" + env;
//     unsigned int hash = string_hash(str);
//     std::string friendlyId = std::to_string(hash);
//     std::string::iterator it;
//     for (it = friendlyId.begin(); it != friendlyId.end(); ++it) {
//         if (*it >= '0' && *it <= '9') {
//             *it = *it - '0' + 'a';
//         }
//     }
//     return friendlyId;
// }

std::vector<std::string> Utility::split_string(std::string input, std::string delimiter) {
    std::vector<std::string> res;

    if (delimiter == "") {
        for (int i = 0; i < input.size(); i++) {
            res.push_back(input.substr(i, 1));
        }
        return res;
    }

    size_t last = 0;
    size_t next = 0;
    while ((next = input.find(delimiter, last)) != std::string::npos) {
        res.push_back(input.substr(last, next - last));
        last = next + delimiter.length();
    }
    res.push_back(input.substr(last));
    return res;
}

/**
 * Courtesy of https://gist.github.com/arthurafarias/56fec2cd49a32f374c02d1df2b6c350f
 */
std::string Utility::encodeURIComponent(std::string decoded) {
    std::ostringstream oss;
    std::regex r("[!'\\(\\)*-.0-9A-Za-z_~]");

    for (char& c : decoded) {
        if (std::regex_match((std::string){c}, r)) {
            oss << c;
        } else {
            oss << "%" << std::uppercase << std::hex << (0xff & c);
        }
    }
    return oss.str();
}

std::string Utility::inflate_string(const std::string& str) {
    // Original version of this function found on https://panthema.net/2007/0328-ZLibString.html,
    // adapted here for our usage.

    // Copyright 2007 Timo Bingmann <tb@panthema.net>
    // Distributed under the Boost Software License, Version 1.0.
    // (See http://www.boost.org/LICENSE_1_0.txt)

    z_stream zs;  // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {  // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

std::string Utility::deflate_string(const std::string& str) {
    // Original version of this function found on https://panthema.net/2007/0328-ZLibString.html,
    // adapted here for our usage.

    // Copyright 2007 Timo Bingmann <tb@panthema.net>
    // Distributed under the Boost Software License, Version 1.0.
    // (See http://www.boost.org/LICENSE_1_0.txt)

    z_stream zs;  // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    // See https://www.zlib.net/manual.html#Advanced for details.
    // The windowBits argument Must match inflate_string and the js client

    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) !=
        Z_OK)
        throw(std::runtime_error("deflateInit failed while compressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();  // set the z_stream's input

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // retrieve the compressed bytes blockwise
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (outstring.size() < zs.total_out) {
            // append the block to the output string
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {  // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

void Utility::append_bytes(std::vector<uint8_t>& buff, uint64_t src, size_t size) {
    for (int i = 0; i < size; i++) {
        uint8_t byte = (src >> (8 * i)) & 0xff;
        buff.push_back(byte);
    }
}

void Utility::append_string(std::vector<uint8_t>& buff, std::string payload) {
    char const* data = payload.data();
    for (int i = 0; i < payload.length(); i++) {
        buff.push_back(data[i]);
    }
}
void Utility::append_header(std::vector<uint8_t>& buff,
                            int32_t type,
                            int32_t is_deflate,
                            int32_t len) {
    // must do int32_t arithmetics because of the js client
    int32_t meta = (type << 1) + is_deflate;
    int32_t value = (len << 4) + meta;
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (value >> (8 * i)) & 0xff;
        buff.push_back(byte);
    }
}

std::vector<uint8_t> Utility::encode_function_message(req_id_t id,
                                                      std::string name,
                                                      std::string& payload) {
    std::vector<uint8_t> buff;
    int32_t len = 7;
    len += 1 + name.length();

    int32_t is_deflate = 0;

    std::string p;
    if (!payload.empty()) {
        if (payload.length() > 150) {
            is_deflate = 1;
            p = deflate_string(payload);

            // char const* buff = p.data();

            // for (int i = 0; i < p.length(); i++) {
            //     std::cout << "buff " << i << " = " << (int32_t)buff[i] << ",\t 0x" << std::hex
            //               << +buff[i] << std::dec << std::endl;
            // }

        } else {
            p = payload;
        }

        len += p.length();
    }
    append_header(buff, 0, is_deflate, len);
    append_bytes(buff, id, 3);
    buff.push_back(name.length());
    append_string(buff, name);
    if (p.length()) {
        append_string(buff, p);
    }

    // std::cout << "> FINAL BUFFER" << std::endl;
    // for (int i = 0; i < buff.size(); i++) {
    //     std::cout << "buff " << i << " = " << (int)buff.at(i) << ",\t 0x" << std::hex <<
    //     +buff.at(i)
    //               << std::dec << std::endl;
    // }

    return buff;
}

std::vector<uint8_t> Utility::encode_observe_message(obs_id_t id,
                                                     std::string name,
                                                     std::string& payload,
                                                     checksum_t checksum) {
    // Type 1 = subscribe
    // | 4 header | 8 id | 8 checksum | 1 name length | * name | [* payload]

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id + 8 B checksum,
     * add the rest later based on payload and name.
     */
    int32_t len = 20;
    len += 1 + name.length();

    int32_t is_deflate = 0;

    std::string p;
    if (!payload.empty()) {
        if (payload.length() > 150) {
            is_deflate = 1;
            p = deflate_string(payload);
        } else {
            p = payload;
        }

        len += p.length();
    }
    append_header(buff, 1, is_deflate, len);
    append_bytes(buff, id, 8);
    append_bytes(buff, checksum, 8);
    buff.push_back(name.length());
    append_string(buff, name);
    if (p.length()) {
        append_string(buff, p);
    }

    return buff;
}

std::vector<uint8_t> Utility::encode_unobserve_message(obs_id_t obs_id) {
    // Type 2 = unsubscribe
    // | 4 header | 8 id |

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id
     */
    int32_t len = 12;

    append_header(buff, 2, 0, len);
    append_bytes(buff, obs_id, 8);
    return buff;
}
std::vector<uint8_t> Utility::encode_get_message(obs_id_t id,
                                                 std::string name,
                                                 std::string& payload,
                                                 checksum_t checksum) {
    // Type 3 = get
    // | 4 header | 8 id | 8 checksum | 1 name length | * name | [* payload]

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id + 8 B checksum,
     * add the rest later based on payload and name.
     */
    int32_t len = 20;
    len += 1 + name.length();

    int32_t is_deflate = 0;

    std::string p;
    if (!payload.empty()) {
        if (payload.length() > 150) {
            is_deflate = 1;
            p = deflate_string(payload);
        } else {
            p = payload;
        }

        len += p.length();
    }
    append_header(buff, 3, is_deflate, len);
    append_bytes(buff, id, 8);
    append_bytes(buff, checksum, 8);
    buff.push_back(name.length());
    append_string(buff, name);
    if (p.length() > 0) {
        append_string(buff, p);
    }

    return buff;
}

std::vector<uint8_t> Utility::encode_subscribe_channel_message(obs_id_t id,
                                                               std::string name,
                                                               std::string& payload,
                                                               bool is_request_subscriber) {
    // Type 5 = subscribe
    // | 4 header | 8 id | 1 name length | * name | * payload |

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id + 8 B checksum,
     * add the rest later based on payload and name.
     */
    int32_t len = 4;
    len += 1 + name.length();

    int32_t is_deflate = is_request_subscriber ? 1 : 0;

    if (!payload.empty()) {
        // do not deflate
        len += payload.length();
    }

    len += 8;

    append_header(buff, 5, is_deflate, len);
    append_bytes(buff, id, 8);
    buff.push_back(name.length());
    append_string(buff, name);

    if (!payload.empty()) {
        append_string(buff, payload);
    }

    return buff;
}

std::vector<uint8_t> Utility::encode_unsubscribe_channel_message(obs_id_t id) {
    // Type 7 = channel__unsubscribe
    // | 4 header | 8 id |

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id + 8 B checksum,
     * add the rest later based on payload and name.
     */
    append_header(buff, 7, false, 12);
    append_bytes(buff, id, 8);

    return buff;
}

std::vector<uint8_t> Utility::encode_publish_channel_message(obs_id_t id, std::string& payload) {
    // | 4 header | 8 id | * payload |

    std::vector<uint8_t> buff;

    int32_t len = 12;

    int32_t is_deflate = 0;

    std::string p;
    if (!payload.empty()) {
        if (payload.length() > 150) {
            is_deflate = 1;
            p = deflate_string(payload);
        } else {
            p = payload;
        }

        len += p.length();
    }

    append_header(buff, 6, is_deflate, len);
    append_bytes(buff, id, 8);

    if (!p.empty()) {
        append_string(buff, p);
    }

    return buff;
}

std::vector<uint8_t> Utility::encode_auth_message(std::string& auth_state) {
    // Type 4 = auth
    // | 4 header | * payload

    std::vector<uint8_t> buff;

    /**
     * Length in bytes. 4 B header + 8 B id + 8 B checksum,
     * add the rest later based on payload and name.
     */
    int32_t len = 4;
    int32_t is_deflate = 0;

    std::string p;
    if (!auth_state.empty()) {
        if (auth_state.length() > 150) {
            is_deflate = 1;
            p = deflate_string(auth_state);
        } else {
            p = auth_state;
        }

        len += p.length();
    }
    append_header(buff, 4, is_deflate, len);
    // append_bytes(buff, p.length, )
    if (!p.empty()) {
        append_string(buff, p);
    }

    return buff;
}

int32_t Utility::get_payload_type(int32_t header) {
    int32_t meta = header & 15;
    int32_t type = meta >> 1;
    return type;
}
int32_t Utility::get_payload_len(int32_t header) {
    int32_t len = header >> 4;
    return len;
}
int32_t Utility::get_payload_is_deflate(int32_t header) {
    int32_t meta = header & 15;
    int32_t is_deflate = meta & 1;
    return is_deflate;
}
int32_t Utility::read_header(std::string buff) {
    // header starts at index[0] and is 4 bytes long
    char const* data = buff.data();
    int32_t res = 0;
    size_t s = 3;
    for (int i = s; i >= 0; i--) {
        res = res * 256 + (uint8_t)data[i];
    }
    return res;
}

uint64_t Utility::read_bytes_from_string(std::string& buff, int start, int len) {
    char const* data = buff.data();
    int32_t res = 0;
    size_t s = len - 1 + start;  // len - 1 + start;
    for (int i = s; i >= start; i--) {
        res = res * 256 + (uint8_t)data[i];
    }
    return res;
}

std::string cycle_chars(std::vector<std::string> encode_chars, int encode_char_index) {
    if (encode_char_index % 2) {
        return encode_chars.at(encode_chars.size() - encode_char_index);
    }
    return encode_chars.at(encode_char_index);
}

std::string Utility::encode(std::string input) {
    int encode_char_index = 0;
    int char_len = 9;
    std::vector<std::string> encode_chars({"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});
    int encode_chars_len = encode_chars.size();

    std::map<std::string, std::string> char_map{
        // clang-format off
        {"0", "l",},
        {"1", "m",},
        {"2", "n",},
        {"3", "o",},
        {"4", "p",},
        {"5", "q",},
        {"6", "r",},
        {"7", "s",},
        {"8", "t",},
        {"9", "u",},
        {"localhost", "a",},
        {"based.io", "b",},
        {"based.dev", "c",},
        {"@based", "d",},
        {"/env-hub", "e",},
        {"admin", "f",},
        {"hub", "g",},
        {"-", "h",},
        {",", "i",},
        {".", "j",},
        {"?", "k"},
        // clang-format on
    };

    std::string str = "";
    for (int i = 0; i < input.length(); i++) {
        bool added = false;
        for (int j = char_len - 1; j > -1; j--) {
            if (i + j > input.length() - 1) {
                continue;
            }
            std::string s = "";
            for (int n = 0; n < j + 1; n++) {
                s += input.at(i + n);
            }
            if (char_map.find(s) != char_map.end()) {
                encode_char_index++;
                if (encode_char_index >= encode_chars_len) {
                    encode_char_index = 0;
                }

                str += cycle_chars(encode_chars, encode_char_index) + char_map.at(s);
                i += s.length() - 1;
                j = -1;
                added = true;
            }
        }
        if (!added) {
            str += input.at(i);
        }
    }
    return str;
}

std::string Utility::decode(std::string input, std::vector<std::string> encode_chars) {
    std::map<std::string, std::string> reverse_char_map;
    int char_len = 1;
    std::vector<std::string> chars{
        ",",        ".based.dev", "localhost:", "localhost", "based.io", "based.dev", "@based",
        "/env-hub", "admin",      "hub",        "900",       "90",       "443",       "80",
        ":",        "%",          "/",          "=",         "<",        "?",         ".",
    };

    std::vector<std::string> real_chars = chars;
    real_chars.insert(real_chars.end(), encode_chars.begin(), encode_chars.end());

    std::vector<std::string> replacement;

    uint8_t idx = 0;
    for (auto& v : real_chars) {
        if (v.length() > char_len) {
            char_len = v.length();
        }
        if (idx > 25) {
            replacement.push_back(std::to_string(idx - 26));
        } else {
            replacement.push_back(string_from_char_code(97 + idx));
        }
        idx++;
    }

    for (size_t i = 0; i < real_chars.size(); i++) {
        reverse_char_map[replacement.at(i)] = real_chars.at(i);
        // std::cout << "{ " << replacement.at(i) << ", " << reverse_char_map.at(replacement.at(i))
        //           << "}" << std::endl;
    }

    // std::cout << "-------> real_chars.size: " << real_chars.size() << std::endl;
    // std::cout << "-------> input.size: " << input.size() << std::endl;
    // std::cout << "-------> encode_chars.size: " << encode_chars.size() << std::endl;
    // std::cout << "-------> reverse_char_map.size: " << reverse_char_map.size() << std::endl;

    std::string str = "";
    for (int i = 0; i < input.length(); i++) {
        // std::cout << "++++++++ input.at(" << i << ") = " << input.at(i) << std::endl;
        std::string c{input.at(i)};
        if (std::find(encode_chars.begin(), encode_chars.end(), c) != encode_chars.end()) {
            std::string key{input.at(i + 1)};
            // std::cout << "++++++++ key = " << key << std::endl;
            str += reverse_char_map.at(key);
            // std::cout << "++++++++ reverse_char_map.at(key) = " << reverse_char_map.at(key)
            //   << std::endl;
            i++;
        } else {
            str += c;
        }
    }
    return str;
}