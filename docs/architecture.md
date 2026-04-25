# Architecture Overview

## Goal

Turn Git history into actionable developer productivity and quality analytics without assuming a vendor-specific workflow.

## Backend Flow

### 1. Commit enumeration

The scanner lists commits with:

- revision scope
- optional date range
- optional commit limit
- `full` or `first-parent` history mode

### 2. Detail enrichment

For each commit, the service reads:

- full commit message
- parents
- changed files and numstat churn
- normalized defect markers such as `ART-123456`
- referenced issue numbers such as `#123`

### 3. Correlation

The analytics layer rolls commits into:

- component hotspot metrics
- author activity summaries
- daily trend buckets
- recent defect-linked commits

### 4. API delivery

The HTTP service exposes the aggregate report through JSON endpoints that the React app can consume directly.

## Data Model

### Core entities

- `CommitSummary`
- `FileChange`
- `ComponentMetric`
- `AuthorMetric`
- `TrendBucket`
- `AiSummary`
- `ScanReport`

### Key metric ideas

- `relevant_commits`: commits carrying a defect marker or issue reference
- `unique_defects`: normalized unique customer-linked identifiers
- `hotspot_score`: simple ranking signal combining defect breadth, commit count, and churn
- `coverage_ratio`: ratio of relevant commits to scanned commits

## API Surface

- `GET /api/v1/report`: full report payload
- `GET /api/v1/summary`: top-level summary, timeline, and narrative summary
- `GET /api/v1/components`: component hotspot rollup
- `GET /api/v1/authors`: author rollup
- `GET /api/v1/commits`: recent commit list, optionally filtered by component
- `GET /api/v1/ai-summary`: optional narrative summary, optionally focused on one component

## Frontend Shape

The dashboard is intentionally small and review-friendly:

- high-signal summary cards
- component hotspot table
- lightweight trend view
- recent commit feed
- insight panel

The UI defaults to demo data when the backend is not reachable so the repo remains easy to review on GitHub.

## Extension Points

- background job queue for scheduled scans
- persistent cache for large repositories
- repo connector adapters beyond local Git
- richer defect taxonomies and customer tags
- external model provider integration behind feature flags
