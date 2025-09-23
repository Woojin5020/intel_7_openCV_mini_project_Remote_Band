# C++ 컴파일러
CXX := g++

# 실행 파일 이름
TARGET := drum

# 소스/오브젝트
SRCS := drum.cpp
OBJS := $(SRCS:.cpp=.o)

# 옵션
CXXFLAGS := -O2 -Wall -std=c++17
CPPFLAGS := $(shell pkg-config --cflags opencv4)
LDLIBS   := $(shell pkg-config --libs opencv4)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
