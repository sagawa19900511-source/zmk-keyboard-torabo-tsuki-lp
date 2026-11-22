if(CONFIG_SHIELD_TORABO_TSUKI_LP_LEFT OR CONFIG_SHIELD_TORABO_TSUKI_LP_RIGHT)
  zephyr_library()
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_library_sources(src/board.c src/mini_trackpad_init_reg.c)
endif()

if(CONFIG_SHIELD_TORABO_TSUKI_LP_LEFT AND CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources(src/mini_scroll_inertia.c src/ball_scroll_inertia.c)
endif()

if(CONFIG_SHIELD_TORABO_TSUKI_LP_LEFT AND CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  # ZMK v0.3 dispatches subscriptions in link order. Run after hold-tap/combo
  # replay, before keymap snapshots the layer state for this press and release.
  function(torabo_add_aml_before_keymap)
    get_target_property(sources app SOURCES)
    list(FIND sources "src/keymap.c" keymap_index)
    if(keymap_index LESS 0)
      message(FATAL_ERROR "Cannot place Torabo AML before ZMK keymap dispatch")
    endif()
    list(INSERT sources ${keymap_index} "${TORABO_AML_SOURCE}")
    set_property(TARGET app PROPERTY SOURCES "${sources}")
  endfunction()
  set(TORABO_AML_SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/ball_aml.c"
      CACHE INTERNAL "Torabo AML source for deferred app registration")
  cmake_language(DEFER DIRECTORY "${APPLICATION_SOURCE_DIR}" CALL torabo_add_aml_before_keymap)
endif()
