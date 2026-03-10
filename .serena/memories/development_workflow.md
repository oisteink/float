# Development Workflow

4-stage gated process. Each stage requires explicit user approval. Artifacts live in `iteration/`.

1. **Spec** → `iteration/spec.md` — requirements, constraints, acceptance criteria
2. **Research** → `iteration/research.md` — APIs, hardware, patterns, pitfalls
3. **Plan** → `iteration/plan.md` — file changes, architecture, tests
4. **Implementation** — execute the plan

Rules:
- Never skip a stage or start next without approval
- Keep stage documents concise
- Each document references previous ones
- Completed iterations archived in `iteration/history/`
