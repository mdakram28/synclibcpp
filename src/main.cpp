#include <math.h>

#include <cstdio>
#include <iomanip>
#include <iostream>

#include "diff.h"

double total_saving = 0;

bool test_diff(std::string &old_str, std::string &new_str) {
    static Json::FastWriter writer;
    JSONCPP_STRING err;
    Json::Value old_json, new_json, diff_json, recon_json;

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(old_str.c_str(), old_str.c_str() + old_str.length(), &old_json,
                       &err)) {
        std::cout << "JSON 1 Parsing error : " << err << std::endl;
        return false;
    }
    if (!reader->parse(new_str.c_str(), new_str.c_str() + new_str.length(), &new_json,
                       &err)) {
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
        double saving = (new_str.length() - diff_str.length()) *100.0/ new_str.length();
        std::cout << "Saving =                                      " << round(saving) << std::endl;
        total_saving += saving;
    } catch (Json::LogicError &err) {
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

int main(int argc, char *argv[]) {
    std::printf("Sync Server!\n");

    std::string json_1 = "1";
    std::string json_2 = "2";
    std::vector<std::string> json_strs_old = {
        "1",
        "2",
        "3.14",

        "null",

        "{}",
        "{\"a\":1}",
        "{\"a\":2}",
        "{\"a\":\"hello\"}",
        "{\"a\":\"world\"}",
        "{\"a\":123,\"b\":\"world\"}",
        "{\"a\":123,\"b\":\"world\",\"c\":{\"d\":678}}",

        "\"test_string_1\"",
        "\"test_string_2\"",

        "[]",
        "[1, 2, 3]",
        "[1, 2, 3, 4, 5, 6]",
        "[1, 2, 3, {}, 5, 6]",
        "[1, 2, 3, 4, null, 6]",
        "[1, 2, 3, 4, null]",
        "[1, 2, 3, {\"a\": 3}, 5, 6]",
        "[1, 2, 3, {\"a\": 4}, 5, 6]",


    };

    std::vector<std::string> json_strs = {
        "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Scheduled\",\"logs\":[]}}",
        "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[]}}",
        "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\"]}}",
        "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\"]}}",
        "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\"]}}",
    };

    int l = json_strs.size();
    int tc = 0;
    int passed = 0;
    for (int i=0; i<l; i++) {
        std::string &old_str = json_strs[i];
        for (int j=i; j<l; j++) {
            std::string &new_str = json_strs[j];

            std::cout << "------- Test Case #" << tc++ << "-------" << std::endl;
            if (test_diff(old_str, new_str)) {
                passed++;
            } else {
                std::cout << "old_json: " << old_str << std::endl;
                std::cout << "new_json: " << new_str << std::endl;
                return EXIT_FAILURE;
            }
        }
    }
    int num_cases = pow(json_strs.size(), 2);
    std::cout << "Passed : " << passed << " / " << num_cases << std::endl;
    std::cout << "Total Saving : " << total_saving/num_cases << " %" << std::endl;
    return EXIT_SUCCESS;
}
