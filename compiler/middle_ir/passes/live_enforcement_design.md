# Live Enforcement Pass Design

Goal: ensure that MIR respects the Live qualifier semantics before SSA and backend.

## Algorithm Outline
1. Build block predecessors/successors and track values marked isLive in HIR.
2. For each function:
   - Validate that any instruction that consumes a Live parameter/return is preserved.
   - Insert fence markers around memory fences to prevent reordering.
   - Emit diagnostics (BOLT-E4101) when a Live value is stored in a non-Live field or dropped.
3. Produce a report summarising Live usage for downstream passes.

## Data Requirements
- HIR metadata (isLive, eturnIsLive).
- MIR instruction metadata (to be extended) indicating Live usage and fence points.

## Future Work
- Track Live values through SSA renaming.
- Integrate with backend scheduling to preserve Live ordering.
