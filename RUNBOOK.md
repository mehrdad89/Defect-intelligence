# Runbook

This file gives you the shortest path to running the project locally.

## Prerequisites

- `git`
- a C++20 compiler
- `make`
- `python3` or another static file server for the frontend

## Option 1: Sample Mode

Use this when you want the full stack up quickly without scanning a real repository.

### Terminal 1: Backend

```bash
cd backend
make
./build/defect-intelligence serve --sample --port 8080
```

The API will be available at:

```bash
http://localhost:8080
```

### Terminal 2: Frontend

```bash
cd frontend
python3 -m http.server 4173
```

The UI will usually be available at:

```bash
http://localhost:4173
```

## Option 2: Real Repository Scan

Use this when you want the backend to scan a real local Git checkout.

### Terminal 1: Backend

```bash
cd backend
make
./build/defect-intelligence serve --repo /absolute/path/to/repo --port 8080
```

Example:

```bash
./build/defect-intelligence serve --repo /absolute/path/to/repo/db2 --port 8080
```

### Terminal 2: Frontend

```bash
cd frontend
python3 -m http.server 4173
```

## One-Shot CLI Scans

Sample report:

```bash
cd backend
./build/defect-intelligence scan --sample
```

Real repository, limited scan:

```bash
cd backend
./build/defect-intelligence scan --repo /absolute/path/to/repo --max-commits 25
```

Mainline-only scan:

```bash
cd backend
./build/defect-intelligence scan --repo /absolute/path/to/repo --history-mode first-parent --max-commits 100
```

## Quick API Checks

Health:

```bash
curl -s http://127.0.0.1:8080/api/v1/health
```

Summary:

```bash
curl -s "http://127.0.0.1:8080/api/v1/summary?maxCommits=25"
```

Insights:

```bash
curl -s "http://127.0.0.1:8080/api/v1/insights?maxCommits=25"
```

Recent commits for one component:

```bash
curl -s "http://127.0.0.1:8080/api/v1/commits?maxCommits=25&component=src%2Fscanner"
```

## Common Issues

If the frontend falls back to sample data:

- make sure the backend is running
- confirm the API base URL in `frontend/index.html` points to the backend port
- verify `http://127.0.0.1:8080/api/v1/health` responds

If a real scan fails with a `HEAD` error:

- confirm the target path is an actual Git repository
- confirm the repository has a valid checked-out `HEAD`

If the frontend is loading the wrong backend:

- edit the `defect-intelligence-api-base-url` meta tag in `frontend/index.html`
- or set `window.DEFECT_INTELLIGENCE_API_BASE_URL` before `src/main.js` loads
