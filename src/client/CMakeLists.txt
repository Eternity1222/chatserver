# 定义了一个SRC_LIST变量，包含了当前目录下的所有源文件
aux_source_directory(. SRC_LIST)

# 指定生成可直接行文件
add_executable(chatclient ${SRC_LIST})
# 指定可执行文件链接时需要依赖的库文件
target_link_libraries(chatclient pthread)
