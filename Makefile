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

.PHONY: run-server4
run-server4:
	./build/hw4/w4_server

.PHONY: run-client4
run-client4:
	./build/hw4/w4

.PHONY: run-server5
run-server5:
	./build/hw5/w5_server

.PHONY: run-client5
run-client5:
	./build/hw5/w5