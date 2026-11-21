#include "content/graph.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "smollergraph/graph.h"
#include "smollergraph/plan.h"
#include "terrain/lands.h"

namespace nanommo {
namespace {

// The ways a player gets from one thing to the next, which a path follows.
// A quest's ASKS_FOR isn't one of them.
constexpr std::string_view kWays =
    "STARTS_IN|STARTS_WITH|KNOWS|GATE|SPAWNS|STANDS|DROPS|SUMMONS|GIVES|"
    "REWARDS|TEACHES|NEXT|INTO|MAKES";

// Each label, and what it's called in a line of Unreachable's.
struct Kind {
  std::string_view label;
  std::string_view called;
};
constexpr Kind kKinds[] = {
    {"Land", "a land"},     {"Monster", "a monster"}, {"Npc", "an NPC"},
    {"Quest", "a quest"},   {"Item", "an item"},      {"Skill", "a skill"},
    {"Recipe", "a recipe"},
};

// Adds the content's nodes and relationships, a name at a time, and keeps
// the first thing that goes wrong.
class Builder {
 public:
  explicit Builder(smollergraph::Graph& graph) : graph_(graph) {}

  // A node of its own for each name, as a name the graph has already would
  // join two things into one.
  void Node(std::string_view name, std::string_view label) {
    if (!status_.ok()) return;
    if (name.size() > smollergraph::Graph::kMaxName) {
      status_ = absl::InvalidArgumentError(
          absl::StrCat("the name ", name, " is too long for a node"));
    } else if (graph_.Find(name)) {
      status_ = absl::InvalidArgumentError(
          absl::StrCat("two things are called ", name));
    } else {
      graph_.SetLabel(graph_.AddNode(name), label);
    }
  }

  void Relationship(std::string_view from, std::string_view type,
                    std::string_view to) {
    if (!status_.ok()) return;
    const std::optional<smollergraph::NodeId> start = graph_.Find(from);
    const std::optional<smollergraph::NodeId> end = graph_.Find(to);
    if (!start || !end) {
      status_ = absl::NotFoundError(
          absl::StrCat("nothing is called ", start ? to : from));
      return;
    }
    graph_.AddRelationship(*start, type, *end);
  }

  const absl::Status& status() const { return status_; }

