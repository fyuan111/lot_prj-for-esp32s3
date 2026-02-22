file(GLOB_RECURSE LOT_MODULE_SRCS
     "${PROJECT_DIR}/src/SYSTEM/*.c"
     "${PROJECT_DIR}/src/HW/*.c"
     "${PROJECT_DIR}/src/APP/*.c")

set(LOT_LAUNCH_ENTRIES "")
foreach(_src ${LOT_MODULE_SRCS})
    file(READ "${_src}" _content)
    set(_layer_rank "9")
    set(_layer_name "OTHER")
    set(_layer_base "900")
    string(FIND "${_src}" "/src/HW/" _idx_hw)
    string(FIND "${_src}" "/src/SYSTEM/" _idx_sys)
    string(FIND "${_src}" "/src/APP/" _idx_app)
    if(NOT _idx_hw EQUAL -1)
        set(_layer_rank "0")
        set(_layer_name "HW")
        set(_layer_base "0")
    elseif(NOT _idx_sys EQUAL -1)
        set(_layer_rank "1")
        set(_layer_name "SYSTEM")
        set(_layer_base "100")
    elseif(NOT _idx_app EQUAL -1)
        set(_layer_rank "2")
        set(_layer_name "APP")
        set(_layer_base "200")
    endif()

    string(REGEX MATCHALL "launch[ \t\r\n]*\\([ \t\r\n]*[0-9]+[ \t\r\n]*,[ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*\\)"
                          _matches "${_content}")
    foreach(_m ${_matches})
        string(REGEX REPLACE
               "launch[ \t\r\n]*\\([ \t\r\n]*([0-9]+)[ \t\r\n]*,[ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*\\)"
               "\\1|\\2"
               _pair
               "${_m}")
        string(REPLACE "|" ";" _parts "${_pair}")
        list(GET _parts 0 _prio)
        list(GET _parts 1 _func)

        math(EXPR _effective_prio "${_layer_base} + ${_prio}")
        set(_prio_key "${_effective_prio}")
        string(LENGTH "${_prio_key}" _len)
        if(_len EQUAL 1)
            set(_prio_key "000${_prio_key}")
        elseif(_len EQUAL 2)
            set(_prio_key "00${_prio_key}")
        elseif(_len EQUAL 3)
            set(_prio_key "0${_prio_key}")
        endif()

        list(APPEND LOT_LAUNCH_ENTRIES "${_prio_key}|${_func}|${_prio}|${_layer_name}|${_effective_prio}")
    endforeach()
endforeach()

list(SORT LOT_LAUNCH_ENTRIES)
list(REMOVE_DUPLICATES LOT_LAUNCH_ENTRIES)

set(LOT_AUTOGEN_C "${CMAKE_CURRENT_BINARY_DIR}/lot_launch_autogen.c")
set(LOT_AUTOGEN_H "${CMAKE_CURRENT_BINARY_DIR}/lot_launch_autogen.h")

file(WRITE "${LOT_AUTOGEN_H}" "#pragma once\n#include \"esp_err.h\"\nesp_err_t lot_run_all_launches(void);\n")
file(WRITE "${LOT_AUTOGEN_C}" "#include \"esp_err.h\"\n#include \"esp_log.h\"\n\nstatic const char *TAG = \"lot_auto_launch\";\n\n")

foreach(_entry ${LOT_LAUNCH_ENTRIES})
    string(REGEX REPLACE "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$" "\\2" _func "${_entry}")
    file(APPEND "${LOT_AUTOGEN_C}" "extern esp_err_t ${_func}(void);\n")
endforeach()

file(APPEND "${LOT_AUTOGEN_C}" "\nesp_err_t lot_run_all_launches(void)\n{\n    esp_err_t ret = ESP_OK;\n")
foreach(_entry ${LOT_LAUNCH_ENTRIES})
    string(REGEX REPLACE "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$" "\\2" _func "${_entry}")
    string(REGEX REPLACE "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$" "\\3" _prio "${_entry}")
    string(REGEX REPLACE "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$" "\\4" _layer "${_entry}")
    string(REGEX REPLACE "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$" "\\5" _eff "${_entry}")
    file(APPEND "${LOT_AUTOGEN_C}" "    ESP_LOGI(TAG, \"launch project: ====>> ${_func}\");\n    ret = ${_func}();\n    if (ret != ESP_OK) {\n        ESP_LOGE(TAG, \"launch layer=%s local=%s effective=%s func=%s failed: %s\", \"${_layer}\", \"${_prio}\", \"${_eff}\", \"${_func}\", esp_err_to_name(ret));\n        return ret;\n    }\n")
endforeach()
file(APPEND "${LOT_AUTOGEN_C}" "    return ESP_OK;\n}\n")

idf_component_register(SRCS "main.c" "${LOT_AUTOGEN_C}"
                       INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
                       REQUIRES nvs_flash wifi ota lcd lot_lvgl time)
