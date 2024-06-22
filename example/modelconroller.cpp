#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <memory>
#include <limits>

using json = nlohmann::json;
using namespace std;

struct ManifestInfo {
    int segment_time;
    vector<int> bitrates;
    vector<vector<int>> segments;
};

struct NetworkPeriod {
    double time;
    double bandwidth;
};

enum class State {
    STARTUP,
    STEADY
};

class AshBolaEnh {
public:
    double Vp;
    double gp;
    vector<double> utilities;
    double placeholder = 0;
    int last_quality = 0;
    State state = State::STARTUP;
    ManifestInfo manifest;

    AshBolaEnh(const ManifestInfo& m, double buffer_size, double gamma_p) : manifest(m), Vp(0), gp(gamma_p) {
        for (int bitrate : manifest.bitrates) {
            utilities.push_back(log(bitrate) - log(manifest.bitrates[0]));  // Normalize utilities
        }
        double buffer = buffer_size;  // This can be a constant or a variable from a config
        Vp = (buffer - manifest.segment_time) / (utilities.back() + gp);
    }

    int select_quality(double predicted_bandwidth) {
        int quality = 0;
        double max_score = -numeric_limits<double>::max();
        double buffer_level = placeholder;  // This should be calculated from actual buffer status

        for (size_t i = 0; i < manifest.bitrates.size(); ++i) {
            double score = (Vp * (utilities[i] + gp) - buffer_level) / manifest.bitrates[i];
            if (score > max_score && manifest.bitrates[i] <= predicted_bandwidth) {
                max_score = score;
                quality = i;
            }
        }
        last_quality = quality;
        return quality;
    }
};

string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    cout << "Full output from Python script:" << endl;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        cout << buffer.data();  // Print each line as it's received
        result += buffer.data();
    }
    cout << "End of script output" << endl;
    return result;
}

double get_predicted_bandwidth(const json& inputData, bool debug = false) {
    string debug_flag = debug ? "debug" : "";
    string command = "echo '" + inputData.dump() + "' | python3 modelhandler.py " + debug_flag;
    cout << "Executing command for bandwidth prediction: " << command << endl;
    string output = exec(command.c_str());
    
    // Attempt to find the last line which should be the numeric value
    std::istringstream iss(output);
    std::string line;
    double bandwidth = 0.0;
    while (std::getline(iss, line)) {
        try {
            bandwidth = std::stod(line);
            cout << "Successfully parsed bandwidth: " << bandwidth << endl;
            return bandwidth;
        } catch (const std::invalid_argument&) {
            continue; // Ignore parse errors and try the next line
        }
    }
    cerr << "No valid numeric output was found. Last tried line: '" << line << "'" << endl;
    throw std::invalid_argument("Failed to parse any numeric output from Python script.");
}

vector<double> prepare_prediction_data(const vector<NetworkPeriod>& network_trace, int segment_index, int window_size) {
    vector<double> data;
    for (int i = segment_index - window_size; i < segment_index; ++i) {
        if (i >= 0 && i < network_trace.size()) {
            double period_time = network_trace[i].time / 1000.0;
            double period_bandwidth = network_trace[i].bandwidth / 1000.0;
            data.push_back(period_time * period_bandwidth);
            data.push_back(period_time);
            data.push_back(period_bandwidth);
        } else {
            // If no data is available, use zeros as placeholder
            data.push_back(0.0);
            data.push_back(0.0);
            data.push_back(0.0);
        }
    }
    return data;
}

int main() {
    ifstream manifest_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/bbb.json"), network_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/sd_fs/trace0001.json");
    json manifest_json, network_json;

    manifest_file >> manifest_json;
    network_file >> network_json;

    ManifestInfo manifest{
        manifest_json.at("segment_duration_ms").get<int>(),
        manifest_json.at("bitrates_kbps").get<vector<int>>(),
        manifest_json.at("segment_sizes_bits").get<vector<vector<int>>>()
    };

    vector<NetworkPeriod> network_trace;
    for (const auto& period : network_json) {
        network_trace.push_back({
            period.at("duration_ms").get<double>(),
            period.at("bandwidth_kbps").get<double>()
        });
    }

    AshBolaEnh abr(manifest, 25000, 5); // Example buffer size and gamma p

    int total_segments = manifest.segments.size();
    double total_bitrate_played = 0;
    int segment_count = 0;
    int window_size = 10;  // Example window size for the model input

    for (int i = 0; i < total_segments; ++i) {
        auto input_data = prepare_prediction_data(network_trace, i, window_size);
        json inputData = {{"data", input_data}}; // Input data for bandwidth prediction
        double predicted_bandwidth = get_predicted_bandwidth(inputData);
        
        int selected_quality = abr.select_quality(predicted_bandwidth);
        total_bitrate_played += manifest.bitrates[selected_quality];
        segment_count++;

        cout << "Segment " << i << ": Quality Index = " << selected_quality
             << ", Bitrate = " << manifest.bitrates[selected_quality] << " Kbps" << endl;
    }

    double average_bitrate = total_bitrate_played / segment_count;
    cout << "Average bitrate played: " << average_bitrate << " Kbps" << endl;

    return 0;
}
