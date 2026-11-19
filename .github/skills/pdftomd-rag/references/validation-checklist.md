# Validation Checklist

Use this checklist before sending final answers from RAG.

## Device Scope

- User specified device or it's clear from context (Tavor or Arbel).
- Tavor silicon revision (Z1/A1) is explicit or clear from current-task context; otherwise the user was asked before searching.
- An explicit current-request revision overrides earlier context; conflicting silicon and `doc_id` selections were clarified.
- Correct `doc_id` filter was applied:
  - Tavor Z1 → requested document revision, or latest verified Z1 document
  - Tavor A1 → requested document revision, or latest verified A1 document
  - Arbel → latest indexed Arbel/NPCM845x spec by default
- Silicon revision was not inferred from document revision, lexical ordering, or an unlabeled legacy spec.
- Explicit document revisions or IDs were honored; unavailable revisions were reported rather than substituted.
- Comparison searches and page fetches were separate, with each side labeled and citing its own `doc_id`.

## Evidence Quality

- `health` reports MCP and Qdrant are healthy.
- The answer is based on retrieved chunks, not assumptions.
- At least one `fetch` call was made for each major claim.
- Supporting pages were verified with `fetch_pages` using the selected `doc_id`.
- Missing Z1 evidence was not filled from A1, or vice versa.

## Scope Quality

- **Device, silicon, and document scope are clear** in the response (including Z1/A1 for Tavor).
- `ip_name` scope is clear for peripheral-specific questions.
- Optional filters were relaxed progressively if needed, but `doc_id` was retained.
- All results have matching `doc_id` in metadata.

## Citation Quality

Every key claim includes:

- `doc_id`
- `chunk_id`
- `section_path`
- `page_start-page_end`

## Ambiguity Handling

- Conflicting snippets are called out explicitly.
- Missing evidence is stated clearly instead of inferred.
- Follow-up question is asked only when ambiguity can change correctness.
