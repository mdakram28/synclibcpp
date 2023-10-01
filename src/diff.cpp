
#include "diff.h"

#include <jsoncpp/json/config.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include <math.h>

#include <cstdio>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#define DEBUG(var) std::cout << #var << " = " << var << std::endl;

#define RET_ERROR(_err_msg) \
    err_msg = _err_msg;     \
    return false;
#define RET_JSON(_ret_json) \
    diff_json = _ret_json;  \
    return true;

#define FOR_EACH_DIFF_KEY(_diff, _key_name, _body)                          \
    auto end = _diff.end();                                                 \
    for (Json::Value::const_iterator it = _diff.begin(); it != end; ++it) { \
        std::string _key_name = it.key().asString();                        \
        if (_key_name == "_t") continue;                                    \
        _body                                                               \
    }

Json::Value make_diff_json(DiffType diffType) {
    Json::Value val;
    val["_t"] = std::string{static_cast<char>(diffType)};
    return val;
}

Json::Value DIFF_DELETE = make_diff_json(DiffType::Delete);

Json::Value DIFF_UNCHANGED = make_diff_json(DiffType::Unchanged);

DiffType get_diff_type(Json::Value &val) {
    const auto type = val.type();
    if (type == Json::ValueType::objectValue && val.isMember("_t")) {
        return static_cast<DiffType>(val["_t"].asString()[0]);
    }
    return DiffType::Replace;
}

bool goto_path(Json::Value **target_obj, std::string &last_key, std::string &path, std::string &err_msg) {
    int prev_sep_ind = -1;
    int next_sep_ind;
    while ((next_sep_ind = path.find('/', prev_sep_ind + 1)) != std::string::npos) {
        std::string target_key = path.substr(prev_sep_ind + 1, next_sep_ind - (prev_sep_ind + 1));
        // DEBUG(prev_sep_ind)
        // DEBUG(next_sep_ind)
        // DEBUG(target_key)
        if ((*target_obj)->isMember(target_key)) {
            *target_obj = &(**target_obj)[target_key];
        } else {
            RET_ERROR("Path to non-existent object : " + target_key);
        }
        prev_sep_ind = next_sep_ind;
    }
    last_key = path.substr(prev_sep_ind + 1);
    return true;
}

bool get_diff_object(const Json::Value &old_json, const Json::Value &new_json,
                     Json::Value &diff_json, std::string &err_msg) {
    Json::Value diff = make_diff_json(DiffType::PatchObject);

    int num_deleted = 0;
    int num_replaced = 0;
    int num_patch_array = 0;
    for (Json::Value::const_iterator it = old_json.begin(); it != old_json.end(); ++it) {
        std::string key = it.key().asString();
        if (new_json.isMember(key)) {
            Json::Value child_diff;
            if (!get_diff(old_json[key], new_json[key], child_diff, err_msg)) {
                RET_ERROR(err_msg);
            }
            DiffType diff_type = get_diff_type(child_diff);

            if ((diff_type == DiffType::PatchObject) && child_diff.size() < 4) {
                FOR_EACH_DIFF_KEY(child_diff, child_key, {
                    diff[key + "/" + child_key] = child_diff[child_key];
                });
            } else if (diff_type != DiffType::Unchanged) {
                diff[key] = child_diff;
                if (diff_type == DiffType::Replace) {
                    num_replaced++;
                }
            }

            if (diff_type == DiffType::PatchArray) {
                num_patch_array++;
            }
        } else {
            num_deleted++;
            diff[key] = DIFF_DELETE;
        }
    }

    // DEBUG(num_deleted)
    // DEBUG(old_json.size())
    if (num_deleted == old_json.size() || num_replaced == old_json.size()) {
        RET_JSON(new_json);
    }

    // New items added
    for (Json::Value::const_iterator it = new_json.begin(); it != new_json.end(); ++it) {
        std::string key = it.key().asString();
        if (old_json.isMember(key)) continue;
        diff[key] = new_json[key];
    }

    if (diff.size() == 1) {
        RET_JSON(DIFF_UNCHANGED);
    }

    if (num_patch_array == diff.size() - 1) {
        Json::Value new_diff = make_diff_json(DiffType::PatchArray);
        FOR_EACH_DIFF_KEY(diff, path, {
            auto arr_diff = diff[path];
            FOR_EACH_DIFF_KEY(arr_diff, inner_path, {
                new_diff[path + "/" + inner_path] = arr_diff[inner_path];
            });
        });
        RET_JSON(new_diff);
    }

    RET_JSON(diff);
}

