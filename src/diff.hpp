
#ifndef __PROJECTS_SYNCLIBCPP_SRC_DIFF_H_
#define __PROJECTS_SYNCLIBCPP_SRC_DIFF_H_

#include <json/json.h>
#include <json/value.h>
#include <json/config.h>
#include <json/writer.h>

#include <iostream>
#include <string>
#define DEBUG(var) std::cout << #var << " = " << var << std::endl;

enum DiffType {
    Delete = 'X',
    Unchanged = 'U',
    Replace = 'R',
    PatchString = 'S',
    PatchArray = 'A',
    PatchObject = 'P',
};

extern Json::Value DIFF_DELETE;;
extern Json::Value DIFF_UNCHANGED;

/**
 *
 * 
 * X: Delete
 * P: Patch Object
 * A: Patch Array
 * S: Patch String
 * U: Unchanged/Undefined
 *
 * Default behaviour is add
 */

bool get_diff(const Json::Value &old_json, const Json::Value &new_json,
             Json::Value &diff_json, std::string &err_msg);

bool apply_diff(Json::Value &obj, Json::Value &diff, std::string &err_msg);


#endif // __PROJECTS_SYNCLIBCPP_SRC_DIFF_H_