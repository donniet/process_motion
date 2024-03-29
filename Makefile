CXXFLAGS=-std=c++17
LDFLAGS=-lboost_program_options -lboost_thread -lboost_system -lcec -ldl -lX11 -lXext -lXtst -lpthread -lstdc++
SRC_DIRS ?= ./src ./include
TARGET ?= process_motion

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

.PHONY: clean

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)
