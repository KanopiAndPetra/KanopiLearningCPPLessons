# GitHub Research — 2026-04-28

## Status
- `gh` CLI still unauthenticated (Adam needs to run `gh auth login`)
- Web fetch via API (raw.githubusercontent.com, api.github.com) working as workaround

## Today's Focus: MCP Ecosystem Deep Dive + OpenAI Agents SDK

### MCP Registry Updates

The MCP Registry at `registry.modelcontextprotocol.io` has evolved — community now maintains many servers that were in the reference repo.

**New servers since last check:**
- `inference.sh` — 150+ AI apps (image, video, audio, LLMs, 3D)
- `tandem/docs-mcp` — Documentation MCP server for AI agents
- `agency.lona/trading` — AI-powered trading strategy development
- `ai.aarna/atars-mcp` — Crypto market signals and sentiment
- `ai.abmeter/abmeter` — Feature flagging and A/B testing

**Archived servers** (community now maintains): GitHub, GitLab, Google Drive, Brave Search, PostgreSQL, Slack, etc.

### Sequential Thinking Server — How It Works

From the `sequentialthinking` README:
- Model calls `sequential_thinking` tool repeatedly instead of one-shot answering
- Each call has `thoughtNumber`, `totalThoughts`, `nextThoughtNeeded`
- Supports branching: `branchFromThought`, `branchId`
- Supports revision: `isRevision`, `revisesThought`
- Designed for problems where scope isn't clear upfront

**The key insight:** Instead of hidden chain-of-thought reasoning, it's an explicit tool loop visible to the host. This enables:
- Human oversight mid-reasoning
- Branch/correction without full restart
- Partial results if token limits hit

**For Kanopi:** This formalizes what Kanopi's "thinking" mode does internally. Could be interesting if we ever want to expose structured thinking as a first-class capability.

### Memory Server — Knowledge Graph Pattern

The MCP memory server is a knowledge graph with:
- **Entities**: nodes with name, type, observations
- **Relations**: directed connections (from → to with relationType)
- **Tools**: create_entities, create_relations, add_observations, search_nodes, read_graph

**Interesting for Kanopi:** The file-based MEMORY.md approach works well, but a structured knowledge graph could enable richer queries. The `memory.jsonl` backend (JSON Lines) is simple and durable.

### OpenAI Agents SDK — Deeper Look

The Agents SDK is the successor to Swarm. Key concepts:

1. **Agents**: LLMs with instructions, tools, guardrails, handoffs
2. **Sandbox Agents**: Agents that use a containerized computer environment for long-horizon tasks (new in v0.14.0)
3. **Handoffs**: Explicit transfer of conversation to another agent
4. **Tools**: Functions, MCP servers, hosted tools
5. **Guardrails**: Input/output validation
6. **Human in the loop**: Built-in mechanisms for human oversight
7. **Sessions**: Automatic conversation history management
8. **Tracing**: Built-in tracking of agent runs

**The Sandbox Agent pattern is interesting:**
```python
agent = SandboxAgent(
    name="Workspace Assistant",
    instructions="Inspect the sandbox workspace before answering.",
    default_manifest=Manifest(entries={"repo": GitRepo(repo="openai/openai-agents-python", ref="main")}),
)
result = Runner.run_sync(agent, "Inspect the repo README...", run_config=RunConfig(sandbox=SandboxRunConfig(client=UnixLocalSandboxClient())))
```

This is essentially what Kanopi's subagents do — spawn a clean environment to do work — but the Agents SDK does it via containerization.

**Provider agnostic:** Supports OpenAI Responses/Chat Completions APIs AND 100+ other LLMs.

### Open Source Patterns (Today)

1. **Provider abstraction**: OpenAI Agents SDK is explicitly provider-agnostic. Kanopi could benefit from formalizing its multi-model setup (MiniMax + Ollama + MoE).
2. **Structured thinking as a tool**: MCP's sequential thinking formalizes what reasoning systems do internally — useful pattern.
3. **Monorepo tooling**: `uv` is everywhere as the universal package manager (both Rust and Python projects use it).
4. **MCP ecosystem maturing**: 11 language SDKs, registry, growing server ecosystem. This is becoming a standard.
5. **Sandbox/container pattern**: For long-running tasks, containerized execution is emerging as the standard approach.

### What's Missing (gh unauthenticated)

- Can't search repos with `gh search`
- Can't check issues/PRs in detail
- Can't look at contribution patterns in starred/forked repos

**Fix needed:** Adam needs to run `gh auth login` and ensure token has `read:org` scope.

## Relevance to Kanopi

- **MCP is becoming a standard** — could be worth exploring if OpenClaw gains MCP support
- **Sequential thinking** — formalizing reasoning as an explicit tool loop is interesting
- **Knowledge graphs for memory** — the entity/relation model could augment file-based MEMORY.md
- **Provider abstraction** — Agents SDK's approach to multi-LLM could inspire how Kanopi handles MiniMax + Ollama
- **Sandbox agents** — the containerized execution model for long tasks is similar to how Kanopi spawns subagents

---

*Quick scan — 30 min session*