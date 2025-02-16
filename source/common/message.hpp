#pragma once
#include "abstract.hpp"
#include "detail.hpp"
#include "fields.hpp"

namespace btrpc {
class JsonMessage : public BaseMessage {
   public:
    using ptr = std::shared_ptr<JsonMessage>;
    virtual std::string serialize() override {
        std::string body;
        bool ret = JSON::serialize(_body, body);
        if (ret == false) {
            return std::string();
        }
        return body;
    }
    virtual bool unserialize(const std::string& msg) override {
        return JSON::unserialize(msg, _body);
    }

   protected:
    Json::Value _body;
};

class JsonRequest : public JsonMessage {
   public:
    using ptr = std::shared_ptr<JsonRequest>;
};
class JsonResponse : public JsonMessage {
   public:
    using ptr = std::shared_ptr<JsonResponse>;
    virtual bool check() override {
        // 在响应中，大部分的响应都只有响应状态码
        // 因此只需要判断响应状态码字段是否存在，类型是否正确即可
        if (_body[KEY_RCODE].isNull() == true) {
            ELOG("响应中没有响应状态码！");
            return false;
        }
        if (_body[KEY_RCODE].isIntegral() == false) {
            ELOG("响应状态码类型错误！");
            return false;
        }
        return true;
    }
    virtual RCode rcode() {
        return (RCode)_body[KEY_RCODE].asInt();
    }
    virtual void setRCode(RCode rcode) {
        _body[KEY_RCODE] = (int)rcode;
    }
};

class RpcRequest : public JsonRequest {
   public:
    using ptr = std::shared_ptr<RpcRequest>;
	RpcRequest() {
		_mtype = MType::REQ_RPC;
	}
    virtual bool check() override {
        // rpc请求中，包含请求方法名称-字符串，参数字段-对象
        if (_body[KEY_METHOD].isNull() == true ||
            _body[KEY_METHOD].isString() == false) {
            ELOG("RPC请求中没有方法名称或方法名称类型错误！");
            return false;
        }
        if (_body[KEY_PARAMS].isNull() == true ||
            _body[KEY_PARAMS].isObject() == false) {
            ELOG("RPC请求中没有参数信息或参数信息类型错误！");
            return false;
        }
        return true;
    }
    std::string method() {
        return _body[KEY_METHOD].asString();
    }
    void setMethod(const std::string& method_name) {
        _body[KEY_METHOD] = method_name;
    }
    Json::Value params() {
        return _body[KEY_PARAMS];
    }
    void setParams(const Json::Value& params) {
        _body[KEY_PARAMS] = params;
    }
};
class TopicRequest : public JsonRequest {
   public:
    using ptr = std::shared_ptr<TopicRequest>;
	TopicRequest() {
		_mtype = MType::REQ_TOPIC;
	}
    virtual bool check() override {
        // rpc请求中，包含请求方法名称-字符串，参数字段-对象
        if (_body[KEY_TOPIC_KEY].isNull() == true ||
            _body[KEY_TOPIC_KEY].isString() == false) {
            ELOG("主题请求中没有主题名称或主题名称类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("主题请求中没有操作类型或操作类型的类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
            (_body[KEY_TOPIC_MSG].isNull() == true ||
             _body[KEY_TOPIC_MSG].isString() == false)) {
            ELOG("主题消息发布请求中没有消息内容字段或消息内容类型错误！");
            return false;
        }
        return true;
    }

    std::string topicKey() {
        return _body[KEY_TOPIC_KEY].asString();
    }
    void setTopicKey(const std::string& key) {
        _body[KEY_TOPIC_KEY] = key;
    }
    TopicOptype optype() {
        return (TopicOptype)_body[KEY_OPTYPE].asInt();
    }
    void setOptype(TopicOptype optype) {
        _body[KEY_OPTYPE] = (int)optype;
    }
    std::string topicMsg() {
        return _body[KEY_TOPIC_MSG].asString();
    }
    void setTopicMsg(const std::string& msg) {
        _body[KEY_TOPIC_MSG] = msg;
    }
};

class ServiceRequest : public JsonRequest {
   public:
    using ptr = std::shared_ptr<ServiceRequest>;
	ServiceRequest() {
		_mtype = MType::REQ_SERVICE;
	}
    virtual bool check() override {
        // rpc请求中，包含请求方法名称-字符串，参数字段-对象
        if (_body[KEY_METHOD].isNull() == true ||
            _body[KEY_METHOD].isString() == false) {
            ELOG("服务请求中没有方法名称或方法名称类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("服务请求中没有操作类型或操作类型的类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() != (int)(ServiceOptype::SERVICE_DISCOVERY) &&
            (_body[KEY_HOST].isNull() == true ||
             _body[KEY_HOST].isObject() == false ||
             _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
             _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
             _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
             _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false)) {
            ELOG("服务请求中主机地址信息错误！");
            return false;
        }
        return true;
    }

    std::string method() {
        return _body[KEY_METHOD].asString();
    }
    void setMethod(const std::string& name) {
        _body[KEY_METHOD] = name;
    }
    ServiceOptype optype() {
        return (ServiceOptype)_body[KEY_OPTYPE].asInt();
    }
    void setOptype(ServiceOptype optype) {
        _body[KEY_OPTYPE] = (int)optype;
    }
    Address host() {
        Address addr;
        addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
        addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
        return addr;
    }
    void setHost(const Address& host) {
        Json::Value val;
        val[KEY_HOST_IP] = host.first;
        val[KEY_HOST_PORT] = host.second;
        _body[KEY_HOST] = val;
    }
};

class RpcResponse : public JsonResponse {
   public:
    using ptr = std::shared_ptr<RpcResponse>;
	RpcResponse() {
		_mtype = MType::RSP_RPC;
	}
    virtual bool check() override {
        if (_body[KEY_RCODE].isNull() == true ||
            _body[KEY_RCODE].isIntegral() == false) {
            ELOG("响应中没有响应状态码,或状态码类型错误！");
            return false;
        }
        if (_body[KEY_RESULT].isNull() == true) {
            ELOG("响应中没有Rpc调用结果,或结果类型错误！");
            return false;
        }
        return true;
    }
    Json::Value result() {
        return _body[KEY_RESULT];
    }
    void setResult(const Json::Value& result) {
        _body[KEY_RESULT] = result;
    }
};
class ConnectResponse : public JsonResponse {
   public:
    using ptr = std::shared_ptr<ConnectResponse>;
	ConnectResponse() {
		_mtype = MType::RSP_CONNECT;
	}
};
class ServiceResponse : public JsonResponse {
   public:
    using ptr = std::shared_ptr<ServiceResponse>;
	ServiceResponse() {
		_mtype = MType::RSP_SERVICE;
	}
    virtual bool check() override {
        if (_body[KEY_RCODE].isNull() == true ||
            _body[KEY_RCODE].isIntegral() == false) {
            ELOG("响应中没有响应状态码,或状态码类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].isNull() == true ||
            _body[KEY_OPTYPE].isIntegral() == false) {
            ELOG("响应中没有操作类型,或操作类型的类型错误！");
            return false;
        }
        if (_body[KEY_OPTYPE].asInt() == (int)(ServiceOptype::SERVICE_DISCOVERY) &&
            (_body[KEY_METHOD].isNull() == true ||
             _body[KEY_METHOD].isString() == false ||
             _body[KEY_HOST].isNull() == true ||
             _body[KEY_HOST].isArray() == false)) {
            ELOG("服务发现响应中响应信息字段错误！");
            return false;
        }
        return true;
    }
	int idleCount(){
		return _body[KEY_IDLE_COUNT].asInt();
	}
	void setIdleCount(int idle){
		_body[KEY_IDLE_COUNT] = idle;
	}
    ServiceOptype optype() {
        return (ServiceOptype)_body[KEY_OPTYPE].asInt();
    }
    void setOptype(ServiceOptype optype) {
        _body[KEY_OPTYPE] = (int)optype;
    }
    std::string method() {
        return _body[KEY_METHOD].asString();
    }
    void setMethod(const std::string& method) {
        _body[KEY_METHOD] = method;
    }
    void setHost(const Address& addr) {
        Json::Value val;
        val[KEY_HOST_IP] = addr.first;
        val[KEY_HOST_PORT] = addr.second;
        _body[KEY_HOST] = val;
    }
    Address host() {
		return std::make_pair(_body[KEY_HOST][KEY_HOST_IP].asString(), _body[KEY_HOST][KEY_HOST_PORT].asInt());
	}
};

// 实现一个消息对象的生产工厂
class MessageFactory {
   public:
    static BaseMessage::ptr create(MType mtype) {
        switch (mtype) {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::RSP_CONNECT:
                return std::make_shared<ConnectResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<ConnectResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
        }
        return BaseMessage::ptr();
    }

    template <typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args) {
        return std::make_shared<T>(std::forward(args)...);
    }
};
}  // namespace btrpc