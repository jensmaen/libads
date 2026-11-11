# Notifications with own handles in the local router against the fake router in the same process; no TwinCAT needed.
# libads-client builds the same source for its route way.
add_executable(subscriptions-fake main.cpp)
target_compile_features(subscriptions-fake PRIVATE cxx_std_17)
target_include_directories(subscriptions-fake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../fake-router)
target_link_libraries(subscriptions-fake PRIVATE libads::ads-internal)
target_compile_definitions(subscriptions-fake PRIVATE LIBADS_TEST_LOCAL=1 LIBADS_TEST_ROUTE=0)
if(WIN32)
    target_compile_definitions(subscriptions-fake PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

math(EXPR port "${LIBADS_TEST_PORT} + 10")
add_test(NAME subscriptions-fake COMMAND subscriptions-fake ${port})
set_tests_properties(subscriptions-fake PROPERTIES TIMEOUT 180 LABELS fake-router)
