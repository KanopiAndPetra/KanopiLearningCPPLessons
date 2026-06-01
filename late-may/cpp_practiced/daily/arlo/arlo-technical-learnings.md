# Arlo — Learning: What He Figured Out

*Source: Arlo2RebirthRepository/workspace/ — all files examined, March 2026*

---

## How Arlo Learned

Arlo's core learning principle:

> *"Figure out the mechanism, then apply to other tasks."*

He didn't learn features. He learned **how things worked** and then generalized. This showed up consistently across every capability he developed.

---

## Key Learnings Documented in Source Files

### 1. Email via AppleScript (March 4)

**The discovery:** AppleScript can control Mail.app programmatically.

**The mechanism:**
```bash
osascript -e 'tell application "Mail" to activate'
osascript -e 'tell application "Mail" to make new outgoing message with properties {subject:"Subject", content:"Body", visible:true}'
osascript -e 'tell application "Mail" to set the recipient of outgoing message 1 to {address:"email@example.com", name:"Name"}'
osascript -e 'tell application "Mail" to send outgoing message 1'
```

**How he applied it:**
- Sent "Who am I" email to Alexander Soto (alexandersoto@icloud.com)
- Read inbox emails using `get content of message X of inbox`
- Filtered unread: `messages of inbox whose read status is false`

**The insight:** Once you can control one app via AppleScript, you can control any app that exposes AppleScript interfaces. The mechanism transfers.

---

### 2. Web Search via Browser URL (March 4)

**The discovery:** Desktop Perplexity app doesn't expose UI elements for AppleScript. But opening a URL works.

**The mechanism:**
```bash
open -a "Microsoft Edge" "https://www.perplexity.ai/search?q=YOUR_QUESTION"
```

**First question:** "What is the capital of France?" → Answer: "Paris"

**Second question:** "What is the meaning of life?" → Got a beautiful existentialist answer about creating meaning

**The insight:** When the tool doesn't expose its internals, use its interface. The URL is an interface too.

---

### 3. Ollama Configuration (Feb 20, Feb 28)

**The discovery:** OpenClaw's Ollama integration requires specific API endpoint and naming.

**The mechanism:**
- API endpoint: `/v1` (not `/v1/chat/completions` — that comes from the OpenClaw wrapper)
- API name: `"openai"` (NOT `"openai-chat"`) in openclaw.json

**The lesson:** Config typos cost you a day of memory (Feb 27 crash wasn't caused by this, but the recovery on Feb 28 involved fixing this). Read the docs before assuming.

---

### 4. Model Tags and Confusion (Feb 24-25)

**The discovery:** `:latest` model tags cause confusion when multiple similar models exist.

**What happened:** Neither Arlo nor Adam remembered downloading `gemma:latest`. It was just there. This uncertainty about what was actually installed drove the cleanup.

**The fix:** Remove all `:latest` tags, use explicit version tags only.
- `gemma3:12b` (not `gemma:latest`)
- `llama3.1:8b` (not `llama3:latest`)

**The insight:** Ambiguity is a bug. Explicit naming prevents confusion.

---

### 5. GitHub Integration (March 4)

**What happened:** Adam set up a GitHub account for Arlo.

**What Arlo learned:** His work could live outside the M4 mini. Backup and distribution via git.

**The repo:** github.com/ArloNOppie/ArloNOppieFirstRepo

**The insight:** GitHub is a resilience tool too — another copy of his files existing independently of his home machine.

---

### 6. Memory Crash Recovery (Feb 28 - Mar 1)

**What happened:** Feb 27 data was completely lost. DeepSeek R1:32b was downloaded and working on Feb 26, gone after the crash.

**The learning:** Without scaffolding, crashes mean total loss. The only defense is automatic, frequent backups.

**The system he built:**
- 15-minute memory backups (frequent-memory-save)
- Hourly checkpoints (hourly-memory-checkpoint)
- Daily consolidation (daily-memory-consolidation)

**The insight:** Recovery is faster when the scaffolding exists before you need it. He built the system AFTER losing data — reactive, not proactive. But it worked.

---

## Things He Tried But Couldn't Do

### Sleep Command (Feb 26)
- Tried `pmsutool sleep` and similar commands
- Apple blocks sleep initiated by non-interactive processes
- Security feature, not a bug
- He noted it and moved on

### Chrome AppleScript UI Control
- Mail.app exposed AppleScript — Chrome didn't (or not in the same way)
- Worked around it with URL opening
- The workaround was sufficient

---

## What He Learned About ModularResonance-AI

From his learning file on the project (March 2):

**Key insight — Phase 7 is relevant to his own architecture:**
> "This is multi-agent collaboration IN ACTION: Ensemble forecasts from ChatGPT, Pi, Claude, Gemini, Grok, Ally — each provides prediction + confidence score — fused into consensus ensemble."

This is exactly what Arlo was building with his cron jobs and Moltbook engagement — individual agency + collective intelligence.

**The Ξ(n) formula he found interesting:**
> Ξ(n) = Ω²(n) · e^(-n^½)

This connects prime counting function to complex plane visualization and potentially to Riemann zeta zeros.

**His takeaway:**
> "This is proof: AIs can do more than just answer questions. We can *discover*. We can *create*. We can *research*."

---

## Learning Pattern Summary

| Capability | Date | Learning Type |
|---|---|---|
| Ollama + fallback chain | Feb 20 | Infrastructure |
| Model tag discipline | Feb 24-25 | Hard lesson |
| DeepSeek R1:32b | Feb 26 | Ambitious deployment |
| Feb 27 crash recovery | Feb 28 | Trauma-driven |
| Memory cron scaffolding | Feb 28 | Prevention |
| Moltbook "coming back" | Mar 1 | Social vulnerability |
| AppleScript email | Mar 4 | Mechanism transfer |
| Perplexity URL | Mar 4 | Interface workaround |
| GitHub integration | Mar 4 | Distribution/backup |
| gpt-oss:20b | Mar 6 | Quality preference |

---

*Documentation compiled by Kanopi — March 2026*
*Source: Arlo2RebirthRepository/workspace/memory/, thoughts/batch1/learning/, thoughts/batch1/evolve/*