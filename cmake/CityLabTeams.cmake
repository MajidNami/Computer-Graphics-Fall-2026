# Finds the team folders and builds each one in isolation:
#
#   teams/teamNN/src/*.cpp -> OBJECT library teamNN_zone, linked into citylab
#
# Every team sees only the SDK (citylab/*.h, GLM) and its own src/ folder.
# Both globs are CONFIGURE_DEPENDS: a new team folder or a new .cpp file is
# found by the next build, without configuring by hand.

function(citylab_collect_teams out_objects)
    file(GLOB team_dirs LIST_DIRECTORIES true RELATIVE "${CMAKE_SOURCE_DIR}/teams" CONFIGURE_DEPENDS
         "${CMAKE_SOURCE_DIR}/teams/team[0-9][0-9]")
    list(SORT team_dirs)
    set(objects "")
    set(built "")
    foreach(team IN LISTS team_dirs)
        set(folder "${CMAKE_SOURCE_DIR}/teams/${team}")
        if(NOT IS_DIRECTORY "${folder}")
            continue()
        endif()
        if(NOT CITYLAB_TEAMS STREQUAL "all" AND NOT team IN_LIST CITYLAB_TEAMS)
            continue()
        endif()
        if(team IN_LIST CITYLAB_SKIP_TEAMS)
            message(STATUS "CityLab: skipping ${team} (CITYLAB_SKIP_TEAMS)")
            continue()
        endif()
        file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${folder}/src/*.cpp")
        if(NOT sources)
            message(WARNING "CityLab: ${team} has no .cpp files in src/")
            continue()
        endif()
        add_library(${team}_zone OBJECT ${sources})
        target_include_directories(${team}_zone PRIVATE "${folder}/src")
        target_compile_definitions(${team}_zone PRIVATE CITYLAB_TEAM_ID="${team}")
        target_link_libraries(${team}_zone PRIVATE citylab_sdk citylab_warnings)
        list(APPEND objects $<TARGET_OBJECTS:${team}_zone>)
        list(APPEND built "${team}")
    endforeach()
    if(built)
        list(JOIN built ", " summary)
    else()
        set(summary "none yet (create yours with: python tools/citylab.py new teamNN)")
    endif()
    message(STATUS "CityLab teams: ${summary}")
    set(${out_objects} ${objects} PARENT_SCOPE)
endfunction()
