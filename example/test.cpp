#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <nlohmann/json.hpp>  // For JSON parsing

using json = nlohmann::json;
using namespace std;

struct ManifestInfo {
    int segment_time;
    vector<int> bitrates;
    vector<vector<int>> segments;  // Corrected to handle a 2D vector
};


struct NetworkPeriod {
    int time;
    int bandwidth;
    int latency;
};

struct DownloadProgress {
    int index;
    int quality;
    int size;
    int downloaded;
    int time;
    int time_to_first_bit;
    int abandon_to_quality;
};

// Dummy function for TensorFlow model prediction
double predict_bandwidth(const vector<double>& data) {
    return 5000;  // Dummy value for bandwidth in kbps
}

class NetworkModel {
public:
    vector<NetworkPeriod> trace;
    int index = 0;
    int time_to_next = 0;

    NetworkModel(const vector<NetworkPeriod>& network_trace) : trace(network_trace) {}

    void next_network_period() {
        index = (index + 1) % trace.size();
        time_to_next = trace[index].time;
    }

    int do_download(int size) {
        int total_time = 0;
        while (size > 0) {
            if (size < trace[index].bandwidth * time_to_next) {
                int time = size / trace[index].bandwidth;
                total_time += time;
                time_to_next -= time;
                size = 0;
            } else {
                total_time += time_to_next;
                size -= trace[index].bandwidth * time_to_next;
                next_network_period();
            }
        }
        return total_time;
    }
};
int main() {
    ifstream manifest_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/bbb.json"), network_file("/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/example/mmsys18/sd_fs/trace0000.json");
    json manifest_json, network_json;

    try {
        manifest_file >> manifest_json;
        network_file >> network_json;
    } catch (const json::parse_error& ex) {
        cerr << "Parse error at byte " << ex.byte << ": " << ex.what() << endl;
        return 1;
    }

    try {
        ManifestInfo manifest{
            manifest_json.at("segment_duration_ms").get<int>(),
            manifest_json.at("bitrates_kbps").get<vector<int>>(),
            manifest_json.at("segment_sizes_bits").get<vector<vector<int>>>()
        };

        vector<NetworkPeriod> network_trace;
        for (const auto& period : network_json) {
            network_trace.push_back({
                period.at("duration_ms").get<int>(),
                period.at("bandwidth_kbps").get<int>(),
                period.at("latency_ms").get<int>()
            });
        }

        NetworkModel networkModel(network_trace);
        // Download first segment at first quality
        // Ensure you're accessing the first segment and first bitrate's size correctly
        if (!manifest.segments.empty() && !manifest.segments[0].empty()) {
            int download_time = networkModel.do_download(manifest.segments[0][0]);
            cout << "Downloaded first segment in " << download_time << " ms" << endl;
        } else {
            cerr << "Segment data is empty or incorrectly formatted." << endl;
            return 1;
        }
    } catch (const json::exception& e) {
        cerr << "JSON error: " << e.what() << endl;
        return 1;
    } catch (const std::exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
