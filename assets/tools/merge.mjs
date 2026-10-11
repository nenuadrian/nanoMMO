import path from 'node:path';
import { NodeIO } from '@gltf-transform/core';
import { dedup, prune, simplify, weld } from '@gltf-transform/functions';
import { mat4 } from 'gl-matrix';
import { MeshoptSimplifier } from 'meshoptimizer';

const KEEP = 0.1;

const IN_PLACE = new Set(['walk', 'run']);

const [characterPath, outPath, ...clipPaths] = process.argv.slice(2);
const io = new NodeIO();
const doc = await io.read(characterPath);
const root = doc.getRoot();
const buffer = root.listBuffers()[0];
const bones = new Map(root.listNodes().map((node) => [node.getName(), node]));
const clips = await Promise.all(clipPaths.map((file) => io.read(file)));

// raylib uses the first skin, and finds a clip's bones by their place in it.
// Each Mixamo mesh has a skin of its own, so we make one, in the clips' order.
const jointNames = clips[0].getRoot().listSkins()[0].listJoints().map((joint) => joint.getName());
const joints = jointNames.map((name) => {
  if (!bones.has(name)) throw new Error(`the character has no bone called ${name}`);
  return bones.get(name);
});
const inverseBinds = new Float32Array(joints.length * 16);
joints.forEach((joint, i) => mat4.invert(inverseBinds.subarray(i * 16, (i + 1) * 16), joint.getWorldMatrix()));
const skin = doc
  .createSkin('skeleton')
  .setSkeleton(joints[0])
  .setInverseBindMatrices(doc.createAccessor().setType('MAT4').setArray(inverseBinds).setBuffer(buffer));
joints.forEach((joint) => skin.addJoint(joint));

for (const node of root.listNodes()) {
  const old = node.getSkin();
  if (!old || !node.getMesh()) continue;
  const remap = old.listJoints().map((joint) => jointNames.indexOf(joint.getName()));
  for (const primitive of node.getMesh().listPrimitives()) {
    const indices = primitive.getAttribute('JOINTS_0').getArray().map((index) => remap[index]);
    primitive.setAttribute('JOINTS_0', doc.createAccessor().setType('VEC4').setArray(indices).setBuffer(buffer));
  }
  node.setSkin(skin);
}
for (const other of root.listSkins()) if (other !== skin) other.dispose();

for (const material of root.listMaterials()) {
  const shade = material.getName().includes('Joints') ? 0.2 : 1.0;
  material.setBaseColorFactor([shade, shade, shade, 1]);
}

for (const animation of root.listAnimations()) animation.dispose();
clips.forEach((clip, i) => {
  const name = path.basename(clipPaths[i], '.glb');
  for (const source of clip.getRoot().listAnimations()) {
    const animation = doc.createAnimation(name);
    for (const channel of source.listChannels()) {
      const bone = bones.get(channel.getTargetNode().getName());
      if (!bone) continue;
      const sampler = channel.getSampler();
      const times = sampler.getInput().getArray().slice();
      const values = sampler.getOutput().getArray().slice();
      if (bone === joints[0] && channel.getTargetPath() === 'translation' && IN_PLACE.has(name)) {
        inPlace(name, times, values);
      }
      const copy = doc
        .createAnimationSampler()
        .setInput(doc.createAccessor().setType('SCALAR').setArray(times).setBuffer(buffer))
        .setOutput(doc.createAccessor().setType(sampler.getOutput().getType()).setArray(values).setBuffer(buffer))
        .setInterpolation(sampler.getInterpolation());
      animation
        .addSampler(copy)
        .addChannel(doc.createAnimationChannel().setTargetNode(bone).setTargetPath(channel.getTargetPath()).setSampler(copy));
    }
  }
});

// Mixamo's walk and run carry the hips forward. Our server moves the players,
// so we take that out, and print the speed that keeps the feet from sliding.
// The other clips keep theirs: a player falls back as it dies.
function inPlace(name, times, values) {
  const last = times.length - 1;
  if (times[last] === 0) return;
  const [x0, z0] = [values[0], values[2]];
  const dx = values[last * 3] - x0;
  const dz = values[last * 3 + 2] - z0;
  for (let k = 0; k <= last; k++) {
    const t = times[k] / times[last];
    values[k * 3] -= x0 + dx * t;
    values[k * 3 + 2] -= z0 + dz * t;
  }
  console.log(`${name}: ${(Math.hypot(dx, dz) / times[last]).toFixed(2)} metres a second`);
}

await MeshoptSimplifier.ready;
await doc.transform(weld(), simplify({ simplifier: MeshoptSimplifier, ratio: KEEP, error: 0.01 }), prune(), dedup());
await io.write(outPath, doc);

const triangles = root
  .listMeshes()
  .flatMap((mesh) => mesh.listPrimitives())
  .reduce((sum, primitive) => sum + primitive.getIndices().getCount() / 3, 0);
const names = root.listAnimations().map((animation) => animation.getName());
console.log(`${path.basename(outPath)}: ${joints.length} bones, ${triangles} triangles, clips ${names.join(', ')}`);
