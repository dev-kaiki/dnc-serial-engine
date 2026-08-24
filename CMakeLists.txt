cmake_minimum_required(VERSION 3.16)
project(dnc_serial_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(DNC_BUILD_TESTS "Build unit tests" ON)

if(WIN32)
    add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601 NOMINMAX)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialPort)

add_library(dnc_core STATIC
    core/config/ConfigurationValidator.cpp
    core/config/MachineConfigRepository.cpp

    core/serial/SerialPortFacade.cpp
    core/serial/PortEnumerator.cpp

    core/dnc/DncEngine.cpp
    core/dnc/DrainController.cpp
    core/dnc/FlowController.cpp
    core/dnc/HandshakeMonitor.cpp
    core/dnc/ProgramSource.cpp
    core/dnc/RxInterpreter.cpp
    core/dnc/StateMachine.cpp
    core/dnc/TxScheduler.cpp
    core/dnc/Watchdog.cpp

    core/diagnostics/CommDiagnostics.cpp
    core/logging/Logger.cpp
    core/simulator/CncSimulator.cpp
    core/storage/HistoryRepository.cpp
    core/storage/SessionTraceRepository.cpp
)

target_include_directories(dnc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dnc_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::SerialPort
)

if(DNC_BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Test)
    if(Qt${QT_VERSION_MAJOR}Test_FOUND)
        enable_testing()
        add_executable(test_configuration_validator tests/unit/test_configuration_validator.cpp)
        target_link_libraries(test_configuration_validator PRIVATE dnc_core Qt${QT_VERSION_MAJOR}::Test)
        add_test(NAME configuration_validator COMMAND test_configuration_validator)
    else()
        message(STATUS "Qt Test not found - skipping unit tests")
    endif()
endif()
