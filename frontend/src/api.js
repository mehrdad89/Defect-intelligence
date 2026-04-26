import { demoReport } from "./demo-data.js";

function readApiBaseUrl() {
  const configuredMeta = document
    .querySelector('meta[name="defect-intelligence-api-base-url"]')
    ?.getAttribute("content")
    ?.trim();

  if (configuredMeta) {
    return configuredMeta;
  }

  if (window.DEFECT_INTELLIGENCE_API_BASE_URL) {
    return window.DEFECT_INTELLIGENCE_API_BASE_URL;
  }

  return "http://localhost:8080";
}

export const API_BASE_URL = readApiBaseUrl();

function buildParams(request) {
  const params = new URLSearchParams();
  if (request.repoPath) {
    params.set("repoPath", request.repoPath);
  }
  if (request.revision) {
    params.set("rev", request.revision);
  }
  if (request.historyMode) {
    params.set("historyMode", request.historyMode);
  }
  if (request.maxCommits) {
    params.set("maxCommits", String(request.maxCommits));
  }
  if (request.sample) {
    params.set("sample", "true");
  }
  return params;
}

export async function fetchReport(request) {
  const params = buildParams(request);
  const suffix = params.toString();
  const response = await fetch(
    `${API_BASE_URL}/api/v1/report${suffix ? `?${suffix}` : ""}`,
    {
      headers: {
        Accept: "application/json",
      },
    },
  );

  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || `Failed to load report: ${response.status}`);
  }

  return response.json();
}

export async function loadReportWithFallback(request) {
  try {
    return await fetchReport(request);
  } catch {
    return demoReport;
  }
}

