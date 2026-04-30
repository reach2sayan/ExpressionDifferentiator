# References — Expression Tree Optimisation

## 1. DAG Conversion & Value Numbering

**Aho, Lam, Sethi, Ullman — "Compilers: Principles, Techniques and Tools" (2nd ed., 2006)**
- Chapter 8.4 — DAG representation of expressions
- Chapter 9 — Common Subexpression Elimination, Global Value Numbering
- The standard compiler textbook ("Dragon Book"). Directly covers converting a
  redundant expression tree into a DAG that shares common subexpressions (e.g.
  a variable `x` appearing four times becomes one node with four edges).

---

## 2. Hash Consing

**Filliâtre, J.-C. & Conchon, S. — "Type-Safe Modular Hash-Consing" (2006)**
- ML Workshop 2006
- Shows how to intern expression nodes at construction time so structurally
  identical subtrees map to the same physical node. Pairs naturally with DAG
  conversion to eliminate redundant storage at zero runtime cost.

---

## 3. Term Rewriting Systems

**Baader, F. & Nipkow, T. — "Term Rewriting and All That" (Cambridge University Press, 1998)**
- ISBN: 978-0521779203
- The standard reference for TRS theory. Covers rule-based simplification
  (x*1 → x, x+0 → x, algebraic identities), confluence, termination, and
  normal forms. Formal foundation for what CAS simplification engines do.

---

## 4. Equality Saturation / E-graphs

**Willsey, M. et al. — "egg: Fast and Extensible Equality Saturation" (POPL 2021)**
- DOI: 10.1145/3434304
- Introduces the `egg` library (Rust). Explores all equivalent expression forms
  simultaneously via e-graphs, then extracts the globally cheapest
  representation. Used in LLVM, JAX, and ML compilers. Readable and practical.

**de Moura, L. & Bjørner, N. — "Z3: An Efficient SMT Solver" (TACAS 2008)**
- DOI: 10.1007/978-3-540-78800-3_24
- Background on equality and uninterpreted functions, relevant to understanding
  the theory behind e-graph equivalence classes.

---

## 5. Common Subexpression Elimination

**LLVM GVN (Global Value Numbering) pass**
- Source: `llvm/lib/Transforms/Scalar/GVN.cpp`
- Practical implementation of CSE on an IR. Useful reference for applying the
  same idea to an expression tree: `x*z` appearing twice in
  `exp(x*z) + x*y*z*w` is detected and shared.

---

## Recommended Reading Order

| Step | Resource | Why |
|------|----------|-----|
| 1 | Dragon Book §8.4 | DAG construction — directly implementable, ~20 pages |
| 2 | Filliâtre hash consing | Node interning at construction time |
| 3 | egg / POPL 2021 | Full equality saturation if algebraic simplification is needed |
| 4 | Baader & Nipkow | Formal grounding for rewrite rules |

---

## Context

These references were collected while investigating the 1544-byte `sizeof` of the
symbolic expression type in `BM_Symbolic_Batched_F4`, which causes 37%+ LLC miss
rates at batch sizes ≥ 1024. The repeated variable leaves (`x`, `y`, `z`, `w`)
are the dominant source of bloat — DAG + hash consing is expected to yield a
3–4× size reduction.
