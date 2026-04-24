FROM python:3.11-slim
RUN apt-get update && apt-get install -y curl wget git bash python3-pip
RUN pip install fastapi uvicorn requests --break-system-packages
COPY . /app
WORKDIR /app
CMD ["python3", "lib/api/main.py"]
