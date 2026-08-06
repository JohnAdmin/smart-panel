# Stock Ticker Runbook

This runbook moved into a Claude Code skill so it loads only when a stock-ticker
question actually comes up, instead of costing context on every session.

- **Diagnosis and verification** — [`.claude/skills/stock-ticker/SKILL.md`](.claude/skills/stock-ticker/SKILL.md)

  Symptom-ordered checks (`N/A` symbols, missing bar, stale layout after save),
  post-flash and post-save verification, where the feature lives in `src/`.

- **Fix log, v1–v5** — [`.claude/skills/stock-ticker/references/version-history.md`](.claude/skills/stock-ticker/references/version-history.md)

  Problem / fix / result / key files for each change. **Append a new entry for
  every stock-related fix.**

- **First-time setup** — [`docs/stock-ticker.md`](docs/stock-ticker.md)

  Getting a Twelve Data API key and configuring symbols in the web portal.

Both files are plain Markdown and readable on their own. In Claude Code the
skill triggers on its own; you can also invoke it with `/stock-ticker`.
