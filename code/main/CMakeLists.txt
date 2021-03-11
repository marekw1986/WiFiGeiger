#idf_component_register(SRCS "main.c" "geiger.c" "ds3231.c"
#                   INCLUDE_DIRS "")

set(COMPONENT_SRCS "main.c")
set(COMPONENT_SRCS "geiger.c")
set(COMPONENT_SRCS "i2c_master.c")
#set(COMPONENT_SRCS "ds3231.c")

register_component()
