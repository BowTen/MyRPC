#pragma once

#include <jsoncpp/json/json.h>
#include <time.h>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace btrpc {

#define LDBG 0
#define LINF 1
#define LERR 2

const char* log_level_str[] = {
    "DEBUG",
    "INFO",
    "ERROR"};

#define LDEFAUT LDBG

#define LOG(level, format, ...)                                                                                             \
    if (level >= LDEFAUT) {                                                                                                 \
        time_t t = time(0);                                                                                                 \
        struct tm* lt = localtime(&t);                                                                                      \
        char time_tmp[32] = {0};                                                                                            \
        strftime(time_tmp, 31, "%Y-%m-%d %T", lt);                                                                          \
        fprintf(stdout, "[%s][%s:%d][%s] " format "\n", btrpc::log_level_str[level], __FILE__, __LINE__, time_tmp, ##__VA_ARGS__); \
    }

#define DLOG(format, ...) LOG(LDBG, format, ##__VA_ARGS__)
#define ILOG(format, ...) LOG(LINF, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(LERR, format, ##__VA_ARGS__)

class JSON {
   public:
    static bool serialize(const Json::Value& val, std::string& body) {
        std::stringstream ss;
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        int ret = sw->write(val, &ss);
        if (ret != 0) {
            ELOG("serialize json failed");
            return false;
        }
        body = ss.str();
        return true;
    }
    static std::string serialize(const Json::Value& val) {
        std::stringstream ss;
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
        int ret = sw->write(val, &ss);
        if (ret != 0) {
            ELOG("serialize json failed");
            return "";
        }
        return ss.str();
    }

    static bool unserialize(const std::string& body, Json::Value& val) {
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        std::string errs;
        int ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
        if (ret == false) {
            ELOG("unserialize json failed: %s", errs.c_str());
            return false;
        }
        return true;
    }
};

class UUID {
   public:
    static std::string uuid() {
        std::stringstream ss;
        // 1. 构造⼀个机器随机数对象
        std::random_device rd;
        // 2. 以机器随机数为种⼦构造伪随机数对象
        std::mt19937 generator(rd());
        // 3. 构造限定数据范围的对象
        std::uniform_int_distribution<int> distribution(0, 255);
        // 4. ⽣成8个随机数，按照特定格式组织成为16进制数字字符的字符串
        for (int i = 0; i < 8; i++) {
            if (i == 4 || i == 6)
                ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
        }
        ss << "-";
        // 5. 定义⼀个8字节序号，逐字节组织成为16进制数字字符的字符串
        static std::atomic<size_t> seq(1);  // 00 00 00 00 00 00 00 01
        size_t cur = seq.fetch_add(1);
        for (int i = 7; i >= 0; i--) {
            if (i == 5)
                ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xFF);
        }
        return ss.str();
    }
};

}  // namespace btrpc