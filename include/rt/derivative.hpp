#pragma once

#include "rt/expr.hpp"

#include <utility>
#include <vector>

namespace ddx::rt {

namespace detail {

// d/du of a one-argument op.  The eighteen transcendentals come from the
// descriptors in expr/unary_math.hpp instantiated at Expr: the rule bodies are
// written against Numeric, so at T = Expr they build nodes instead of
// computing.  There is no second copy of the chain rule.
// A template, so `if constexpr` actually discards: several descriptors define
// deriv_from_value, which reuses the primal node instead of recomputing it --
// d(exp)/du becomes the exp node itself rather than a second one.
template <typename Fn>
[[nodiscard]] inline Expr rule(const Expr &u, const Expr &fu) {
  if constexpr (impl::detail::has_deriv_from_value_v<Fn, Expr>) {
    return Fn::deriv_from_value(u, fu);
  } else {
    return Fn::deriv(u);
  }
}

[[nodiscard]] inline Expr partial(OpCode op, const Expr &u, const Expr &fu) {
  switch (op) {
  case OpCode::Neg:
    return Expr{-1};
  // `abs` is absent from the table because its derivative is a sign, not a
  // function of the primal; u/|u| is that sign in the ops we have.
  case OpCode::Abs:
    return u / fu;
#define DDX_RT_PARTIAL(fn, Op, label)                                          \
  case OpCode::Op:                                                             \
    return rule<impl::detail::Op##Fn<Expr>>(u, fu);
    DDX_UNARY_MATH_TABLE(DDX_RT_PARTIAL)
#undef DDX_RT_PARTIAL
  default:
    return Expr{0};
  }
}

// (d/dl, d/dr) of a two-argument op, given the node for the result.
[[nodiscard]] inline std::pair<Expr, Expr>
partials(OpCode op, const Expr &l, const Expr &r, const Expr &f) {
  switch (op) {
  case OpCode::Add:
    return {Expr{1}, Expr{1}};
  case OpCode::Mul:
    return {r, l};
  case OpCode::Div:
    return {Expr{1} / r, -f / r};
  case OpCode::Pow:
    return {r * pow(l, r - Expr{1}), f * log(l)};
  case OpCode::Atan2: {
    const Expr d = l * l + r * r;
    return {r / d, -l / d};
  }
  case OpCode::Hypot:
    return {l / f, r / f};
  // max/min are (l + r +/- |l - r|)/2, so the partial is the step that picks
  // the winner, written with the ops we have rather than a comparison.
  case OpCode::Max: {
    const Expr s = (l - r) / abs(l - r);
    return {(Expr{1} + s) / Expr{2}, (Expr{1} - s) / Expr{2}};
  }
  case OpCode::Min: {
    const Expr s = (l - r) / abs(l - r);
    return {(Expr{1} - s) / Expr{2}, (Expr{1} + s) / Expr{2}};
  }
  default:
    return {Expr{0}, Expr{0}};
  }
}

} // namespace detail

struct Gradient {
  NodeId value = no_node;      // the node for f itself
  std::vector<NodeId> partial; // one per symbol, in Builder::symbols() order
};

// One reverse sweep over the whole graph, pushing adjoints from each node to
// its children -- the structural analogue of reverse_sweep in
// drivers/symbolic.hpp, accumulating *nodes* rather than values.  The result
// shares every subexpression it can, because the builder interns.
//
// The sweep appends to the same builder it reads: new nodes land above the
// snapshot, so reverse id order stays a topological order of what came before.
[[nodiscard]] inline Gradient gradient(Builder &b, NodeId root) {
  const auto n = static_cast<NodeId>(b.size());
  std::vector<NodeId> adj(n, no_node);
  adj[root] = b.constant(1.0);

  const auto add_to = [&](NodeId child, const Expr &contribution) {
    const NodeId c = contribution.id(b);
    adj[child] =
        adj[child] == no_node ? c : (Expr{b, adj[child]} + Expr{b, c}).id(b);
  };

  for (NodeId v = n; v-- > 0;) {
    if (adj[v] == no_node) {
      continue;
    }
    const Node node = b[v]; // by value: building below may reallocate
    if (is_leaf(node.op)) {
      continue;
    }
    const Expr a{b, adj[v]};
    const Expr self{b, v};
    if (arity_of(node.op) == 1) {
      const Expr u{b, node.a};
      add_to(node.a, a * detail::partial(node.op, u, self));
    } else {
      const Expr l{b, node.a};
      const Expr r{b, node.b};
      const auto [dl, dr] = detail::partials(node.op, l, r, self);
      add_to(node.a, a * dl);
      add_to(node.b, a * dr);
    }
  }

  Gradient g{.value = root, .partial = {}};
  g.partial.reserve(b.symbols().size());
  for (std::uint32_t s = 0; s < b.symbols().size(); ++s) {
    NodeId found = no_node;
    for (NodeId v = 0; v < n; ++v) {
      if (b[v].op == OpCode::Var && b[v].slot == s) {
        found = adj[v];
        break;
      }
    }
    g.partial.push_back(found == no_node ? b.constant(0.0) : found);
  }
  return g;
}

} // namespace ddx::rt
