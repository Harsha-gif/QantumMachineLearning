// bayesian_neuron.h
#ifndef BAYESIAN_NEURON_H
#define BAYESIAN_NEURON_H

#include <vector>
#include <cmath>
#include <random>
#include <numeric>

using namespace std;

// A simple probabilistic Bayesian Layer
class BayesianLayer {
private:
    int input_size;
    int output_size;
    vector<vector<double>> mu_weights;    // Mean of weights
    vector<vector<double>> sigma_weights; // Variance of weights
    vector<double> mu_bias;

    mt19937 gen;

public:
    BayesianLayer(int in_size, int out_size) : input_size(in_size), output_size(out_size) {
        random_device rd;
        gen = mt19937(rd());
        
        // Initialize with dummy pre-trained distributions
        mu_weights.resize(out_size, vector<double>(in_size, 1.5));
        sigma_weights.resize(out_size, vector<double>(in_size, 0.5)); // The uncertainty spread
        mu_bias.resize(out_size, -0.5);

        // Add some variation for the QKD use case
        if (in_size == 3 && out_size == 3) {
            mu_weights = {{ 6.5, -2.1, 0.5}, {-1.2, 4.0, -1.0}, {3.0, 3.0, 1.0}};
            sigma_weights = {{ 1.5, 0.8, 0.2}, {0.5, 1.2, 0.3}, {1.0, 1.0, 0.5}};
        }
    }

    // Stochastic Forward Pass: Samples weights from N(mu, sigma^2)
    vector<double> forward(const vector<double>& input) {
        vector<double> output(output_size, 0.0);
        
        for (int i = 0; i < output_size; i++) {
            double sum = mu_bias[i];
            for (int j = 0; j < input_size; j++) {
                // Sample a weight from the Gaussian distribution
                normal_distribution<double> dist(mu_weights[i][j], sigma_weights[i][j]);
                double sampled_weight = dist(gen);
                sum += input[j] * sampled_weight;
            }
            // Sigmoid activation
            output[i] = 1.0 / (1.0 + exp(-sum));
        }
        return output;
    }
};

class BayesianAnomalyDetector {
private:
    BayesianLayer hidden_layer;
    BayesianLayer output_layer;

public:
    BayesianAnomalyDetector() : hidden_layer(3, 3), output_layer(3, 1) {}

    // Runs a single stochastic pass
    double single_pass(double qber, double sift_eff, double dark_counts) {
        vector<double> input = {qber, sift_eff, dark_counts};
        vector<double> hidden_out = hidden_layer.forward(input);
        vector<double> final_out = output_layer.forward(hidden_out);
        return final_out[0];
    }
    
    // Runs Monte Carlo sampling to quantify uncertainty
    struct BNNResult {
        double mean_threat;
        double uncertainty_stddev;
    };

    BNNResult monte_carlo_inference(double qber, double sift_eff, double dark_counts, int num_samples = 50) {
        vector<double> predictions;
        double sum = 0.0;
        
        for(int i = 0; i < num_samples; i++) {
            double pred = single_pass(qber, sift_eff, dark_counts);
            predictions.push_back(pred);
            sum += pred;
        }
        
        double mean = sum / num_samples;
        
        double variance_sum = 0.0;
        for(double p : predictions) {
            variance_sum += (p - mean) * (p - mean);
        }
        double stddev = sqrt(variance_sum / num_samples);
        
        return {mean, stddev};
    }
};

#endif
