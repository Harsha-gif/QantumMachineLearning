#  Agentic QKD-SOC: Quantum Key Distribution Simulation

An end-to-end, multi-process simulation of a Quantum Key Distribution (QKD) system (BB84 protocol). This architecture is secured by a custom, from-scratch C++ Bayesian Neural Network and monitored by an autonomous Python-based Large Language Model (LLM) Security Operations Center (SOC).

##  System Architecture

This project simulates a complete quantum cryptography pipeline and cyber-attack detection system across three distinct technological layers:

1. **The Quantum Layer (C++ IPC Simulation)**
   * **Alice & Bob:** Processes communicating via UNIX sockets using a custom Inter-Process Communication (IMSG) protocol. They exchange quantum states and bases using the BB84 protocol.
   * **The Channel (Eve):** A simulated quantum channel that introduces an Intercept-Resend attack based on configurable probabilities.

2. **The Machine Learning Layer (C++ Bayesian Neural Network)**
   * A custom-built `BayesianAnomalyDetector` runs in an isolated ML Monitor process.
   * Unlike standard neural networks, this layer uses **Stochastic Monte Carlo Inference** to output not just a *Threat Confidence* score, but also an *Epistemic Uncertainty* metric (σ).
   * This allows the system to mathematically distinguish between an active cyber attack and natural hardware sensor drift.

3. **The Agentic SOC Layer (Python + Ollama LLM)**
   * When the Bayesian network detects a high-confidence threat, it fires a JSON payload via a TCP socket to a Python server.
   * The Python server acts as an autonomous Security Operations Center, feeding the telemetry into a local LLM (via Ollama) to dynamically generate executive incident reports and recommend immediate mitigation strategies.

##  File Structure

Ensure your project directory contains the following files:
* `qkd_ml_sim.cpp` - The C++ orchestrator containing the IMSG architecture and the Alice, Bob, Channel, and ML Monitor processes.
* `bayesian_neuron.h` - The custom C++ header for the Bayesian Neural Network and Monte Carlo sampling.
* `llm_server.py` - The Python server and LLM integration script.
* `Makefile` - Build automation script.

##  Prerequisites

* **C++ Compiler:** `g++` (supporting C++11 or higher)
* **Python 3.x:** With the `requests` library installed (`pip install requests`)
* **Ollama (Optional but Recommended):** A local LLM engine running a model like `llama3` or `mistral` on port `11434`. *(Note: The Python script includes a fallback simulator if Ollama is not installed).*

##  How to Build and Run

### Start the Autonomous LLM SOC
Open your terminal and start the Python server first so it can listen for telemetry.

python3 llm_server.py

### Start the qkd_ml_sim simulator.
Open another terminal and first compile the C++ file.

g++ qkd_ml_sim.cpp
./a.out
