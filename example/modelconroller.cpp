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
#include <iomanip>

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
    vector<double> bandwidth_history;
    const double initial_bandwidth_estimate = 1000.0;
    const int max_history_length = 10;

    AshBolaEnh(const ManifestInfo& m, double buffer_size, double gamma_p) : manifest(m), Vp(0), gp(gamma_p) {
        double utility_offset = 1 - log(manifest.bitrates[0]);
        for (int bitrate : manifest.bitrates) {
            utilities.push_back(log(bitrate) + utility_offset);  // Normalize utilities
        }
        double buffer = max(buffer_size, AshBolaEnh::minimum_buffer + AshBolaEnh::minimum_buffer_per_level * manifest.bitrates.size());
        gp = (utilities.back() - 1) / (buffer / AshBolaEnh::minimum_buffer - 1);
        Vp = AshBolaEnh::minimum_buffer / gp;
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

    double smooth_bandwidth() {
        if (bandwidth_history.empty()) {
            return initial_bandwidth_estimate;
        }
        double alpha = 0.1;  // Smoothing factor
        double smoothed_bandwidth = bandwidth_history.back();
        for (int i = bandwidth_history.size() - 2; i >= 0; --i) {
            smoothed_bandwidth = alpha * bandwidth_history[i] + (1 - alpha) * smoothed_bandwidth;
        }
        return smoothed_bandwidth;
    }

    bool detect_oscillation() {
        if (bandwidth_history.size() < max_history_length) {
            return false;
        }
        int increase = 0, decrease = 0;
        for (size_t i = 1; i < bandwidth_history.size(); ++i) {
            if (bandwidth_history[i] > bandwidth_history[i - 1]) {
                increase++;
            } else if (bandwidth_history[i] < bandwidth_history[i - 1]) {
                decrease++;
            }
        }
        return increase > 0 && decrease > 0;
    }

    double predict_bandwidth(const vector<NetworkPeriod>& network_trace, int segment_index, int window_size) {
        auto data = prepare_prediction_data(network_trace, segment_index, window_size);
        json inputData = {{"data", data}};
        double predicted_bandwidth = get_predicted_bandwidth(inputData);

        bandwidth_history.push_back(predicted_bandwidth);
        if (bandwidth_history.size() > max_history_length) {
            bandwidth_history.erase(bandwidth_history.begin());
        }

        if (detect_oscillation()) {
            return smooth_bandwidth();
        }

        // Handle sudden bandwidth drops
        if (bandwidth_history.size() > 1 && (bandwidth_history[bandwidth_history.size() - 2] - predicted_bandwidth) / bandwidth_history[bandwidth_history.size() - 2] > 0.5) {
            return smooth_bandwidth();
        }

        return predicted_bandwidth;
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

private:
    static const double minimum_buffer;
    static const double minimum_buffer_per_level;
    static const double low_buffer_safety_factor;
    static const double low_buffer_safety_factor_init;

    double get_predicted_bandwidth(const json& inputData, bool debug = false) {
        string debug_flag = debug ? "debug" : "";
        string command = "echo '" + inputData.dump() + "' | python3 modelhandler.py " + debug_flag;
        cout << "Executing command for bandwidth prediction: " << command << endl;
        string output = exec(command.c_str());

        // Attempt to find the last line which should be the numeric value
        istringstream iss(output);
        string line;
        double bandwidth = 0.0;
        while (getline(iss, line)) {
            try {
                bandwidth = stod(line);
                cout << "Successfully parsed bandwidth: " << bandwidth << endl;
                return bandwidth;
            } catch (const invalid_argument&) {
                continue; // Ignore parse errors and try the next line
            }
        }
        cerr << "No valid numeric output was found. Last tried line: '" << line << "'" << endl;
        throw invalid_argument("Failed to parse any numeric output from Python script.");
    }

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
};

const double AshBolaEnh::minimum_buffer = 10000;
const double AshBolaEnh::minimum_buffer_per_level = 2000;
const double AshBolaEnh::low_buffer_safety_factor = 0.5;
const double AshBolaEnh::low_buffer_safety_factor_init = 0.9;

int main() {
    ifstream manifest_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/bbb.json"), network_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/hd_fs/trace0007.json");
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
    double played_utility = 0;
    int segment_count = 0;
    int window_size = 10;  // Example window size for the model input
    double total_play_time = 0;
    double rebuffer_time = 0;
    int rebuffer_event_count = 0;
    double total_bitrate_change = 0;
    double total_log_bitrate_change = 0;
    int overestimate_count = 0;
    double overestimate_average = 0;
    int goodestimate_count = 0;
    double goodestimate_average = 0;
    double estimate_average = 0;
    double rampup_time = 0;
    double total_reaction_time = 0;
    double startup_time = 0;

    for (int i = 0; i < total_segments; ++i) {
        double predicted_bandwidth = abr.predict_bandwidth(network_trace, i, window_size);

        int selected_quality = abr.select_quality(predicted_bandwidth);
        total_bitrate_played += manifest.bitrates[selected_quality];
        played_utility += abr.utilities[selected_quality];
        segment_count++;

        cout << "Segment " << i << ": Quality Index = " << selected_quality
             << ", Bitrate = " << manifest.bitrates[selected_quality] << " Kbps" << endl;

        total_play_time += manifest.segment_time; // Assuming constant segment time for simplicity

        // Additional logic for rebuffering, bitrate changes, and other metrics can be added here
    }

    double to_time_average = 1 / (total_play_time / manifest.segment_time);
    double average_bitrate = total_bitrate_played / segment_count;
    cout << fixed << setprecision(2);
    cout << "Average bitrate played: " << average_bitrate << " Kbps" << endl;
    cout << "Buffer size: " << 25000 << endl;
    cout << "Total played utility: " << played_utility << endl;
    cout << "Time average played utility: " << (played_utility * to_time_average) << endl;
    cout << "Total played bitrate: " << total_bitrate_played << endl;
    cout << "Time average played bitrate: " << (total_bitrate_played * to_time_average) << endl;
    cout << "Total play time: " << (total_play_time / 1000) << " seconds" << endl;
    cout << "Total play time chunks: " << (total_play_time / manifest.segment_time) << endl;
    cout << "Total rebuffer: " << (rebuffer_time / 1000) << " seconds" << endl;
    cout << "Rebuffer ratio: " << (rebuffer_time / total_play_time) << endl;
    cout << "Time average rebuffer: " << (rebuffer_time / 1000 * to_time_average) << endl;
    cout << "Total rebuffer events: " << rebuffer_event_count << endl;
    cout << "Time average rebuffer events: " << (rebuffer_event_count * to_time_average) << endl;
    cout << "Total bitrate change: " << total_bitrate_change << endl;
    cout << "Time average bitrate change: " << (total_bitrate_change * to_time_average) << endl;
    cout << "Total log bitrate change: " << total_log_bitrate_change << endl;
    cout << "Time average log bitrate change: " << (total_log_bitrate_change * to_time_average) << endl;
    cout << "Time average score: " << (to_time_average * (played_utility - 5 * rebuffer_time / manifest.segment_time)) << endl;
    if (overestimate_count == 0) {
        cout << "Over estimate count: 0" << endl;
        cout << "Over estimate: 0" << endl;
    } else {
        cout << "Over estimate count: " << overestimate_count << endl;
        cout << "Over estimate: " << overestimate_average << endl;
    }
    if (goodestimate_count == 0) {
        cout << "Leq estimate count: 0" << endl;
        cout << "Leq estimate: 0" << endl;
    } else {
        cout << "Leq estimate count: " << goodestimate_count << endl;
        cout << "Leq estimate: " << goodestimate_average << endl;
    }
    cout << "Estimate: " << estimate_average << endl;
    if (rampup_time == 0) {
        cout << "Rampup time: " << (total_segments * manifest.segment_time / 1000) << " seconds" << endl;
    } else {
        cout << "Rampup time: " << (rampup_time / 1000) << " seconds" << endl;
    }
    cout << "Total reaction time: " << (total_reaction_time / 1000) << " seconds" << endl;

    return 0;
}
