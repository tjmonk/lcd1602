cmake_minimum_required(VERSION 3.10)

include(GNUInstallDirs)

project(lcd1602
	VERSION 0.1
    DESCRIPTION "LCD 16 char x 2 line LCD display service"
)

add_executable( ${PROJECT_NAME}
	src/lcd.c
    src/lcd_ctrl.c
    src/lcd_io.c
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE inc
)

target_link_libraries( ${PROJECT_NAME}
	varserver
)

set_target_properties( ${PROJECT_NAME}
		       PROPERTIES OUTPUT_NAME lcd1602
)

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
