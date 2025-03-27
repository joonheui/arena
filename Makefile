CFLAGS = -fsanitize=address

CREATE = create_random_board
AGENT = my_agent

all: $(CREATE) $(AGENT)

$(CREATE): create_random_board.c
	gcc $< -o $@ $(CFLAGS)

$(AGENT): my_agent.c
	gcc $< -o $@ $(CFLAGS)

run: all
	./$(CREATE) | ./$(AGENT)

clean:
	rm -f $(CREATE) $(AGENT)
