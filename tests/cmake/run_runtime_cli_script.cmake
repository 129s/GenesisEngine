if(NOT DEFINED GENESIS_RUNTIME_CLI)
  message(FATAL_ERROR "GENESIS_RUNTIME_CLI is required")
endif()
if(NOT DEFINED SCRIPT_PATH)
  message(FATAL_ERROR "SCRIPT_PATH is required")
endif()
if(NOT DEFINED ROOT_PATH)
  message(FATAL_ERROR "ROOT_PATH is required")
endif()
if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

execute_process(
  COMMAND "${GENESIS_RUNTIME_CLI}" run-script "${SCRIPT_PATH}" --root "${ROOT_PATH}" --max-steps 256 --after-steps 32 --quiet
  RESULT_VARIABLE GENESIS_CLI_EXIT
  OUTPUT_VARIABLE GENESIS_CLI_STDOUT
  ERROR_VARIABLE GENESIS_CLI_STDERR
)

if(NOT GENESIS_CLI_EXIT EQUAL 0)
  message(STATUS "stdout:\n${GENESIS_CLI_STDOUT}")
  message(STATUS "stderr:\n${GENESIS_CLI_STDERR}")
  message(FATAL_ERROR "runtime-cli run-script failed with exit code: ${GENESIS_CLI_EXIT}")
endif()

