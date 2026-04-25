# Repository Checklist

## Before Publishing

- keep product naming and examples generic
- keep defect markers neutral, such as `[art123456]`
- keep sample analytics synthetic
- avoid company-specific screenshots, URLs, or filesystem paths

## Local Review

- confirm `DI_DEFAULT_REPO_PATH` is not committed in a real `.env`
- confirm scan commands in screenshots or docs do not show private filesystem paths
- confirm frontend demo data still looks synthetic
- confirm any future issue-tracker integration does not embed tokens or internal URLs
- confirm any repository you scan locally is ignored if it produces exported data artifacts

## Recommended Hardening

- choose a license
- add CI for backend builds and frontend checks
- add tests for regex parsing and large-repo scan performance
- add auth if the API will be exposed beyond localhost
