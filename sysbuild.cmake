if(BOARD STREQUAL "nrf5340_audio_dk" AND BOARD_QUALIFIERS STREQUAL "nrf5340/cpuapp")
  set(layout nrf5340)
  set(secondary_board "nrf5340_audio_dk/nrf5340/cpunet")
elseif(BOARD STREQUAL "nrf54l15dk" AND BOARD_QUALIFIERS STREQUAL "nrf54l15/cpuapp")
  set(layout nrf54l15)
  set(secondary_board "nrf54l15dk/nrf54l15/cpuflpr/xip")
else()
  message(FATAL_ERROR
    "Supported targets: nrf5340_audio_dk/nrf5340/cpuapp and nrf54l15dk/nrf54l15/cpuapp")
endif()

set(${DEFAULT_IMAGE}_DTC_OVERLAY_FILE
    "${APP_DIR}/layouts/${layout}_primary.dtsi;${APP_DIR}/layouts/boot.overlay"
    CACHE STRING "" FORCE)
set(${DEFAULT_IMAGE}_EXTRA_CONF_FILE "${APP_DIR}/layouts/${layout}_boot.conf"
    CACHE STRING "" FORCE)

set(image_id 1)
foreach(image app_a app_b app_c app_d)
  if(image_id LESS 3)
    set(image_board "${BOARD}/${BOARD_QUALIFIERS}")
    set(image_layout primary)
    set(image_conf primary)
  else()
    set(image_board "${secondary_board}")
    set(image_layout secondary)
    set(image_conf secondary)
  endif()
  set(${image}_DEBUGGER_IMAGE_ID ${image_id} CACHE STRING "" FORCE)
  set(${image}_DTC_OVERLAY_FILE
      "${APP_DIR}/layouts/${layout}_${image_layout}.dtsi;${APP_DIR}/layouts/${image}.overlay"
      CACHE STRING "" FORCE)
  set(${image}_EXTRA_CONF_FILE "${APP_DIR}/layouts/${layout}_${image_conf}.conf"
      CACHE STRING "" FORCE)
  ExternalZephyrProject_Add(APPLICATION ${image} SOURCE_DIR ${APP_DIR}/images BOARD ${image_board})
  math(EXPR image_id "${image_id} + 1")
endforeach()