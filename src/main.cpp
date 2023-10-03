#include <math.h>

#include <vector>

#include "diff.hpp"
#include "syncserver.cpp"

double total_saving = 0;

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
        std::cout << "                                   Saving = " << round(saving) << std::endl;
        total_saving += saving;
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

int main(int argc, char const* argv[]) {
    // Create and start server
    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(8000);
    std::shared_ptr<WsStateServer> server = std::make_shared<WsStateServer>(1, tcp::endpoint{address, port});
    server->start_server();

    // Create StateVar
    std::shared_ptr<StateVar> state_var = std::make_shared<StateVar>();
    std::thread sync_thread([&] {
        while (true) {
            std::cout << "Syncing ...." << std::endl;
            state_var->sync();
            sleep(5);
        }
    });
    state_var->on_update([](std::shared_ptr<StateValue> state) {
        std::cout << "State Updated :: " << state->value << std::endl;
    });

    // Connect
    state_var->add_transport(server);

    sleep(10000);
    return EXIT_SUCCESS;
}

int ___main(int argc, char* argv[]) {
    std::printf("Sync Server!\n");

    std::string json_1 = "1";
    std::string json_2 = "2";
    std::vector<std::string> json_strs_1 = {
        // "1",
        // "2",
        // "3.14",

        // "null",

        // "{}",
        // "{\"a\":1}",
        // "{\"a\":2}",
        // "{\"a\":\"hello\"}",
        // "{\"a\":\"world\"}",
        // "{\"a\":123,\"b\":\"world\"}",
        // "{\"a\":123,\"b\":\"world\",\"c\":{\"d\":678}}",
        "{\"key1\":\"a\",\"key2\":\"b\",\"key3\":\"c\",\"key4\":\"d\"}",
        "{\"key1\":\"a\",\"key3\":\"c\",\"key4\":\"d\",\"key5\":\"b\"}",

        // "\"test_string____________________1\"",
        // "\"test_string____________________2\"",
        // "[{\"a\":\"test_string____________________1\"}]",
        // "[{\"a\":\"test_string____________________1\nsecondline\"}]",

        // "[]",
        // "[1, 2, 3]",
        // "[1, 2, 3, 4, 5, 6]",
        // "[1, 2, 3, {}, 5, 6]",
        // "[1, 2, 3, 4, null, 6]",
        // "[1, 2, 3, 4, null]",
        "[1, 2, {\"a\": 3}, {\"a\": {\"b\":\"hello\"}, \"c\":1}, 5, 6]",
        "[1, 2, {\"a\": 3}, {\"a\": {\"b\":\"world\"}, \"c\":1}, 5, 6]",

        // "[[1,2,3],[4,{\"a\":1,\"b\":3},6],[7,8,9]]",
        // "[[1,2,3],[4,{\"a\":2,\"b\":3},6],[7,8,9]]"
    };

    std::vector<std::string> json_strs_2 = {
        // "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Scheduled\",\"logs\":[]}}",
        // "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[]}}",
        // "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\"]}}",
        // "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\"]}}",
        // "{\"job1\":{\"name\":\"Job 1\",\"status\":\"Running\",\"logs\":[\"line1\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\",\"line2\"]}}",
    };

    std::vector<std::string> json_strs;
    json_strs.insert(json_strs.end(), json_strs_1.begin(), json_strs_1.end());
    json_strs.insert(json_strs.end(), json_strs_2.begin(), json_strs_2.end());

    int l = json_strs.size();
    int tc = 0;
    int passed = 0;
    for (int i = 0; i < l; i++) {
        std::string& old_str = json_strs[i];
        for (int j = i + 1; j < l; j++) {
            std::string& new_str = json_strs[j];

            std::cout << "------- Test Case #" << tc++ << " : " << i << ", " << j << "-------" << std::endl;
            if (test_diff(old_str, new_str)) {
                passed++;
            } else {
                std::cout << "old_json: " << old_str << std::endl;
                std::cout << "new_json: " << new_str << std::endl;
                return EXIT_FAILURE;
            }
        }
    }
    std::cout << "Passed : " << passed << " / " << tc << std::endl;
    std::cout << "Total Saving : " << total_saving / tc << " %" << std::endl;
    return EXIT_SUCCESS;
}
