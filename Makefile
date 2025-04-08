CXXFLAGS = -g --std=c++17
INCLUDE = -Ithirdparty/freetype-2.13.3/include -I../
LDFLAGS = -Lthirdparty/freetype-2.13.3/objs/.libs/ -lX11 -lglfw -lvulkan -lfreetype

SRCS := $(wildcard *.cpp)
SRCS := $(filter-out app.cpp, $(SRCS))

HEADERS = $(wildcard *.h)

SHADERS = $(wildcard shaders/*.vert)
SHADERS += $(wildcard shaders/*.frag)

EXAMPLE_SRCS = $(wildcard examples/*.cpp)
EXAMPLE_EXECS := $(EXAMPLE_SRCS:.cpp=)
EXAMPLE_EXECS := $(patsubst examples/%,%,$(EXAMPLE_EXECS))

all: app examples tests

app: shaders $(SRCS) $(HEADERS) $(SHADERS)
	$(CXX) $(CXXFLAGS) -o $@.out $@.cpp $(SRCS) $(INCLUDE) $(LDFLAGS)

examples: $(EXAMPLE_EXECS)

%: examples/%.cpp $(SRCS) $(SHADERS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o examples/$@.out $< $(SRCS) $(INCLUDE) $(LDFLAGS)

TEST_SRCS = $(wildcard tests/*.cpp)
TEST_EXECS := $(TEST_SRCS:.cpp=)
TEST_EXECS := $(patsubst tests/%,%,$(TEST_EXECS))

tests: $(TEST_EXECS)

%: tests/%.cpp $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o tests/$@.out $< $(SRCS) $(INCLUDE) $(LDFLAGS) -Itests/thirdparty/googletest/googletest/include/ -Ltests/thirdparty/ -lgtest -lgtest_main

.PHONY: clean

clean: 
	rm -f *.out
	rm -f examples/*.out
	rm -f tests/*.out

shaders: $(SHADERS)
	glslc shaders/shaderFlatColorQuad.vert -o shaderFlatvs.spv
	xxd -i shaderFlatvs.spv > shaderFlatVS.h
	glslc shaders/shaderFlatColorQuad.frag -o shaderFlatfs.spv
	xxd -i shaderFlatfs.spv > shaderFlatFS.h
	glslc shaders/shaderTexturedQuad.vert -o shaderTexturedvs.spv
	xxd -i shaderTexturedvs.spv > shaderTexturedVS.h
	glslc shaders/shaderTexturedQuad.frag -o shaderTexturedfs.spv
	xxd -i shaderTexturedfs.spv > shaderTexturedFS.h
	glslc shaders/shaderText.frag -o shaderTextfs.spv
	xxd -i shaderTextfs.spv > shaderTextFS.h
	glslc shaders/shaderLineQuad.vert -o shaderLinevs.spv
	xxd -i shaderLinevs.spv > shaderLineVS.h
	glslc shaders/shaderLineQuad.frag -o shaderLinefs.spv
	xxd -i shaderLinefs.spv > shaderLineFS.h
	rm *.spv
