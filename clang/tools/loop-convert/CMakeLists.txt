set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(loop-convert
  loop-convert.cpp
  )

target_link_libraries(loop-convert
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
  )

install(TARGETS loop-convert
  RUNTIME DESTINATION bin)
