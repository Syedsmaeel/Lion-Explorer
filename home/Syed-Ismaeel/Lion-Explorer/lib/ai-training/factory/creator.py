import os, sys

def create_fusion_project(name, requirements):
    project_dir = f"projects/{name}"
    os.makedirs(project_dir, exist_ok=True)
    
    # Generate the platform architecture
    with open(f"{project_dir}/README.md", "w") as f:
        f.write(f"# {name}\n\nGenerated autonomously by Lion Explorer AI.\n\n## Requirements:\n{requirements}")
    
    # Generate a sample Fusion Action
    with open(f"{project_dir}/.github/workflows/fusion.yml", "w") as f:
        f.write("name: Fusion Action\non: [push]\njobs: {build: {runs-on: ubuntu-latest, steps: [{run: 'echo Fusion Active'}]}}")
        
    print(f"🦁 Fusion-Creator: {name} project successfully synthesized.")

if __name__ == "__main__":
    create_fusion_project(sys.argv[1], sys.argv[2])
