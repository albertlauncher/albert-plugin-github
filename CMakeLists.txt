cmake_minimum_required(VERSION 3.16)
project(github VERSION 1.0.1)

find_package(Albert REQUIRED)

albert_plugin(QT Widgets Network)
