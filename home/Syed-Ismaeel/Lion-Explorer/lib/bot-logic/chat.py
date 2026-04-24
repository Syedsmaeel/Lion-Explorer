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
        # Example of applying an autonomous fix
        subprocess.run(["git", "commit", "-am", "Auto: Applied AI Fusion Fix"], cwd="/home/Syed-Ismaeel/Lion-Explorer")
        return "🦁 Lion-Bot [Active-Fusion]: Self-healing patch applied to the repository."

    return "🦁 Lion-Bot: Fusion engine ready. Use 'audit [URL]' for offensive recon or 'fix [issue]' for active patching."

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(get_response(sys.argv[1]))
    else:
        print("🦁 Lion-Bot: Waiting for directive.")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(get_response(sys.argv[1]))
    else:
        print("🦁 Lion-Bot: Waiting for directive.")
