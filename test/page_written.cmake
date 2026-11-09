# SPDX-License-Identifier: AGPL-3.0-only
# The page as the native program writes it: the display, and every key.
file(MAKE_DIRECTORY "${OUT}")
execute_process(COMMAND "${PROGRAM}" "${OUT}/static.html" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "the program failed: ${code}")
endif()
file(READ "${OUT}/static.html" page)
foreach(shown IN ITEMS "data-view=\"Calculator\"" "data-view=\"Display\"" ">0<" ">AC<" ">%<" ">=<" ">.<" ">( )<" ">⌫<"
                       ">0<" ">1<" ">2<" ">3<" ">4<" ">5<" ">6<" ">7<" ">8<" ">9<" "<title>Calculator</title>")
  string(FIND "${page}" "${shown}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "not on the page: ${shown}")
  endif()
endforeach()
message(STATUS "the page has the display and every key")
