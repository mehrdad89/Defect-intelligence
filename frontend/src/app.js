import {
  html,
  startTransition,
  useDeferredValue,
  useEffect,
  useState,
} from "./react-cdn.js";
import { API_BASE_URL, loadReportWithFallback } from "./api.js";
import { demoReport } from "./demo-data.js";

function formatPercent(value) {
  return `${(value * 100).toFixed(1)}%`;
}

function formatDate(value) {
  const parsed = new Date(value);
  if (Number.isNaN(parsed.getTime())) {
    return value;
  }
  return parsed.toLocaleDateString(undefined, {
    month: "short",
    day: "numeric",
  });
}

function truncateHash(hash) {
  return hash.slice(0, 8);
}

function isSampleReport(report) {
  const repository = report?.metadata?.repository;
  return typeof repository === "string" && repository.startsWith("sample");
}

function StatCard({ label, value, note }) {
  return html`
    <article className="stat-card">
      <span className="stat-label">${label}</span>
      <strong className="stat-value">${value}</strong>
      <span className="stat-note">${note}</span>
    </article>
  `;
}

function Pill({ children, tone }) {
  return html`
    <span className=${`pill ${tone === "cool" ? "pill-cool" : ""}`.trim()}>
      ${children}
    </span>
  `;
}

function TrendBars({ report }) {
  const maxValue = Math.max(...report.timeline.map((bucket) => bucket.defectRefs), 1);

  return html`
    <section className="panel">
      <div className="panel-head">
        <div>
          <p className="eyebrow">Signal Rhythm</p>
          <h2>Recent defect-linked activity</h2>
        </div>
      </div>
      <div className="trend-grid">
        ${report.timeline.map(
          (bucket) => html`
            <div className="trend-item" key=${bucket.date}>
              <div
                className="trend-bar"
                style=${{
                  height: `${Math.max((bucket.defectRefs / maxValue) * 100, 12)}%`,
                }}
                title=${`${bucket.defectRefs} defect refs on ${bucket.date}`}
              />
              <span className="trend-date">${formatDate(bucket.date)}</span>
            </div>
          `,
        )}
      </div>
    </section>
  `;
}

function HotspotTable({ components, selectedComponent, onSelect }) {
  return html`
    <section className="panel">
      <div className="panel-head">
        <div>
          <p className="eyebrow">Hotspots</p>
          <h2>Component ranking</h2>
        </div>
        <p className="support-text">Click a row to focus the recent commit feed on one area.</p>
      </div>

      <div className="table-wrap">
        <table className="hotspot-table">
          <thead>
            <tr>
              <th>Component</th>
              <th>Defects</th>
              <th>Commits</th>
              <th>Authors</th>
              <th>Churn</th>
              <th>Top Files</th>
            </tr>
          </thead>
          <tbody>
            ${components.map((component) => {
              const selected = component.component === selectedComponent;
              return html`
                <tr
                  key=${component.component}
                  className=${selected ? "selected-row" : ""}
                  onClick=${() => onSelect(component.component)}
                >
                  <td>
                    <div className="component-cell">
                      <strong>${component.component}</strong>
                      <span>Score ${component.hotspotScore.toFixed(1)}</span>
                    </div>
                  </td>
                  <td>${component.uniqueDefects}</td>
                  <td>${component.commits}</td>
                  <td>${component.authors}</td>
                  <td>${component.totalChurn}</td>
                  <td>${component.topFiles.slice(0, 2).join(" · ")}</td>
                </tr>
              `;
            })}
          </tbody>
        </table>
      </div>
    </section>
  `;
}

