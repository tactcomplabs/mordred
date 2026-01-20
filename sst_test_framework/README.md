This folder contains the framework needed to test this element using the standard SST testsuite approach documented [here](https://sst-simulator.org/sst-docs/docs/guides/dev/testframework).

The actual tests are implemented in the `tests` folder in this repo and the CMake/CTest framework is now used for this repo.

To switch back to using the SST framework, the following steps would be necessary:
- The remaining contents of this folder should be moved to the `tests` folder.
- The following line should be added to `tests/CMakeLists.txt` to register the tests with SST:<br>
`install(CODE "execute_process(COMMAND sst-register SST_ELEMENT_TESTS mordred=${CMAKE_CURRENT_SOURCE_DIR})")`
- The testsuite can be executed (after registration): `sst-test-elements -w "*mordred*"`
