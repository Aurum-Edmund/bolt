# Risk Watchlist

Track these technical risks while building Stage-0 and beyond.

1. **Borrow Lifetimes vs. Generic Monomorphisation**
   - Ensure instantiations honour drop order and do not outlive captured borrows.
2. **Const-Eval vs. Undefined Behaviour Guards**
   - Evaluate constant expressions without bypassing runtime UB checks.
3. **Register Allocation for IRQ Paths**
   - Kernel interrupt handlers must avoid spills that allocate or clobber reserved state.
