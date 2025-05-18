#pragma once
#include "../common/message.hpp"
#include "../common/net.hpp"
#include "../common/thread_poll.hpp"

namespace myrpc {
namespace server {

enum class VType {
    BOOL = 0,
    INTEGRAL,
    NUMERIC,
    STRING,
    ARRAY,
    OBJECT,
};
class MethodDescribe {
   public:
    using ptr = std::shared_ptr<MethodDescribe>;
    using MethodCallback = std::function<void(const Json::Value&, Json::Value&)>;  //参数  结果
    using ParamsDescribe = std::pair<std::string, VType>;
    MethodDescribe(std::string&& mname, std::vector<ParamsDescribe>&& desc, VType vtype, MethodCallback&& handler, bool use_io_thread = false)
        : _method_name(std::move(mname)), _callback(std::move(handler)), _params_desc(std::move(desc)), _return_type(vtype), _use_io_thread(use_io_thread) {}
    const std::string& method() { return _method_name; }
    // 针对收到的请求中的参数进行校验
    bool paramCheck(const Json::Value& params) {
        // 对params进行参数校验---判断所描述的参数字段是否存在，类型是否一致
        for (auto& desc : _params_desc) {
            if (params.isMember(desc.first) == false) {
                ELOG("参数字段完整性校验失败！%s 字段缺失！", desc.first.c_str());
                return false;
            }
            if (check(desc.second, params[desc.first]) == false) {
                ELOG("%s 参数类型校验失败！", desc.first.c_str());
                return false;
            }
        }
        return true;
    }
    bool call(const Json::Value& params, Json::Value& result) {
		_callback(params, result);
		std::string res_json;
		myrpc::JSON::serialize(result, res_json);
        if (rtypeCheck(result) == false) {
            ELOG("回调处理函数中的响应信息校验失败！");
            return false;
        }
        return true;
    }
	bool useIOThread() {
		return _use_io_thread;
	}

   private:
    bool rtypeCheck(const Json::Value& val) {
        return check(_return_type, val);
    }
    bool check(VType vtype, const Json::Value& val) {
        switch (vtype) {
            case VType::BOOL:
                return val.isBool();
            case VType::INTEGRAL:
                return val.isIntegral();
            case VType::NUMERIC:
                return val.isNumeric();
            case VType::STRING:
                return val.isString();
            case VType::ARRAY:
                return val.isArray();
            case VType::OBJECT:
                return val.isObject();
        }
        return false;
    }

   private:
    std::string _method_name;                  // 方法名称
    MethodCallback _callback;                 // 实际的业务回调函数
    std::vector<ParamsDescribe> _params_desc;  // 参数字段格式描述
    VType _return_type;                        // 结果作为返回值类型的描述
	bool _use_io_thread; 				       // 是否使用io线程
};

class SDescribeFactory {
   public:
    void setMethodName(const std::string& name) {
        _method_name = name;
    }
    void setReturnType(VType vtype) {
        _return_type = vtype;
    }
    void setParamsDesc(const std::string& pname, VType vtype) {
        _params_desc.push_back(MethodDescribe::ParamsDescribe(pname, vtype));
    }
    void setCallback(const MethodDescribe::MethodCallback& cb) {
        _callback = cb;
    }
	void setUseIOThread(bool use_io_thread) {
		_use_io_thread = use_io_thread;
	}
    MethodDescribe::ptr build() {
        return std::make_shared<MethodDescribe>(std::move(_method_name),
                                                 std::move(_params_desc), _return_type, std::move(_callback), _use_io_thread);
    }

   private:
    std::string _method_name;
    MethodDescribe::MethodCallback _callback;                 // 实际的业务回调函数
    std::vector<MethodDescribe::ParamsDescribe> _params_desc;  // 参数字段格式描述
    VType _return_type;                                         // 结果作为返回值类型的描述
	bool _use_io_thread = false;  // 是否使用io线程
};

class ServiceManager {
   public:
    using ptr = std::shared_ptr<ServiceManager>;
    void insert(const MethodDescribe::ptr& desc) {
        std::unique_lock<std::mutex> lock(_mutex);
        _services.insert(std::make_pair(desc->method(), desc));
    }
    MethodDescribe::ptr select(const std::string& method_name) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _services.find(method_name);
        if (it == _services.end()) {
            return MethodDescribe::ptr();
        }
        return it->second;
    }
    void remove(const std::string& method_name) {
        std::unique_lock<std::mutex> lock(_mutex);
        _services.erase(method_name);
    }

   private:
    std::mutex _mutex;
    std::unordered_map<std::string, MethodDescribe::ptr> _services;
};

class RpcRouter {
   public:
    using ptr = std::shared_ptr<RpcRouter>;
    RpcRouter(size_t numThreads = 0)
        : _service_manager(std::make_shared<ServiceManager>()),
		  _thread_pool(std::make_shared<ThreadPool>(numThreads)) {}
    // 这是注册到Dispatcher模块针对rpc请求进行回调处理的业务函数
    void onRpcRequest(const BaseConnection::ptr& conn, RpcRequest::ptr& request) {
		DLOG("收到rpc请求 rid=%s", request->rid().c_str());
        // 1. 查询客户端请求的方法描述--判断当前服务端能否提供对应的服务
        auto service = _service_manager->select(request->method());
        if (service.get() == nullptr) {
            ELOG("%s 服务未找到！", request->method().c_str());
            return response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
        }
        // 2. 进行参数校验，确定能否提供服务
        if (service->paramCheck(request->params()) == false) {
            ELOG("%s 服务参数校验失败！", request->method().c_str());
            return response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
        }
		auto call = [this, service, request, conn](bool inLoop) {
			// 3. 调用业务回调接口进行业务处理
			Json::Value result;
			bool ret = service->call(request->params(), result);
			if (ret == false) {
				ELOG("%s 服务返回参数校验失败！", request->method().c_str());
				return response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR, inLoop);
			}
			// 4. 处理完毕得到结果，组织响应，向客户端发送
			return response(conn, request, result, RCode::RCODE_OK, inLoop);
		};
		if(service->useIOThread() || _thread_pool->getThreadNum() == 0){
			std::cerr << "\nrun in io thread\n\n";
			call(true);
		}else{
			_thread_pool->enqueue(std::bind(call, false));
		}
    }
    void registerMethod(const MethodDescribe::ptr& service) {
        return _service_manager->insert(service);
    }

   private:
    void response(const BaseConnection::ptr& conn,
                  const RpcRequest::ptr& req,
                  const Json::Value& res,
                  RCode rcode,
				  bool inLoop = false) {
        auto msg = MessageFactory::create<RpcResponse>();
        msg->setId(req->rid());
        msg->setMType(myrpc::MType::RSP_RPC);
        msg->setRCode(rcode);
        msg->setResult(res);
		std::string json;
		myrpc::JSON::serialize(msg->result(), json);
		DLOG("发送rpc响应 orid=%s, rrid=%s", req->rid().c_str(), msg->rid().c_str());
        if(inLoop) conn->send(msg);
		else conn->sendInLoop(msg);
    }

   private:
    ServiceManager::ptr _service_manager;
	ThreadPool::ptr _thread_pool;
};

}  // namespace server
}  // namespace myrpc