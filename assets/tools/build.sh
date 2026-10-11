#!/bin/sh
set -eu

convert() {
  fbx2gltf --binary --skinning-weights 4 --anim-framerate bake30 \
    --input "mixamo/$1.fbx" --output "glb/$2"
}

mkdir -p glb out
convert "X Bot - T-Pose" x_bot
convert "Y Bot - T-Pose" y_bot
convert "Idle" idle
convert "Walking" walk
convert "Slow Run" run
convert "Stable Sword Outward Slash" attack
convert "Dying Backwards" death

for character in x_bot y_bot; do
  node tools/merge.mjs "glb/$character.glb" "out/$character.glb" \
    glb/idle.glb glb/walk.glb glb/run.glb glb/attack.glb glb/death.glb
done
