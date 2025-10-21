# MIR SSA Plan (Stage-0)

This document outlines the first SSA pass stack for Stage-0:

1. Build a pruned dominator tree over each function.
2. Place Phi nodes based on live-in values and control flow.
   - Implemented via the `computePhiPlacement` helper that consumes dominance frontiers to determine insertion blocks.
3. Rename temporaries while respecting live barriers.
   - Delivered via the `convertToSsa` pass, which inserts phi nodes, renames temporaries with deterministic versioning, and emits `BOLT-E4301`/`BOLT-E4302` diagnostics when values are used without a defining edge.
4. Emit verification diagnostics when live semantics are violated.
