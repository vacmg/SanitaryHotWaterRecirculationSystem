idf_component_register(SRCS "main.cpp"
        "SimpleComms/SimpleComms.cpp"
                    INCLUDE_DIRS "."
        "./SimpleComms"
)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
        ArduinoStreamUtils
        GIT_REPOSITORY  git@github.com:bblanchon/ArduinoStreamUtils.git
        GIT_TAG         v1.9.1
        PATCH_COMMAND  ${CMAKE_COMMAND} -E touch <SOURCE_DIR>/CMakeLists.txt
)

FetchContent_MakeAvailable(ArduinoStreamUtils)

message(WARNING "Fuente descargada en: ${arduinostreamutils_SOURCE_DIR}")
# TODO queda configurar la librería para poder usarla