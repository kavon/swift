
add_swift_host_library(swiftAIR STATIC
  AIROps.cpp
  Inflate.cpp
  AIRGenModule.cpp
)
add_dependencies(swiftAIR SwiftAIROpsIncGen)
target_link_libraries(swiftAIR PRIVATE
  swiftAST
  MLIRIR
  MLIRPass
  MLIRFuncDialect
)

set_swift_llvm_is_available(swiftAIR)
