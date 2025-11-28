#pragma once

#include <string>
#include <vector>
#include <cstdint>

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const uint8_t* data, size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    uint32_t val = 0;
    int valb = -6;

    for (size_t i = 0; i < len; i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            out.push_back(kBase64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        out.push_back(kBase64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (out.size() % 4) out.push_back('=');

    return out;
}
