# Mixamo files

The characters and their animations come from [Mixamo](https://www.mixamo.com), Adobe's free
library of rigged characters and motion-captured animations. Its terms let us use them in a game,
but not hand out the files themselves. So they stay out of git, and you download your own.

Sign in at mixamo.com with a free Adobe account, and download these into this folder:

| File | From | Download settings |
|---|---|---|
| `X Bot - T-Pose.fbx` | Characters: X Bot | FBX Binary, T-pose |
| `Y Bot - T-Pose.fbx` | Characters: Y Bot | FBX Binary, T-pose |
| `Idle.fbx` | Animations: Idle | FBX Binary, Without Skin, 30 frames per second, no keyframe reduction |
| `Walking.fbx` | Animations: Walking | FBX Binary, Without Skin, 30 frames per second, no keyframe reduction |
| `Slow Run.fbx` | Animations: Slow Run | FBX Binary, Without Skin, 30 frames per second, no keyframe reduction |
| `Stable Sword Outward Slash.fbx` | Animations: Stable Sword Outward Slash | FBX Binary, Without Skin, 30 frames per second, no keyframe reduction |
| `Dying Backwards.fbx` | Animations: Dying Backwards | FBX Binary, Without Skin, 30 frames per second, no keyframe reduction |

Then, from the top of the repo, make the characters the client draws:

```bash
docker build --target characters --output assets/characters .
```

That writes `x_bot.glb` and `y_bot.glb` into `assets/characters`, which git ignores too. Without
them, the client draws the players as cubes.