bool get_diff_array(const Json::Value &old_json, const Json::Value &new_json,
                    Json::Value &diff_json, std::string &err_msg) {
    Json::Value diff = make_diff_json(DiffType::PatchArray);

    int start = 0;
    int old_length = old_json.size();
    int new_length = new_json.size();
    int min_length = std::min(old_length, new_length);
    int end = old_length;
    int new_end = new_length;

    Json::Value first_diff;
    for (; start < min_length; start++) {
        if (!get_diff(old_json[start], new_json[start], first_diff, err_msg)) {
            RET_ERROR(err_msg);
        }
        if (get_diff_type(first_diff) != DiffType::Unchanged) {
            break;
        }
    }

    for (; end > start && new_end > start; end--, new_end--) {
        Json::Value item_diff;
        if (!get_diff(old_json[end - 1], new_json[new_end - 1], item_diff, err_msg)) {
            RET_ERROR(err_msg);
        }
        if (get_diff_type(item_diff) != DiffType::Unchanged) {
            break;
        }
    }

    if (end <= start && old_length == new_length) {
        RET_JSON(DIFF_UNCHANGED);
    }

    if (new_length == old_length) {
        Json::Value diff_array = Json::arrayValue;
        int curr_start = start;
        int i = start;
        for (; i < end; i++) {
            Json::Value item_diff;
            if (!get_diff(old_json[i], new_json[i], item_diff, err_msg)) {
                RET_ERROR(err_msg);
            }
            if (get_diff_type(item_diff) == DiffType::Unchanged) {
                if (diff_array.size() == 1) {
                    diff[std::to_string(curr_start)] = diff_array[0];
                    diff_array.clear();
                } else if (diff_array.size() > 1) {
                    diff[std::to_string(curr_start) + ":" + std::to_string(i)] = diff_array;
                    diff_array.clear();
                }
                curr_start = i + 1;
            } else {
                diff_array.append(item_diff);
            }
        }
        if (diff_array.size() == 1) {
            diff[std::to_string(curr_start)] = diff_array[0];
        } else if (diff_array.size() > 1) {
            diff[std::to_string(curr_start) + ":" + std::to_string(i)] = diff_array;
        }
        RET_JSON(diff);
    } else {
        Json::Value diff_array = Json::arrayValue;

        int i = start;
        for (; i < std::min(end, new_end); i++) {
            Json::Value item_diff;
            if (!get_diff(old_json[i], new_json[i], item_diff, err_msg)) {
                RET_ERROR(err_msg);
            }
            diff_array.append(item_diff);
        }

        for (; i < new_end; i++) {
            diff_array.append(new_json[i]);
        }

        std::string key = std::to_string(start) + ":" + std::to_string(end);
        diff[key] = diff_array;
        RET_JSON(diff);
    }
}

bool get_diff(const Json::Value &old_json, const Json::Value &new_json,
              Json::Value &diff_json, std::string &err_msg) {
    auto old_type = old_json.type(), new_type = new_json.type();

    if (old_type != new_type) {
        RET_JSON(new_json);
    }

    switch (old_type) {
        case Json::ValueType::intValue:
        case Json::ValueType::nullValue:
        case Json::ValueType::realValue:
        case Json::ValueType::uintValue:
        case Json::ValueType::booleanValue:
            if (old_json == new_json) {
                RET_JSON(DIFF_UNCHANGED);
            } else {
                RET_JSON(new_json);
            }
        case Json::ValueType::stringValue:
            if (old_json == new_json) {
                RET_JSON(DIFF_UNCHANGED);
            } else {
                RET_JSON(new_json);
            }
        case Json::ValueType::arrayValue:
            return get_diff_array(old_json, new_json, diff_json, err_msg);
        case Json::ValueType::objectValue:
            return get_diff_object(old_json, new_json, diff_json, err_msg);
        default:
            RET_ERROR("Invalid JSON type");
    }

    RET_ERROR("Unreachable code");
}

