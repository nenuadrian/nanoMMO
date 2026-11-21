// Writes the game's content as a graph in smollerGraph, into a folder that
// smollerGraph's sg can serve, with its explorer, and says what a new player
// can't reach:
//
//   nanommo_graph content content.sg
//   sg serve content.sg 8474
#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "content/content.h"
#include "content/graph.h"
#include "smollergraph/graph.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: nanommo_graph <content folder> <graph folder>\n";
    return 1;
  }
  const absl::StatusOr<nanommo::Content> content =
      nanommo::ReadContent(argv[1]);
  if (!content.ok()) {
    std::cerr << "nanommo_graph: " << content.status().message() << '\n';
    return 1;
  }
  smollergraph::Graph graph;
  absl::Status status = graph.Open(argv[2]);
  if (status.ok()) status = nanommo::AddContentGraph(*content, graph);
  if (!status.ok()) {
    std::cerr << "nanommo_graph: " << status.message() << '\n';
    return 1;
  }
  std::cout << graph.node_count() << " nodes, " << graph.relationship_count()
            << " relationships\n";
  absl::StatusOr<std::vector<std::string>> lines = nanommo::Unreachable(graph);
  const absl::StatusOr<std::vector<std::string>> cycles =
      nanommo::Cycles(graph);
  if (!lines.ok() || !cycles.ok()) {
    std::cerr << "nanommo_graph: "
              << (lines.ok() ? cycles.status() : lines.status()).message()
              << '\n';
    return 1;
  }
  lines->insert(lines->end(), cycles->begin(), cycles->end());
  for (const std::string& line : *lines) std::cout << line << '\n';
  graph.Flush();
  // Something out of reach, or that goes round, is an error, so a script can
  // tell.
  return lines->empty() ? 0 : 1;
}