 private:
  smollergraph::Graph& graph_;
  absl::Status status_;
};

// The rows' first column, as strings.
std::set<std::string> Names(const smollergraph::Result& result) {
  std::set<std::string> names;
  for (const std::vector<smollergraph::Cell>& row : result.rows) {
    const std::optional<smollergraph::Value>& name = row[0].value;
    if (name) names.insert(smollergraph::ValueText(*name));
  }
  return names;
}

}  // namespace

absl::Status AddContentGraph(const Content& content,
                             smollergraph::Graph& graph) {
  Builder add(graph);
  add.Node(kNewPlayer, "Player");
  for (const Land& land : kLands) add.Node(land.name, "Land");
  for (const auto& [id, kind] : content.monsters.kinds) {
    add.Node(kind.name(), "Monster");
  }
  for (const auto& [id, kind] : content.npcs.kinds) {
    add.Node(kind.name(), "Npc");
  }
  for (const auto& [id, quest] : content.quests) {
    add.Node(quest.name(), "Quest");
  }
  for (const auto& [id, item] : content.items.by_id) {
    add.Node(item.name(), "Item");
  }
  for (const auto& [id, skill] : content.skills.by_id) {
    add.Node(skill.name(), "Skill");
  }
  for (const auto& [id, recipe] : content.recipes) {
    add.Node(recipe.name(), "Recipe");
  }
  if (!add.status().ok()) return add.status();

  // A new player starts in the first land, with the kit and the starting
  // skills, the swing on Space among them.
  add.Relationship(kNewPlayer, "STARTS_IN", kLands[0].name);
  for (const items::Stack& stack : content.items.starting) {
    add.Relationship(kNewPlayer, "STARTS_WITH",
                     content.items.by_id.at(stack.item()).name());
  }
  for (const uint32_t skill : content.skills.starting) {
    add.Relationship(kNewPlayer, "KNOWS",
                     content.skills.by_id.at(skill).name());
  }
  for (const Gate& gate : kGates) {
    add.Relationship(kLands[gate.land].name, "GATE", kLands[gate.to].name);
  }

  // Who's in each land: the monsters that spawn there, alone or in camps,
  // and the NPCs that stand there.
  for (const monsters::Spawn& spawn : content.monsters.spawns) {
    add.Relationship(spawn.land(), "SPAWNS",
                     content.monsters.kinds.at(spawn.kind()).name());
  }
  for (const monsters::Camps& camps : content.monsters.camps) {
    add.Relationship(camps.land(), "SPAWNS",
                     content.monsters.kinds.at(camps.kind()).name());
  }
  for (const npcs::Spawn& spawn : content.npcs.spawns) {
    add.Relationship(spawn.land(), "STANDS",
                     content.npcs.kinds.at(spawn.kind()).name());
  }

  // What each monster drops, and the ones a boss calls in as it goes.
  for (const auto& [id, kind] : content.monsters.kinds) {
    for (const monsters::Drop& drop : kind.loot()) {
      add.Relationship(kind.name(), "DROPS",
                       content.items.by_id.at(drop.item()).name());
    }
    for (const monsters::Phase& phase : kind.phases()) {
      for (const monsters::Add& more : phase.adds()) {
        add.Relationship(kind.name(), "SUMMONS",
                         content.monsters.kinds.at(more.kind()).name());
      }
    }
  }

  // What each NPC teaches and gives, what each quest pays and asks for, and
  // the swing each skill goes on into.
  for (const auto& [id, kind] : content.npcs.kinds) {
    for (const uint32_t skill : kind.teaches()) {
      add.Relationship(kind.name(), "TEACHES",
                       content.skills.by_id.at(skill).name());
    }
  }
  for (const auto& [id, quest] : content.quests) {
    add.Relationship(content.npcs.kinds.at(quest.giver()).name(), "GIVES",
                     quest.name());
    for (const quests::Prize& prize : quest.prizes()) {
      add.Relationship(quest.name(), "REWARDS",
                       content.items.by_id.at(prize.item()).name());
    }
    add.Relationship(quest.name(), "ASKS_FOR",
                     content.monsters.kinds.at(quest.kill().monster()).name());
  }
  for (const auto& [id, skill] : content.skills.by_id) {
    if (skill.next() != 0) {
      add.Relationship(skill.name(), "NEXT",
                       content.skills.by_id.at(skill.next()).name());
    }
  }

  // What goes into each recipe, and what it makes.
  for (const auto& [id, recipe] : content.recipes) {
    for (const recipes::ItemCount& input : recipe.inputs()) {
      add.Relationship(content.items.by_id.at(input.item()).name(), "INTO",
                       recipe.name());
    }
    add.Relationship(recipe.name(), "MAKES",
                     content.items.by_id.at(recipe.output().item()).name());
  }
  return add.status();
}

absl::StatusOr<std::vector<std::string>> Unreachable(
    const smollergraph::Graph& graph) {
  std::vector<std::string> lines;
  std::set<std::string> monsters;
  for (const Kind& kind : kKinds) {
    absl::StatusOr<smollergraph::Result> all = smollergraph::Execute(
        graph, absl::StrCat("MATCH (x:", kind.label, ") RETURN x.name"));
    if (!all.ok()) return all.status();
    // A shortest path from a new player to each node with the label, along the
    // ways a player gets about, and a row for each node that has one.
    absl::StatusOr<smollergraph::Result> reached = smollergraph::Execute(
        graph, absl::StrCat("MATCH p = shortestPath((s {name: '", kNewPlayer,
                            "'})-[:", kWays, "*]->(x:", kind.label,
                            ")) RETURN x.name"));
    if (!reached.ok()) return reached.status();
    const std::set<std::string> got = Names(*reached);
    if (kind.label == "Monster") monsters = got;
    for (const std::string& name : Names(*all)) {
      if (!got.contains(name)) {
        lines.push_back(absl::StrCat(name, ", ", kind.called));
      }
    }
  }
  // A quest is finished by killing its monster, so a monster out of reach
  // leaves its quests unfinished, wherever their givers stand.
  absl::StatusOr<smollergraph::Result> asks = smollergraph::Execute(
      graph, "MATCH (q:Quest)-[:ASKS_FOR]->(m) RETURN q.name, m.name");
  if (!asks.ok()) return asks.status();
  for (const std::vector<smollergraph::Cell>& row : asks->rows) {
    const std::optional<smollergraph::Value>& quest = row[0].value;
    const std::optional<smollergraph::Value>& asked = row[1].value;
    if (!quest || !asked) continue;
    const std::string monster = smollergraph::ValueText(*asked);
    if (!monsters.contains(monster)) {
      lines.push_back(absl::StrCat(smollergraph::ValueText(*quest),
                                   " asks for the ", monster,
                                   ", which no new player can reach"));
    }
  }
  return lines;
}

}  // namespace nanommo
