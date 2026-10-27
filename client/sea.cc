#include "client/sea.h"

#include <raymath.h>

#include <cmath>
#include <vector>

#include "client/triangles.h"

namespace nanommo {

namespace {

// Waves: three of them, each its own way and length, raise and lower each
// corner of the sea, less where it's shallow.
constexpr const char* kVertexShader = R"(#version 330
in vec3 vertexPosition;
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
uniform vec3 eye;
uniform float seconds;
uniform sampler2D ground;
uniform float halfSize;
uniform float cell;
uniform float height;
uniform float wavelength;
uniform float pace;
out vec3 world;
out float depth;
// From the ground up to the water, at each pixel.
out float gap;
// How deep the still sea is, at one of the triangle's corners.
flat out float below;
// How high up its wave that corner is, from 0 at the bottom to 1 at the top.
flat out float crest;

const vec2 kWays[3] = vec2[](vec2(1.0, 0.35), vec2(-0.45, 1.0),
                             vec2(0.6, -0.8));
// Each one's length, as a share of the longest's.
const float kLengths[3] = float[](1.0, 0.71, 0.53);

void main() {
  world = (matModel * vec4(vertexPosition, 1.0)).xyz;
  // The corner of the ground's cells under this corner of the sea, which is
  // the edge's past the edge of the terrain.
  ivec2 corner = clamp(ivec2(round((world.xz + halfSize) / cell)), 0,
                       textureSize(ground, 0).x - 1);
  float bottom = texelFetch(ground, corner, 0).r;
  below = world.y - bottom;
  // Far away, a cell is a few pixels, and waves that small only flicker.
  float calm = 1.0 - smoothstep(40.0, 100.0, distance(world.xz, eye.xz));
  // In the shallows, waves lap at the shore.
  calm *= mix(0.25, 1.0, smoothstep(0.0, 2.5, below));
  float wave = 0.0;
  for (int i = 0; i < 3; ++i) {
    vec2 way = normalize(kWays[i]);
    float k = 6.2831853 / (wavelength * kLengths[i]);
    // Each moves as fast as a wave of its length does at sea, at its pace.
    float phase = dot(way, world.xz) * k + seconds * pace * sqrt(9.8 * k);
    wave += sin(phase) / 3.0;
  }
  crest = 0.5 + 0.5 * wave;
  world.y += height * calm * wave;
  gap = world.y - bottom;
  gl_Position = matProjection * matView * vec4(world, 1.0);
  depth = gl_Position.w;
}
)";

// Facets: each triangle is lit flat, like the ground's, with the normal of its
// face.
constexpr const char* kFragmentShader = R"(#version 330
in vec3 world;
in float depth;
in float gap;
flat in float below;
flat in float crest;
uniform vec4 deep;
uniform vec4 shallow;
uniform vec4 edge;
uniform vec3 sun;
uniform vec3 sunlight;
uniform vec3 ambient;
uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;
uniform float emission;
uniform float glint;
uniform float bands;
uniform vec3 eye;
uniform float reach;
out vec4 pixel;

void main() {
  // How far the world moves from this pixel to the next one across and the
  // next one up. Both lie in the triangle's face, so across them is its
  // normal, the same for each of its pixels.
  vec3 surface = normalize(cross(dFdx(world), dFdy(world)));
  // Turned up, whichever way round the two were.
  if (surface.y < 0.0) {
    surface = -surface;
  }
  float sunlit = max(dot(surface, normalize(sun)), 0.0);
  if (bands > 0.0) {
    sunlit = min(floor(sunlit * bands), bands - 1.0) / (bands - 1.0);
  }
  vec4 color = mix(shallow, deep, smoothstep(0.2, 2.0, below));
  // Along the line where it meets the ground.
  color = mix(color, edge, 1.0 - smoothstep(0.08, 0.14, gap));
  // The tops of the waves give off the most light, and the bottoms none.
  vec3 light = ambient + sunlight * sunlit + emission * 4.0 * crest * crest;
  // The sun, reflected off the surface, where it comes back to the eye.
  vec3 reflected = reflect(-normalize(sun), surface);
  float shine = pow(max(dot(reflected, normalize(eye - world)), 0.0), 24.0);
  vec3 lit = color.rgb * light + sunlight * shine * glint;
  float fog = clamp((depth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
  // Into the sky towards the edge of the sea, wherever the fog ends.
  fog = max(fog, smoothstep(reach - 30.0, reach, distance(world.xz, eye.xz)));
  pixel = vec4(mix(lit, fogColor, fog), color.a);
}
)";

// In metres from the camera to each side of the sea, a little past the fog.
constexpr float kReach = 160.0f;

void Set(Shader shader, const char* name, const void* value, int type) {
  SetShaderValue(shader, GetShaderLocation(shader, name), value, type);
}

void SetColor(Shader shader, const char* name, Color color) {
  const Vector4 normalized = ColorNormalize(color);
  Set(shader, name, &normalized, SHADER_UNIFORM_VEC4);
}

}  // namespace

Sea::Sea(const Terrain& terrain, const Look& look)
    : shader_(LoadShaderFromMemory(kVertexShader, kFragmentShader)) {
  // A square around the camera, cut into the ground's cells, two triangles
  // each, the way the ground's are, which the waves move. So the ground under
  // each triangle is flat, and the gap up to the water is right at each pixel.
  const int cells = static_cast<int>(2.0f * kReach / Terrain::kCell);
  const auto corner = [](int i, int j) {
    return Vector3{i * Terrain::kCell - kReach, 0.0f,
                   j * Terrain::kCell - kReach};
  };
  Triangles grid;
  for (int j = 0; j < cells; ++j) {
    for (int i = 0; i < cells; ++i) {
      grid.Add(corner(i, j), corner(i + 1, j + 1), corner(i + 1, j), WHITE);
      grid.Add(corner(i, j), corner(i, j + 1), corner(i + 1, j + 1), WHITE);
    }
  }
  model_ = LoadModelFromMesh(grid.Upload());
  model_.materials[0].shader = shader_;
  Set(shader_, "reach", &kReach, SHADER_UNIFORM_FLOAT);

  const Liquid& liquid = look.sea;
  SetColor(shader_, "deep", liquid.deep);
  SetColor(shader_, "shallow", liquid.shallow);
  SetColor(shader_, "edge", liquid.edge);
  Set(shader_, "height", &liquid.height, SHADER_UNIFORM_FLOAT);
  Set(shader_, "wavelength", &liquid.wavelength, SHADER_UNIFORM_FLOAT);
  Set(shader_, "pace", &liquid.pace, SHADER_UNIFORM_FLOAT);
  Set(shader_, "glint", &liquid.glint, SHADER_UNIFORM_FLOAT);
  Set(shader_, "emission", &liquid.emission, SHADER_UNIFORM_FLOAT);

  // The ground's heights, one float a corner, as a texture the vertex shader
  // can read.
  const int corners = terrain.cells() + 1;
  std::vector<float> heights(corners * corners);
  for (int j = 0; j < corners; ++j) {
    for (int i = 0; i < corners; ++i) {
      heights[j * corners + i] = terrain.Corner(i, j);
    }
  }
  ground_ = LoadTextureFromImage({.data = heights.data(),
                                  .width = corners,
                                  .height = corners,
                                  .mipmaps = 1,
                                  .format = PIXELFORMAT_UNCOMPRESSED_R32});
  // raylib hands the material's textures to the shader as it draws.
  model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = ground_;
  shader_.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader_, "ground");
  const float half_size = terrain.half_size();
  Set(shader_, "halfSize", &half_size, SHADER_UNIFORM_FLOAT);
  Set(shader_, "cell", &Terrain::kCell, SHADER_UNIFORM_FLOAT);
}

