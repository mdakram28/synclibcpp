#ifndef __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_
#define __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_

#define DEBUG(var_name) std::cout << "DEBUG: " << #var_name << " = " << var_name << std::endl;

#include <functional>
#include <memory>
#include <set>

#include "diff.hpp"

struct StateValue {
    Json::Value value;
    uint64_t time;
};

struct StateDiff {
    Json::Value diff;
    uint64_t time;

   public:
    static bool from_string(std::string& s, StateDiff& diff, std::string& err_msg);
    std::string to_string();
};

typedef std::function<void(std::shared_ptr<StateValue> state)> OnUpdateCallback;
typedef std::function<void(const std::string& peer_id, StateDiff& diff)> OnDiffReceiveCallback;
typedef uint64_t CallbackId;

class StateTransport {
   public:
    std::map<CallbackId, OnDiffReceiveCallback> listeners;
    CallbackId last_callback_id;

    virtual std::vector<std::string> get_peers() = 0;
    virtual bool send_diff(const std::string& peer_id, StateDiff& diff) = 0;
    CallbackId add_listener(OnDiffReceiveCallback callback);
    void remove_listener(CallbackId id);
};

class StateVar : public std::enable_shared_from_this<StateVar> {
   private:
    std::map<std::string, std::shared_ptr<StateValue>> peer_states;
    std::shared_ptr<StateValue> state;
    std::set<std::shared_ptr<StateTransport>> transports;
    void diff_received(const std::string& peer_id, StateDiff& diff);

    std::map<CallbackId, OnUpdateCallback> on_update_listeners;
    CallbackId last_callback_id;

   public:
    StateVar();
    void add_transport(std::shared_ptr<StateTransport> transport);
    void update(std::shared_ptr<StateValue> new_value);
    void sync();
    CallbackId on_update(OnUpdateCallback callback);
};

#endif  // __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_