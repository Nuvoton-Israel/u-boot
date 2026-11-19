# MCP Tools Reference

## Tool List

- `health() -> {status, qdrant_ok, index_stats}`
- `list_docs() -> list[str]`
- `list_ips() -> list[str]`
- `search(query: str, top_k: int = 8, filters: dict = {}) -> list`
- `fetch(chunk_id: str) -> {text, metadata}`
- `fetch_pages(pages: list[int], doc_id: str = "") -> {page:int -> markdown}`

## Search Filters

`search(..., filters={...})` applies exact-match metadata filtering (case-insensitive string equality).

Use these common filter keys from chunk metadata:

- `doc_id`
- `ip_name`
- `kind` (`normal` or `register`)
- `section_path`
- `title`
- `source_pdf`
- `page_start`
- `page_end`

## Device/doc_id Requirement

**Always include `doc_id` filter for queries.** Resolve device and silicon scope first, then discover matching documents with `list_docs()`:

- **Tavor Z1** → latest document revision applicable to Z1
- **Tavor A1** → latest document revision applicable to A1
- **Tavor, silicon unspecified** → use clear task context or ask Z1 versus A1 before searching
- **Arbel** → latest indexed Arbel/NPCM845x spec

Default policy:

- Silicon revision and document revision are separate selection criteria.
- Honor an explicit document revision or `doc_id`; otherwise prefer the latest document within the selected silicon revision.
- Verify applicability for unlabeled documents; do not guess or substitute another silicon revision when evidence is missing.
- For comparisons, run separate searches and page fetches for each resolved `doc_id`.

If device or Tavor silicon scope remains ambiguous after checking context, ask before searching:

```python
# DON'T do this (mixes devices and silicon revisions):
search("clock enable", filters={})

# DO this (constrain to the selected silicon and document revision):
search("clock enable", filters={"doc_id": "<resolved_tavor_z1_doc_id>"})
```

## Practical Filter Strategy

1. Always start with `doc_id` (resolved for the selected device and silicon revision).
2. Add `ip_name` if user specifies a module (UART, TIMER, etc.).
3. Add `kind=register` for register-specific questions.
4. If no results with full filters:
   - Drop `kind`, retry.
5. If still weak:
   - Drop `ip_name`, retry with just `doc_id`.
6. Never remove `doc_id` to broaden a hardware query. Report missing evidence; comparisons use separate scoped searches.

## Multi-Doc Behavior

- `fetch_pages` can return an error when multiple documents exist and `doc_id` is omitted.
- Always provide the selected `doc_id` for page fetches, including each side of a comparison.
- Verify that `fetch` result metadata matches the selected `doc_id` before using the evidence.

## Transport Endpoints

- Streamable HTTP: `http://127.0.0.1:8765/mcp`
- SSE: `http://127.0.0.1:8765/sse`