bool apply_diff_PatchObject(Json::Value &obj, Json::Value &diff, std::string &err_msg) {
    for (Json::Value::const_iterator it = diff.begin(); it != diff.end(); ++it) {
        std::string key = it.key().asString();
        Json::Value &child_diff = diff[key];
        if (key == "_t") continue;

        Json::Value *target_obj = &obj;
        std::string last_key;
        if (!goto_path(&target_obj, last_key, key, err_msg)) {
            RET_ERROR(err_msg);
        }

        if (target_obj->isMember(last_key)) {
            if (get_diff_type(child_diff) == DiffType::Delete) {
                target_obj->removeMember(last_key);
            } else {
                if (!apply_diff((*target_obj)[last_key], child_diff, err_msg)) {
                    RET_ERROR(err_msg);
                }
            }
        } else {
            (*target_obj)[last_key] = child_diff;
        }
    }

    return true;
}

bool apply_diff_PatchArray(Json::Value &obj, Json::Value &diff, std::string &err_msg) {
    for (Json::Value::const_iterator it = diff.begin(); it != diff.end(); ++it) {
        std::string path = it.key().asString();
        if (path == "_t") continue;
        Json::Value &child_diff = diff[path];
        Json::Value *target_obj = &obj;
        std::string last_key;
        if (!goto_path(&target_obj, last_key, path, err_msg)) {
            RET_ERROR(err_msg);
        }

        Json::Value &new_arr = *target_obj;

        if (new_arr.type() != Json::ValueType::arrayValue) {
            RET_ERROR("Cannot apply 'A': Old obj is not of array type");
        }

        if (last_key.find(':') == std::string::npos) {
            int ind = std::stoi(last_key);
            if (!apply_diff(new_arr[ind], child_diff, err_msg)) {
                RET_ERROR(err_msg);
            }
            continue;
        }

        int start = std::stoi(last_key.substr(0, last_key.find(":")));
        int end = std::stoi(last_key.substr(last_key.find(":") + 1));
        int old_length = new_arr.size();
        int new_length = old_length - (end - start) + it->size();
        int new_end = new_length - (old_length - end);

        if (old_length != new_length) {
            Json::Value old_arr = *target_obj;
            new_arr.resize(new_length);
            for (int i = 0; i < old_length - end; i++) {
                new_arr[new_length - i - 1] = old_arr[old_length - i - 1];
            }
        }

        int i = start;
        for (; i < std::min(end, new_end); i++) {
            if (!apply_diff(new_arr[i], child_diff[i - start], err_msg)) {
                RET_ERROR(err_msg);
            }
        }

        for (; i < new_end; i++) {
            new_arr[i] = child_diff[i - start];
        }
    }

    return true;
}

bool apply_diff(Json::Value &obj, Json::Value &diff, std::string &err_msg) {
    const Json::ValueType obj_type = obj.type();

    const DiffType diff_type = get_diff_type(diff);
    switch (diff_type) {
        case DiffType::Delete:
            RET_ERROR("Cannot apply delete diff");
        case DiffType::Unchanged:
            return true;
        case DiffType::Replace:
            obj = diff;
            return true;
        case DiffType::PatchObject:
            return apply_diff_PatchObject(obj, diff, err_msg);
        case DiffType::PatchArray:
            return apply_diff_PatchArray(obj, diff, err_msg);
        case DiffType::PatchString:
        default:
            RET_ERROR("Unsupported diff type : " + std::string{static_cast<char>(diff_type)});
    }

    RET_ERROR("Unreachable code");
}