export interface Metadata {
  generatedAt: string;
  repository: string;
  revision: string;
  historyMode: string;
}

export interface Summary {
  totalCommitsScanned: number;
  relevantCommits: number;
  uniqueDefects: number;
  components: number;
  authors: number;
  periodStart: string;
  periodEnd: string;
  coverageRatio: number;
}

export interface FileChange {
  path: string;
  component: string;
  fileKind: string;
  added: number;
  deleted: number;
  churn: number;
}

export interface CommitRecord {
  hash: string;
  author: string;
  authoredAt: string;
  subject: string;
  message: string;
  parents: string[];
  defectIds: string[];
  issueRefs: number[];
  components: string[];
  totalChurn: number;
  files: FileChange[];
}

export interface ComponentInsight {
  component: string;
  commits: number;
  uniqueDefects: number;
  filesTouched: number;
  authors: number;
  totalChurn: number;
  activeDays: number;
  hotspotScore: number;
  topAuthors: string[];
  topFiles: string[];
}

export interface AuthorInsight {
  author: string;
  commits: number;
  uniqueDefects: number;
  components: number;
  totalChurn: number;
  topComponents: string[];
}

export interface TrendBucket {
  date: string;
  commits: number;
  defectRefs: number;
}

export interface AiSummary {
  available: boolean;
  provider: string;
  narrative: string;
  highlights: string[];
  nextActions: string[];
}

export interface Report {
  metadata: Metadata;
  summary: Summary;
  componentInsights: ComponentInsight[];
  authorInsights: AuthorInsight[];
  timeline: TrendBucket[];
  commits: CommitRecord[];
  aiSummary: AiSummary;
}

