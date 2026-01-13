cmake_minimum_required(VERSION 3.16)
project(github VERSION 1.0.1)

find_package(Albert REQUIRED)
find_package(QCoro6 REQUIRED COMPONENTS Core Network)
find_package(Qt6Keychain REQUIRED)

albert_plugin(
    QT
        Concurrent
        Network
        Widgets
    LINK PRIVATE
        QCoro6::Core
        QCoro6::Network
        $<BUILD_LOCAL_INTERFACE:qt6keychain>
)
