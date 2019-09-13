# Time Zone Database
# http://www.iana.org/time-zones
#
# Usage:
#
#   list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/res/cmake)
#
#   include(tzdata)
#   tzdata("2019a" ${CMAKE_CURRENT_SOURCE_DIR}/tzdata)
#
#   install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tzdata DESTINATION bin)
#

include_guard(GLOBAL)

function(tzdata version destination)
  if(NOT EXISTS ${destination}/windowsZones.xml)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata/windowsZones.xml)
      message(STATUS "Downloading windowsZones.xml ...")
      file(DOWNLOAD "https://raw.githubusercontent.com/unicode-org/cldr/master/common/supplemental/windowsZones.xml"
        ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata/windowsZones.xml)
    endif()
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata/windowsZones.xml DESTINATION ${destination})
  endif()

  set(tzdata_names
    africa
    antarctica
    asia
    australasia
    backward
    etcetera
    europe
    pacificnew
    northamerica
    southamerica
    systemv
    leapseconds
    version)

  foreach(name ${tzdata_names})
    if(NOT EXISTS ${destination}/${name})
      if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata/${name})
        if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata${version}.tar.gz)
          message(STATUS "Downloading tzdata${version}.tar.gz ...")
          file(DOWNLOAD "https://data.iana.org/time-zones/releases/tzdata${version}.tar.gz"
            ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata${version}.tar.gz)
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E
          tar xf ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata${version}.tar.gz
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata)
      endif()
      file(COPY ${CMAKE_CURRENT_BINARY_DIR}/res/tzdata/${name} DESTINATION ${destination})
    endif()
  endforeach()
endfunction()
