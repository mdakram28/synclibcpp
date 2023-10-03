#include <gtest/gtest.h>
#include <fstream>

#include "diff.hpp"

bool test_diff(std::string& old_str, std::string& new_str) {
    static Json::FastWriter writer;
    JSONCPP_STRING err;
    Json::Value old_json, new_json, diff_json, recon_json;

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(old_str.c_str(), old_str.c_str() + old_str.length(), &old_json, &err)) {
        std::cout << "JSON 1 Parsing error : " << err << std::endl;
        return false;
    }
    if (!reader->parse(new_str.c_str(), new_str.c_str() + new_str.length(), &new_json, &err)) {
        std::cout << "JSON 2 Parsing error : " << err << std::endl;
        return false;
    }

    std::string err_msg;

    try {
        if (!get_diff(old_json, new_json, diff_json, err_msg)) {
            std::cout << "get_diff failed: " << err_msg << std::endl;
            return false;
        }

        recon_json = old_json;
        if (!apply_diff(recon_json, diff_json, err_msg)) {
            std::cout << "apply_diff failed: " << err_msg << std::endl;
            std::cout << "JSON DIff: " << diff_json << std::endl;
            return false;
        }
        std::string diff_str = writer.write(diff_json);
        double saving = (new_str.length() - diff_str.length()) * 100.0 / new_str.length();
        if (saving < 0) {
            std::cout << "Negative saving" << std::endl;
            return false;
        }
    } catch (Json::LogicError& err) {
        std::cout << "Exception: " << err.what() << std::endl;
        return false;
    }
    std::cout << "JSON DIff: " << diff_json << std::endl;

    if (recon_json != new_json) {
        std::cout << "reconstructed match faied: " << err_msg << std::endl;
        std::cout << "Reconstructed : " << recon_json << std::endl;
        return false;
    }

    return true;
}

// Demonstrate some basic assertions.
TEST(DiffTest, BasicAssertions) {
    std::ifstream f("/mnt/c/projects/synclibcpp/test/resources/objects.json");
    Json::Value jsons;
    f >> jsons;

    std::cout << "File read" << std::endl;

    std::map<std::string, std::string> json_strs;

    for (Json::Value::const_iterator it=jsons.begin(); it!=jsons.end(); ++it) {
        json_strs[it.key().asString()] = it->toStyledString();
    }

    for (auto p1: json_strs) {
        for(auto p2: json_strs) {
            DEBUG(p1.second);
            DEBUG(p2.second);
            bool passed = test_diff(p1.second, p2.second);
            EXPECT_TRUE(passed);
        }
    }
}