function CommitFeed({ commits, selectedComponent }) {
  return html`
    <section className="panel">
      <div className="panel-head">
        <div>
          <p className="eyebrow">Recent Work</p>
          <h2>
            ${selectedComponent
              ? `Recent commits in ${selectedComponent}`
              : "Recent defect-linked commits"}
          </h2>
        </div>
      </div>

      <div className="commit-feed">
        ${commits.map(
          (commit) => html`
            <article className="commit-card" key=${commit.hash}>
              <div className="commit-meta">
                <${Pill} tone="cool">${truncateHash(commit.hash)}</${Pill}>
                <span>${commit.author}</span>
                <span>${formatDate(commit.authoredAt)}</span>
              </div>
              <h3>${commit.subject}</h3>
              <p>${commit.message}</p>
              <div className="pill-row">
                ${commit.defectIds.map(
                  (defectId) => html`<${Pill} key=${defectId}>${defectId}</${Pill}>`,
                )}
                ${commit.components.map(
                  (component) => html`
                    <${Pill} tone="cool" key=${component}>${component}</${Pill}>
                  `,
                )}
              </div>
            </article>
          `,
        )}
      </div>
    </section>
  `;
}

function InsightPanel({ report }) {
  return html`
    <section className="panel insight-panel">
      <div className="panel-head">
        <div>
          <p className="eyebrow">Insight Summary</p>
          <h2>Narrative overview</h2>
        </div>
        <${Pill}>${report.insightSummary.source}</${Pill}>
      </div>
      <p className="insight-narrative">${report.insightSummary.narrative}</p>
      <div className="insight-grid">
        <div>
          <h3>Highlights</h3>
          <ul className="signal-list">
            ${report.insightSummary.highlights.map(
              (highlight) => html`<li key=${highlight}>${highlight}</li>`,
            )}
          </ul>
        </div>
        <div>
          <h3>Next actions</h3>
          <ul className="signal-list">
            ${report.insightSummary.nextActions.map(
              (action) => html`<li key=${action}>${action}</li>`,
            )}
          </ul>
        </div>
      </div>
    </section>
  `;
}

