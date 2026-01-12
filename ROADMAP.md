# DIY-AI Roadmap

## Vision
Build a lightweight, WordNet‑powered assistant that can interpret software
development prompts, maintain conversational context, and generate structured
signals for downstream codegen without heavy hardcoded English rules.

## Current Capabilities
- WordNet‑driven meaning sketch with hypernym expansion.
- JSON mode for codegen pipelines (actions/entities/constraints/top terms).
- REPL chatbot with SDLC and design‑concept focus.
- Context memory with confidence scores for language/platform/framework.
- Memory summary and reflection loop (`/summary`, `/reflect`).

## Near Term (0.1 → 0.3)
- Stabilize build ergonomics
  - Remove `DEFAULTPATH` redefinition warning.
  - Add `wn-chat` to `diy-ai` standalone build instructions.
- Improve context retention
  - Confidence decay + reinforcement (time‑weighted evidence).
  - Explicit conflict detection (e.g., Java vs. Python).
- Better intent parsing
  - Promote actions based on verb hypernyms (task‑oriented verbs).
  - Track user constraints (OS, UI, performance, security).
- Output tooling
  - `--json` mode for `wn-chat` (state snapshot per turn).
  - `--quiet` mode to suppress per‑turn reflections.

## Mid Term (0.4 → 0.6)
- SDLC planning
  - Auto‑generate a minimal project plan (requirements → design → build → test).
  - Structured “next steps” based on current intent and missing info.
- Design inference
  - Component extraction (UI, storage, networking, domain logic).
  - Heuristic architecture suggestions (layered, client/server, pipeline).
- Codegen handoff
  - Emit codegen‑ready schema: `{tasks, components, io, tests, risks}`.
  - Configurable templates for different languages (no framework assumptions).

## Long Term (0.7+)
- Adaptive memory
  - Session profiles with persistent user preferences.
  - Cross‑turn summarization with confidence and provenance.
- Interactive refinement
  - Question ranking by information gain.
  - “What changed?” diff between user intents.
- Extensibility
  - Plugin hooks for other lexical resources or domain vocabularies.
  - Lightweight embeddings fallback (optional, offline).

## Non‑Goals
- Full natural language understanding.
- Large external ML dependencies in core builds.

## Metrics to Track
- Intent stability across turns (confidence drift).
- Clarification success rate (fewer repeated questions).
- Precision of extracted language/platform/framework.
