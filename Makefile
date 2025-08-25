CXX      := c++
CXXFLAGS := -std=c++98 -Wall -Wextra -Werror
NAME     := ft_irc

SRC_DIR  := src
INC_DIR  := include
SRC      := $(SRC_DIR)/main.cpp $(SRC_DIR)/utils.cpp $(SRC_DIR)/Cmd_core.cpp $(SRC_DIR)/Proto.cpp $(SRC_DIR)/State.cpp $(SRC_DIR)/Net.cpp

OBJ      := $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
