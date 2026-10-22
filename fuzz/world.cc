#include "server/world.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "absl/log/absl_check.h"
#include "content/content.h"
#include "server/levels.h"
#include "server/monsters.h"
#include "server/npcs.h"
#include "terrain/terrain.h"

// A client can send any input, so the fuzzer plays with any input, and checks
// what the world has to keep true.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  // Making the island takes a while, so each input plays on a copy of one.
  static const nanommo::World* const island = [] {
    const nanommo::Content content =
        *nanommo::ReadContent(NANOMMO_CONTENT_FOLDER);
    auto* world = new nanommo::World(1, nanommo::Terrain(1), content.items,
                                     content.skills);
    nanommo::SpawnMonsters(*world, 0, content.monsters);
    nanommo::SpawnNpcs(*world, 0, content.npcs);
    return world;
  }();
  FuzzedDataProvider input(data, size);
  // Any 32 bits make a float, NaNs and infinities too, as a client could send.
  const auto any_float = [&input] {
    return std::bit_cast<float>(input.ConsumeIntegral<uint32_t>());
  };
  nanommo::World world = *island;
  while (input.remaining_bytes() > 0) {
    const uint32_t id = input.ConsumeIntegralInRange<uint32_t>(1, 3);
    const bool playing = world.players().contains(id);
    switch (input.ConsumeIntegralInRange(0, 5)) {
      case 0:
        // Up to level four, where the last skill is.
        if (!playing) {
          world.Join(id, {.name = "bot",
                          .experience = input.ConsumeIntegralInRange(0u, 999u),
                          .weapon = 1});
        }
        break;
      case 1:
        world.SetInput(id, any_float(), any_float(), input.ConsumeBool(),
                       input.ConsumeBool());
        break;
      case 2:
        world.Step(std::chrono::milliseconds(50));
        break;
      case 3:
        world.UseSkill(id, input.ConsumeIntegralInRange<uint32_t>(0, 5));
        break;
      case 4:
        world
            .Learn(id, input.ConsumeIntegralInRange<uint32_t>(0, 9),
                   input.ConsumeIntegralInRange<uint32_t>(0, 5))
            .IgnoreError();
        break;
      default:
        if (playing) {
          world.Leave(id);
        }
        break;
    }
  }
  for (const auto& [id, player] : world.players()) {
    ABSL_CHECK(std::isfinite(player.x) && std::isfinite(player.z));
    ABSL_CHECK_GE(player.health, 0);
    ABSL_CHECK_LE(
        player.health,
        nanommo::MaxHealth(nanommo::LevelFor(player.experience).number));
  }
  return 0;
}
