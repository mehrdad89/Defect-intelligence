import { demoReport } from "./demoData";
import type { Report } from "./types";

export interface ReportRequest {
  repoPath?: string;
  revision?: string;
  historyMode?: "full" | "first-parent";
  maxCommits?: number;
  sample?: boolean;
}

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL ?? "http://localhost:8080";

function buildParams(request: ReportRequest): URLSearchParams {
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

export async function fetchReport(request: ReportRequest): Promise<Report> {
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

  return (await response.json()) as Report;
}

export async function loadReportWithFallback(request: ReportRequest): Promise<Report> {
  try {
    return await fetchReport(request);
  } catch {
    return demoReport;
  }
}

export { API_BASE_URL };

