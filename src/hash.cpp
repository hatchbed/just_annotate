#include <just_annotate/hash.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <openssl/md5.h>
#include <openssl/sha.h>
#include <spdlog/spdlog.h>

std::string sha256(const std::string& path) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    std::ifstream file(path, std::ifstream::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);
        return {};
    }

    const size_t buffer_size = 32768;
    char* buffer = new char[buffer_size];
    while (file.good()) {
        file.read(buffer, buffer_size);
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    delete[] buffer;
    file.close();

    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}

std::string md5sum(const std::string& path) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_CTX md5;
    MD5_Init(&md5);

    std::ifstream file(path, std::ifstream::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);
        return {};
    }

    const size_t buffer_size = 32768;
    char* buffer = new char[buffer_size];
    while (file.good()) {
        file.read(buffer, buffer_size);
        MD5_Update(&md5, buffer, file.gcount());
    }
    delete[] buffer;
    file.close();

    MD5_Final(hash, &md5);

    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}
