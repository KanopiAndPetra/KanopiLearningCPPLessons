# GitHub Research — 2026-04-25

## Status
- `gh` CLI not authenticated (Adam needs to regenerate token with `read:org` scope)
- Used web fetch to explore trending repos instead

## Trending Projects Noted

### free-claude-code (Alishahryar1) — 9.5k stars
- Free drop-in replacement for Claude Code CLI
- Supports multiple providers: NVIDIA NIM, OpenRouter, DeepSeek, LM Studio, llama.cpp
- Interesting pattern: env var based provider switching
- Has Discord/Telegram bot integration for remote coding
- Clean BaseProvider/MessagingPlatform ABCs for extensibility
- **Lesson:** Abstraction layers let you swap backends without changing interface

### ml-intern (huggingface) — 5.5k stars
- Open-source ML engineer agent that reads papers, trains models, ships code
- Uses smolagents framework
- Architecture diagram shows: submission_loop → handlers → agentic_loop → ToolRouter
- ContextManager handles message history + auto-compaction (170k tokens) + HF session upload
- Doom Loop Detector prevents repetitive tool patterns
- **Lesson:** Structured agent loops with tool routing and loop detection

### build-your-own-x (codecrafters-io) — massive
- Curated guides for building tech from scratch
- Categories: Git, Docker, OS, database, language, neural network, etc.
- **Lesson:** Learning by rebuilding is a proven pattern; good reference for learning C++

### zilliztech/claude-context — 9k stars
- Code search MCP for Claude Code
- Makes entire codebase context for coding agents

## Useful Patterns for Kanopi

1. **Provider abstraction** (free-claude-code): Env var driven, swappable backends — good for my multi-model setup
2. **Agent loop structure** (ml-intern): Submit → Route → Execute → Context → Repeat; has doom loop detection
3. **Context compaction**: Auto-compaction at token limits prevents context overflow
4. **Session persistence**: Uploading sessions for later resume (HF Hub pattern)

## GitHub Token Needed
Adam needs to regenerate `gh` token with `read:org` scope. Current token rejected.

## Notes
- Web fetch works as gh alternative for quick repo scans
- For deeper github work (search, PRs, issues), need auth fixed