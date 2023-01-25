
set(EXT_ARDUINO_LIB_DIR $ENV{HOME}/Arduino/libraries/)

set(LIB_SDIRS
	"uCRC16Lib/src"
)

foreach(SDIR ${LIB_SDIRS})
	set(LIB_SRC_DIR ${LIB_SRC_DIR} ${EXT_ARDUINO_LIB_DIR}/${SDIR})
endforeach()

#set(LOCAL_LIB_SDIRS
#)

idf_component_register(
	SRC_DIRS
		${LIB_SRC_DIR}
		${LOCAL_LIB_SDIRS}
	INCLUDE_DIRS
		${LIB_SRC_DIR}
		${LOCAL_LIB_SDIRS}
	REQUIRES arduino)
