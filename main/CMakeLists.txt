file(GLOB AWS_IOT_SRCS ../src/aws_iot/*.cpp)
file(GLOB CAMERA_SRCS ../src/camera/*.cpp)
file(GLOB WEBSV_SRCS ../src/websv/*.cpp)

set(COMPONENT_SRCS ${AWS_IOT_SRCS} ${CAMERA_SRCS} ${WEBSV_SRCS} "main.cpp" "Tasks.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "." "../src/aws_iot/" "../src/camera/" "../src/websv/")

register_component()

component_compile_options(-std=c++17)

if(CONFIG_EXAMPLE_EMBEDDED_CERTS)
target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/certificate.pem.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/private.pem.key" TEXT)
endif()