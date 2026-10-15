FROM python:3.12-slim
WORKDIR /app
# PyTorch's build for the CPU: the server has no GPU, and the CUDA build is gigabytes bigger.
RUN pip install --no-cache-dir torch==2.14.1 --index-url https://download.pytorch.org/whl/cpu
ENV HOST=0.0.0.0
COPY requirements-server.txt .
RUN pip install --no-cache-dir -r requirements-server.txt
COPY server.py tokenizer.py ./
EXPOSE 8000
CMD ["python", "server.py"]
