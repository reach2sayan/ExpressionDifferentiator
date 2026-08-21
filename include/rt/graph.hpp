#pragma once

#include "rt/builder.hpp"
#include "rt/opcode.hpp"

#include <nwgraph/adjacency.hpp>
#include <nwgraph/edge_list.hpp>

#include <execution>
#include <span>
#include <string>
#include <vector>

namespace ddx::rt {

// The static graph: a builder frozen into CSR, which is the form codegen walks.
// Operand position rides along as an edge attribute, because a CSR row is a set
// and `a / b` is not `b / a`.
class Graph {
public:
  using edge_list_type =
      nw::graph::edge_list<nw::graph::directedness::directed, std::uint32_t>;
  using adjacency_type = nw::graph::adjacency<0, std::uint32_t>;

  struct Property {
    OpCode op = OpCode::Const;
    double value = 0.0;
    std::uint32_t slot = 0;
  };

  [[nodiscard]] static Graph freeze(const Builder &b,
                                    std::span<const NodeId> outputs) {
    Graph g;
    g.symbols_.assign(b.symbols().begin(), b.symbols().end());
    g.outputs_.assign(outputs.begin(), outputs.end());
    g.properties_.reserve(b.size());

    edge_list_type el(b.size());
    el.open_for_push_back();
    for (NodeId v = 0; v < b.size(); ++v) {
      const Node &n = b[v];
      g.properties_.push_back({.op = n.op, .value = n.value, .slot = n.slot});
      if (arity_of(n.op) >= 1) {
        el.push_back(v, n.a, 0);
      }
      if (arity_of(n.op) == 2) {
        el.push_back(v, n.b, 1);
      }
    }
    el.close_for_push_back();

    // Sequenced: these graphs are hundreds of nodes, where dispatching to a
    // thread pool costs more than the pass it replaces.
    g.children_ = adjacency_type(el, false, std::execution::seq);
    g.mark_live();
    return g;
  }

  [[nodiscard]] std::size_t size() const { return properties_.size(); }
  [[nodiscard]] const Property &operator[](NodeId v) const {
    return properties_[v];
  }
  [[nodiscard]] const adjacency_type &children() const { return children_; }
  [[nodiscard]] std::span<const NodeId> outputs() const { return outputs_; }
  [[nodiscard]] const std::vector<std::string> &symbols() const {
    return symbols_;
  }
  [[nodiscard]] bool live(NodeId v) const { return live_[v]; }
  [[nodiscard]] std::size_t live_count() const {
    return static_cast<std::size_t>(std::ranges::count(live_, true));
  }

  // Operands in slot order.  Arity is at most two, so this is a fixed pair
  // rather than a vector.
  [[nodiscard]] std::array<NodeId, 2> operands(NodeId v) const {
    std::array<NodeId, 2> out{no_node, no_node};
    for (auto &&[child, slot] : children_[v]) {
      out[slot] = static_cast<NodeId>(child);
    }
    return out;
  }

private:
  // Nothing but the outputs and what they reach needs to be emitted.  Ids are
  // topological, so one descending pass settles reachability.
  void mark_live() {
    live_.assign(properties_.size(), false);
    for (const NodeId o : outputs_) {
      live_[o] = true;
    }
    for (NodeId v = static_cast<NodeId>(properties_.size()); v-- > 0;) {
      if (!live_[v]) {
        continue;
      }
      for (auto &&[child, slot] : children_[v]) {
        live_[static_cast<NodeId>(child)] = true;
      }
    }
  }

  std::vector<Property> properties_;
  adjacency_type children_;
  std::vector<NodeId> outputs_;
  std::vector<std::string> symbols_;
  std::vector<bool> live_;
};

} // namespace ddx::rt
