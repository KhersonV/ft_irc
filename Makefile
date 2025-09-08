CXX      := c++
CXXFLAGS := -std=c++98 -Wall -Wextra -Werror
NAME     := ft_irc

SRC_DIR  := src
INC_DIR  := include
SRC      := $(addprefix $(SRC_DIR)/, main.cpp utils.cpp Cmd_core2.cpp Proto.cpp State.cpp Net.cpp Cmd_pass.cpp Cmd_mode.cpp Cmd_nick.cpp Cmd_user.cpp Cmd_topic.cpp Cmd_join.cpp Cmd_invite.cpp Cmd_quit.cpp)

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
