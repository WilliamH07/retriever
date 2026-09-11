cmake_minimum_required(VERSION 3.8)
project(retriever_protocol)

find_package(ament_cmake REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)
# Deux chemins, et le second n'est pas un oubli de style.
#
# rt_framing.h — partagé avec le firmware — fait #include "retriever_protocol.h"
# sans préfixe, parce que côté ESP-IDF le composant expose directement le
# répertoire qui contient le fichier. Pour que le MÊME fichier compile ici, il
# faut exposer aussi le répertoire qui le contient directement. Sans cette
# seconde ligne, toute unité qui inclut transport.hpp échoue à la compilation.
target_include_directories(${PROJECT_NAME} INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/retriever_protocol>"
  "$<INSTALL_INTERFACE:include>"
  "$<INSTALL_INTERFACE:include/retriever_protocol>")
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS ${PROJECT_NAME} EXPORT export_${PROJECT_NAME})

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_include_directories(include)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # Les fichiers générés ne passeront jamais uncrustify et n'ont pas à le faire :
  # ils ne sont pas écrits à la main et ne sont jamais relus comme du code source.
  set(ament_cmake_uncrustify_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
