.PHONY: build
build:
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make

.PHONY: run-lobby2
run-lobby2:
	./build/hw2/hw2_lobby

.PHONY: run-game2
run-game2:
	./build/hw2/hw2_game

.PHONY: run-client2
run-client2:
	./build/hw2/hw2_client