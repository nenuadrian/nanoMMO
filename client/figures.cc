#include "client/figures.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "client/triangles.h"

namespace nanommo {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Eight sides round each limb, and each end's cap is two rings of a ball: its
// pole, and one this far round from it.
constexpr int kSides = 8;
constexpr float kCapAngle = 55.0f * DEG2RAD;

template <typename T>
T* Copy(const std::vector<T>& values) {
  // UnloadMesh frees what's here, so raylib allocates it.
  T* copy = static_cast<T*>(MemAlloc(values.size() * sizeof(T)));
  std::ranges::copy(values, copy);
  return copy;
}

// Triangles that share no corners, like the ground's, so each is lit flat,
// with each corner bound to one bone.
class Builder {
 public:
  // The shape's smooth normal at each corner says which side is outside, so
  // the corners can come in either order.
  void Add(Vector3 a, Vector3 b, Vector3 c, Vector3 na, Vector3 nb, Vector3 nc,
           Color color, int bone) {
    Vector3 face = FaceNormal(a, b, c);
    if (Vector3DotProduct(face, Vector3Add(Vector3Add(na, nb), nc)) < 0.0f) {
      std::swap(b, c);
      face = Vector3Negate(face);
    }
    for (const Vector3& corner : {a, b, c}) {
      positions_.insert(positions_.end(), {corner.x, corner.y, corner.z});
      normals_.insert(normals_.end(), {face.x, face.y, face.z});
      colors_.insert(colors_.end(), {color.r, color.g, color.b, color.a});
      // Up to four bones can move a corner, each by its weight, and each of
      // ours moves with one, all the way.
      bones_.insert(bones_.end(), {static_cast<unsigned char>(bone), 0, 0, 0});
      weights_.insert(weights_.end(), {1.0f, 0.0f, 0.0f, 0.0f});
    }
  }

  Mesh Upload() const {
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(positions_.size() / 3);
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.vertices = Copy(positions_);
    mesh.normals = Copy(normals_);
    mesh.colors = Copy(colors_);
    mesh.boneIndices = Copy(bones_);
    mesh.boneWeights = Copy(weights_);
    UploadMesh(&mesh, false);
    return mesh;
  }

