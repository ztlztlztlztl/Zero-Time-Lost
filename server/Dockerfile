ARG PYTHON_IMAGE=python:3.12-slim
FROM ${PYTHON_IMAGE}
WORKDIR /app
COPY server/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY server/__init__.py server/app.py server/store.py server/jobs.py server/validation.py server/release.json /app/server/
COPY scripts/mail_worker.py /app/scripts/mail_worker.py
COPY scripts/product-version.txt /app/scripts/product-version.txt
ENV ZTL_DB=/data/ztl.sqlite PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
CMD ["uvicorn", "server.app:app", "--host", "0.0.0.0", "--port", "8000", "--no-access-log", "--timeout-graceful-shutdown", "20"]
