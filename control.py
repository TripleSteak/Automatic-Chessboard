from ctypes import *
import ctypes

so_file = "ChessAlgorithm.so"

chess_algorithm = CDLL(so_file)
chess_algorithm.run_chess_algorithm.argtypes = c_char_p,

while True:
	chess_algorithm.init_board()
	chess_algorithm.print_board()

	while chess_algorithm.running:
		print ("White's turn:" if chess_algorithm.get_turn() == chess_algorithm.get_white() else "Black's turn:")
		command = input()
		b_command = command.encode()

		buf = create_string_buffer(128)
		buf.value = b_command
		chess_algorithm.run_chess_algorithm(buf)
	chess_algorithm.reset_game()
