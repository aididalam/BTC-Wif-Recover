# Multi-platform Makefile for recoverwif

TARGET = recoverwif
SRC = recoverwif.cpp

# macOS Build
mac:
	$(CXX) -std=c++17 -Wall -O3 -pthread \
	-I/opt/homebrew/include \
	-I/opt/homebrew/opt/openssl@3/include \
	-L/opt/homebrew/lib \
	-L/opt/homebrew/opt/openssl@3/lib \
	-o $(TARGET) $(SRC) \
	-lsecp256k1 -lssl -lcrypto

# Linux Build
linux:
	$(CXX) -std=c++17 -Wall -O3 -pthread \
	-I/usr/include \
	-I/usr/local/include \
	-L/usr/lib \
	-L/usr/local/lib \
	-o $(TARGET) $(SRC) \
	-lsecp256k1 -lssl -lcrypto

# Windows (MinGW) Build
win:
	$(CXX) -std=c++17 -Wall -O2 -pthread \
	-IC:/OpenSSL/include \
	-IC:/secp256k1/include \
	-LC:/OpenSSL/lib \
	-LC:/secp256k1/lib \
	-o $(TARGET).exe $(SRC) \
	-lsecp256k1 -lssl -lcrypto -lws2_32 -lgdi32

# Clean
clean:
	rm -f recoverwif recoverwif.exe
