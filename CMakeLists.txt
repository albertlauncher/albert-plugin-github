cmake_minimum_required(VERSION 3.16)
project(github VERSION 1.0.1)

find_package(Albert REQUIRED)
find_package(QCoro6 REQUIRED COMPONENTS Core Network)

albert_plugin(
    QT Widgets Network
    LINK PRIVATE QCoro6::Core QCoro6::Network
)
