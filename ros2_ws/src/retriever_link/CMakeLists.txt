cmake_minimum_required(VERSION 3.8)
project(retriever_link)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(retriever_msgs REQUIRED)
find_package(retriever_protocol REQUIRED)

# ---------------------------------------------------------------------------
#  Le cadrage vient du firmware, et c'est voulu.
#
#  Ces trois fichiers C sont compilés ici ET dans le firmware ESP32, depuis le
#  même endroit. Les recopier dans ce paquet serait plus simple à empaqueter et
#  garantirait qu'ils divergent un jour. Le dépôt est un seul projet (§P.2-2) :
#  le chemin relatif est donc légitime.
#
#  Pour construire ce paquet hors du dépôt, passer -DRETRIEVER_FIRMWARE_DIR=...
# ---------------------------------------------------------------------------
set(RETRIEVER_FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware"
    CACHE PATH "Racine du repertoire firmware/ du depot Retriever")

set(PORTABLE_DIR "${RETRIEVER_FIRMWARE_DIR}/components/retriever_link/portable")
if(NOT EXISTS "${PORTABLE_DIR}/rt_framing.c")
  message(FATAL_ERROR
      "Sources de cadrage introuvables dans ${PORTABLE_DIR}.\n"
      "Ce paquet se construit depuis le depot Retriever complet. "
      "Sinon, passer -DRETRIEVER_FIRMWARE_DIR=<chemin>/firmware")
endif()

add_library(retriever_framing STATIC
  "${PORTABLE_DIR}/rt_cobs.c"
  "${PORTABLE_DIR}/rt_crc16.c"
  "${PORTABLE_DIR}/rt_framing.c")
target_include_directories(retriever_framing PUBLIC "${PORTABLE_DIR}")
target_link_libraries(retriever_framing PUBLIC retriever_protocol::retriever_protocol)
set_target_properties(retriever_framing PROPERTIES
  C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
#  Bibliothèque
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED
  src/serial_transport.cpp
  src/socketcan_transport.cpp
  src/imu_conversion.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
target_link_libraries(${PROJECT_NAME} PUBLIC retriever_framing)
ament_target_dependencies(${PROJECT_NAME}
  rclcpp sensor_msgs geometry_msgs retriever_protocol)

# ---------------------------------------------------------------------------
#  Nœud
# ---------------------------------------------------------------------------
add_executable(link_bridge src/bridge_node.cpp)
target_link_libraries(link_bridge ${PROJECT_NAME})
ament_target_dependencies(link_bridge
  rclcpp sensor_msgs geometry_msgs visualization_msgs diagnostic_msgs
  diagnostic_updater tf2 tf2_ros retriever_msgs retriever_protocol)

install(TARGETS ${PROJECT_NAME} retriever_framing
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS link_bridge DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp sensor_msgs geometry_msgs retriever_protocol)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_imu_conversion test/test_imu_conversion.cpp)
  target_link_libraries(test_imu_conversion ${PROJECT_NAME})
  ament_target_dependencies(test_imu_conversion sensor_msgs retriever_protocol)
endif()

ament_package()
