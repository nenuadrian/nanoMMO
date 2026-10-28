#ifndef NANOMMO_CLIENT_FIGURES_H_
#define NANOMMO_CLIENT_FIGURES_H_

#include <raylib.h>

#include <vector>

#include "content/figures.pb.h"

namespace nanommo {

/// Puts the parts in place of the model's meshes, each bound to its own bone
/// alone, where the bone rests in the model's bind pose, so the model's clips
/// pose the parts as they posed its mesh. `metre` is a metre in the model's
/// units. A part on a bone the model doesn't have is left out.
void Dress(Model& model, const std::vector<figures::Part>& parts, float metre);

/// The parts' mesh, bound to the model's bones the way Dress binds them, for
/// something that goes over the model's own, such as the hair.
Mesh Parts(const Model& model, const std::vector<figures::Part>& parts,
           float metre);

}  // namespace nanommo

#endif  // NANOMMO_CLIENT_FIGURES_H_
