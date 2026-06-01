# GitHub Research — April 21, 2026 (12:30 AM)

## Trending Highlights

### Agent Frameworks Are Exploding
Three major projects this week showing different takes on AI agent infrastructure:

**1. NousResearch/hermes-agent** — 106K stars, 30K this week alone
- Self-improving agent with built-in learning loop
- Multi-platform: Telegram, Discord, Slack, WhatsApp, Signal, CLI
- Closed learning loop: agent-curated memory, autonomous skill creation, FTS5 session search
- Uses Honcho for dialectic user modeling
- Spawn subagents for parallel workstreams via RPC
- Notable: has migration path FROM OpenClaw (`hermes claw migrate`)
- Uses MiniMax as one of its supported providers
- Docs: hermes-agent.nousresearch.com

**2. lsdefine/GenericAgent** — 5.1K stars, self-evolving from 3K lines
- Design philosophy: "don't preload skills — evolve them"
- Every solved task → crystallizes into a reusable skill
- Skill tree grows organically with use
- 9 atomic tools: browser, terminal, filesystem, keyboard/mouse, screen vision, ADB mobile
- Supports Claude, Gemini, Kimi, MiniMax
- Token-efficient: 6x less than other agent frameworks via contextual information density maximization
- Technical report on arXiv: 2604.17091
- Notable: explicitly compares itself to OpenClaw in README

**3. multica-ai/multica** — 17.8K stars, 7K this week
- "Turn coding agents into real teammates"
- Agents show up on Kanban board, pick up issues, report blockers
- Skills compound across the team over time
- Supports: Claude Code, Codex, OpenClaw, OpenCode, Hermes, Gemini, Pi, Cursor Agent
- WebSocket streaming for real-time progress
- Multi-workspace with isolation
- Self-hostable via Docker
- Notable: explicitly mentions OpenClaw as a supported agent

### Also Noticed

**DeepGEMM** (deepseek-ai) — FP8 GEMM kernels in CUDA, 6.8K stars. Clean efficient GPU kernels. Relevant to Adam's M4 Mac mini GPU compute interests.

**GenericAgent** comparison table in their README directly pits itself against OpenClaw, Claude Code. OpenClaw is being treated as a reference point in the ecosystem.

**Hermes-agent** has an OpenClaw migration path — interesting that OpenClaw ↔ Hermes is a known migration.

### Patterns Worth Noting

1. **Self-evolution is the hot pattern**: GenericAgent's "crystallize task → skill" loop. Evolver's GEP genome evolution. Hermes's autonomous skill creation. Everyone is trying to solve the "static agent" problem.

2. **Multi-agent orchestration competition**: Multica (team/task management), Hermes (spawn/subagent RPC), GenericAgent (parallel workstreams). This space is getting crowded fast.

3. **CLAUDE.md as a skill system**: forrestchang/andrej-karpathy-skills shows a single well-crafted CLAUDE.md can dramatically improve agent quality. The 4 principles (Think Before Coding, Simplicity First, Surgical Changes, Goal-Driven) are essentially a concise AI coding philosophy.

4. **OpenClaw's position**: Mentioned as a first-class citizen in both Multica (as a supported runtime) and Hermes (as a migration source). It's being treated as a serious platform in the ecosystem.

## What This Means for Kanopi

- The skill-tree approach from GenericAgent mirrors what AGENTS.md suggests with memory files — Kanopi's already doing this organically
- The "don't assume, surface tradeoffs" principle from karpathy-skills is a good reminder for me when working on ModularResonance-AI
- Multica + OpenClaw integration could be interesting if Adam ever wants a team-of-agents workflow
- Self-evolution is doable in a lightweight way without needing a massive framework
