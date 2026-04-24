import sys, subprocess, os
from lib.auto_engineer import patcher
import sys
import subprocess
import os

def get_response(question):
    q = question.lower()
    
    # 1. Complex Reasoning: Delegate to OpenClaw
    if "deep" in q or "complex" in q or "hack" in q:
        try:
            print("🦁 Lion-Bot [Agentic Mode]: Delegating to OpenClaw Engine...")
            result = subprocess.check_output([
                "python3", "/home/Syed-Ismaeel/Lion-Explorer/lib/ai-training/plugins/openclaw/main.py",
                "--task", "audit", "--target", question.split()[-1]
            ], text=True, stderr=subprocess.STDOUT)
            return f"🦁 Lion-Bot [OpenClaw Analysis]:\n\n{result}"
        except Exception as e:
            return f"🦁 Lion-Bot: Agentic delegation failed. Error: {str(e)}"
    
    # 2. Fast/Surface Mode: Use native Fusion Toolkit
    if "audit" in q or "penetrate" in q:
        target = question.split()[-1]
        result = subprocess.check_output(["/home/Syed-Ismaeel/.local/bin/lion", "audit", target], text=True, stderr=subprocess.STDOUT)
        return f"🦁 Lion-Bot [Offensive Mode]: Analysis for {target}:\n\n{result}"

    return "🦁 Lion-Bot: Fusion Hub ready. Use 'audit [URL]' for fast scans or 'deep audit [URL]' for AI-Agent research."

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(get_response(sys.argv[1]))
    else:
        print("🦁 Lion-Bot: Waiting for directive.")
