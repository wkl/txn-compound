add_definitions(
  -D__USE_GNU
  -D_GNU_SOURCE
)

#add_definitions(-std=c++11)

set(tc_posix_SRC
  tc_impl_posix.c
  splice_copy.c
)

include_directories(
  "${GTEST}"
  "${GTEST}/include"
)

include_directories(
  "${GMOCK}"
  "${GMOCK}/include"
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}")

set(test_LIB
  pthread
  gtest
  gtest_main
)

function (add_unittest TestName LibName)
  add_executable(${TestName} "${CMAKE_CURRENT_SOURCE_DIR}/${TestName}.cpp")
  target_link_libraries(${TestName} ${base_LIBS} ${boost_LIBS} ${test_LIB} ${LibName})
  add_test(NAME ${TestName} COMMAND ${TestName})
endfunction (add_unittest)

add_library(tc_impl_posix STATIC ${tc_posix_SRC})
#target_link_libraries(tc_impl_posix log config_parsing)
#set_target_properties(tc_impl_posix PROPERTIES POSITION_INDEPENDENT_CODE on)
