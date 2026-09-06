execute_process(
  COMMAND "${NM}" -u --dynamic "${ELF}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE symbols
  ERROR_VARIABLE errors
)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Cannot audit module imports: ${errors}")
endif()
string(REPLACE "\n" ";" lines "${symbols}")
foreach(line IN LISTS lines)
  if(line MATCHES "zonai_survey|wwpg|totk|_ZN4sead7Color4f11cElement(Min|Max)E")
    message(FATAL_ERROR "Unsupported undefined module symbol: ${line}")
  endif()
endforeach()
