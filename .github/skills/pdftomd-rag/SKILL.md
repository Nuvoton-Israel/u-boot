---
name: pdftomd-rag
description: Query hardware specification documents indexed in pdftomd RAG through MCP tools. Use for register offsets, bit fields, reset or init sequences, IP configuration, section lookups, or page-cited answers. Resolve Tavor Z1 versus A1 before searching, filter by doc_id, then verify with fetch and fetch_pages.
---

# pdftomd RAG Skill

## Device Mapping

Map user device names to document families before searching, then resolve to a `doc_id` with `list_docs()`:

- **Tavor Z1** → latest indexed Tavor/NPCM9mnx specification explicitly applicable to Z1
- **Tavor A1** → latest indexed Tavor/NPCM9mnx specification explicitly applicable to A1
- **Tavor / NPCM9mnx without a silicon revision** → resolve Z1 or A1 from the request or clear task context; otherwise ask before searching
- **Arbel** → latest indexed Arbel/NPCM845x specification by default

Rules:

- **Silicon revision** (Z1/A1) and **document revision** (for example, Rev.0.75) are different. Never choose A1 over Z1 merely because it appears newer; both can have the same document revision.
- Prefer the latest **document revision within the selected silicon revision**, unless the user requests a particular document revision or `doc_id`.
- Do not hardcode the active document list; discover it with `list_docs()`.
- Confirm silicon applicability from the document name or verified document evidence. Do not assign an unlabeled legacy spec to Z1 or A1 by guessing.
- If applicability or document ordering is ambiguous, ask rather than sorting IDs lexically or treating a suffix such as `new` as proof.
- If the requested silicon/document revision is unavailable, report it; do not silently substitute another revision.

## Clarification Workflow

1. Use the explicit device, silicon revision, or `doc_id` in the current request. An explicit revision change overrides earlier conversation context.
2. Fill missing scope from clear context for the current task. A mention of another revision in a comparison or background material is not a selection.
3. If the device is still ambiguous, ask: *"Are you asking about **Tavor** (NPCM9mnx) or **Arbel** (NPCM845x)?"*
4. For Tavor, if the silicon revision is still ambiguous, ask: *"Should I use the **Tavor Z1** or **Tavor A1** specification?"* Do not default to A1, search both, or use a generic "latest Tavor" spec.
5. Use `list_docs()` to resolve the selected scope. A request for "Tavor Rev.0.75" alone does not distinguish Z1 from A1. If an explicit `doc_id` conflicts with the requested silicon revision, ask which scope is intended.
6. For an explicit Z1/A1 comparison, resolve both documents and keep searches, evidence, and claims separate.

## Core Workflow

1. Call `health` first.
2. **Clarify device and Tavor silicon revision** using the workflow above.
3. Call `list_docs()` to discover available documents.
4. Resolve a `doc_id` for the selected device, silicon revision, and document revision.
5. Run `search(query, top_k, filters={"doc_id": "...", ...})` with `doc_id` filter always set.
6. Verify evidence with `fetch(chunk_id)` and `fetch_pages(..., doc_id="<selected_doc_id>")` before final claims. Check that fetched chunk metadata matches the selected document.
7. State the selected device, silicon revision for Tavor, and document revision; cite `doc_id`, `section_path`, `page_start-page_end`, and `chunk_id`.

## When To Load References

- Read `references/mcp-tools.md` for exact tool contracts, metadata fields, and filter behavior.
- Read `references/query-patterns.md` when search quality is weak or question type is ambiguous.
- Read `references/troubleshooting.md` when tools fail or return empty results.
- Read `references/validation-checklist.md` before finalizing high-stakes answers.

## Output Rules

- Do not invent register values, offsets, or reset sequences.
- If evidence is partial or conflicting, state uncertainty clearly.
- Use the latest document revision only within the selected silicon scope.
- Never fill missing Z1 evidence from A1, or vice versa. Report the gap.
- For comparisons, use separate `doc_id`-filtered searches and page fetches; label each side and its citations. Missing evidence on one side does not prove the behavior is absent.