export function App() {
  const [report, setReport] = useState(demoReport);
  const [repoPath, setRepoPath] = useState("");
  const [sampleMode, setSampleMode] = useState(true);
  const [historyMode, setHistoryMode] = useState("full");
  const [maxCommits, setMaxCommits] = useState("500");
  const [loading, setLoading] = useState(false);
  const [loadSource, setLoadSource] = useState("sample data");
  const [error, setError] = useState(null);
  const [notice, setNotice] = useState(null);
  const [componentSearch, setComponentSearch] = useState("");
  const [selectedComponent, setSelectedComponent] = useState(null);

  const deferredSearch = useDeferredValue(componentSearch);

  useEffect(() => {
    void refreshReport(true);
  }, []);

  async function refreshReport(initialLoad = false) {
    setLoading(true);
    setError(null);
    setNotice(null);

    try {
      const { report: nextReport, warning } = await loadReportWithFallback({
        repoPath: repoPath.trim() || undefined,
        historyMode,
        maxCommits: Number(maxCommits) || undefined,
        sample: sampleMode || (!repoPath.trim() && initialLoad),
        allowDemoFallback: sampleMode || initialLoad,
      });

      startTransition(() => {
        setReport(nextReport);
        setSelectedComponent(null);
      });

      setLoadSource(isSampleReport(nextReport) ? "sample data" : "live API");
      setNotice(warning);
    } catch (loadError) {
      setLoadSource("request failed");
      setError(loadError instanceof Error ? loadError.message : "Unknown loading error");
    } finally {
      setLoading(false);
    }
  }

  function handleSubmit(event) {
    event.preventDefault();
    void refreshReport();
  }

  const visibleComponents = report.componentInsights.filter((component) => {
    if (!deferredSearch.trim()) {
      return true;
    }
    const search = deferredSearch.toLowerCase();
    return (
      component.component.toLowerCase().includes(search) ||
      component.topFiles.some((file) => file.toLowerCase().includes(search))
    );
  });

  const visibleCommits = report.commits
    .filter((commit) =>
      selectedComponent ? commit.components.includes(selectedComponent) : true,
    )
    .slice(0, 6);

  return html`
    <div className="shell">
      <div className="ambient ambient-left"></div>
      <div className="ambient ambient-right"></div>

      <header className="hero">
        <div className="hero-copy">
          <${Pill}>Defect Intelligence Platform</${Pill}>
          <h1>Turn defect-linked Git history into action.</h1>
          <p className="hero-text">
            A small quality analytics cockpit for engineering leads: scan commit history,
            detect customer-linked markers like <code>[art123456]</code>, and see where the
            maintenance load keeps concentrating.
          </p>
          <div className="hero-meta">
            <${Pill} tone="cool">API ${API_BASE_URL}</${Pill}>
            <${Pill}>${loadSource}</${Pill}>
            <${Pill} tone="cool">${report.metadata.historyMode}</${Pill}>
          </div>
        </div>

        <form className="control-panel" onSubmit=${handleSubmit}>
          <label>
            Repository path
            <input
              placeholder="/absolute/path/to/repo"
              value=${repoPath}
              onChange=${(event) => setRepoPath(event.currentTarget.value)}
            />
          </label>
          <div className="control-row">
            <label>
              History mode
              <select
                value=${historyMode}
                onChange=${(event) => setHistoryMode(event.currentTarget.value)}
              >
                <option value="full">full</option>
                <option value="first-parent">first-parent</option>
              </select>
            </label>
            <label>
              Max commits
              <input
                value=${maxCommits}
                onChange=${(event) => setMaxCommits(event.currentTarget.value)}
              />
            </label>
          </div>
          <label className="checkbox-row">
            <input
              checked=${sampleMode}
              type="checkbox"
              onChange=${(event) => setSampleMode(event.currentTarget.checked)}
            />
            Use synthetic demo data
          </label>
          <button disabled=${loading} type="submit">
            ${loading ? "Scanning..." : "Refresh report"}
          </button>
          ${notice ? html`<p className="notice-text">${notice}</p>` : null}
          ${error ? html`<p className="error-text">${error}</p>` : null}
        </form>
      </header>

      <section className="stats-grid">
        <${StatCard}
          label="Relevant Commits"
          value=${String(report.summary.relevantCommits)}
          note=${`${report.summary.totalCommitsScanned} scanned`}
        />
        <${StatCard}
          label="Unique Defects"
          value=${String(report.summary.uniqueDefects)}
          note=${`${report.summary.components} components touched`}
        />
        <${StatCard}
          label="Coverage"
          value=${formatPercent(report.summary.coverageRatio)}
          note=${`${report.summary.authors} active authors`}
        />
        <${StatCard}
          label="Window"
          value=${`${formatDate(report.summary.periodStart)} - ${formatDate(report.summary.periodEnd)}`}
          note=${report.metadata.revision}
        />
      </section>

      <section className="panel filter-panel">
        <div className="panel-head">
          <div>
            <p className="eyebrow">Explore</p>
            <h2>Search hotspot components</h2>
          </div>
          <p className="support-text">
            Filter by component name or one of the top touched files.
          </p>
        </div>
        <input
          className="search-input"
          placeholder="scanner, dashboard, report_controller..."
          value=${componentSearch}
          onChange=${(event) => setComponentSearch(event.currentTarget.value)}
        />
      </section>

      <div className="content-grid">
        <div className="main-column">
          <${HotspotTable}
            components=${visibleComponents}
            selectedComponent=${selectedComponent}
            onSelect=${(component) =>
              setSelectedComponent((current) =>
                current === component ? null : component,
              )}
          />
          <${CommitFeed}
            commits=${visibleCommits}
            selectedComponent=${selectedComponent}
          />
        </div>

        <div className="side-column">
          <${TrendBars} report=${report} />
          <${InsightPanel} report=${report} />
        </div>
      </div>
    </div>
  `;
}
