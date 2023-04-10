#include "utility.hpp"

#include <zlib.h>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <json.hpp>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

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

std::vector<uint8_t> Utility::encode_function_message(int32_t id,
                                                      std::string name,
                                                      std::string& payload) {
    std::vector<uint8_t> buff;
    int32_t len = 7;
    len += 1 + name.length();

    int32_t is_deflate = 0;

    std::string p;
    if (payload.length() > 0) {
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

std::vector<uint8_t> Utility::encode_observe_message(uint32_t id,
                                                     std::string name,
                                                     std::string& payload,
                                                     uint64_t checksum) {
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
    if (payload.length() > 0) {
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

std::vector<uint8_t> Utility::encode_unobserve_message(uint32_t obs_id) {
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
std::vector<uint8_t> Utility::encode_get_message(uint64_t id,
                                                 std::string name,
                                                 std::string& payload,
                                                 uint64_t checksum) {
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
    if (payload.length() > 0) {
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
    if (auth_state.length() > 0) {
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
    if (p.length() > 0) {
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

int64_t Utility::read_bytes_from_string(std::string& buff, int start, int len) {
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

std::string Utility::encode(std::string& input) {
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

std::string Utility::decode(std::string& input) {
    std::vector<std::string> encode_chars({"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});
    std::map<std::string, std::string> reverse_char_map{
        // clang-format off
        {"a", "localhost"},
        {"b", "based.io"},
        {"c", "based.dev"},
        {"d", "@based"},
        {"e", "/env-hub"},
        {"f", "admin"},
        {"g", "hub"},
        {"h", "-"},
        {"i", ","},
        {"j", "."},
        {"k", "?"},
        {"l", "0"},
        {"m", "1"},
        {"n", "2"},
        {"o", "3"},
        {"p", "4"},
        {"q", "5"},
        {"r", "6"},
        {"s", "7"},
        {"t", "8"},
        {"u", "9"}
        // clang-format on
    };

    std::string str = "";
    for (int i = 0; i < input.length(); i++) {
        std::string c{input.at(i)};
        if (std::find(encode_chars.begin(), encode_chars.end(), c) != encode_chars.end()) {
            std::string key{input.at(i + 1)};
            str += reverse_char_map.at(key);
            i++;
        } else {
            str += c;
        }
    }
    return str;
}