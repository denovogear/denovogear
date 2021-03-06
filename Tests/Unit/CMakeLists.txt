###############################################################################
# AddUnitTest
# A macro that will add a unit test to the testing queue
# Example: AddUnitTest(dng::seq)
#   - creates the `Unit.dng::seq` test  
#   - creates the `unittest_dng_seq` target from the file ./dng/seq.cc
#   - builds the target and runs the right test test-command
#   - the target is linked against Boost::UNIT_TEST_FRAMEWORK and libdng

macro(AddUnitTest NAME)
  string(REPLACE "::" "/" head "${NAME}")
  string(REPLACE "::" "_" exe "${NAME}")
  set(target "unittest_${exe}")
  add_executable("${target}" EXCLUDE_FROM_ALL "${head}.cc")
  # TODO WIN64: add .exe if needed
  set_target_properties("${target}" PROPERTIES OUTPUT_NAME "${head}_test")
  get_filename_component(dir "${head}" DIRECTORY)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${dir}")

  target_include_directories("${target}" BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/src/include")
  target_link_libraries("${target}"
    Boost::unit_test_framework
    libdng
  )

  add_test("Unit.${NAME}" ${CMAKE_CTEST_COMMAND}
    --build-and-test "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
    --build-generator ${CMAKE_GENERATOR}
    --build-nocmake
    --build-noclean
    --build-project "${CMAKE_PROJECT_NAME}" 
    --build-target "${target}" 
    --test-command "${CMAKE_CURRENT_BINARY_DIR}/${head}_test" 
        --log_level=test_suite --report_level=short
  )
endmacro()

###############################################################################
# Add Unit tests based on CXX namespaces

AddUnitTest(hts::hts)
AddUnitTest(hts::bam)
AddUnitTest(hts::bcf)

AddUnitTest(dng::io::bam)
AddUnitTest(dng::io::ped)
AddUnitTest(dng::cigar)
AddUnitTest(dng::genotype)
AddUnitTest(dng::library)
AddUnitTest(dng::probability)
AddUnitTest(dng::multithread)
AddUnitTest(dng::mutation)
AddUnitTest(dng::peel)
AddUnitTest(dng::pedigree)
AddUnitTest(dng::regions)
AddUnitTest(dng::relationship_graph)
AddUnitTest(dng::seq)
AddUnitTest(dng::stats)
AddUnitTest(dng::utility)
