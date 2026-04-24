import base64
import subprocess
import os

def apply_patch(repo, file_path, content, message):
    encoded_content = base64.b64encode(content.encode()).decode()
    # Use GitHub CLI to commit the fix remotely
    subprocess.run(["gh", "api", "-X", "PUT", f"/repos/{repo}/contents/{file_path}", 
                   "-f", f"message={message}", 
                   "-f", f"content={encoded_content}", 
                   "-f", "branch=master"], check=True)
    return f"🦁 Lion-Bot: Successfully pushed autonomous patch to {repo}/{file_path}"

if __name__ == "__main__":
    # Example usage: apply_patch("Syedsmaeel/Lion-Explorer", "README.md", "Updated by Lion-Bot", "Auto: Patch applied")
    pass
