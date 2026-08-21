#pragma once

#include "rt/apply.hpp"
#include "rt/opcode.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ddx::rt {

using NodeId = std::uint32_t;
inline constexpr NodeId no_node = ~NodeId{0};

struct Node {
  OpCode op = OpCode::Const;
  NodeId a = no_node;
  NodeId b = no_node;
  double value = 0.0;     // Const
  std::uint32_t slot = 0; // Var
};

// The mutable half of the graph: nodes are interned as they are formed, so an
// id *is* the identity of a subexpression and a repeat costs a hash lookup.
// Everything here mirrors what the operator factories in expr/values.hpp do at
// compile time, one level cheaper -- structural identity is a uint32 compare
// rather than a type comparison.
class Builder {
public:
  [[nodiscard]] NodeId constant(double v) {
    return intern({.op = OpCode::Const, .value = v});
  }

  [[nodiscard]] NodeId variable(std::string_view name) {
    for (std::uint32_t i = 0; i < symbols_.size(); ++i) {
      if (symbols_[i] == name) {
        return intern({.op = OpCode::Var, .slot = i});
      }
    }
    symbols_.emplace_back(name);
    return intern({.op = OpCode::Var,
                   .slot = static_cast<std::uint32_t>(symbols_.size() - 1)});
  }

  [[nodiscard]] NodeId make(OpCode op, NodeId a, NodeId b = no_node) {
    if (const NodeId folded = fold(op, a, b); folded != no_node) {
      return folded;
    }
    if (is_commutative(op) && b != no_node && b < a) {
      std::swap(a, b);
    }
    return intern({.op = op, .a = a, .b = b});
  }

  [[nodiscard]] const Node &operator[](NodeId id) const { return nodes_[id]; }
  [[nodiscard]] std::size_t size() const { return nodes_.size(); }
  [[nodiscard]] std::span<const Node> nodes() const { return nodes_; }
  [[nodiscard]] const std::vector<std::string> &symbols() const {
    return symbols_;
  }

  [[nodiscard]] bool is_constant(NodeId id, double v) const {
    return nodes_[id].op == OpCode::Const && nodes_[id].value == v;
  }

private:
  struct Key {
    OpCode op;
    NodeId a, b;
    std::uint64_t extra;
    bool operator==(const Key &) const = default;
  };
  struct KeyHash {
    std::size_t operator()(const Key &k) const noexcept {
      std::size_t h = static_cast<std::size_t>(k.op);
      auto mix = [&h](std::uint64_t v) {
        h ^= std::hash<std::uint64_t>{}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) +
             (h >> 2);
      };
      mix(k.a);
      mix(k.b);
      mix(k.extra);
      return h;
    }
  };

  static Key key_of(const Node &n) {
    const std::uint64_t extra = n.op == OpCode::Const
                                    ? std::bit_cast<std::uint64_t>(n.value)
                                    : std::uint64_t{n.slot};
    return {n.op, n.a, n.b, extra};
  }

  NodeId intern(const Node &n) {
    const auto [it, fresh] =
        interned_.try_emplace(key_of(n), static_cast<NodeId>(nodes_.size()));
    if (fresh) {
      nodes_.push_back(n);
    }
    return it->second;
  }

  // The rewrites of expr/simplify.hpp.  x*0 -> 0, 0/x -> 0 and (n/d)*d -> n are
  // not IEEE-faithful; as there, they cancel arithmetic the derivative rules
  // manufactured rather than anything a caller wrote.
  NodeId fold(OpCode op, NodeId a, NodeId b) {
    const bool ca = a != no_node && nodes_[a].op == OpCode::Const;
    const bool cb = b != no_node && nodes_[b].op == OpCode::Const;

    if (arity_of(op) == 1) {
      if (ca) {
        return constant(apply1(op, nodes_[a].value));
      }
      if (op == OpCode::Neg && nodes_[a].op == OpCode::Neg) {
        return nodes_[a].a;
      }
      return no_node;
    }
    if (ca && cb) {
      return constant(apply2(op, nodes_[a].value, nodes_[b].value));
    }

    switch (op) {
    case OpCode::Add:
      if (ca && nodes_[a].value == 0.0)
        return b;
      if (cb && nodes_[b].value == 0.0)
        return a;
      break;
    case OpCode::Mul:
      if ((ca && nodes_[a].value == 0.0) || (cb && nodes_[b].value == 0.0))
        return constant(0.0);
      if (ca && nodes_[a].value == 1.0)
        return b;
      if (cb && nodes_[b].value == 1.0)
        return a;
      if (const NodeId n = cancel_quotient(a, b); n != no_node)
        return n;
      if (const NodeId n = cancel_quotient(b, a); n != no_node)
        return n;
      break;
    case OpCode::Div:
      if (a == b)
        return constant(1.0);
      if (ca && nodes_[a].value == 0.0)
        return constant(0.0);
      if (cb && nodes_[b].value == 1.0)
        return a;
      break;
    case OpCode::Pow:
      if (cb && nodes_[b].value == 0.0)
        return constant(1.0);
      if (cb && nodes_[b].value == 1.0)
        return a;
      break;
    default:
      break;
    }
    return no_node;
  }

  // (n/d) * d -> n.  On a DAG the denominator match is an id compare.
  NodeId cancel_quotient(NodeId quotient, NodeId x) const {
    const Node &q = nodes_[quotient];
    return q.op == OpCode::Div && q.b == x ? q.a : no_node;
  }

  std::vector<Node> nodes_;
  std::unordered_map<Key, NodeId, KeyHash> interned_;
  std::vector<std::string> symbols_;
};

} // namespace ddx::rt
