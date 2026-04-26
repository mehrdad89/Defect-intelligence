import { React, createRoot, html } from "./react-cdn.js";
import { App } from "./app.js";

createRoot(document.getElementById("root")).render(
  html`<${React.StrictMode}><${App} /></${React.StrictMode}>`,
);

