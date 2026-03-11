
add_swift_host_library(swiftAIR STATIC
  Inflate.cpp
)
target_link_libraries(swiftAIR PRIVATE
  swiftAST
  MLIRIR
  MLIRPass
)

set_swift_llvm_is_available(swiftAIR)
