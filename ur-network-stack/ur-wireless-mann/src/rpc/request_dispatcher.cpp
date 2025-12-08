#include "urwt/rpc/request_dispatcher.hpp"

namespace urwt {
namespace rpc {

RequestDispatcher::RequestDispatcher() {}

RequestDispatcher::~RequestDispatcher() {}

void RequestDispatcher::registerHandler(RPCAction action, std::shared_ptr<IRequestHandler> handler) {
    handlers_[action] = handler;
}

void RequestDispatcher::unregisterHandler(RPCAction action) {
    handlers_.erase(action);
}

RPCResponse RequestDispatcher::dispatch(const RPCRequest& request) {
    auto validationResult = request.validate();
    if (!validationResult.isOk()) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INVALID_REQUEST,
            validationResult.error()
        );
    }
    
    RPCAction action = stringToAction(request.action);
    
    if (action == RPCAction::Unknown) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::METHOD_NOT_FOUND,
            "Unknown action: " + request.action
        );
    }
    
    auto it = handlers_.find(action);
    if (it == handlers_.end()) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::METHOD_NOT_FOUND,
            "No handler registered for action: " + request.action
        );
    }
    
    try {
        return it->second->handle(request);
    } catch (const std::exception& e) {
        return createErrorResponse(
            request.transaction_id,
            ErrorCodes::INTERNAL_ERROR,
            std::string("Handler exception: ") + e.what()
        );
    }
}

bool RequestDispatcher::hasHandler(RPCAction action) const {
    return handlers_.find(action) != handlers_.end();
}

bool RequestDispatcher::isHandlerAsync(RPCAction action) const {
    auto it = handlers_.find(action);
    if (it == handlers_.end()) {
        return false;
    }
    return it->second->isAsync();
}

size_t RequestDispatcher::getHandlerCount() const {
    return handlers_.size();
}

} // namespace rpc
} // namespace urwt
