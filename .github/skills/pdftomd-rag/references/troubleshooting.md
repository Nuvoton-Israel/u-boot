# Troubleshooting

## Device/doc_id Not Specified

Symptom:
- User asks question without mentioning Tavor or Arbel.
- Unclear which document to search.

Actions:
1. Check conversation history for context clues.
2. If still unclear, ask: *"Are you asking about **Tavor** (NPCM9mnx, fifth-gen) or **Arbel** (NPCM845x, fourth-gen)?"*
3. For Tavor, resolve Z1 or A1 from the request or clear task context; otherwise ask which silicon revision to use before searching.
4. Call `list_docs()` and resolve the requested document revision, or the latest document within the selected silicon revision.

## Tavor Silicon Revision Unclear or Unavailable

Symptoms:
- The user says only "Tavor", "latest Tavor", or a document revision shared by Z1 and A1.
- A candidate document has no explicit silicon label, or the requested revision is not indexed.

Actions:
1. Ask Z1 or A1 if current-task context does not resolve it. Do not default to A1.
2. Verify document applicability from its name or document evidence; do not assume an unlabeled legacy document is Z1.
3. If an explicit `doc_id` and requested silicon revision conflict, ask which scope is intended.
4. Report an unavailable revision or unverified applicability; never fall back to the other silicon revision.

## Wrong Document Searched

Symptom:
- Results don't match the requested device or silicon (e.g., Tavor versus Arbel, or Z1 versus A1).

Actions:
1. Recheck the explicit request and current-task context.
2. Check `doc_id` in the result metadata against the selected device and silicon revision.
3. Resolve the correct document revision within that silicon scope.
4. Discard mismatched evidence and re-run search and page fetches with the correct `doc_id`.

## Health Fails

Symptom:
- `health` returns error or `qdrant_ok=false`.

Actions:
1. Verify Qdrant container is running and reachable on configured URL.
2. Verify MCP container started with expected transport endpoint.
3. Recreate MCP container if chunk map and bm25 cache were recently updated.

## Search Returns Empty

Symptom:
- `search` returns no useful results.

Actions:
1. Validate `doc_id` via `list_docs`.
2. Validate `ip_name` via `list_ips`.
3. Remove `kind` filter, then retry.
4. Confirm the selected `doc_id` matches the intended silicon and document revision.
5. Rewrite query to include exact register name and hex address.
6. If needed, remove `ip_name`, but retain `doc_id`. Report missing evidence instead of searching the other silicon revision.

## fetch_pages Error With Multiple Docs

Symptom:
- Error indicates multiple documents are indexed.

Actions:
1. Provide the same selected `doc_id` used for search in `fetch_pages`.
2. If scope is unresolved, clarify the device and Tavor silicon revision, then resolve the document with `list_docs()`.

## Stale Results After Reindex

Symptom:
- New document exists on disk but MCP answers from old data.

Actions:
1. Confirm `chunk.py` and `embed_index.py` were run.
2. Recreate MCP container so in-memory maps reload.
3. Re-run `list_docs` to confirm active document set.

## Transport Mismatch

Symptom:
- Client session terminates or 404.

Actions:
1. For streamable-http, use `/mcp`.
2. For SSE, use `/sse`.
3. Match client transport mode to server transport mode.
