
#include "statevar.hpp"

#include <chrono>
#include <memory>

// const std::string SELF = "_";

Json::FastWriter writer;
Json::CharReaderBuilder builder;
std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

bool StateDiff::from_string(std::string& s, StateDiff& diff, std::string& err_msg) {
    Json::Value diff_json;
    if (!reader->parse(s.c_str(), s.c_str() + s.length(), &diff_json, &err_msg)) {
        std::cout << "JSON 1 Parsing error : " << err_msg << std::endl;
        return false;
    }
    diff.diff = diff_json["diff"];
    diff.time = diff_json["time"].asUInt64();

    return true;
}

std::string StateDiff::to_string() {
    Json::Value ret_json;
    ret_json["time"] = time;
    ret_json["diff"] = diff;

    return std::move(writer.write(ret_json));
}

StateVar::StateVar() {
    this->state = std::make_shared<StateValue>(StateValue{Json::nullValue, 0});
}

void StateVar::add_transport(std::shared_ptr<StateTransport> transport) {
    transport->add_listener(
        std::bind(&StateVar::diff_received, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    transports.insert(transport);
}

void StateVar::diff_received(const std::string& peer_id, StateDiff& diff) {
    if (peer_states.find(peer_id) == peer_states.end()) {
        peer_states[peer_id] = std::make_shared<StateValue>();
    }
    if (diff.time <= peer_states[peer_id]->time)
        return;
    std::cout << "Applying diff to " << peer_id << std::endl;
    std::string err_msg;
    if (!apply_diff(peer_states[peer_id]->value, diff.diff, err_msg)) {
        std::cout << "Error when applying diff: " << err_msg << std::endl;
        std::cout << "old_state: " << peer_states[peer_id]->value << std::endl;
        std::cout << "Diff: " << diff.diff << std::endl;
    }
    peer_states[peer_id]->time = diff.time;

    state = std::make_shared<StateValue>(StateValue{peer_states[peer_id]->value, peer_states[peer_id]->time});

    for (auto& listener : on_update_listeners) {
        listener.second(state);
    }
}

CallbackId StateVar::on_update(OnUpdateCallback callback) {
    on_update_listeners[last_callback_id++] = callback;
    return last_callback_id;
}

void StateVar::update(std::shared_ptr<StateValue> new_value) {
    state = new_value;
}

void StateVar::sync() {
    // Syncs state to peer_states, will not update state
    std::string err_msg;
    int num_synced = 0;

    for (auto& transport : transports) {
        for (auto& peer_id : transport->get_peers()) {
            if (peer_states.find(peer_id) == peer_states.end()) {
                peer_states[peer_id] = std::make_shared<StateValue>();
            }
            auto& peer_state = peer_states[peer_id];
            if (peer_state->time >= state->time)
                continue;

            StateDiff diff;
            diff.time = state->time;

            if (!get_diff(peer_state->value, state->value, diff.diff, err_msg)) {
                std::cout << "Error when diffing : " << err_msg << std::endl;
                std::cout << "Old state : " << peer_state->value << std::endl;
                std::cout << "New state : " << state->value << std::endl;
                err_msg.clear();
                continue;
            }

            transport->send_diff(peer_id, diff);

            peer_states[peer_id] = state;
            num_synced++;
        }
    }

    if (num_synced > 0) {
        state = std::make_shared<StateValue>(StateValue{state->value, state->time});
    }
}

CallbackId StateTransport::add_listener(OnDiffReceiveCallback callback) {
    listeners[last_callback_id++] = callback;
    return last_callback_id;
}

void StateTransport::remove_listener(CallbackId id) {
    listeners.erase(id);
}
