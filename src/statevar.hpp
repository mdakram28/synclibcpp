#ifndef __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_
#define __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_

#include <functional>
#include <memory>
#include <set>

#include "diff.hpp"

struct StateDiff {
    Json::Value diff;
    uint64_t time;
};

struct StateValue {
    Json::Value value;
    uint64_t time;
};

using StatePtr = std::shared_ptr<StateValue>;
using OnUpdateCallbackType = std::function<void(StatePtr state)>;

class StateTransport {
   public:
    virtual void send_diff(const std::string &peer_id, StateDiff &diff){};
    virtual void on_receive_diff(std::function<void(const std::string &peer_id, StateDiff &diff)> callback){};
};

class StateVar {
   private:
    std::map<std::string, StatePtr> peer_states;
    StatePtr state;
    StateTransport &transport;
    void diff_received(const std::string &peer_id, StateDiff &diff);
    std::set<OnUpdateCallbackType> on_update_listeners;

   public:
    StateVar(StateTransport &transport);
    void update(StatePtr new_value);
    void sync();
    void on_update(OnUpdateCallbackType callback);
};

#endif  // __PROJECTS_SYNCLIBCPP_SRC_STATEVAR_HPP_