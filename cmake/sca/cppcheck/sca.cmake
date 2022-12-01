find_program(CPPCHECK_TOOL cppcheck)
set(CMAKE_C_CPPCHECK
    ${CPPCHECK_TOOL}
    --enable=warning
    --addon=cert
    --inconclusive
    --force
    --inline-suppr
    CACHE INTERNAL ""
)
