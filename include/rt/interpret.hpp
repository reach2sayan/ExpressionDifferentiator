#pragma once

#include "rt/apply.hpp"
#include "rt/builder.hpp"

#include <span>
#include <vector>

namespace ddx::rt {

// Evaluate every node once, in id order.  Ids are assigned as subexpressions
// are formed, so a child always precedes its parent and one forward pass
// suffices; shared subexpressions are computed once because they are one node.
// This is the reference the JIT is checked against.
[[nodiscard]] inline std::vector<double>
evaluate_all(const Builder &b, std::span<const double> point) {
  std::vector<double> v(b.size());
  for (NodeId i = 0; i < b.size(); ++i) {
    const Node &n = b[i];
    switch (arity_of(n.op)) {
    case 0:
      v[i] = n.op == OpCode::Const ? n.value : point[n.slot];
      break;
    case 1:
      v[i] = apply1(n.op, v[n.a]);
      break;
    default:
      v[i] = apply2(n.op, v[n.a], v[n.b]);
      break;
    }
  }
  return v;
}

[[nodiscard]] inline double evaluate(const Builder &b, NodeId root,
                                     std::span<const double> point) {
  return evaluate_all(b, point)[root];
}

} // namespace ddx::rt
