#pragma once

#include "rt/expr.hpp"

#include <algorithm>
#include <ranges>
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
[[nodiscard]] inline constexpr Expr rule(const Expr &u, const Expr &fu) {
  if constexpr (impl::detail::has_deriv_from_value_v<Fn, Expr>) {
    return Fn::deriv_from_value(u, fu);
  } else {
    return Fn::deriv(u);
  }
}

// d/du of a one-argument op.  The eighteen transcendentals come from the
// descriptors in expr/unary_math.hpp instantiated at Expr: the rule bodies are
// written against Numeric, so at T = Expr they build nodes instead of
// computing.  `neg` and `abs` come from rt/opcode.hpp's table, which carries
// their partials for the same reason -- there is no second copy of the chain
// rule anywhere.
[[nodiscard]] inline constexpr Expr partial(OpCode op, const Expr &u,
                                            const Expr &f) {
  using T = Expr;
  switch (op) {
#define DDX_RT_PARTIAL(fn, Op, label, functor, dU)                             \
  case OpCode::Op:                                                             \
    return dU;
    DDX_RT_UNARY_TABLE(DDX_RT_PARTIAL)
#undef DDX_RT_PARTIAL
#define DDX_RT_PARTIAL(fn, Op, label)                                          \
  case OpCode::Op:                                                             \
    return rule<impl::detail::Op##Fn<Expr>>(u, f);
    DDX_UNARY_MATH_TABLE(DDX_RT_PARTIAL)
#undef DDX_RT_PARTIAL
  default:
    return Expr{0};
  }
}

// (d/dl, d/dr) of a two-argument op, given the node for the result.
[[nodiscard]] inline constexpr std::pair<Expr, Expr>
partials(OpCode op, const Expr &l, const Expr &r, const Expr &f) {
  using T = Expr;
  switch (op) {
#define DDX_RT_PARTIALS(fn, Op, label, functor, dL, dR)                        \
  case OpCode::Op:                                                             \
    return {dL, dR};
    DDX_RT_BINARY_TABLE(DDX_RT_PARTIALS)
#undef DDX_RT_PARTIALS
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
[[nodiscard]] inline constexpr Gradient gradient(Builder &b, NodeId root) {
  const auto n = static_cast<NodeId>(b.size());
  std::vector<NodeId> adj(n, no_node);
  adj[root] = b.constant(1.0);

  const auto add_to = [&](NodeId child, const Expr &contribution) {
    const NodeId c = contribution.id(b);
    adj[child] =
        adj[child] == no_node ? c : (Expr{b, adj[child]} + Expr{b, c}).id(b);
  };

  // Explicitly indexed, not a filtered view: the body writes adjoints into
  // entries this traversal has not reached yet, and a lazy filter would be
  // reading state the loop is still changing.
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

  // One pass to collect the leaves, rather than re-scanning the graph per
  // symbol.
  Gradient g{.value = root,
             .partial = std::vector<NodeId>(b.symbols().size(), no_node)};
  for (const auto [v, node] : std::views::enumerate(b.nodes().first(n)) |
                                  std::views::filter([](const auto &entry) {
                                    return std::get<1>(entry).op == OpCode::Var;
                                  })) {
    g.partial[node.slot] = adj[v];
  }
  // A symbol the expression never mentions has no leaf, so no adjoint reached
  // it; those partials are the literal zero.
  std::ranges::replace(g.partial, no_node, b.constant(0.0));
  return g;
}

} // namespace ddx::rt
