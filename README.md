# Defect Intelligence Platform

Defect Intelligence Platform scans Git history, detects customer-linked defect markers such as `[art123456]`, correlates them with files, components, authors, and time ranges, and exposes the results through REST APIs and a React dashboard.

The project is intentionally vendor-neutral and keeps repository-specific assumptions out of the scanner, analytics model, and UI.

## What Is Included

- `backend/`: modern C++20 service with a batched Git scan pipeline, aggregation layer, optional narrative summary generation, and HTTP API
- `frontend/`: no-build React dashboard loaded from CDN for executive summaries, component hotspots, author activity, and recent defect-linked commits
- `docs/`: architecture notes and a repository checklist

## Product Shape

The backend focuses on a practical workflow:

1. enumerate Git commits from a target repository
2. read commit bodies and changed files
3. detect defect markers such as `[art123456]` or `ART-123456`
4. correlate those commits with components, files, authors, churn, and dates
5. surface the results through API endpoints and a small UI

The implementation stays intentionally pragmatic:

- no repo-specific assumptions are hard-coded
- sample mode is available when you do not want to point at a real repository yet
- summary generation works locally by default and can be extended to external providers later

## Backend Quick Start

The backend uses only the C++ standard library and the system `git` executable.

```bash
cd backend
make
./build/defect-intelligence scan --sample
./build/defect-intelligence serve --sample --port 8080
```

To scan a real repository:

```bash
./build/defect-intelligence scan --repo /absolute/path/to/repo --history-mode full --max-commits 500
./build/defect-intelligence serve --repo /absolute/path/to/repo --port 8080
```

Available API routes:

- `GET /api/v1/health`
- `GET /api/v1/config`
- `GET /api/v1/report`
- `GET /api/v1/summary`
- `GET /api/v1/components`
- `GET /api/v1/authors`
- `GET /api/v1/commits`
- `GET /api/v1/insights`

Useful query parameters:

- `repoPath=/absolute/path/to/repo`
- `rev=HEAD`
- `historyMode=full` or `historyMode=first-parent`
- `since=2026-01-01`
- `until=2026-04-25`
- `maxCommits=500`
- `sample=true`
- `component=src/scanner`

## Frontend Quick Start

The frontend is a static React app that loads React from a CDN and runs directly in the browser.

```bash
cd frontend
python3 -m http.server 4173
```

Open `http://localhost:4173`.

Set the API base URL by editing the `<meta name="defect-intelligence-api-base-url">` tag in [frontend/index.html](/Users/mehrdad.mehraban/repo/Defect-intelligence/frontend/index.html:1), or set `window.DEFECT_INTELLIGENCE_API_BASE_URL` before `main.js` loads.

If the API is unavailable, the UI falls back to sample data so the experience is still reviewable.

For the exact two-terminal startup flow, see [RUNBOOK.md](/Users/mehrdad.mehraban/repo/Defect-intelligence/RUNBOOK.md:1).

## Repository Notes

- generic terminology is used throughout the code and docs
- sample data is synthetic
- repository-specific paths and secrets belong in local environment files only
- model-backed summary integration can remain disabled by default

See [docs/publishability.md](/Users/mehrdad.mehraban/repo/Defect-intelligence/docs/publishability.md) for the checklist.

## Next Product Steps

- add persistent scan caching in SQLite or Postgres
- enrich issue references from GitHub, Jira, or Azure DevOps
- schedule background scans for large repositories
- connect the summary hook to an external provider behind a feature flag
- add team and release lineage overlays for quality analytics
