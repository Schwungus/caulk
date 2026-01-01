# Only used in GH Actions release workflow.

set(CMAKE_C_COMPILER cosmocc)
set(CMAKE_CXX_COMPILER cosmoc++)
set(CMAKE_AR cosmoar)
set(CMAKE_RANLIB cosmoranlib)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_CROSSCOMPILING OFF)
