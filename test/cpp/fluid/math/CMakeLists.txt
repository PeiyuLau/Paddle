cc_test(
  selected_rows_functor_test
  SRCS selected_rows_functor_test.cc
  DEPS allocator phi)
cc_test(
  im2col_test
  SRCS im2col_test.cc
  DEPS phi)
cc_test(
  vol2col_test
  SRCS vol2col_test.cc
  DEPS phi)
cc_test(
  beam_search_test
  SRCS beam_search_test.cc
  DEPS beam_search)
if(WITH_GPU)
  nv_test(
    selected_rows_functor_gpu_test
    SRCS selected_rows_functor_test.cu.cc
    DEPS phi)
endif()
if(WITH_ROCM)
  hip_test(
    selected_rows_functor_gpu_test
    SRCS selected_rows_functor_test.cu.cc
    DEPS phi)
endif()
cc_test(
  concat_test
  SRCS concat_test.cc
  DEPS concat_and_split)

if(WITH_TESTING AND TEST im2col_test)
  set_tests_properties(im2col_test PROPERTIES TIMEOUT 120)
endif()
