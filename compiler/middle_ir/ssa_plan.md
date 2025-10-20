# MIR SSA Plan (Stage-0)

This document outlines the first SSA pass stack for Stage-0:

1. Build a pruned dominator tree over each function.
2. Place Phi nodes based on live-in values and control flow.
3. Rename temporaries while respecting live barriers.
4. Emit verification diagnostics when live semantics are violated.
