// Writes the game's content as a graph in smollerGraph, into a folder that
// smollerGraph's sg can serve, with its explorer:
//
//   nanommo_graph content content.sg
//   sg serve content.sg 8474
#include <iostream>

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
  graph.Flush();
  return 0;
}
