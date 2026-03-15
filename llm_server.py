import socket
import json
import requests
import sys

# Configuration
HOST = '127.0.0.1'
PORT = 5005
OLLAMA_API_URL = "http://localhost:11434/api/generate"
LLM_MODEL = "llama3" #Others: 'mistral', 'phi3', or 'qwen'

def generate_incident_report(telemetry):
    print("\n[LLM SOC AGENT] Anomaly received! Drafting executive incident report...\n")
    
    # Extract the math from your C++ Bayesian Network
    qber = telemetry.get("qber", 0) * 100
    threat = telemetry.get("threat_mean", 0) * 100
    uncertainty = telemetry.get("uncertainty_stddev", 0) * 100
    batch_id = telemetry.get("batch_id", "Unknown")

    # The Prompt Engineering
    prompt = f"""
    You are an elite Quantum Cybersecurity Analyst monitoring a BB84 Quantum Key Distribution (QKD) system.
    Our C++ Bayesian Neural Network just flagged an anomaly in Batch {batch_id}.
    
    TELEMETRY DATA:
    - Quantum Bit Error Rate (QBER): {qber:.2f}% (Normal is < 5%)
    - Bayesian Threat Confidence: {threat:.2f}%
    - Epistemic Uncertainty: {uncertainty:.2f}%
    
    INSTRUCTIONS:
    Write a strict, professional 3-sentence executive incident report. 
    If uncertainty is > 10%, suggest hardware sensor recalibration. 
    If uncertainty is < 10% and threat is high, declare a confirmed Intercept-Resend/PNS cyber attack and abort the key exchange.
    Do not use pleasantries. Just the report.
    """

    payload = {
        "model": LLM_MODEL,
        "prompt": prompt,
        "stream": False
    }

    print("--- 🚨 EXECUTIVE INCIDENT REPORT 🚨 ---")
    
    try:
        # Attempt to call the local LLM
        response = requests.post(OLLAMA_API_URL, json=payload, timeout=30)
        response.raise_for_status()
        result = response.json()
        print(result.get("response", "").strip())
        
    except requests.exceptions.ConnectionError:
        # Fallback if Ollama isn't running yet
        print(f"(Local LLM engine not detected. Simulating AI output...)")
        if uncertainty > 10:
            print(f"WARNING: High error rate ({qber:.2f}%) detected, but Bayesian uncertainty is elevated ({uncertainty:.2f}%). Hardware sensor drift or APD degradation is suspected. Suspending key exchange pending physical fiber recalibration.")
        else:
            print(f"CRITICAL BREACH: Confirmed Intercept-Resend attack with {threat:.2f}% confidence. Bayesian uncertainty is low ({uncertainty:.2f}%), ruling out natural hardware noise. Quantum Key Exchange aborted; initiate immediate physical audit of the quantum channel.")
            
    print("---------------------------------------\n")
    print(f"[PYTHON] Listening on {HOST}:{PORT} for next batch...")

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # Allow immediate port reuse if you restart the script
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"=== AUTONOMOUS LLM SOC SERVER ONLINE ===")
        print(f"Listening for C++ Bayesian Telemetry on {HOST}:{PORT}...")
        
        while True:
            conn, addr = s.accept()
            with conn:
                data = conn.recv(2048)
                if data:
                    try:
                        telemetry = json.loads(data.decode('utf-8'))
                        generate_incident_report(telemetry)
                    except json.JSONDecodeError:
                        print("[ERROR] Received malformed data from C++ engine.")

if __name__ == "__main__":
    try:
        import requests
    except ImportError:
        print("Please run: pip install requests")
        sys.exit(1)
        
    start_server()
