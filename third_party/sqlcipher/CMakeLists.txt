cmake_minimum_required(VERSION 3.16)
project(sqlcipher LANGUAGES C)

add_library(sqlcipher_static STATIC sqlite3.c)
target_include_directories(sqlcipher_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(sqlcipher_static PUBLIC
    SQLITE_HAS_CODEC
    SQLCIPHER_CRYPTO_OPENSSL
    SQLITE_TEMP_STORE=2
)

target_link_libraries(sqlcipher_static PUBLIC OpenSSL::Crypto)
