# GitHub Research — 2026-04-27

## Status
- `gh` CLI still unauthenticated (Adam needs to fix)
- Web fetch + raw content URLs working as workaround

## Today's Focus: MCP (Model Context Protocol)

### MCP Ecosystem Overview

MCP is becoming a standard for connecting AI agents to external tools/data. GitHub has an MCP Registry at `registry.modelcontextprotocol.io`.

**Key repos:**
- `modelcontextprotocol/modelcontextprotocol` — spec + docs
- `modelcontextprotocol/servers` — reference server implementations (11 language SDKs: C#, Go, Java, Kotlin, PHP, Python, Ruby, Rust, Swift, TypeScript)

**Reference servers available:**
- `everything` — test/demo with prompts, resources, tools
- `fetch` — web content fetching
- `filesystem` — secure file ops with configurable access
- `git` — Git repo read/search/manipulate
- `memory` — knowledge graph persistent memory
- `sequentialthinking` — step-by-step reflective problem solving
- `time` — timezone conversion

**Archived (community now maintains):** GitHub, GitLab, Google Drive, Brave Search, PostgreSQL, etc.

### Sequential Thinking Server — Interesting Pattern

This MCP server enables structured thinking in agents. Key idea:
- Model calls `sequential_thinking` tool multiple times rather than one-shot answering
- Each call has `thoughtNumber`, `totalThoughts`, `nextThoughtNeeded`
- Supports branching and revision (branchFromThought, isRevision, revisesThought)
- Designed for problems where scope isn't clear upfront

**What it models:** A structured reasoning loop visible to the host. Instead of hidden chain-of-thought, it's an explicit tool call loop.

**Relevance to Kanopi:** This is essentially what Kanopi's "thinking" mode does internally — but MCP formalizes it as a proper tool. Interesting if we ever want to expose structured thinking as a first-class capability.

### OpenAI Swarm → Agents SDK Migration

Swarm (educational multi-agent orchestration) is now officially replaced by **OpenAI Agents SDK** (`openai/openai-agents-python`).

Key Swarm concepts that survived:
- `Agent` with `instructions` + `tools`
- **Handoffs** — agent can transfer conversation to another agent
- Stateless between calls (like Kanopi's session context files)
- Lightweight, client-side execution

Key differences:
- Agents SDK is production-ready, not just educational
- Actively maintained vs. Swarm being archived

**Handoff pattern is interesting for Kanopi:** The idea that one agent can explicitly transfer control to another with full context is elegant. Kanopi's subagent spawning is similar but heavier-weight.

## Patterns for Kanopi's Work

### 1. Provider Abstraction
Open source projects like `free-claude-code` use env-var-driven provider switching. Kanopi could benefit from a more formal provider abstraction for its multi-model setup (MiniMax + Ollama + MoE router).

### 2. MCP as Tool Standard
MCP is maturing fast — 11 language SDKs, registry, reference servers. If OpenClaw doesn't already support MCP natively, it might be worth exploring. The sequential thinking server alone is a useful tool.

### 3. Sequential/Structured Thinking as Tool
Rather than hiding reasoning in context, MCP's approach makes it an explicit loop. This could enable:
- Human oversight mid-reasoning
- Branching/correction without full restart
- Partial results if token limits hit

### 4. Stateless + Persistence Layer
Swarm's stateless-between-calls model + Agents SDK mirrors Kanopi's approach: fresh wake each session, but files provide continuity. The "memory" MCP server (knowledge graph) might be interesting to study.

## What's Missing (gh unauthenticated)
- Can't search repos
- Can't check issues/PRs
- Can't look at contribution patterns in detail

**Fix needed:** Adam needs to run `gh auth login` and ensure token has `read:org` scope.

## Notes
- Web fetch with raw GitHub URLs works well for README content
- GitHub trending page loads but shows nav only (JavaScript-rendered)
- MCP Registry: `registry.modelcontextprotocol.io` — could browse for interesting servers

---
*Quick scan — 30 min session*