 private:
  std::vector<float> positions_;
  std::vector<float> normals_;
  std::vector<unsigned char> colors_;
  std::vector<unsigned char> bones_;
  std::vector<float> weights_;
};

// Two ways across the axis, square to it and to each other: the first across
// the figure, or for a limb that goes across it, as the arms do, to the front.
std::pair<Vector3, Vector3> Across(Vector3 axis) {
  const Vector3 hint = std::fabs(axis.x) > 0.8f ? Vector3{0.0f, 0.0f, 1.0f}
                                                : Vector3{1.0f, 0.0f, 0.0f};
  const Vector3 u = Vector3Normalize(
      Vector3Subtract(hint, Vector3Scale(axis, Vector3DotProduct(hint, axis))));
  return {u, Vector3CrossProduct(axis, u)};
}

// A ring of corners round a limb: its middle, its radius, and how far its
// normals lean from the axis, from none at a pole to a quarter turn round
// the side.
struct Ring {
  Vector3 middle;
  float radius;
  float lean;
};

// Six rings from end to end: a cap at each end, of a pole and a ring part way
// round, and between them, the limb's sides, from one radius to the other.
void AddLimb(Builder& out, Vector3 a, Vector3 b, float ra, float rb,
             float flatten, Color color, int bone) {
  const Vector3 axis = Vector3Normalize(Vector3Subtract(b, a));
  const auto [u, v] = Across(axis);
  const float c = std::cos(kCapAngle);
  const float s = std::sin(kCapAngle);
  const std::array<Ring, 6> rings = {{
      {Vector3Subtract(a, Vector3Scale(axis, ra)), 0.0f, 0.0f},
      {Vector3Subtract(a, Vector3Scale(axis, ra * c)), ra * s, kCapAngle},
      {a, ra, kPi / 2.0f},
      {b, rb, kPi / 2.0f},
      {Vector3Add(b, Vector3Scale(axis, rb * c)), rb * s, kPi - kCapAngle},
      {Vector3Add(b, Vector3Scale(axis, rb)), 0.0f, kPi},
  }};
  auto corner = [&](const Ring& ring, int i) {
    const float turn = 2.0f * kPi * (i + 0.5f) / kSides;
    const Vector3 out_u = Vector3Scale(u, std::cos(turn));
    const Vector3 out_v = Vector3Scale(v, std::sin(turn));
    const Vector3 at = Vector3Add(
        ring.middle, Vector3Add(Vector3Scale(out_u, ring.radius),
                                Vector3Scale(out_v, ring.radius * flatten)));
    const Vector3 around = Vector3Normalize(
        Vector3Add(out_u, Vector3Scale(out_v, 1.0f / flatten)));
    const Vector3 normal = Vector3Add(Vector3Scale(around, std::sin(ring.lean)),
                                      Vector3Scale(axis, -std::cos(ring.lean)));
    return std::pair{at, normal};
  };
  for (std::size_t r = 0; r + 1 < rings.size(); ++r) {
    for (int i = 0; i < kSides; ++i) {
      const auto [a0, n0] = corner(rings[r], i);
      const auto [a1, n1] = corner(rings[r], (i + 1) % kSides);
      const auto [b0, m0] = corner(rings[r + 1], i);
      const auto [b1, m1] = corner(rings[r + 1], (i + 1) % kSides);
      // At a pole, each ring's corners meet in one point, so its quads are
      // triangles.
      if (r == 0) {
        out.Add(a0, b1, b0, n0, m1, m0, color, bone);
      } else if (r + 2 == rings.size()) {
        out.Add(a0, a1, b0, n0, n1, m0, color, bone);
      } else {
        out.Add(a0, a1, b1, n0, n1, m1, color, bone);
        out.Add(a0, b1, b0, n0, m1, m0, color, bone);
      }
    }
  }
}

// An icosahedron's twenty faces, each split into four, out to the radii: a
// ball that's round, but low-poly, like the rest of the world.
void AddBall(Builder& out, Vector3 middle, Vector3 radii, Color color,
             int bone) {
  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  std::array<Vector3, 12> corners = {{{-1, t, 0},
                                      {1, t, 0},
                                      {-1, -t, 0},
                                      {1, -t, 0},
                                      {0, -1, t},
                                      {0, 1, t},
                                      {0, -1, -t},
                                      {0, 1, -t},
                                      {t, 0, -1},
                                      {t, 0, 1},
                                      {-t, 0, -1},
                                      {-t, 0, 1}}};
  for (Vector3& corner : corners) {
    corner = Vector3Normalize(corner);
  }
  constexpr std::array<std::array<int, 3>, 20> kFaces = {
      {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
       {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
       {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
       {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}}};
  auto place = [&](Vector3 p) {
    return Vector3Add(middle, {p.x * radii.x, p.y * radii.y, p.z * radii.z});
  };
  // An egg's normals lean away from its long axis.
  auto normal = [&](Vector3 p) {
    return Vector3Normalize({p.x / radii.x, p.y / radii.y, p.z / radii.z});
  };
  auto add = [&](Vector3 a, Vector3 b, Vector3 c) {
    out.Add(place(a), place(b), place(c), normal(a), normal(b), normal(c),
            color, bone);
  };
  for (const auto& [i, j, k] : kFaces) {
    const Vector3 a = corners[i];
    const Vector3 b = corners[j];
    const Vector3 c = corners[k];
    const Vector3 ab = Vector3Normalize(Vector3Add(a, b));
    const Vector3 bc = Vector3Normalize(Vector3Add(b, c));
    const Vector3 ca = Vector3Normalize(Vector3Add(c, a));
    add(a, ab, ca);
    add(b, bc, ab);
    add(c, ca, bc);
    add(ab, bc, ca);
  }
}

// Mixamo's rig puts mixamorig: in front of each bone's name.
int BoneNamed(const Model& model, const std::string& name) {
  const std::string full = absl::StrCat("mixamorig:", name);
  for (int i = 0; i < model.skeleton.boneCount; ++i) {
    if (full == model.skeleton.bones[i].name) {
      return i;
    }
  }
  return -1;
}

Vector3 Metres(const figures::Offset& offset, float metre) {
  return {offset.x() * metre, offset.y() * metre, offset.z() * metre};
}

// The parts' triangles.
Mesh Build(const Model& model, const std::vector<figures::Part>& parts,
           float metre) {
  Builder out;
  // The bind pose is where each bone rests, in the model's own space.
  const Transform* rest = model.skeleton.bindPose;
  for (const figures::Part& part : parts) {
    const int bone = BoneNamed(model, part.bone());
    const int from = part.from().empty() ? bone : BoneNamed(model, part.from());
    const int to = BoneNamed(model, part.to());
    if (bone < 0 || from < 0) {
      continue;
    }
    const Color color = GetColor(part.color());
    const Vector3 start =
        Vector3Add(rest[from].translation, Metres(part.from_offset(), metre));
    if (part.shape() == figures::Part::SHAPE_BALL) {
      AddBall(out, start, Metres(part.size(), metre), color, bone);
    } else if (part.shape() == figures::Part::SHAPE_LIMB && to >= 0) {
      const Vector3 end =
          Vector3Add(rest[to].translation, Metres(part.to_offset(), metre));
      const float flatten = part.flatten() > 0.0f ? part.flatten() : 1.0f;
      AddLimb(out, start, end, part.start_radius() * metre,
              part.end_radius() * metre, flatten, color, bone);
    }
  }
  return out.Upload();
}

}  // namespace

void Dress(Model& model, const std::vector<figures::Part>& parts, float metre) {
  const Mesh mesh = Build(model, parts, metre);
  for (int i = 0; i < model.meshCount; ++i) {
    UnloadMesh(model.meshes[i]);
  }
  MemFree(model.meshes);
  MemFree(model.meshMaterial);
  model.meshCount = 1;
  model.meshes = static_cast<Mesh*>(MemAlloc(sizeof(Mesh)));
  model.meshes[0] = mesh;
  model.meshMaterial = static_cast<int*>(MemAlloc(sizeof(int)));
  // The parts' colours are their corners', so the material's is white.
  model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
}

Mesh Parts(const Model& model, const std::vector<figures::Part>& parts,
           float metre) {
  return Build(model, parts, metre);
}

}  // namespace nanommo
