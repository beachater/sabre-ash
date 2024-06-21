#include <iostream>
#include <cmath>
#include <vector>
#include <map>
#include <tensorflow/core/public/session.h>
#include <tensorflow/cc/saved_model/loader.h>

extern bool verbose;
extern Manifest manifest;

class AshBola : public Abr {
public:
    AshBola(std::map<std::string, double> config) {
        utility_offset = -std::log(manifest.bitrates[0]);
        for (const auto& b : manifest.bitrates) {
            utilities.push_back(std::log(b) + utility_offset);
        }

        gp = config["gp"];
        buffer_size = config["buffer_size"];
        abr_osc = config["abr_osc"];
        abr_basic = config["abr_basic"];
        Vp = (buffer_size - manifest.segment_time) / (utilities.back() + gp);

        last_seek_index = 0;
        last_quality = 0;

        tensorflow::SavedModelBundleLite bundle;
        tensorflow::LoadSavedModel(tensorflow::SessionOptions(), tensorflow::RunOptions(), "/home/beachater/Thesis/simulation/sabre-ash/sabre-ash/src/ashBOLA3g4g.h5", {"serve"}, &bundle);
        session = std::move(bundle.session);

        if (verbose) {
            for (size_t q = 0; q < manifest.bitrates.size(); ++q) {
                double b = manifest.bitrates[q];
                double u = utilities[q];
                double l = Vp * (gp + u);
                if (q == 0) {
                    std::cout << q << " " << l << std::endl;
                } else {
                    size_t qq = q - 1;
                    double bb = manifest.bitrates[qq];
                    double uu = utilities[qq];
                    double ll = Vp * (gp + (b * uu - bb * u) / (b - bb));
                    std::cout << q << " " << l << " <- " << qq << " " << ll << std::endl;
                }
            }
        }
    }

    std::vector<std::vector<double>> prepare_prediction_data(const std::vector<NetworkPeriod>& network_trace, int segment_index, int window_size) {
        std::vector<std::vector<double>> data;
        for (int i = segment_index - window_size; i < segment_index; ++i) {
            if (i >= 0 && i < network_trace.size()) {
                const auto& period = network_trace[i];
                double period_time = period.time / 1000.0;
                double period_bandwidth = period.bandwidth / 1000.0;
                data.push_back({ period_time * period_bandwidth, period_time, period_bandwidth });
                std::cout << "windowsize " << window_size << std::endl;
            }
        }

        if (data.size() < window_size) {
            std::vector<std::vector<double>> padding(window_size - data.size(), std::vector<double>(3, 0));
            data.insert(data.begin(), padding.begin(), padding.end());
        }

        assert(data.size() == window_size);
        return data;
    }

    double predict_bandwidth(const std::vector<NetworkPeriod>& network_trace, int segment_index) {
        int window_size = 20;
        auto data = prepare_prediction_data(network_trace, segment_index, window_size);
        tensorflow::Tensor input_tensor(tensorflow::DT_FLOAT, tensorflow::TensorShape({ 1, window_size, 3 }));
        auto input_tensor_mapped = input_tensor.tensor<float, 3>();

        for (size_t i = 0; i < data.size(); ++i) {
            for (size_t j = 0; j < data[i].size(); ++j) {
                input_tensor_mapped(0, i, j) = data[i][j];
            }
        }

        std::vector<tensorflow::Tensor> outputs;
        session->Run({ {"serving_default_input_1", input_tensor} }, { "StatefulPartitionedCall" }, {}, &outputs);
        double predicted_bandwidth = outputs[0].matrix<float>()(0, 0);
        return predicted_bandwidth * 1000.0;
    }

    int quality_from_buffer(double predicted_throughput) {
        double level = get_buffer_level();
        int quality = 0;
        double score = -1;
        for (size_t q = 0; q < manifest.bitrates.size(); ++q) {
            double s = ((Vp * (utilities[q] + gp) - level) / manifest.bitrates[q]);
            if (score == -1 || s > score) {
                quality = q;
                score = s;
            }
        }
        return quality;
    }

    std::pair<int, double> get_quality_delay(int segment_index) {
        double predicted_throughput = predict_bandwidth(network_trace, segment_index);

        if (!abr_basic) {
            double t = std::min(segment_index - last_seek_index, static_cast<int>(manifest.segments.size()) - segment_index);
            t = std::max(t / 2.0, 3.0);
            t = t * manifest.segment_time;
            buffer_size = std::min(buffer_size, t);
            Vp = (buffer_size - manifest.segment_time) / (utilities.back() + gp);
        }

        int quality = quality_from_buffer(predicted_throughput);
        double delay = 0;

        if (quality > last_quality) {
            int quality_t = quality_from_throughput(predicted_throughput);
            if (quality <= quality_t) {
                delay = 0;
            } else if (last_quality > quality_t) {
                quality = last_quality;
                delay = 0;
            } else {
                if (!abr_osc) {
                    quality = quality_t + 1;
                    delay = 0;
                } else {
                    quality = quality_t;
                    double b = manifest.bitrates[quality];
                    double u = utilities[quality];
                    double l = Vp * (gp + u);
                    delay = std::max(0.0, get_buffer_level() - l);
                    if (quality == manifest.bitrates.size() - 1) {
                        delay = 0;
                    }
                }
            }
        }

        last_quality = quality;
        return { quality, delay };
    }

    void report_seek(double where) {
        last_seek_index = std::floor(where / manifest.segment_time);
    }

    int check_abandon(const Progress& progress, double buffer_level) {
        if (abr_basic) return -1;

        double remain = progress.size - progress.downloaded;
        if (progress.downloaded <= 0 || remain <= 0) return -1;

        int abandon_to = -1;
        double score = (Vp * (gp + utilities[progress.quality]) - buffer_level) / remain;
        if (score < 0) return -1;

        for (int q = 0; q < progress.quality; ++q) {
            double other_size = progress.size * manifest.bitrates[q] / manifest.bitrates[progress.quality];
            double other_score = (Vp * (gp + utilities[q]) - buffer_level) / other_size;
            if (other_size < remain && other_score > score) {
                score = other_score;
                abandon_to = q;
            }
        }

        if (abandon_to != -1) {
            last_quality = abandon_to;
        }

        return abandon_to;
    }

private:
    double utility_offset;
    std::vector<double> utilities;
    double gp;
    double buffer_size;
    bool abr_osc;
    bool abr_basic;
    double Vp;
    int last_seek_index;
    int last_quality;
    std::unique_ptr<tensorflow::Session> session;
};

extern std::map<std::string, std::unique_ptr<Abr>> abr_list;
abr_list["ashbola"] = std::make_unique<AshBola>();
