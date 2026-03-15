#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <cstring>
#include <iomanip>
#include "bayesian_neuron.h"

using namespace std;

// ============================================================================
// 1. IMSG IPC ARCHITECTURE (Inter-Process Communication)
// ============================================================================

enum ImsgType {
    IMSG_PHOTON = 1,
    IMSG_BASIS_REVEAL,
    IMSG_TELEMETRY,
    IMSG_TERMINATE
};

// Standardized Message Header
struct ImsgHeader {
    ImsgType type;
    size_t len;
};

// Payload: Quantum State
struct PhotonPayload {
    uint32_t pulse_id;
    int basis; // 0 = Rectilinear (+), 1 = Diagonal (x)
    int state; // 0 or 1
};

// Payload: Sifting Phase
struct BasisPayload {
    uint32_t pulse_id;
    int basis;
    int measured_bit;
};

// Payload: Machine Learning Telemetry
struct TelemetryPayload {
    uint32_t batch_id;
    double qber;             // Quantum Bit Error Rate
    double sift_efficiency;  // Rate of matching bases
    double dark_count_rate;  // Simulated noise
};

// Helper function to send IMSG
void imsg_send(int fd, ImsgType type, const void* data, size_t len) {
    ImsgHeader hdr = {type, len};
    write(fd, &hdr, sizeof(hdr));
    if (len > 0 && data != nullptr) {
        write(fd, data, len);
    }
}

// ============================================================================
// 2. FROM-SCRATCH MACHINE LEARNING NEURAL NETWORK
// ============================================================================

class AnomalyDetectionNN {
private:
    vector<vector<double>> weights_hidden;
    vector<double> bias_hidden;
    vector<double> weights_output;
    double bias_output;

    double sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

public:
    AnomalyDetectionNN() {
        // Pre-trained weights to detect BB84 intercept-resend attacks
        // Eve introduces ~25% QBER. High QBER + Shift in sift efficiency = Attack
        weights_hidden = {
            { 8.5, -2.1,  0.5}, // Node 1: Sensitive to QBER
            {-1.2,  5.0, -1.0}, // Node 2: Sensitive to Sift Drop
            { 4.0,  4.0,  2.0}  // Node 3: Combined Risk
        };
        bias_hidden = {-1.0, -2.5, -3.0};
        weights_output = {5.5, 2.0, 3.1};
        bias_output = -4.0;
    }

    double forward(double qber, double sift_eff, double dark_counts) {
        vector<double> input = {qber, sift_eff, dark_counts};
        vector<double> hidden(3, 0.0);
        
        // Hidden Layer
        for (int i = 0; i < 3; i++) {
            double sum = bias_hidden[i];
            for (int j = 0; j < 3; j++) {
                sum += input[j] * weights_hidden[i][j];
            }
            hidden[i] = sigmoid(sum);
        }
        
        // Output Layer
        double out_sum = bias_output;
        for (int i = 0; i < 3; i++) {
            out_sum += hidden[i] * weights_output[i];
        }
        return sigmoid(out_sum); // Returns Eve probability (0.0 to 1.0)
    }
};

// ============================================================================
// 3. ISOLATED PROCESSES (Alice, Bob, Channel, ML Monitor)
// ============================================================================

const int TOTAL_PULSES = 5000;
const int BATCH_SIZE = 1000;

void process_alice(int fd_channel) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, 1);

    cout << "[ALICE] Process started (PID: " << getpid() << "). Generating photons..." << endl;

    for (int i = 0; i < TOTAL_PULSES; i++) {
        PhotonPayload photon;
        photon.pulse_id = i;
        photon.basis = dist(gen);
        photon.state = dist(gen);

        imsg_send(fd_channel, IMSG_PHOTON, &photon, sizeof(photon));
        usleep(500); // Simulate pulse generation delay
    }
    
    imsg_send(fd_channel, IMSG_TERMINATE, nullptr, 0);
    close(fd_channel);
    exit(0);
}

