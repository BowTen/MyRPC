#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace btrpc {

typedef std::pair<std::string, int> Address;

struct AddrHash {
    size_t operator()(const Address& a) const {
        // 使用 std::hash 分别对 pair 的两个元素进行哈希，然后组合结果
        return std::hash<std::string>()(a.first) ^ (std::hash<int>()(a.second) << 1);
    }
};

const int INF = 0x3f3f3f3f;

#define KEY_METHOD "method"
#define KEY_PARAMS "parameters"
#define KEY_TOPIC_KEY "topic_key"
#define KEY_TOPIC_MSG "topic_msg"
#define KEY_OPTYPE "optype"
#define KEY_IDLE_COUNT "idle_count"
#define KEY_HOST "host"
#define KEY_HOST_IP "ip"
#define KEY_HOST_PORT "port"
#define KEY_RCODE "rcode"
#define KEY_RESULT "result"

enum class MType {
    REQ_RPC = 0,
    RSP_RPC,
	RSP_CONNECT,
    REQ_TOPIC,
    RSP_TOPIC,
    REQ_SERVICE,
    RSP_SERVICE
};

enum class RCode {
    RCODE_OK = 0,
	RCODE_CONNECT_OVERFLOW,
    RCODE_PARSE_FAILED,
    RCODE_ERROR_MSGTYPE,
    RCODE_INVALID_MSG,
    RCODE_DISCONNECTED,
    RCODE_INVALID_PARAMS,
    RCODE_NOT_FOUND_SERVICE,
    RCODE_INVALID_OPTYPE,
    RCODE_NOT_FOUND_TOPIC,
	RCODE_DUPLICATE_REGISTRY,
	RCODE_HEARTBEAT_WRONG,
    RCODE_INTERNAL_ERROR
};
static std::string errReason(RCode code) {
    static std::vector<std::string> err_map = {
        "成功处理！",
		"服务器连接超限！",
        "消息解析失败！",
        "消息类型错误！",
        "无效消息",
        "连接已断开！",
        "无效的Rpc参数！",
        "没有找到对应的服务！",
        "无效的操作类型",
        "没有找到对应的主题！",
		"重复注册服务！",
		"心跳检测失败！",
        "内部错误！"};
    if (code < RCode::RCODE_OK || code > RCode::RCODE_INTERNAL_ERROR)
        return "未知错误！";
    else
        return err_map[(int)code];
}

enum class RType {
    REQ_ASYNC = 0,
    REQ_CALLBACK
};

enum class TopicOptype {
    TOPIC_CREATE = 0,
    TOPIC_REMOVE,
    TOPIC_SUBSCRIBE,
    TOPIC_CANCEL,
    TOPIC_PUBLISH
};

enum class ServiceOptype {
    SERVICE_REGISTRY = 0,
	SERVICE_DEREGISTER,
	SERVICE_DETECT,
    SERVICE_DISCOVERY,
    SERVICE_ONLINE,
    SERVICE_OFFLINE,
	SERVICE_RETURN,
	SERVICE_UPDATE,
    SERVICE_UNKNOW
};
}  // namespace btrpc