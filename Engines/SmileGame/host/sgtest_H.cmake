add_executable(sgtest_H sgtest_H.c)
target_link_libraries(sgtest_H PRIVATE smilegame)
add_test(NAME battle_H COMMAND sgtest_H)
set_tests_properties(battle_H PROPERTIES TIMEOUT 1200)
