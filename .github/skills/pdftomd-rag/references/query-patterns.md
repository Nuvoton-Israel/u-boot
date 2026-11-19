# Query Patterns

## Device/Document Disambiguation

Before running any search, identify the device and, for Tavor, the silicon revision.

**Tavor indicators:**
- Explicit mention: "Tavor", "NPCM9mnx", "fifth-generation BMC"
- Context clues: "newer generation", "1.5 GHz A35", "DDR5", "Caliptra 2.1"

**Arbel indicators:**
- Explicit mention: "Arbel", "NPCM845x", "fourth-generation BMC"
- Context clues: "previous generation", "1.0 GHz A35", "DDR4", "Caliptra"

**Action if ambiguous:**
```
User: "What's the clock enable sequence?"
→ You: "Are you asking about Tavor (NPCM9mnx) or Arbel (NPCM845x)?"
```

Device-family clues do not distinguish Tavor Z1 from A1. Use an explicit
silicon revision or clear current-task context; otherwise ask which one.
An explicit revision change overrides earlier context.

Once clarified, use `list_docs()` to resolve the requested document revision,
or the latest document within the selected silicon revision:

```
Filters: {"doc_id": "<resolved_tavor_z1_doc_id>"}  # Tavor Z1 selected
  or  {"doc_id": "<resolved_tavor_a1_doc_id>"}  # Tavor A1 selected
  or  {"doc_id": "<latest_arbel_doc_id>"}  # For Arbel by default
```

Do not infer silicon applicability for an unlabeled document. Verify it or ask.

### Selection Examples

| Request/context | Action |
| --- | --- |
| "Tavor Z1 UART reset" | Latest Z1 document; Z1-only search and page fetch |
| "Tavor A1 UART reset" | Latest A1 document; A1-only search and page fetch |
| "Tavor UART reset", no silicon context | Ask Z1 or A1 before searching |
| "Latest Tavor spec" or "Tavor Rev.0.75", no silicon context | Ask Z1 or A1; document revision alone is insufficient |
| Follow-up "What about UART?" in an explicitly Z1 task | Retain Z1 |
| "Now check A1" after a Z1 task | Switch to A1 |
| "Compare Z1 and A1 UART reset" | Separate searches and citations for each |
| Explicit `doc_id` conflicts with requested silicon | Ask which scope is intended |
| Requested silicon/document revision is unavailable | Report unavailable; do not substitute |

For example, the index observed on 2026-09-22 contained both
`NPCM9mnx_AS_Rev.0.75_A1` and `NPCM9mnx_AS_Rev.0.75_Z1_new`.
These illustrate the same document revision for different silicon revisions,
not permanent defaults. Rediscover IDs with `list_docs()` for each task.

## Register and Offset Questions

Use when user asks for register location, bit range, access, reset value, or address.

- Primary query pattern:
- `<REGISTER_NAME> offset <HEX_ADDRESS>`
- Example: `TIMER_CTRL offset 0x0100`

- Required filters (always include):
- `doc_id` (resolved for the selected device, silicon, and document revision)
- `ip_name` (if known)
- `kind=register`

- Example with full filters:
```
search(
  "TIMER_CTRL offset 0x0100",
  filters={"doc_id": "<resolved_doc_id>", "kind": "register"}
)
```

## Reset and Init Sequence Questions

Use when user asks for order of operations or programming steps.

- Query pattern:
- `reset sequence and clock enable for <IP>`
- Example: `reset and clock enable sequence for UART`

- Required filters (always include):
- `doc_id` (resolved for the selected device, silicon, and document revision)
- `ip_name` if known

- Example:
```
search(
  "reset and clock enable sequence for UART",
  filters={"doc_id": "<resolved_doc_id>", "ip_name": "UART"}
)
```

## Section Lookup Questions

Use when user names chapter/section, feature, or block description.

- Query pattern:
- `<section phrase> <feature phrase>`
- Example: `virtual UART configuration registers`

- Required filters (always include):
- `doc_id` (resolved for the selected device, silicon, and document revision)

- Example:
```
search(
  "virtual UART configuration registers",
  filters={"doc_id": "<resolved_doc_id>"}
)
```

## Cross-Device Comparison

Use when user explicitly asks to compare Tavor vs Arbel or between revisions.

1. Resolve Tavor Z1 or A1 before a Tavor/Arbel comparison, then search separately:
   ```
   results_tavor = search(query, filters={"doc_id": "<tavor_doc_id>"})
   results_arbel = search(query, filters={"doc_id": "<arbel_doc_id>"})
   ```
2. Fetch full chunk evidence and pages with the matching `doc_id` for each side.
3. Compare only claims backed by evidence from both devices.
4. Call out missing evidence explicitly (e.g., "No evidence found in the selected Tavor spec"). Missing evidence does not prove absence.
5. Cite both `doc_id`s in comparison results.

For cross-revision comparison within the same device family, follow the same pattern with two explicit revision `doc_id`s.
For Z1 versus A1, label each side with its silicon and document revision;
never merge their register values, reset sequences, or applicability claims.

## Query Rewrite Heuristics

If retrieval quality is low, rewrite in this order:

1. Add exact register token and hex token.
2. Replace verbose question with keyword phrase.
3. Add likely module name (`UART`, `TIMER`, `I2C`, etc.).
4. Remove stop words and punctuation noise.

## Citation Expectations

For each key claim, include:

- Device and silicon revision (Z1/A1 for Tavor), plus document revision
- `doc_id`
- `chunk_id`
- `section_path`
- `page_start-page_end`
