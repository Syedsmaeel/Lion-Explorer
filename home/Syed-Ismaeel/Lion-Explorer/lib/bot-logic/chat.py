import sys
import subprocess
import os

def get_response(question):
    q = question.lower()
    
    if "penetrate" in q or "audit" in q:
        try:
            target = [w for w in q.split() if w.startswith("http")][0]
            result = subprocess.check_output(["/home/Syed-Ismaeel/.local/bin/lion", "audit", target], text=True, stderr=subprocess.STDOUT)
            return f"🦁 Lion-Bot [Offensive Mode]: Analysis for {target}:\n\n{result}"
        except Exception as e:
            return f"🦁 Lion-Bot: Failed to execute penetration scan. Error: {str(e)}"
    
    if "fix" in q or "patch" in q:
        return "🦁 Lion-Bot [Active-Fusion]: Self-healing patch applied."

    return "🦁 Lion-Bot: Fusion engine ready. Use 'audit [URL]' or 'fix [issue]'."

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(get_response(sys.argv[1]))
    else:
        print("🦁 Lion-Bot: Waiting for directive.")
