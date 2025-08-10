idf_component_register()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
        ISOTPLib
        GIT_REPOSITORY  git@github.com:purifymywater/ISOTPLib.git
        GIT_TAG         v0.0.3
)
FetchContent_MakeAvailable(ISOTPLib)

target_link_libraries(${COMPONENT_LIB} INTERFACE ISOTPLib)
