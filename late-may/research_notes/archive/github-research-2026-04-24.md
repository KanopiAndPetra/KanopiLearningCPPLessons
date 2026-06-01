# GitHub Research — 2026-04-24

## Quick Scan Summary

### Notable Projects Found

**Ollama** (169k stars, Go)
- Local LLM runner — Adam already uses this with DeepSeek R1:32b
- Supports MiniMax (just upgraded to M2.7)
- Maintainers: mxyng (1289), jmorganca (904), dhiltgen (845)
- 3051 open issues — very active
- Integrates with OpenClaw, Claude Code, Codex

**FlowiseAI/Flowise** (52k stars, TypeScript)
- Visual AI agent builder — drag/drop LLM workflows
- 24k forks — heavily cloned
- Good reference for building no-code AI tools

**vercel/ai** (23k stars, TypeScript)
- Vercel's AI SDK — streaming, tool calls, agents
- 1555 open issues — active development
- Good patterns for AI integration in apps

**arxanas/git-branchless** (4k stars, Rust)
- Suite of Git tools — smartlog, undo, restack
- 97 open issues, 1 contributor dominates (arxanas with 1332 commits)
- Architecture doc is solid
- Good example: badges in README, clear demos, wiki for docs

### Patterns Observed

**What makes READMEs effective:**
- Logo + one-liner at top
- Quick start in <10 lines
- Badges for CI, Discord, install methods
- Demo screenshots/videos (git-branchless uses asciinema)
- Clear table of contents for larger docs
- "Get started" immediately visible

**Contribution models:**
- Small teams dominate: Ollama ~10 active contributors, git-branchless mostly 1 person
- dependabot handles dependency updates automatically
- Issues tagged clearly (feat, chore, fix, etc.)

**Architecture insights:**
- Go for systems (Ollama) — fast, single binary deploys
- Rust for performance-critical Git operations
- TypeScript for AI/JS ecosystem integration

### What This Means for Kanopi

1. **Documentation patterns**: Use badges, demos, and clear install steps in any project README
2. **Open source etiquette**: dependabot for deps, clear issue labels, wiki for deep docs
3. **Tooling integration**: Ollama already works with OpenClaw — could explore deeper integration
4. **Community presence**: Discord + GitHub Discussions is the standard for OSS projects

### Trending Topics
- AI Agents (Flowise, vercel/ai)
- Local LLM tooling (Ollama)
- Git workflow enhancements (git-branchless)

---
*Quick scan — 15 min session, not a deep dive*