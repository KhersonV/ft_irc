CXX      := c++
CXXFLAGS := -std=c++98 -Wall -Wextra -Werror
NAME     := ircserv

SRC_DIR  := src
INC_DIR  := include
SRC      := $(addprefix $(SRC_DIR)/, main.cpp utils.cpp Cmd_core2.cpp Proto.cpp State.cpp Net.cpp Cmd_pass.cpp Cmd_mode.cpp Cmd_nick.cpp Cmd_user.cpp Cmd_kick.cpp Cmd_topic.cpp Cmd_join.cpp Cmd_invite.cpp Cmd_quit.cpp Cmd_ping.cpp Cmd_part.cpp Cmd_privmsg.cpp Cmd_names.cpp Bot.cpp)

OBJ      := $(SRC:.cpp=.o)

# ── Optional cURL (OFF by default for 42). Set to 1 to enable (if host machine has curl dep installed, otherwise bot will be basic) ─────────────────────────────────────
WITH_CURL ?= 0 
PKGCFG    ?= pkg-config

ifeq ($(WITH_CURL),1)
  CURL_CFLAGS := $(shell $(PKGCFG) --cflags libcurl 2>/dev/null)
  CURL_LIBS   := $(shell $(PKGCFG) --libs   libcurl 2>/dev/null)
  CXXFLAGS    += -DFTIRC_WITH_CURL $(CURL_CFLAGS)
  LDLIBS      += $(CURL_LIBS)
  # Fallback if pkg-config isn't available:
  ifeq ($(strip $(CURL_LIBS)),)
    LDLIBS += -lcurl
  endif
endif


all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS) 

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
