#ifndef NANOMMO_CONTENT_GRAPH_H_
#define NANOMMO_CONTENT_GRAPH_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/content.h"
#include "smollergraph/graph.h"

namespace nanommo {

/// The node each path starts from: someone who's just made a character, with
/// the land, the kit and the skills a new player gets.
inline constexpr char kNewPlayer[] = "A new player";

/// Adds the content to a graph in smollerGraph: a node for each land, kind of
/// monster, kind of NPC, quest, item and skill, labelled with what it is, and
/// a relationship for each way a player gets from one to the next, from a new
/// player to their land, through its gates, to who's in each land, and on to
/// what they drop, give and teach. A quest also asks for its monster, which is
/// what it takes to finish it, not a way to get anywhere. Or says why it can't:
/// two things with one name, which would be one node, or a name too long for
/// a node.
absl::Status AddContentGraph(const Content& content,
                             smollergraph::Graph& graph);

/// What's wrong with the content as a graph, a line each: what no path from a
/// new player reaches, such as "Leather Cap, an item", and each quest whose
/// monster is out of reach, which nobody can finish. Or the query's error.
absl::StatusOr<std::vector<std::string>> Unreachable(
    const smollergraph::Graph& graph);

}  // namespace nanommo

#endif  // NANOMMO_CONTENT_GRAPH_H_