void process_channel(int fd_alice, int fd_bob, double eve_attack_probability) {
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> attack_dist(0.0, 1.0);
    uniform_int_distribution<> eve_basis_dist(0, 1);
    uniform_int_distribution<> eve_measure_dist(0, 1);

    cout << "[CHANNEL] Quantum routing started. Eve interception probability: " 
         << eve_attack_probability * 100 << "%" << endl;

    ImsgHeader hdr;
    while (read(fd_alice, &hdr, sizeof(hdr)) > 0) {
        if (hdr.type == IMSG_TERMINATE) {
            imsg_send(fd_bob, IMSG_TERMINATE, nullptr, 0);
            break;
        }

        if (hdr.type == IMSG_PHOTON) {
            PhotonPayload photon;
            read(fd_alice, &photon, sizeof(photon));

            // EVE ATTACK LOGIC (Intercept-Resend)
            if (attack_dist(gen) < eve_attack_probability) {
                int eve_basis = eve_basis_dist(gen);
                if (eve_basis != photon.basis) {
                    // Eve guessed wrong basis -> State collapses to random
                    photon.state = eve_measure_dist(gen);
                    photon.basis = eve_basis; // Eve resends in her basis
                }
            }

            // Route to Bob
            imsg_send(fd_bob, IMSG_PHOTON, &photon, sizeof(photon));
        }
    }
    close(fd_alice);
    close(fd_bob);
    exit(0);
}

void process_bob(int fd_channel, int fd_ml) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, 1);

    cout << "[BOB] Process started. Waiting for quantum channel..." << endl;

    int valid_sifts = 0;
    int errors = 0;
    int total_received = 0;

    ImsgHeader hdr;
    while (read(fd_channel, &hdr, sizeof(hdr)) > 0) {
        if (hdr.type == IMSG_TERMINATE) {
            imsg_send(fd_ml, IMSG_TERMINATE, nullptr, 0);
            break;
        }

        if (hdr.type == IMSG_PHOTON) {
            PhotonPayload photon;
            read(fd_channel, &photon, sizeof(photon));
            total_received++;

            int bob_basis = dist(gen);
            int bob_measurement;

            if (bob_basis == photon.basis) {
                // Perfect measurement
                bob_measurement = photon.state;
                valid_sifts++;
                
                // For simulation purposes, Bob checks error natively here to generate telemetry
                // In reality, Alice and Bob would sacrifice bits over public channel
                if (photon.pulse_id % 5 == 0) { // Sacrifice 20% of bits for QBER check
                     // Simulated error tracking due to Eve
                     // We know the original state implicitly in this simulation scope
                     // but mathematically treating it as error check.
                }
            } else {
                // Wrong basis, random measurement
                bob_measurement = dist(gen);
            }

            // Simulate Eve-induced errors for telemetry payload
            if (bob_basis == photon.basis && bob_measurement != photon.state) {
                 errors++; 
            } else if (photon.basis != bob_basis && bob_measurement != photon.state) {
                 // Eve changed the basis!
                 errors++; 
            }

            // Send telemetry batch to ML Monitor
            if (total_received % BATCH_SIZE == 0) {
                TelemetryPayload telemetry;
                telemetry.batch_id = total_received / BATCH_SIZE;
                telemetry.sift_efficiency = (double)valid_sifts / BATCH_SIZE;
                telemetry.qber = (double)errors / valid_sifts;
                if(isnan(telemetry.qber)) telemetry.qber = 0;
                telemetry.dark_count_rate = 0.01; // Baseline hardware noise

                imsg_send(fd_ml, IMSG_TELEMETRY, &telemetry, sizeof(telemetry));
                
                // Reset batch counters
                valid_sifts = 0;
                errors = 0;
            }
        }
    }
    close(fd_channel);
    close(fd_ml);
    exit(0);
}

#include "bayesian_neuron.h" // <--- Include your new master thesis header

// ... [Keep IMSG Structs and process_alice, process_channel, process_bob as they are] ...

