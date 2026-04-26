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

function buildHttpError(message, status) {
  const error = new Error(message);
  error.status = status;
  return error;
}

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
    const body = await response.text();
    let message = body || `Failed to load report: ${response.status}`;

    try {
      const parsed = JSON.parse(body);
      if (parsed?.error?.message) {
        message = parsed.error.message;
      }
    } catch {
      // Keep the raw response body when the error payload is not JSON.
    }

    throw buildHttpError(message, response.status);
  }

  return response.json();
}

export async function loadReportWithFallback(request) {
  try {
    return {
      report: await fetchReport(request),
      warning: null,
    };
  } catch (error) {
    const canUseBundledDemo = Boolean(request.sample || request.allowDemoFallback);
    const isNetworkLikeFailure =
      !(error instanceof Error) || typeof error.status !== "number";

    if (canUseBundledDemo && isNetworkLikeFailure) {
      return {
        report: demoReport,
        warning: "Backend unavailable. Showing bundled sample data instead.",
      };
    }

    throw error;
  }
}
