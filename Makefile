
INC_PATH = -I /usr/local/include/gm -I /user/include
LIB_PATH = -L /usr/local/lib -l gm -l ncurses -l usb-1.0  -l hidapi-libusb

CC = g++-4.9
CFLAGS = -Wall -std=c++11 $(INC_PATH)

SOURCES = main.cpp args.cpp FSO.cpp debug_gm.cpp coarse_align.cpp gm_network_controller.cpp gm_server.cpp tracking.cpp half_auto_align.cpp sfp_auto_align.cpp logger.cpp tracking_analysis.cpp
OBJECTS = $(SOURCES:.cpp=.o)
NAME = gmServer

all: clean $(NAME)

$(NAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIB_PATH) -o $(NAME)

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(NAME) *.o