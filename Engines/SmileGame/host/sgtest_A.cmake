add_executable(sgtest_A sgtest_A.c)
target_link_libraries(sgtest_A PRIVATE smilegame)
add_test(NAME commands COMMAND sgtest_A)
set_tests_properties(commands PROPERTIES TIMEOUT 1200)
