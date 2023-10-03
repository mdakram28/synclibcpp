
#include "statevar.hpp"

#include <chrono>
#include <memory>

// const std::string SELF = "_";

StateVar::StateVar(StateTransport &transport) : transport(transport) {
    transport.on_receive_diff(std::bind(&StateVar::diff_received, this, std::placeholders::_1, std::placeholders::_2));
    this->state = std::make_shared<StateValue>(StateValue{Json::nullValue, 0});
}

void StateVar::diff_received(const std::string &peer_id, StateDiff &diff) {
    if (diff.time <= peer_states[peer_id]->time) return;

    std::string err_msg;
    if (!apply_diff(peer_states[peer_id]->value, diff.diff, err_msg)) {
        std::cout << "Error when applying diff: " << err_msg << std::endl;
        std::cout << "old_state: " << peer_states[peer_id]->value << std::endl;
        std::cout << "Diff: " << diff.diff << std::endl;
    }
    peer_states[peer_id]->time = diff.time;

    state = std::make_shared<StateValue>(StateValue{peer_states[peer_id]->value, peer_states[peer_id]->time});

    for (auto &listener : on_update_listeners) {
        listener(state);
    }
}

void StateVar::on_update(OnUpdateCallbackType callback) {
    on_update_listeners.insert(callback);
}

void StateVar::update(StatePtr new_value) {
    state = new_value;
}

void StateVar::sync() {
    // Syncs state to peer_states, will not update state
    std::string err_msg;
    int num_synced = 0;

    for (auto &peer : peer_states) {
        if (peer.second->time >= state->time) continue;

        StatePtr old_state = peer.second;

        StateDiff diff;
        diff.time = state->time;

        if (!get_diff(old_state->value, state->value, diff.diff, err_msg)) {
            std::cout << "Error when diffing : " << err_msg << std::endl;
            std::cout << "Old state : " << old_state->value << std::endl;
            std::cout << "New state : " << state->value << std::endl;
            err_msg.clear();
            continue;
        }

        transport.send_diff(peer.first, diff);
        peer_states[peer.first] = state;
        num_synced++;
    }

    if (num_synced > 0) {
        state = std::make_shared<StateValue>(StateValue{state->value, state->time});
    }
}