// UnloadModel leaves the model's shader and textures alone.
Sea::~Sea() {
  UnloadModel(model_);
  UnloadTexture(ground_);
  UnloadShader(shader_);
}

void Sea::Draw(const Camera3D& camera, const Atmosphere& atmosphere,
               float bands, float seconds) const {
  Set(shader_, "sun", atmosphere.sun.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "sunlight", atmosphere.sunlight.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "ambient", atmosphere.ambient.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "fogColor", atmosphere.sky.data(), SHADER_UNIFORM_VEC3);
  Set(shader_, "fogNear", &atmosphere.fog_near, SHADER_UNIFORM_FLOAT);
  Set(shader_, "fogFar", &atmosphere.fog_far, SHADER_UNIFORM_FLOAT);
  Set(shader_, "bands", &bands, SHADER_UNIFORM_FLOAT);
  Set(shader_, "eye", &camera.position, SHADER_UNIFORM_VEC3);
  Set(shader_, "seconds", &seconds, SHADER_UNIFORM_FLOAT);
  // It follows the camera a whole cell at a time, so that each corner of it
  // stays where a corner of the ground is, and the waves, which go by where
  // they are in the world, don't move with it.
  const float x =
      std::round(camera.position.x / Terrain::kCell) * Terrain::kCell;
  const float z =
      std::round(camera.position.z / Terrain::kCell) * Terrain::kCell;
  DrawMesh(model_.meshes[0], model_.materials[0],
           MatrixTranslate(x, Terrain::kSeaLevel, z));
}

}  // namespace nanommo
