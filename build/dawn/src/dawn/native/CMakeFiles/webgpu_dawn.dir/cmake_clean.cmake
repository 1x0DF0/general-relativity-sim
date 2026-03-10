file(REMOVE_RECURSE
  "libwebgpu_dawn.a"
  "libwebgpu_dawn.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/webgpu_dawn.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
