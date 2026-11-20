#include "content/graph.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "smollergraph/graph.h"
#include "terrain/lands.h"

namespace nanommo {
namespace {

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
    graph_.AddRelationship(*graph_.Find(from), type, *graph_.Find(to));
  }

  const absl::Status& status() const { return status_; }

 private:
  smollergraph::Graph& graph_;
  absl::Status status_;
};

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
  return add.status();
}

}  // namespace nanommo
