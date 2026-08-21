#pragma once

#include "rt/builder.hpp"
#include "rt/derivative.hpp"
#include "rt/opcode.hpp"

#include <boost/graph/compressed_sparse_row_graph.hpp>
#include <boost/range/iterator_range_core.hpp>

#include <algorithm>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ddx::rt {

// The static graph: a builder frozen into CSR, which is the form codegen walks.
// Operand position rides along as an edge attribute, because a CSR row is a set
// and `a / b` is not `b / a`.
template <impl::Numeric T = double> class Graph {
public:
  // Operand position rides along as an edge property, because a CSR row is a
  // set and `a / b` is not `b / a`.
  using adjacency_type =
      boost::compressed_sparse_row_graph<boost::directedS, boost::no_property,
                                         std::uint32_t>;
  using vertex_type = boost::graph_traits<adjacency_type>::vertex_descriptor;

  using value_type = T;

  struct Property {
    OpCode op = OpCode::Const;
    T value{};
    std::uint32_t slot = 0;
  };

  [[nodiscard]] static Graph freeze(const Builder<T> &b,
                                    std::span<const NodeId> outputs) {
    Graph g;
    g.symbols_.assign(b.symbols().begin(), b.symbols().end());
    g.outputs_.assign(outputs.begin(), outputs.end());
    g.properties_.reserve(b.size());

    std::vector<std::pair<std::size_t, std::size_t>> edges;
    std::vector<std::uint32_t> slots;
    edges.reserve(b.size() * 2);
    slots.reserve(b.size() * 2);

    for (const auto [v, n] : std::views::enumerate(b.nodes())) {
      g.properties_.push_back({.op = n.op, .value = n.value, .slot = n.slot});
      const auto id = static_cast<std::size_t>(v);
      if (arity_of(n.op) >= 1) {
        edges.emplace_back(id, n.a);
        slots.push_back(0);
      }
      if (arity_of(n.op) == 2) {
        edges.emplace_back(id, n.b);
        slots.push_back(1);
      }
    }

    g.children_ =
        adjacency_type(boost::edges_are_unsorted_multi_pass, edges.begin(),
                       edges.end(), slots.begin(), b.size());
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

  // A range, so the walks below are range-for and compose with the algorithms.
  [[nodiscard]] auto operand_edges(NodeId v) const {
    return boost::make_iterator_range(
        boost::out_edges(static_cast<vertex_type>(v), children_));
  }

  // The nodes some output depends on, in id order -- which is topological, so a
  // consumer emits them in one pass and never has to skip anything itself.
  // The view refers to this graph; it must not outlive it.
  [[nodiscard]] auto live_nodes() const {
    return std::views::iota(NodeId{0}, static_cast<NodeId>(size())) |
           std::views::filter([this](NodeId v) { return live_[v]; });
  }

  [[nodiscard]] std::size_t live_count() const {
    return static_cast<std::size_t>(std::ranges::distance(live_nodes()));
  }

  // Properties in id order, for a caller that wants to walk them itself.
  [[nodiscard]] std::span<const Property> properties() const {
    return properties_;
  }

  // Operands in slot order.  Arity is at most two, so this is a fixed pair
  // rather than a vector.
  [[nodiscard]] std::array<NodeId, 2> operands(NodeId v) const {
    std::array out{no_node, no_node};
    const auto [first, last] =
        boost::out_edges(static_cast<vertex_type>(v), children_);
    std::ranges::for_each(
        std::ranges::subrange(first, last), [&](const auto &e) {
          out[children_[e]] = static_cast<NodeId>(boost::target(e, children_));
        });
    return out;
  }

private:
  // Nothing but the outputs and what they reach needs to be emitted.  Ids are
  // topological, so one descending pass settles reachability.
  void mark_live() {
    live_.assign(properties_.size(), false);
    std::ranges::for_each(outputs_, [this](NodeId o) { live_[o] = true; });
    // Explicitly indexed, not a filtered view: the body marks entries this
    // descent has not reached yet, and a lazy filter would be reading the
    // state the loop is still changing.
    for (NodeId v = static_cast<NodeId>(properties_.size()); v-- > 0;) {
      if (!live_[v]) {
        continue;
      }
      for (const auto &edge : operand_edges(v)) {
        live_[static_cast<NodeId>(boost::target(edge, children_))] = true;
      }
    }
  }

  std::vector<Property> properties_;
  adjacency_type children_;
  std::vector<NodeId> outputs_;
  std::vector<std::string> symbols_;
  std::vector<bool> live_;
};

// Assembling what a Graph is frozen with.  Freezing takes a list of output
// nodes, and building that list by hand is the same four lines everywhere --
// sweep, take the value, append the partials, freeze.  Each step here names one
// of those, and `build` is the only thing that produces a Graph.
//
//   const auto graph = GraphBuilder{b}.value(f).gradient().build();
//
// The class template argument is deduced from the builder.
template <impl::Numeric T = double> class GraphBuilder {
public:
  explicit constexpr GraphBuilder(Builder<T> &b) noexcept : builder_(&b) {}

  // The function the kernel computes
  constexpr GraphBuilder &value(const RTExpression<T> &root) {
    root_ = root.id(*builder_);
    outputs_.assign(1, root_);
    return *this;
  }

  // Every partial, in symbol order, one output each.  One reverse sweep.
  constexpr GraphBuilder &gradient() {
    const auto g = rt::gradient(*builder_, root_);
    outputs_.insert(outputs_.end(), g.partial.begin(), g.partial.end());
    return *this;
  }

  // Anything else worth a column of its own.
  constexpr GraphBuilder &output(const RTExpression<T> &e) {
    outputs_.push_back(e.id(*builder_));
    return *this;
  }

  [[nodiscard]] Graph<T> build() const {
    return Graph<T>::freeze(*builder_, outputs_);
  }

private:
  Builder<T> *builder_;
  NodeId root_ = no_node;
  std::vector<NodeId> outputs_;
};

template <impl::Numeric T> GraphBuilder(Builder<T> &) -> GraphBuilder<T>;

} // namespace ddx::rt
