#pragma once

#include "rt/apply.hpp"
#include "rt/opcode.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
  [[nodiscard]] constexpr NodeId constant(double v) {
    return intern({.op = OpCode::Const, .value = v});
  }

  [[nodiscard]] constexpr NodeId variable(std::string_view name) {
    const auto it = std::ranges::find(symbols_, name);
    // Index before growing: emplace_back invalidates `it`, and for a new symbol
    // the slot is the old size, which is what end() - begin() already is.
    const auto slot = static_cast<std::uint32_t>(it - symbols_.begin());
    if (it == symbols_.end()) {
      symbols_.emplace_back(name);
    }
    return intern({.op = OpCode::Var, .slot = slot});
  }

  [[nodiscard]] constexpr NodeId make(OpCode op, NodeId a, NodeId b = no_node) {
    if (const auto folded = fold(op, a, b)) {
      return *folded;
    }
    if (is_commutative(op) && b != no_node && b < a) {
      std::swap(a, b);
    }
    return intern({.op = op, .a = a, .b = b});
  }

  [[nodiscard]] constexpr const Node &operator[](NodeId id) const {
    return nodes_[id];
  }
  [[nodiscard]] constexpr std::size_t size() const { return nodes_.size(); }
  [[nodiscard]] constexpr std::span<const Node> nodes() const { return nodes_; }
  [[nodiscard]] constexpr const std::vector<std::string> &symbols() const {
    return symbols_;
  }

  [[nodiscard]] constexpr bool is_constant(NodeId id, double v) const {
    return nodes_[id].op == OpCode::Const && nodes_[id].value == v;
  }

private:
  // Open addressing in a plain vector rather than a hash map: unordered_map
  // has no constexpr support, and it is the only thing that would keep the
  // builder out of constant evaluation.  At these sizes it is also the faster
  // structure -- no node allocation and no pointer chase.
  static constexpr std::uint64_t payload_of(const Node &n) {
    return n.op == OpCode::Const ? std::bit_cast<std::uint64_t>(n.value)
                                 : std::uint64_t{n.slot};
  }

  static constexpr bool same(const Node &l, const Node &r) {
    return l.op == r.op && l.a == r.a && l.b == r.b &&
           payload_of(l) == payload_of(r);
  }

  static constexpr std::size_t hash_of(const Node &n) {
    std::uint64_t h = static_cast<std::uint64_t>(n.op);
    const auto mix = [&h](std::uint64_t v) {
      h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
      h *= 0xff51afd7ed558ccdULL;
      h ^= h >> 33;
    };
    mix(n.a);
    mix(n.b);
    mix(payload_of(n));
    return static_cast<std::size_t>(h);
  }

  constexpr void rehash() {
    table_.assign(table_.empty() ? 64 : table_.size() * 2, no_node);
    const std::size_t mask = table_.size() - 1;
    for (NodeId id = 0; id < nodes_.size(); ++id) {
      std::size_t i = hash_of(nodes_[id]) & mask;
      while (table_[i] != no_node) {
        i = (i + 1) & mask;
      }
      table_[i] = id;
    }
  }

  constexpr NodeId intern(const Node &n) {
    // Keep the table at most half full; linear probing degrades sharply past
    // that.
    if ((nodes_.size() + 1) * 2 > table_.size()) {
      rehash();
    }
    const std::size_t mask = table_.size() - 1;
    std::size_t i = hash_of(n) & mask;
    while (table_[i] != no_node) {
      if (same(nodes_[table_[i]], n)) {
        return table_[i];
      }
      i = (i + 1) & mask;
    }
    const auto id = static_cast<NodeId>(nodes_.size());
    nodes_.push_back(n);
    table_[i] = id;
    return id;
  }

  // The rewrites of expr/simplify.hpp.  x*0 -> 0, 0/x -> 0 and (n/d)*d -> n are
  // not IEEE-faithful; as there, they cancel arithmetic the derivative rules
  // manufactured rather than anything a caller wrote.
  constexpr std::optional<NodeId> fold(OpCode op, NodeId a, NodeId b) {
    const bool ca = a != no_node && nodes_[a].op == OpCode::Const;
    const bool cb = b != no_node && nodes_[b].op == OpCode::Const;

    if (arity_of(op) == 1) {
      if (ca) {
        return constant(apply(op, nodes_[a].value));
      }
      if (op == OpCode::Neg && nodes_[a].op == OpCode::Neg) {
        return nodes_[a].a;
      }
      return std::nullopt;
    }
    if (ca && cb) {
      return constant(apply(op, nodes_[a].value, nodes_[b].value));
    }

    switch (op) {
    case OpCode::Add:
      if (ca && nodes_[a].value == 0.0) {
        return b;
      } else if (cb && nodes_[b].value == 0.0) {
        return a;
      }
      break;
    case OpCode::Mul:
      if ((ca && nodes_[a].value == 0.0) || (cb && nodes_[b].value == 0.0)) {
        return constant(0.0);
      }
      if (ca && nodes_[a].value == 1.0)
        return b;
      if (cb && nodes_[b].value == 1.0)
        return a;
      // Either operand may be the quotient; or_else says "that ordering, else
      // the other" without repeating the statement.
      if (const auto n = cancel_quotient(a, b).or_else(
              [&] { return cancel_quotient(b, a); })) {
        return n;
      }
      break;
    case OpCode::Div:
      if (a == b) {
        return constant(1.0);
      } else if (ca && nodes_[a].value == 0.0) {
        return constant(0.0);
      } else if (cb && nodes_[b].value == 1.0) {
        return a;
      }
      break;
    case OpCode::Pow:
      if (cb && nodes_[b].value == 0.0) {
        return constant(1.0);
      } else if (cb && nodes_[b].value == 1.0) {
        return a;
      }
      break;
    default:
      break;
    }
    return std::nullopt;
  }

  // (n/d) * d -> n.  On a DAG the denominator match is an id compare.
  constexpr std::optional<NodeId> cancel_quotient(NodeId quotient,
                                                  NodeId x) const {
    const Node &q = nodes_[quotient];
    return q.op == OpCode::Div && q.b == x ? std::optional{q.a} : std::nullopt;
  }

  std::vector<Node> nodes_;
  std::vector<NodeId>
      table_; // power-of-two capacity; no_node marks a free slot
  std::vector<std::string> symbols_;
};

} // namespace ddx::rt
