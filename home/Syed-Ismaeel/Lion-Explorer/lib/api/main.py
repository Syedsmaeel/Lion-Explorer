from fastapi import FastAPI
import subprocess
import uvicorn

app = FastAPI()

@app.get("/")
def read_root():
    return {"status": "Lion Explorer Online", "message": "The Autonomous Engine is persistent and ready."}

@app.post("/mission")
def start_mission(target: str, mode: str):
    # Triggers the autonomous scanner/hunter
    subprocess.Popen(["bash", "/home/Syed-Ismaeel/Lion-Explorer/lib/hunter-fusion/hunter.sh", target])
    return {"message": f"Mission {mode} initiated against {target}"}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
