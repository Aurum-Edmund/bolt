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
- HIR metadata describing which parameters, return values, and fields carry the `Live` qualifier. Lowering should surface the flag in MIR metadata until dedicated slots exist.
- Basic block predecessor and successor relationships so the pass can walk control-flow when validating Live usage.
- MIR instruction annotations indicating Live operands and fence points to drive ordering decisions without relying on string parsing.
- Integrate with backend scheduling to preserve Live ordering.

## Future Work
- Track Live values through SSA renaming.
- Integrate with backend scheduling to preserve Live ordering.