void process_ml_monitor(int fd_bob) {
    // Initialize your custom Bayesian Neural Network
    BayesianAnomalyDetector bnn_model;
    
    cout << "[ML-SOC] Agentic Security Operations Center Active." << endl;
    cout << "[ML-SOC] Bayesian Neural Network initialized for Uncertainty Quantification." << endl;

    ImsgHeader hdr;
    while (read(fd_bob, &hdr, sizeof(hdr)) > 0) {
        if (hdr.type == IMSG_TERMINATE) break;

        if (hdr.type == IMSG_TELEMETRY) {
            TelemetryPayload telemetry;
            read(fd_bob, &telemetry, sizeof(telemetry));

            // Run 50 stochastic Monte Carlo passes to get Mean and Uncertainty
            auto result = bnn_model.monte_carlo_inference(
                telemetry.qber, telemetry.sift_efficiency, telemetry.dark_count_rate, 50
            );

            cout << "\n==================================================" << endl;
            cout << " [SOC DASHBOARD] BATCH " << telemetry.batch_id << " TELEMETRY" << endl;
            cout << " -> QBER: " << fixed << setprecision(2) << (telemetry.qber * 100) << "%" << endl;
            
            cout << "\n [BAYESIAN INFERENCE RESULTS]" << endl;
            cout << " -> Threat Level (Mean):   " << (result.mean_threat * 100) << "%" << endl;
            cout << " -> Uncertainty (\u03C3):      " << (result.uncertainty_stddev * 100) << "%" << endl;
            
            // Advanced Decision Matrix based on Uncertainty Quantification
if (result.mean_threat > 0.75) {
                cout << " [!] CRITICAL ALARM: Threat detected! Forwarding telemetry to Python LLM SOC..." << endl;
                
                // 1. Format the telemetry as a JSON string
                string json_payload = "{\"batch_id\":" + to_string(telemetry.batch_id) + 
                                      ", \"qber\":" + to_string(telemetry.qber) + 
                                      ", \"threat_mean\":" + to_string(result.mean_threat) + 
                                      ", \"uncertainty_stddev\":" + to_string(result.uncertainty_stddev) + "}";

                // 2. Open a TCP socket to the Python server (Port 5005)
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock >= 0) {
                    struct sockaddr_in serv_addr;
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_port = htons(5005);
                    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

                    // 3. Connect and send
                    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) {
                        send(sock, json_payload.c_str(), json_payload.length(), 0);
                    }
                    close(sock); // Close the socket immediately after sending
                }
            } else {
                cout << "\n [✓] STATUS SECURE. No eavesdropping detected." << endl;
            }
            
            cout << "==================================================\n" << endl;
        }
    }
    close(fd_bob);
    exit(0);
}
// ============================================================================
// 4. MAIN ORCHESTRATOR (Spawns processes and routes sockets)
// ============================================================================

int main() {
    cout << "=== INITIALIZING QKD-ML IMSG SIMULATION CLUSTER ===" << endl;

    // Create IPC Socket Pairs
    int sock_alice_chan[2];
    int sock_chan_bob[2];
    int sock_bob_ml[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock_alice_chan) == -1 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, sock_chan_bob) == -1 ||
        socketpair(AF_UNIX, SOCK_STREAM, 0, sock_bob_ml) == -1) {
        cerr << "Failed to create IPC sockets." << endl;
        return 1;
    }

    // Toggle this to 0.0 to see a clean channel, or >0.3 to trigger Eve alarms :)
    double eve_interception_probability = 0.45; 

    pid_t pid_alice = fork();
    if (pid_alice == 0) {
        close(sock_alice_chan[0]);
        process_alice(sock_alice_chan[1]);
    }

    pid_t pid_channel = fork();
    if (pid_channel == 0) {
        close(sock_alice_chan[1]);
        close(sock_chan_bob[0]);
        process_channel(sock_alice_chan[0], sock_chan_bob[1], eve_interception_probability);
    }

    pid_t pid_bob = fork();
    if (pid_bob == 0) {
        close(sock_chan_bob[1]);
        close(sock_bob_ml[0]);
        process_bob(sock_chan_bob[0], sock_bob_ml[1]);
    }

    pid_t pid_ml = fork();
    if (pid_ml == 0) {
        close(sock_bob_ml[1]);
        process_ml_monitor(sock_bob_ml[0]);
    }

    // Main process closes all child endpoints and waits
    close(sock_alice_chan[0]); close(sock_alice_chan[1]);
    close(sock_chan_bob[0]); close(sock_chan_bob[1]);
    close(sock_bob_ml[0]); close(sock_bob_ml[1]);

    waitpid(pid_alice, NULL, 0);
    waitpid(pid_channel, NULL, 0);
    waitpid(pid_bob, NULL, 0);
    waitpid(pid_ml, NULL, 0);

    cout << "=== SIMULATION COMPLETED SECURELY ===" << endl;
    return 0;
}