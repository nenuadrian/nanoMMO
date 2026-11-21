add_library(nanommo_client_core
  settings.proto
  window.cc audio.cc jukebox.cc sound.cc synth.cc foley.cc soundscape.cc soundboard.cc
  ambience.cc daylight.cc weather.cc
  settings.cc gui.cc menu.cc renderer.cc
  offscreen.cc postprocess.cc outline.cc
  camera.cc scene.cc input.cc glsl.cc local_connection.cc inbox.cc
  login_screen.cc nameplates.cc combat_text.cc player_frame.cc boss_bar.cc hotbar.cc
  bag_window.cc guild_window.cc trade_window.cc duel_window.cc round_trip.cc prediction.cc
  interpolation.cc
  character_screen.cc
  chat_window.cc talk_window.cc trainer_window.cc quest_window.cc picking.cc
  animator.cc characters.cc rig.cc moves.cc fx.cc reactions.cc telegraphs.cc figures.cc triangles.cc ground.cc trees.cc chunks.cc
  sea.cc sky.cc
  gate.cc
  loading.cc death_screen.cc cube.cc cursor.cc map.cc
  theme.cc compass.cc predicted_bag.cc
)
protobuf_generate(TARGET nanommo_client_core
  IMPORT_DIRS ${PROJECT_SOURCE_DIR} PROTOC_OUT_DIR ${PROJECT_BINARY_DIR}
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/settings.pb.cc
  PROPERTIES COMPILE_OPTIONS -w
)
target_include_directories(nanommo_client_core PUBLIC
  ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR}
)
target_link_libraries(nanommo_client_core PUBLIC
  raylib imgui nanommo_protocol nanommo_content nanommo_inventory nanommo_walking nanommo_terrain
  nanommo_world asio
  absl::memory absl::status absl::statusor absl::strings
)
# The servers are over sockets on a computer, and through the gateway over a
# WebSocket in a browser, which gives a page no sockets.
if(EMSCRIPTEN)
  target_sources(nanommo_client_core PRIVATE servers_web.cc websocket_link.cc)
else()
  target_sources(nanommo_client_core PRIVATE connection.cc servers.cc)
endif()

file(GLOB content_files CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/content/*.txtpb)
add_custom_target(nanommo_client_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/monsters
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/monsters
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/weapons
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/weapons
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/trees
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/trees
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/music
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/music
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/sounds
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sounds
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${PROJECT_SOURCE_DIR}/assets/fonts
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts
  COMMAND ${CMAKE_COMMAND} -E make_directory
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${content_files} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/content
)

add_executable(nanommo_client main.cc)
target_link_libraries(nanommo_client PRIVATE nanommo_client_core)
add_dependencies(nanommo_client nanommo_client_assets)

# In a browser: a page with the game's canvas, on WebGL 2, and the files the
# client reads, preloaded into the page's own file system where it reads them.
# The music is licensed, so it stays out. Each --preload-file is SHELL:, or CMake
# would take the repeated flag for a duplicate and keep the first alone. A
# browser draws a page's frame once its code hands back, and raylib waits for
# the next frame in WindowShouldClose, so Asyncify lets that wait hand back, in
# the middle of our loop, and carry on where it was.
if(EMSCRIPTEN)
  set_target_properties(nanommo_client PROPERTIES SUFFIX ".html")
  set(preloads "")
  foreach(folder content fonts monsters weapons trees)
    list(APPEND preloads
      "SHELL:--preload-file ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${folder}@/${folder}")
  endforeach()
  target_link_options(nanommo_client PRIVATE
    -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
    -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -lwebsocket.js
    -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 ${preloads}
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html
  )
  # The page around the canvas is the game's own (web/shell.html).
  set_property(TARGET nanommo_client APPEND PROPERTY
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
  # The sound player, the benchmark and the tests run on this machine.
  return()
endif()

add_executable(nanommo_sounds sounds_main.cc)
target_link_libraries(nanommo_sounds PRIVATE nanommo_client_core)
add_dependencies(nanommo_sounds nanommo_client_assets)

add_executable(nanommo_client_bench bench_main.cc)
target_link_libraries(nanommo_client_bench PRIVATE nanommo_client_core)
add_dependencies(nanommo_client_bench nanommo_client_assets)

add_executable(nanommo_client_tests
  renderer_test.cc sea_test.cc animator_test.cc characters_test.cc
  moves_test.cc rig_test.cc fx_test.cc telegraphs_test.cc
  figures_test.cc picking_test.cc
  jukebox_test.cc sound_test.cc settings_test.cc bag_window_test.cc
  quest_window_test.cc combat_text_test.cc nameplates_test.cc boss_bar_test.cc
  login_screen_test.cc
  loading_test.cc death_screen_test.cc gate_test.cc map_test.cc
  theme_test.cc compass_test.cc predicted_bag_test.cc chat_window_test.cc
  guild_window_test.cc trade_window_test.cc duel_window_test.cc inbox_test.cc
  round_trip_test.cc prediction_test.cc interpolation_test.cc
  daylight_test.cc ambience_test.cc
)
target_link_libraries(nanommo_client_tests PRIVATE nanommo_client_core GTest::gtest_main)
add_dependencies(nanommo_client_tests nanommo_client_assets)
target_compile_definitions(nanommo_client_tests PRIVATE
  NANOMMO_CONTENT="${PROJECT_SOURCE_DIR}/content"
)

include(GoogleTest)
gtest_discover_tests(nanommo_client_tests)
