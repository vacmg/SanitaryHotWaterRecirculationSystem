idf_component_register(SRCS "main.cpp"
        "SimpleComms/SimpleComms.cpp"
                    INCLUDE_DIRS "." "./SimpleComms"
)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
        ArduinoStreamUtils
        GIT_REPOSITORY  git@github.com:bblanchon/ArduinoStreamUtils.git
        GIT_TAG         v1.9.1
        PATCH_COMMAND  ${CMAKE_COMMAND} -E touch <SOURCE_DIR>/CMakeLists.txt
)

FetchContent_Populate(ArduinoStreamUtils)
message(NOTICE "Fuente descargada en: ${arduinostreamutils_SOURCE_DIR}")

# Crear un target 'wrapper' que represente ArduinoStreamUtils en este proyecto.
# Usamos un nombre sin guiones para evitar problemas de nombres en CMake.
add_library(arduino_stream_utils INTERFACE)

# Exponer los includes de la fuente descargada y también el include local donde
# está sa_onstack_compat.h para compatibilidad.
target_include_directories(arduino_stream_utils
    INTERFACE
        ${arduinostreamutils_SOURCE_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Enlazar el componente con el wrapper (nombre sin guiones)
target_link_libraries(${COMPONENT_LIB} PUBLIC arduino_stream_utils)
