include(FetchContent)
include(CMakeParseArguments)

function(ecewo_define_plugin NAME)
    set(oneValueArgs REPO DEFAULT_VERSION)
    cmake_parse_arguments(P "" "${oneValueArgs}" "" ${ARGN})

    if(NOT P_REPO)
        message(FATAL_ERROR
            "ecewo_define_plugin(${NAME}) requires REPO")
    endif()

    set(ECEWO_PLUGIN_${NAME}_REPO
        ${P_REPO}
        CACHE INTERNAL "ecewo plugin ${NAME} repo"
    )

    if(P_DEFAULT_VERSION)
        set(ECEWO_PLUGIN_${NAME}_DEFAULT_VERSION
            ${P_DEFAULT_VERSION}
            CACHE INTERNAL "ecewo plugin ${NAME} default version"
        )
    endif()
endfunction()

function(ecewo_plugin NAME)
    set(oneValueArgs VERSION REPO)
    cmake_parse_arguments(P "" "${oneValueArgs}" "" ${ARGN})

    if(NOT DEFINED ECEWO_PLUGIN_${NAME}_REPO)
        message(FATAL_ERROR "Unknown ecewo plugin: ${NAME}")
    endif()

    # Repo override
    if(P_REPO)
        set(PLUGIN_REPO ${P_REPO})
    else()
        set(PLUGIN_REPO ${ECEWO_PLUGIN_${NAME}_REPO})
    endif()

    # Version logic
    if(P_VERSION)
        set(PLUGIN_TAG ${P_VERSION})
        message(STATUS "Adding ecewo plugin ${NAME} @ ${PLUGIN_TAG}")
    elseif(DEFINED ECEWO_PLUGIN_${NAME}_DEFAULT_VERSION)
        set(PLUGIN_TAG ${ECEWO_PLUGIN_${NAME}_DEFAULT_VERSION})
        message(STATUS
            "Adding ecewo plugin ${NAME} @ ${PLUGIN_TAG} (default)")
    else()
        set(PLUGIN_TAG main)
        message(WARNING
            "Adding ecewo plugin ${NAME} WITHOUT version pinning. "
            "Using 'main' branch (may be unstable)."
        )
    endif()

    FetchContent_Declare(
        ECEWO_PLUGIN_${NAME}
        GIT_REPOSITORY ${PLUGIN_REPO}
        GIT_TAG ${PLUGIN_TAG}
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(ECEWO_PLUGIN_${NAME})

    if(NOT TARGET ecewo::${NAME})
        message(FATAL_ERROR
            "${NAME} plugin did not define target ecewo::${NAME}")
    endif()
endfunction()

function(ecewo_plugins)
    foreach(entry IN LISTS ARGN)

        string(FIND "${entry}" "@" at_pos)

        if(at_pos GREATER -1)
            string(SUBSTRING "${entry}" 0 ${at_pos} plugin)
            math(EXPR ver_pos "${at_pos} + 1")
            string(SUBSTRING "${entry}" ${ver_pos} -1 version)

            if(version STREQUAL "")
                message(FATAL_ERROR
                    "Invalid plugin spec '${entry}'. "
                    "Expected format: name@version")
            endif()

            ecewo_plugin(${plugin} VERSION ${version})
        else()
            ecewo_plugin(${entry})
        endif()

    endforeach()
endfunction()

include(${CMAKE_CURRENT_LIST_DIR}/registry.cmake)
