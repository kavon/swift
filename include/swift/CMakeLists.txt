add_subdirectory(Runtime)

if(SWIFT_BUILD_REMOTE_MIRROR)
  add_subdirectory(SwiftRemoteMirror)
endif()

if(SWIFT_INCLUDE_TOOLS)
  configure_file(Config.h.in ${CMAKE_CURRENT_BINARY_DIR}/Config.h
                ESCAPE_QUOTES @ONLY)
  list(APPEND CMAKE_MODULE_PATH "${MLIR_MAIN_SRC_DIR}/cmake/modules")
  include(AddMLIR)
  add_subdirectory(AIR)
  add_subdirectory(AST)
  add_subdirectory(Option)
endif()

