# live enforcement pass design

Goal: ensure that MIR respects the live qualifier semantics before SSA and backend.

## Algorithm Outline
1. Build block predecessors/successors and track values marked isLive in HIR.
2. For each function:
   - Validate that any instruction that consumes a live parameter/return is preserved.
   - Insert fence markers around memory fences to prevent reordering.
   - Emit diagnostics (BOLT-E4101) when a live value is stored in a non-live field, dropped, or when live-qualified blocks are missing terminators.
3. Produce a report summarising live usage for downstream passes.

## Data Requirements
- HIR metadata describing which parameters, return values, and fields carry the `live` qualifier. Lowering should surface the flag in MIR metadata until dedicated slots exist.
- Basic block predecessor and successor relationships so the pass can walk control-flow when validating live usage.
- MIR instruction annotations indicating live operands and fence points to drive ordering decisions without relying on string parsing.
- Track live values through SSA renaming.
- Integrate with backend scheduling to preserve live ordering.
## Future Work
- Track Live values through SSA renaming.
- Integrate with backend scheduling to preserve Live ordering.
