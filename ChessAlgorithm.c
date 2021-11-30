#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define BOARD_SIZE 10
#define BOARD_START 1

// piece IDs
const int pawn = 0;
const int knight = 1;
const int bishop = 2;
const int rook = 3;
const int queen = 4;
const int king = 5;

// colours
const int white = 0;
const int black = 1;

struct piece {
    int piece_id;
    int colour;
    char letter;
};

const struct piece white_pawn = {pawn, white, 'P'};
const struct piece white_knight = {knight, white, 'N'};
const struct piece white_bishop = {bishop, white, 'B'};
const struct piece white_rook = {rook, white, 'R'};
const struct piece white_queen = {queen, white, 'Q'};
const struct piece white_king = {king, white, 'K'};
const struct piece black_pawn = {pawn, black, 'P'};
const struct piece black_knight = {knight, black, 'N'};
const struct piece black_bishop = {bishop, black, 'B'};
const struct piece black_rook = {rook, black, 'R'};
const struct piece black_queen = {queen, black, 'Q'};
const struct piece black_king = {king, black, 'K'};
const struct piece null = {-1, -1};

// promoted pieces
const struct piece white_knight_p = {knight, white, 'N'};
const struct piece white_bishop_p = {bishop, white, 'B'};
const struct piece white_rook_p = {rook, white, 'R'};
const struct piece white_queen_p = {queen, white, 'Q'};
const struct piece black_knight_p = {knight, black, 'N'};
const struct piece black_bishop_p = {bishop, black, 'B'};
const struct piece black_rook_p = {rook, black, 'R'};
const struct piece black_queen_p = {queen, black, 'Q'};

// verbal replacements for letters (because b and d are confusing)
const char *a_code = "apple";
const char *b_code = "banana";
const char *c_code = "cash";
const char *d_code = "donut";
const char *e_code = "eggplant";
const char *f_code = "falafel";
const char *g_code = "garlic";
const char *h_code = "hazelnut";

// where are the kings (0-8)?
int kingRow [2];
int kingCol [2];

// which column is available for en passant?
int enPassantCol [2];

// castling requirements (have these pieces moved?)
bool kingMoved [2];
bool aRookMoved [2];
bool hRookMoved [2];

// motor position:
// (0,0) represents the A1 corner of the ENTIRE board
// (13, 13) represents the H8 corner of the ENTIRE board
// values of .5 are tile centres
float motorRow = 0, motorCol = 0;

// counter, reset every time a pawn is moved or a piece is captured (at 0 will force draw)
// (50 move rule), though 100 total since it's 50 moves for each side
int movesTillDraw = 100;

// game variables
int turn; // whose turn is it? white/black
bool running; // whether the game is running
bool is_running() { return running; }

/*
* Collection of functions for storing and retrieving motor/magnet commands
*/
int magnet_toggle = 0;
int x_motor_axis = 1;
int y_motor_axis = 2;
int both_motor_axis = 3;

float motor_overflow = 0.45f; // how much further to go than the required distance, to "center the piece"

struct next_command {
    int command_type; // 0 -- magnet toggle, 1 -- double motor axis, 2 -- single motor axis, 3 -- both motors at once
    int i1;
    float f1;
    float f2;
};
struct next_command command_queue [24]; // fixed allocation
int num_commands = 0; // number of commands

bool has_commands() {
    return num_commands > 0;
}

int get_command_type() {
    return command_queue[0].command_type;
}

void go_next_command() { // deletes the top element in the linked list
    for(int i = 0; i < num_commands - 1; i++) command_queue[i] = command_queue[i + 1]; // shift everything up
    num_commands--;
}

int get_int_command_value() { // also discards
    int value = command_queue[0].i1;
    go_next_command();
    return value;
}

float get_float_command_value_a() {
    float value = command_queue[0].f1;
    return value;
}

float get_float_command_value_b() { // also discards
    float value = command_queue[0].f2;
    go_next_command();
    return value;
}

void queue_command(int type, int i1, float f1, float f2) { // adds to queue
    if(num_commands >= 24) return;
    printf("%f %f\n", f1, f2);
    struct next_command q_command = {type, i1, f1, f2};
    command_queue[num_commands] = q_command;
    num_commands++;
}

/*
* narration tools
*/
char *narration; // message to say
void set_tts(char *text) { narration = text; } // set the narration

char *get_tts() {
    if(narration == NULL) return "";
    char *returnValue = narration;
    narration = NULL;
    return returnValue;
}

void print_tts_message(char *message) {
    narration = message;
    printf("[MESSAGE] %s\n", message);
}

char promote_letter = 'q'; // if promotion happens, use this (default queen)

struct piece *board [BOARD_SIZE][BOARD_SIZE]; // row-first -> in chess notation, [A-H][1-8]
struct piece *board_clone [BOARD_SIZE][BOARD_SIZE];

void clone_board() { // used for motor pathfinding, since the virtual board will be updated but not the actual board
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            board_clone[i][j] = board[i][j];
        }
    }
}

int other_colour(int colour) { return colour == white ? black : white; }
bool piece_equal(struct piece *a, const struct piece *b) { return a->piece_id == b->piece_id && a->colour == b->colour; }
bool is_piece_letter(char c) { return (c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k'); }
bool is_rank(char c) { return ((c >= 'a' && c <= 'h') || c == '$'); } // the "$" means check all rows/files
bool is_file(char c) { return ((c >= '1' && c <= '8') || c == '$'); }
int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }
int get_turn() { return turn; }
int get_white() { return white; }

char *min_pointer(char *a, char *b) {
    if(a == NULL && b == NULL) return NULL;
    if(a == NULL) return b;
    if(b == NULL) return a;
    return a < b ? a : b;
}

bool prefix(char *pre, char *str) {
    if(pre == NULL || str == NULL) return false;
    return strncmp(pre, str, strlen(pre)) == 0;
}

// determines where the king are on the board, and updates the local variables
void find_kings() {
    for(int row = 0; row < 8; row++) {
        for(int col = 0; col < 8; col++) {
            if(board[BOARD_START + row][BOARD_START + col]->piece_id == king) {
                if(board[BOARD_START + row][BOARD_START + col]->colour == white) {
                    kingRow[0] = row;
                    kingCol[0] = col;
                } else {
                    kingRow[1] = row;
                    kingCol[1] = col;
                }
            }
        }
    }
}

// not the same as resetting the board, assumes that pieces are already placed in the correct positions
void init_board() {
    for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) board[i][j] = &null;
    for(int i = BOARD_START; i < BOARD_START + 8; i++) {
        board[BOARD_START + 1][i] = &white_pawn;
        board[BOARD_START + 6][i] = &black_pawn;
    }

    board[BOARD_START][BOARD_START] = board[BOARD_START][BOARD_START + 7] = &white_rook;
    board[BOARD_START][BOARD_START + 1] = board[BOARD_START][BOARD_START + 6] = &white_knight;
    board[BOARD_START][BOARD_START + 2] = board[BOARD_START][BOARD_START + 5] = &white_bishop;
    board[BOARD_START][BOARD_START + 3] = &white_queen;
    board[BOARD_START][BOARD_START + 4] = &white_king;

    board[BOARD_START + 7][BOARD_START] = board[BOARD_START + 7][BOARD_START + 7] = &black_rook;
    board[BOARD_START + 7][BOARD_START + 1] = board[BOARD_START + 7][BOARD_START + 6] = &black_knight;
    board[BOARD_START + 7][BOARD_START + 2] = board[BOARD_START + 7][BOARD_START + 5] = &black_bishop;
    board[BOARD_START + 7][BOARD_START + 3] = &black_queen;
    board[BOARD_START + 7][BOARD_START + 4] = &black_king;

    find_kings();
    enPassantCol[0] = enPassantCol[1] = -1; // -1 for none
    kingMoved[0] = kingMoved[1] = aRookMoved[0] = aRookMoved[1] = hRookMoved[0] = hRookMoved[1] = false;

    turn = white; // white starts
    running = true; // start the game
}

// lowercase for white, capital for black
// "onBoard" means whether the printed piece is on the actual 8x8 game area, as opposed to off to the side
char print_piece(struct piece *p, bool onBoard) {
    if(piece_equal(p, &null)) return onBoard ? '_' : '.';

    int diff = p->colour == white ? 32 : 0; // added to white pieces to change to uppercase
    return p->letter + diff;
}

// for debugging algorithm, not relevant in final program
void print_board() {
    printf("[BOARD]\n");
    for(int i = BOARD_SIZE - 1; i >= 0; i--) {
        printf("[BOARD]     ");
        for(int j = 0; j < BOARD_SIZE; j++) printf("%c ", print_piece(board[i][j], i >= BOARD_START && i < BOARD_START + 8 && j >= BOARD_START && j < BOARD_START + 8));
        printf("\n");
    }
    printf("[BOARD]\n");
}

void print_clone_board() {
    for(int i = BOARD_SIZE - 1; i >= 0; i--) {
        printf("[CLONE]     ");
        for(int j = 0; j < BOARD_SIZE; j++) printf("%c ", print_piece(board_clone[i][j], i >= BOARD_START && i < BOARD_START + 8 && j >= BOARD_START && j < BOARD_START + 8));
        printf("\n");
    }
}

// returns the first empty spot on the chess board (useful for deciding where to put captured pieces)
// "white" determines if we start searching for positions from white's side
int first_empty_spot(bool white) {
    for(int row = 0; row < BOARD_SIZE; row++) for(int col = 0; col < BOARD_SIZE; col++) {
        //printf("piece at (%d, %d) is %d\n", white ? row : BOARD_SIZE - row - 1, col, board[white ? row : BOARD_SIZE - row - 1][col]->piece_id);
        if(piece_equal(board[white ? row : BOARD_SIZE - row - 1][col], &null)) return (white ? row : BOARD_SIZE - row - 1) * BOARD_SIZE + col; // allows the position to be returned as one integer
    }
}

// pre: parsed represents the char array to fill with the translated content
//      input represents the raw input from the user
//
// *for simplicity's sake, we're going to force the player to say the word "pawn" before moving a pawn
//
// AVOID USING THE WORD "TO" AS A CONJUNCTION!
void understand(char *parsed, char *input) {
    // turn everything to lowercase
    for(int i = 0; i < strlen(input); i++) {
        if(input[i] >= 'A' & input[i] <= 'Z') input[i] += 32;
    }

    //strcpy(parsed, input);
    char *pieceToMove = min_pointer(strstr(input, "pawn"), strstr(input, "pine"));
    pieceToMove = min_pointer(strstr(input, "pond"), pieceToMove); // point to the keyword that represents the first piece mentioned in the input
    pieceToMove = min_pointer(strstr(input, "night"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "horse"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "bishop"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "rook"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "queen"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "king"), pieceToMove); // don't say more than one piece lol

    char *castle = strstr(input, "castle"); // point toward keyword that represents whether we should castle
    if(castle != NULL) { // castling!
        if(prefix("queen", pieceToMove)) strcpy(parsed, "o-o-o");
        else if(prefix("king", pieceToMove)) strcpy(parsed, "o-o");
    } else if (pieceToMove != NULL) {
        int letterIndices [2] = {-1, -1}; // search for file commands, e.g. "alpha"
        int numberIndices [2] = {-1, -1}; // EDITED: instead of giving index, now just holds the actual number
        int letterIndexCnt = 0, numberIndexCnt = 0, totalIndexCnt = 2; // keep track of which array slot to fill
        for(int i = 0; i < strlen(input); i++) {
            if(prefix(a_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(b_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(c_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(d_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(e_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(f_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(g_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(h_code, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;

            else if(numberIndexCnt < totalIndexCnt && (prefix("1", input + i) || prefix("one", input + i) || prefix("won", input + i))) numberIndices[numberIndexCnt++] = 1;
            else if(numberIndexCnt < totalIndexCnt && (prefix("2", input + i) || prefix("two", input + i) || prefix("too", input + i) || prefix("to", input + i))) numberIndices[numberIndexCnt++] = 2;
            else if(numberIndexCnt < totalIndexCnt && (prefix("3", input + i) || prefix("three", input + i))) numberIndices[numberIndexCnt++] = 3;
            else if(numberIndexCnt < totalIndexCnt && (prefix("4", input + i) || prefix("four", input + i) || prefix("for", input + i))) numberIndices[numberIndexCnt++] = 4;
            else if(numberIndexCnt < totalIndexCnt && (prefix("5", input + i) || prefix("five", input + i))) numberIndices[numberIndexCnt++] = 5;
            else if(numberIndexCnt < totalIndexCnt && (prefix("6", input + i) || prefix("six", input + i))) numberIndices[numberIndexCnt++] = 6;
            else if(numberIndexCnt < totalIndexCnt && (prefix("7", input + i) || prefix("seven", input + i))) numberIndices[numberIndexCnt++] = 7;
            else if(numberIndexCnt < totalIndexCnt && (prefix("8", input + i) || prefix("eight", input + i) || prefix("ate", input + i))) numberIndices[numberIndexCnt++] = 8;
        }

        //printf("%d %d %d %d\n", letterIndices[0], letterIndices[1], numberIndices[0], numberIndices[1]);

        // update parsed char
        if(letterIndexCnt > 0 && numberIndexCnt > 0) {
            char newParsed [6];
            newParsed[0] = (prefix("horse", pieceToMove) ? 'n' : pieceToMove[0]);
            newParsed[1] = letterIndexCnt > 1 ? input[letterIndices[0]] : '$';
            newParsed[2] = numberIndexCnt > 1 ? numberIndices[0] + '0' : '$';
            newParsed[3] = letterIndexCnt > 1 ? input[letterIndices[1]] : input[letterIndices[0]];
            newParsed[4] = numberIndexCnt > 1 ? numberIndices[1] + '0' : numberIndices[0] + '0';
            newParsed[5] = '\0';
            strcpy(parsed, newParsed);

            if(newParsed[0] == 'p') { // change promotion piece
                if(strstr(input, "queen") != NULL) promote_letter = 'q';
                else if(strstr(input, "rook") != NULL) promote_letter = 'r';
                else if(strstr(input, "bishop") != NULL) promote_letter = 'b';
                else if(strstr(input, "night") != NULL || strstr(input, "horse") != NULL) promote_letter = 'n';
            }
        } else strcpy(parsed, "");
    } else strcpy(parsed, "");
}

// move inputs must be grammatically valid (no capitals!):
// for now, that means ra1h8 (rook from a1 to h8, we ignore captures for now)
//
// TODO: convert all possible input into this form (e.g. from axb3 => pa2b3)
bool validate_input(char *input) {
    if(!strcmp(input, "o-o") || !strcmp(input, "o-o-o")) return true; // castling bypasses the required notation
    bool flag;
    if(strlen(input) < 5) flag = false;
    else flag = is_piece_letter(input[0]) && is_rank(input[1]) && is_file(input[2]) && is_rank(input[3]) && is_file(input[4]);
    if(!flag) print_tts_message("Invalid input. Try again. ");
    return flag;
}

// this method checks whether chess allows the move
//      things to add later, don't forget: en passant, castling, promotion, 3 fold repetition, 50 move draw
//
// pre: rows and cols given WITHOUT BOARD_START added
bool legal_move(int srcRow, int srcCol, int destRow, int destCol) {
    struct piece *srcPiece = board[BOARD_START + srcRow][BOARD_START + srcCol];
    struct piece *destPiece = board[BOARD_START + destRow][BOARD_START + destCol];

    if(destPiece->colour == srcPiece->colour || (srcRow == destRow && srcCol == destCol)) return false; // same colour or same tile
    bool isCapture = !piece_equal(destPiece, &null); // whether this move is considered a capture

    if(srcPiece->piece_id == pawn) {
        if(enPassantCol[other_colour(srcPiece->colour)] == destCol && abs(srcCol - destCol) == 1 && ((srcPiece->colour == white && destRow - srcRow == 1)
            || (srcPiece->colour == black && srcRow - destRow == 1)) && !isCapture && board[BOARD_START + srcRow][BOARD_START + destCol]->piece_id == pawn) {
            return true;
        } // en passant
        if(srcCol == destCol && !isCapture) { // straight motion, no capture
            if((srcPiece->colour == white && destRow <= srcRow) || (srcPiece->colour == black && destRow >= srcRow)) return false; // no moving backwards

            int distMoved = abs(srcRow - destRow);
            if(distMoved == 2 && ((srcPiece->colour == white && srcRow == 1) || (srcPiece->colour == black && srcRow == 6))) return true; // two square pawn advance from start
            else if(distMoved == 1) return true; // one square advance
        } else if(abs(srcCol - destCol) == 1) { // diagonal capture
            if(((srcPiece->colour == white && destRow - srcRow == 1) || (srcPiece->colour == black && srcRow - destRow == 1)) && isCapture) return true; // diagonal capture on pawn
        }
        return false;
    }

    int deltaRow = abs(srcRow - destRow);
    int deltaCol = abs(srcCol - destCol);
    // non-pawn pieces
    if(srcPiece->piece_id == knight) { // 2 in one direction, 1 perpendicular
        if((deltaRow == 2 && deltaCol == 1) || (deltaRow == 1 && deltaCol == 2)) return true;
    }

    if(srcPiece->piece_id == bishop || srcPiece->piece_id == queen) { // diagonal movement
        if(deltaRow == deltaCol) {
            int smallerRow = min(srcRow, destRow) + 1;
            int largerRow = max(srcRow, destRow) - 1;
            int smallerCol = min(srcCol, destCol) + 1;
            int largerCol = max(srcCol, destCol) - 1;

            if((destRow > srcRow && destCol > srcCol) || (destRow < srcRow && destCol < srcCol)) { // bottom-left to top-right diagonal
                for(int i = 0; i + smallerCol <= largerCol; i++) {
                    if(!piece_equal(board[BOARD_START + smallerRow + i][BOARD_START + smallerCol + i], &null)) return false; // obstruction
                }
            } else for(int i = 0; i + smallerCol <= largerCol; i++) {
                if(!piece_equal(board[BOARD_START + smallerRow + i][BOARD_START + largerCol - i], &null)) return false; // obstruction
            }
            return true;
        }
    }

    if(srcPiece->piece_id == rook || srcPiece->piece_id == queen) { // parallel to axis movement
        if(deltaRow == 0) {
            int smallerCol = min(srcCol, destCol) + 1;
            int largerCol = max(srcCol, destCol) - 1;

            for(int col = smallerCol; col <= largerCol; col++) {
                if(!piece_equal(board[BOARD_START + srcRow][BOARD_START + col], &null)) return false;
            }
            return true;
        } else if (deltaCol == 0) {
            int smallerRow = min(srcRow, destRow) + 1;
            int largerRow = max(srcRow, destRow) - 1;

            for(int row = smallerRow; row <= largerRow; row++) {
                if(!piece_equal(board[BOARD_START + row][BOARD_START + srcCol], &null)) return false;
            }
            return true;
        }
    }

    if(srcPiece->piece_id == king) { // king moves like queen, but just one tile (TODO: don't allow moving into check)
        if(((deltaRow == 0 || deltaCol == 0) || (deltaRow == deltaCol)) && (deltaRow <= 1 && deltaCol <= 1)) return true;
    }

    return false;
}

// colour represents the colour BEING attacked
// row and col given WITHOUT "BOARD_START" added
bool tile_attacked(int row, int col, int colour) {
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            if(board[BOARD_START + i][BOARD_START + j]->colour != colour && legal_move(i, j, row, col)) return true;
        }
    }
    return false;
}

// whether castling onto this side is legal
bool legal_castle_queenside(int colour) {
    int targetFile = (colour == white ? 0 : 7);
    if(!(tile_attacked(targetFile, 2, colour) || tile_attacked(targetFile, 3, colour) || tile_attacked(targetFile, 4, colour))) { // tiles king pass through can't be attacked to castle
        if(!(aRookMoved[colour] || kingMoved[colour])) { // pieces already moved
            if(piece_equal(board[BOARD_START + targetFile][BOARD_START + 1], &null) && piece_equal(board[BOARD_START + targetFile][BOARD_START + 2], &null) && piece_equal(board[BOARD_START + targetFile][BOARD_START + 3], &null)) return true; // no obstructing pieces
        }
    }
    print_tts_message("Can't castle now.\n");
    return false; // obstructing pieces
}

bool legal_castle_kingside(int colour) {
    int targetFile = (colour == white ? 0 : 7);
    if(!(tile_attacked(targetFile, 4, colour) || tile_attacked(targetFile, 5, colour) || tile_attacked(targetFile, 6, colour))) { // tiles king pass through can't be attacked to castle
        if(!(hRookMoved[colour] || kingMoved[colour])) { // pieces already moved
            if(piece_equal(board[BOARD_START + targetFile][BOARD_START + 5], &null) && piece_equal(board[BOARD_START + targetFile][BOARD_START + 6], &null)) return true; // no obstructing pieces
        }
    }
    print_tts_message("Can't castle now.\n");
    return false; // obstructing pieces
}

// see validate_input() for details (format should be "Ra1h3" etc)
// this method ensures that notation is correct (identified the correct piece, etc.)
bool validate_move(char *input, int turn) {
    // handle castling separately
    if(!strcmp(input, "o-o")) return legal_castle_kingside(turn);
    else if(!strcmp(input, "o-o-o")) return legal_castle_queenside(turn);

    //printf("testing %s\n", input);
    if(input[1] == '$') { // variable file
        for(int i = 0; i < 8; i++) { // test all files
            input[1] = 'a' + i;
            if(validate_move(input, turn)) return true;
        }
        return false;
    } else if (input[2] == '$') { // variable rank, file fixed
        for(int i = 0; i < 8; i++) { // test all ranks
            input[2] = '1' + i;
            if(validate_move(input, turn)) return true;
        }
        input[2] = '$'; // reset, in case further files need the rank iteration again
        return false;
    }

    struct piece *toMove = board[BOARD_START + input[2] - '1'][BOARD_START + input[1] - 'a'];
    if(toMove->colour == turn && toMove->letter + 32 == input[0] && legal_move(input[2] - '1', input[1] - 'a', input[4] - '1', input[3] - 'a')) return true;
    return false;
}

// returns true if the given colour's king is currently under threat
bool under_check(int colour) {
    return tile_attacked(kingRow[colour], kingCol[colour], colour);
}

// given the current board state, determines if the given colour has a valid move
bool has_valid_move(int colour) {
    bool moveAvailable = false; // whether a valid move exists
    bool continueCheckmateCheck = true; // whether to keep going with the following gigaloop

    // disgusting loop
    for(int srcRow = 0; srcRow < 8 && continueCheckmateCheck; srcRow++) {
        for(int srcCol = 0; srcCol < 8 && continueCheckmateCheck; srcCol++) {
            for(int destRow = 0; destRow < 8 && continueCheckmateCheck; destRow++) {
                for(int destCol = 0; destCol < 8 && continueCheckmateCheck; destCol++) {
                    if(srcRow == destRow && srcCol == destCol) continue; // same tile
                    if(board[BOARD_START + srcRow][BOARD_START + srcCol]->colour != colour) continue; // opponent piece or open tile

                    if(legal_move(srcRow, srcCol, destRow, destCol)) { // legal move, begin simulation
                        struct piece *dest = board[BOARD_START + destRow][BOARD_START + destCol];
                        struct piece *src = board[BOARD_START + srcRow][BOARD_START + srcCol];

                        board[BOARD_START + destRow][BOARD_START + destCol] = src;
                        board[BOARD_START + srcRow][BOARD_START + srcCol] = &null;
                        find_kings();

                        if(!under_check(colour)) { // this move does not result in staying/getting checked, so it is valid
                            moveAvailable = true;
                            continueCheckmateCheck = false;
                            //printf("escape move found at %c%d -> %c%d\n", srcCol + 'A', srcRow + 1, destCol + 'A', destRow + 1);
                        }

                        board[BOARD_START + destRow][BOARD_START + destCol] = dest;
                        board[BOARD_START + srcRow][BOARD_START + srcCol] = src;
                        find_kings();
                    }
                }
            }
        }
    }

    return moveAvailable;
}

// motor will move the given distance ACROSS rows (UNUSED)
void motor_move_row(float delta) {
    motorRow += delta;
    queue_command(x_motor_axis, 0, 0, delta);
    printf("[DEBUG] $MOTOR$ moved in Y: %f tiles\n", delta);
}

// motor will move the given distance ALONG rows (UNUSED)
void motor_move_col(float delta) {
    motorCol += delta;
    queue_command(y_motor_axis, 0, 0, delta);
    printf("[DEBUG] $MOTOR$ moved in X: %f tiles\n", delta);
}

void motor_move_both(float deltaX, float deltaY, bool withOverflow) {
    motorRow += deltaX;
    motorCol += deltaY;

    /*if(withOverflow) {
    float distNoOverflow = sqrt(deltaX * deltaX + deltaY * deltaY);
    float overflowPercent = (distNoOverflow + motor_overflow) / distNoOverflow - 1.0f; // should be a low percentage (0 - 30%)?

    float motorOverflowX = overflowPercent * deltaX;
    float motorOverflowY = overflowPercent * deltaY;
    queue_command(both_motor_axis, 0, deltaX + motorOverflowX, deltaY + motorOverflowY);
    queue_command(both_motor_axis, 0, -motorOverflowX, -motorOverflowY);
    //printf("%f %f\n", (deltaX + motorOverflowX), (deltaY + motorOverflowY));

    } elseI*/ queue_command(both_motor_axis, 0, deltaX, deltaY);

    printf("[DEBUG] $MOTOR$ moved in two axes: %f, %f tiles\n", deltaX, deltaY);
}

// turns the magnet on/off
void toggle_magnet(bool state) {
    int toggle = state ? 1 : 0;
    queue_command(magnet_toggle, toggle, 0, 0);
    printf(state ? "[DEBUG] $MAGNET$: ON\n" : "[DEBUG] $MAGNET$: OFF\n");
}

// figure out all the tiles that can be reached from this tile
void initial_spread(int *array, int startRow, int startCol) {
    if(startRow < 0 || startCol < 0 || startRow >= BOARD_SIZE || startCol >= BOARD_SIZE) return; // out of bounds
    if(array[startRow * BOARD_SIZE + startCol] != 0) return; // already checked

    array[startRow * BOARD_SIZE + startCol] = 1;
    // check traversable tiles
    if(startRow > 0 && piece_equal(board_clone[startRow - 1][startCol], &null)) initial_spread(array, startRow - 1, startCol);
    if(startRow < BOARD_SIZE - 1 && piece_equal(board_clone[startRow + 1][startCol], &null)) initial_spread(array, startRow + 1, startCol);
    if(startCol > 0 && piece_equal(board_clone[startRow][startCol - 1], &null)) initial_spread(array, startRow, startCol - 1);
    if(startCol < BOARD_SIZE - 1 && piece_equal(board_clone[startRow][startCol + 1], &null)) initial_spread(array, startRow, startCol + 1);
}

// spreads the juice of this tile (denoted by n) to all adjacent empty tiles
// avoidRow and avoidCol represent the starting piece, which must be kept at 0
void spread_juice(int *array, int startRow, int startCol, int n) {
    if(startRow < 0 || startCol < 0 || startRow >= BOARD_SIZE || startCol >= BOARD_SIZE) return; // out of bounds
    if(array[startRow * BOARD_SIZE + startCol] != 0) return; // already visited

    array[startRow * BOARD_SIZE + startCol] = n; // this tile can be reached from the original start tile, so n remains constant

    if(startRow > 0 && piece_equal(board_clone[startRow - 1][startCol], &null)) spread_juice(array, startRow - 1, startCol, n);
    if(startRow < BOARD_SIZE - 1 && piece_equal(board_clone[startRow + 1][startCol], &null)) spread_juice(array, startRow + 1, startCol, n);
    if(startCol > 0 && piece_equal(board_clone[startRow][startCol - 1], &null)) spread_juice(array, startRow, startCol - 1, n);
    if(startCol < BOARD_SIZE - 1 && piece_equal(board_clone[startRow][startCol + 1], &null)) spread_juice(array, startRow, startCol + 1, n);
}

// once we know which tiles can be accessed immediately, populate remaining tiles with higher integers
//  thus, the int on each tile minus 1 = the number of pieces needed to be moved out of the way to get here
//  the next number to work with (e.g. finding tiles that should be n=2)
void further_spread(int *array, int n, int startRow, int startCol) {
    bool containsZeros = false;
    for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) if(array[i * BOARD_SIZE + j] == 0) containsZeros = true;

    if(containsZeros) {
        for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
            if(i == startRow && j == startCol) continue; // don't touch starting square
            if(array[i * BOARD_SIZE + j] != 0) continue; // this tile has already been calculated

            bool neighbouringAccessPoint = false; // a neighbouring (n - 1) will allow this tile to be a (n)
            if(i > 0 && array[(i - 1) * BOARD_SIZE + j] == n - 1) neighbouringAccessPoint = true;
            if(i < BOARD_SIZE - 1 && array[(i + 1) * BOARD_SIZE + j] == n - 1) neighbouringAccessPoint = true;
            if(j > 0 && array[i * BOARD_SIZE + j - 1] == n - 1) neighbouringAccessPoint = true;
            if(j < BOARD_SIZE - 1 && array[i * BOARD_SIZE + j + 1] == n - 1) neighbouringAccessPoint = true;
            if(neighbouringAccessPoint) spread_juice(array, i, j, n); // finds adjacent tiles
        }
        further_spread(array, n+1, startRow, startCol); // keep going until no zeros left
    }
}

// returns the number of tiles on the path - 1
int find_path_back(bool *path, int *initial_reach, int *paths, int startRow, int startCol, int destRow, int destCol) {
    bool visited [BOARD_SIZE][BOARD_SIZE] = {0};
    int minDists [BOARD_SIZE][BOARD_SIZE];
    int ref [BOARD_SIZE][BOARD_SIZE]; // points to the tile from which the path came
    for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
        minDists[i][j] = 10000;
        ref[i][j] = -1;
    }

    minDists[startRow][startCol] = 0;
    int pq [128]; // custom priority queue LOL
    pq[0] = startRow * BOARD_SIZE + startCol;
    visited[startRow][startCol] = true;
    int *pqStart = pq;
    int pqLen = 1;

    while(pqLen != 0) {
        int curRow = pqStart[0] / BOARD_SIZE;
        int curCol = pqStart[0] % BOARD_SIZE;
        pqStart++;
        pqLen--;

        bool curIsPiece = !piece_equal(board_clone[curRow][curCol], &null);
        //printf("%d %d: set [0][0] to %d\n", curRow, curCol, ref[0][0]);

        int nextRows [4] = {curRow - 1, curRow + 1, curRow, curRow};
        int nextCols [4] = {curCol, curCol, curCol - 1, curCol + 1};
        for(int i = 0; i < 4; i++) {
            if(nextRows[i] < 0 || nextRows[i] >= BOARD_SIZE || nextCols[i] < 0 || nextCols[i] >= BOARD_SIZE) continue; // off the board
            if(visited[nextRows[i]][nextCols[i]]) continue; // no need to see again
            // two cases:
            //      1) the neighbour tile is one lower on the initial reach, meaning we're one step closer
            //      2) the neighbour tile is the same, but this tile doesn't have a piece (we don't need to descend)
            if((initial_reach[nextRows[i] * BOARD_SIZE + nextCols[i]] < initial_reach[curRow * BOARD_SIZE + curCol])
                || (!curIsPiece && initial_reach[nextRows[i] * BOARD_SIZE + nextCols[i]] == initial_reach[curRow * BOARD_SIZE + curCol])) {
                visited[nextRows[i]][nextCols[i]] = true;
                minDists[nextRows[i]][nextCols[i]] = minDists[curRow][curCol] + 1;
                ref[nextRows[i]][nextCols[i]] = curRow * BOARD_SIZE + curCol;
                pqStart[pqLen] = nextRows[i] * BOARD_SIZE + nextCols[i]; // add to priority queue
                pqLen++; // accumulate priority queue length
            }
        }
    }

    int trackRow = destRow, trackCol = destCol;
    int counter = 0;
    while(ref[trackRow][trackCol] != -1) {
        paths[counter] = trackRow * BOARD_SIZE + trackCol;
        path[trackRow * BOARD_SIZE + trackCol] = true;
        int newTrackRow = ref[trackRow][trackCol] / BOARD_SIZE;
        trackCol = ref[trackRow][trackCol] % BOARD_SIZE;
        trackRow = newTrackRow;
        counter++;
    }
    path[startRow * BOARD_SIZE + startCol] = true;
    paths[counter] = startRow * BOARD_SIZE + startCol;
    path[destRow * BOARD_SIZE + destCol] = false; // remove the piece that needs to be moved from path

    return minDists[destRow][destCol];
}

// all values are given in terms of full board dimensions
// returns the length of the path, excluding the starting piece
int min_disruption(bool *path, int *paths, int startRow, int startCol, int endRow, int endCol) {
    int initial_reach [BOARD_SIZE][BOARD_SIZE] = {0};
    initial_spread(initial_reach, startRow, startCol);
    further_spread(initial_reach, 2, startRow, startCol);
    initial_reach[startRow][startCol] = 1;

    /*printf("\n-------initial reach array------\n");
    for(int i = BOARD_SIZE - 1; i >= 0; i--) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            printf("%d ", initial_reach[i][j]);
        }
        printf("\n");
    }*/

    int moves = find_path_back(path, initial_reach, paths, endRow, endCol, startRow, startCol); // go backwards
    /*printf("\n------path array------- [%d moves] \n", moves);
    for(int i = BOARD_SIZE - 1; i >= 0; i--) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            printf("%d ", path[i * BOARD_SIZE + j]);
        }
        printf("\n");
    }*/
    return moves;
}

// calculate closest exits
void closest_exit(int *pathExitsOrdered, int *closestExits, int length) {
    for(int i = 1; i < length; i++) closestExits[i] = -1;
    for(int i = 1; i < length; i++) if(pathExitsOrdered[i] != 0) closestExits[i] = i; // the closest exit is the current tile
    while(true) {
        bool flag = true; // whether we can terminate
        for(int i = 1; i < length; i++) {
            if(closestExits[i] == -1) { // no exit found yet
                flag = false;
                if(i > 1 && closestExits[i - 1] != -1) closestExits[i] = closestExits[i - 1];
                else if(i < length - 1 && closestExits[i + 1] != -1) closestExits[i] = closestExits[i + 1];
            }
        }
        if(flag) break;
    }

    /*printf("path exits: ");
    for(int i = 1; i < length; i++) printf("%d ", pathExitsOrdered[i]);
    printf("\n");

    printf("closest exits: ");
    for(int i = 1; i < length; i++) printf("%d ", closestExits[i]);
    printf("\n");*/
}

// removes obstacles along the path until there are none left, then moves the main piece, then puts everything back
// paths: an ordered array of path coordinates, from start to end
// closestExit: the closest exit to each square on the path, in the order of paths
// srcRow...destCol: are for the final target piece movement
// exitDist: distance currently being evacuated, items closest to exit first go first (to ensure no collision)
//
// edge case not considered: if there aren't enough exits for all the pieces that need to be evacuated
void clear_path(bool *path, int *paths, int *pathExitsOrdered, int *closestExit, int length, int srcRow, int srcCol, int destRow, int destCol, int exitDist) {
    closest_exit(pathExitsOrdered, closestExit, length);

    bool pieceNeedsMove = false;
    for(int i = 1; i < length; i++) {
        if(!piece_equal(board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE], &null)) {
            //printf("need to move the piece at (%d, %d)\n", paths[i] / BOARD_SIZE, paths[i] % BOARD_SIZE);
            pieceNeedsMove = true;
            if(abs(closestExit[i] - i) == exitDist) { // exit this item
                // run direct motor instruct command to move along path tiles one by one until exit is reached
                // use motor to exit to any valid exit

                // move motor to the piece's starting position, turn on magnet
                //printf("moving the piece at (%d, %d)...\n", paths[i] / BOARD_SIZE, paths[i] % BOARD_SIZE);
                motor_move_both(paths[i] / BOARD_SIZE + 0.5f - motorRow, paths[i] % BOARD_SIZE + 0.5f - motorCol, false);
                toggle_magnet(true);

                int curPathPos = i;
                while(curPathPos != closestExit[i]) { // move to the exit slot
                    int nextPathPos = (curPathPos > closestExit ? curPathPos-- : curPathPos++);
                    motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // move to next path tile
                }

                int exitRow = -1, exitCol = -1;
                int nextRows [4] = {(paths[curPathPos] / BOARD_SIZE) - 1, (paths[curPathPos] / BOARD_SIZE) + 1, (paths[curPathPos] / BOARD_SIZE), (paths[curPathPos] / BOARD_SIZE)};
                int nextCols [4] = {(paths[curPathPos] % BOARD_SIZE), (paths[curPathPos] % BOARD_SIZE), (paths[curPathPos] % BOARD_SIZE) - 1, (paths[curPathPos] % BOARD_SIZE) + 1};
                for(int k = 0; k < 4; k++) {
                    if(nextRows[k] < 0 || nextRows[k] >= BOARD_SIZE || nextCols[k] < 0 || nextCols[k] >= BOARD_SIZE) continue; // off the board
                    if(path[nextRows[k] * BOARD_SIZE + nextCols[k]]) continue; // still on the path, isn't an exit;
                    if(piece_equal(board_clone[nextRows[k]][nextCols[k]], &null)) { // exit found
                        exitRow = nextRows[k];
                        exitCol = nextCols[k];
                        break;
                    }
                }
                if(exitRow == -1 || exitCol == -1) printf("WARNING HELP HELP WHAT IS GOING ON?\n");

                motor_move_both(exitRow - (paths[curPathPos] / BOARD_SIZE), exitCol - (paths[curPathPos] % BOARD_SIZE), true); // move the item onto the exit tile
                toggle_magnet(false); // turn off magnet
                pathExitsOrdered[curPathPos]--; // one less path here!
                board_clone[exitRow][exitCol] = board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE]; // change board clone to match
                board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE] = &null;
                closest_exit(pathExitsOrdered, closestExit, length); // recalculate closest exits

                // perform recursive call, then move the piece back into original place
                clear_path(path, paths, pathExitsOrdered, closestExit, length, srcRow, srcCol, destRow, destCol, exitDist);

                // use motor to move the piece back
                motor_move_both(exitRow + 0.5f - motorRow, exitCol + 0.5f - motorCol, false);
                toggle_magnet(true);
                motor_move_both((paths[curPathPos] / BOARD_SIZE) - exitRow, (paths[curPathPos] % BOARD_SIZE) - exitCol, true); // move the piece back onto the path
                while(curPathPos != i) { // move from exit slot back to original slot
                    int nextPathPos = (curPathPos > i ? curPathPos-- : curPathPos++);
                    motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // move to next path tile
                }
                toggle_magnet(false);
                board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE] = board_clone[exitRow][exitCol]; // change board clone to match
                board_clone[exitRow][exitCol] = &null;
                return; // piece has been moved back, so we can return
            }
        }
    }
    // check if the list is empty, if so run base case (move the actual target piece)
    // if the list is not empty, increment exitDist and run the function again
    if(pieceNeedsMove) {
        // no pieces are able to be moved with given nearest exit length, so we will accumulate it by 1
        clear_path(path, paths, pathExitsOrdered, closestExit, length, srcRow, srcCol, destRow, destCol, exitDist + 1);
        return;
    } else { // base case
        //printf("BASE CASE REACHED! moving from (%d, %d) to (%d, %d) a distance of %d tiles\n", srcRow, srcCol, destRow, destCol, length);
        // set up motor to move piece
        motor_move_both(srcRow + 0.5f - motorRow, srcCol + 0.5f - motorCol, false);
        toggle_magnet(true);

        for(int curPathPos = 0; curPathPos < length; curPathPos++) {
            int nextPathPos = curPathPos + 1;
            motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // move to next path tile
        }

        toggle_magnet(false); // WE'RE DONE!
        return;
    }
}

// motor will move a piece from the given src to the given dest
// direct: whether to go there in a straight line, or move obstructing pieces first
//      note that going between tiles does not work given hardware constraints.
void motor_instruct(int srcRow, int srcCol, int destRow, int destCol, bool direct) {
    if(direct) {
        motor_move_both(srcRow + 0.5f - motorRow, srcCol + 0.5f - motorCol, false); // move to source position
        toggle_magnet(true); // turn on electromagnet
        motor_move_both(destRow - srcRow, destCol - srcCol, true); // move at once, as the crow flies
        toggle_magnet(false); // turn magnet off
    } else { // move along lines
        bool path [BOARD_SIZE][BOARD_SIZE] = {0}; // all set to false, set tiles to true when they're on the path
        int paths [64] = {0}; // ordered list of paths

        int length = min_disruption(path, paths, srcRow, srcCol, destRow, destCol); // does min dist calculations, dijkstra's, and draws the final path onto the path array

        int pathExits [BOARD_SIZE][BOARD_SIZE] = {0}; // exits from each tile
        for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
            if(path[i][j]) { // on the path
                int nextRows [4] = {i - 1, i + 1, i, i};
                int nextCols [4] = {j, j, j - 1, j + 1};
                for(int k = 0; k < 4; k++) {
                    if(nextRows[k] < 0 || nextRows[k] >= BOARD_SIZE || nextCols[k] < 0 || nextCols[k] >= BOARD_SIZE) continue; // off the board
                    if(path[nextRows[k]][nextCols[k]]) continue; // still on the path, isn't an exit
                    if(piece_equal(board_clone[nextRows[k]][nextCols[k]], &null)) {
                        pathExits[i][j]++;
                    }
                }
            }
        }

        /*printf("\n----all paths---- (size: %d)\n", length);
        for(int i = 0; i < length; i++) printf("%d\n", paths[i]);
        printf("\n------path exits array-------\n");
            for(int i = BOARD_SIZE - 1; i >= 0; i--) {
                for(int j = 0; j < BOARD_SIZE; j++) {
                    printf("%d ", pathExits[i][j]);
                }
            printf("\n");
        }*/
        int pathExitsOrdered [BOARD_SIZE * BOARD_SIZE]; // ignore position 0 because it's the start
        for(int i = 1; i < length; i++) pathExitsOrdered[i] = pathExits[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE];
        int closestExits [BOARD_SIZE * BOARD_SIZE]; // ignore position 0 because it's the start

        clear_path(path, paths, pathExitsOrdered, closestExits, length, srcRow, srcCol, destRow, destCol, 0);
    }
}

// pre: rows/cols given WITH BOARD_START added
//      captured is a pointer reference to the piece that should be taken off
// post: moves the captured piece into a holding tile off of the 8x8 game area
void deposit_captured(int pieceRow, int pieceCol, int colour, struct piece *captured) {
    int depositSpot = first_empty_spot(colour); // put the piece on the opposite colour's side

    board[depositSpot / BOARD_SIZE][depositSpot % BOARD_SIZE] = captured;
    motor_instruct(pieceRow, pieceCol, depositSpot / BOARD_SIZE, depositSpot % BOARD_SIZE, false); // call motor instructions to move piece off
}

// returns true if the move succeeds (if king is left open, will revert)
// move should already be deemed "legal" by standard rules (movement, captures, NOT including check, etc.)
//
// pre: rows/cols given WITH BOARD_START added; turn indicates the colour of the current player
bool move_piece(int srcRow, int srcCol, int destRow, int destCol, int turn) {
    struct piece *dest = board[destRow][destCol];
    struct piece *src = board[srcRow][srcCol];
    struct piece *adj = board[srcRow][destCol]; // potential en passant victim
    clone_board();

    // move piece
    board[destRow][destCol] = src;
    board[srcRow][srcCol] = &null;
    if(src->piece_id == pawn && abs(srcRow - destRow) == 1 && abs(srcCol - destCol) == 1) { // diagonal pawn move, is it a capture or en passant?
        if(piece_equal(dest, &null)) { // no piece on the diagonal, therefore en passant
            board[srcRow][destCol] = &null;
        }
    }
    find_kings();

    if(under_check(turn)) { // king vulnerable, move piece back
        board[destRow][destCol] = dest;
        board[srcRow][srcCol] = src;
        board[srcRow][destCol] = adj;
        find_kings();

        print_tts_message("Not a legal move, you will be under check!");
        return false;
    }

    if(src->piece_id == pawn && abs(srcRow - destRow) == 1 && abs(srcCol - destCol) == 1) { // diagonal pawn move, is it a capture or en passant?
        if(piece_equal(dest, &null)) { // no piece on the diagonal, therefore en passant
            deposit_captured(srcRow, destCol, other_colour(turn), adj); // move pawn off of the board
            printf("en passant\n");
        }
    }
    if(!piece_equal(dest, &null)) // something was in the dest spot, meaning it was captured
        deposit_captured(destRow, destCol, other_colour(turn), dest);

    if(src->piece_id == pawn && abs(srcRow - destRow) == 2) // pawn double advance, check for en passant opportunity
        enPassantCol[turn] = srcCol - BOARD_START;
    else enPassantCol[turn] = -1; // reset en passant

    if(src->piece_id == pawn && (destRow == BOARD_START || destRow == BOARD_START + 7)) { // prompt promotion
        char outputMessage [32] = "Promotion for _____, to ";
        if(turn == white) strncpy(outputMessage + 14, "white", 5);
        else strncpy(outputMessage + 14, "black", 5);
        switch(promote_letter) {
        case 'q':
            strcat(outputMessage, "queen");
            board[destRow][destCol] = (turn == white ? &white_queen_p : &black_queen_p);
            break;
        case 'r':
            strcat(outputMessage, "rook");
            board[destRow][destCol] = (turn == white ? &white_rook_p : &black_rook_p);
            break;
        case 'b':
            strcat(outputMessage, "bishop");
            board[destRow][destCol] = (turn == white ? &white_bishop_p : &black_bishop_p);
            break;
        case 'n':
            strcat(outputMessage, "knight");
            board[destRow][destCol] = (turn == white ? &white_knight_p : &black_knight_p);
            break;
        }
        print_tts_message(outputMessage);
    }

    // motor instructions
    motor_instruct(srcRow, srcCol, destRow, destCol, board[destRow][destCol]->piece_id != knight); // only the knight moves indirectly

    // check if castling is still legal
    kingMoved[white] = kingMoved[white] || !piece_equal(board[BOARD_START][BOARD_START + 4], &white_king);
    aRookMoved[white] = aRookMoved[white] || !piece_equal(board[BOARD_START][BOARD_START], &white_rook);
    hRookMoved[white] = hRookMoved[white] || !piece_equal(board[BOARD_START][BOARD_START + 7], &white_rook);
    kingMoved[black] = kingMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START + 4], &black_king);
    aRookMoved[black] = aRookMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START], &black_rook);
    hRookMoved[black] = hRookMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START + 7], &black_rook);

    // check 50 move rule
    if(src->piece_id == pawn || !piece_equal(dest, &null)) movesTillDraw = 100; // pawn moved or capture happened, reset
    else movesTillDraw--;

    return true;
}

// ASSUMES that legality check for castling has already been made
bool move_castle(int colour, bool kingSide) {
    int targetFile = (colour == white ? 0 : 7);
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 7 : 0)] = &null; // remove rook
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 5 : 3)] = (colour == white ? &white_rook : &black_rook); // reposition rook
    clone_board(); // clone board after rook is moved, which is always a guaranteed straight line with no interruptions

    board[BOARD_START + targetFile][BOARD_START + 4] = &null; // remove king
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 6 : 2)] = (colour == white ? &white_king : &black_king); // reposition king

    // appropriate motor commands (move rook first)
    motor_instruct(BOARD_START + targetFile, BOARD_START + (kingSide ? 7 : 0), BOARD_START + targetFile, BOARD_START + (kingSide ? 5 : 3), true);
    motor_instruct(BOARD_START + targetFile, BOARD_START + 4, BOARD_START + targetFile, BOARD_START + (kingSide ? 6 : 2), false);
    return true; // always succeeds because legality is presumed to have already been checked
}

// calls move_piece, except takes string sequence as input
bool move_piece_char(char *input, int turn) {
    // handle castling separately
    if(!strcmp(input, "o-o")) return move_castle(turn, true);
    else if(!strcmp(input, "o-o-o")) return move_castle(turn, false);

    return move_piece(BOARD_START + input[2] - '1', BOARD_START + input[1] - 'a', BOARD_START + input[4] - '1', BOARD_START + input[3] - 'a', turn);
}

// colour represents the player that JUST made a move
// returns 0 for check, 1 for checkmate, 2 for stalement, -1 for none of the above
int analyze_board(int colour) {
    bool underCheck = under_check(other_colour(colour));
    bool hasMoves = has_valid_move(other_colour(colour));

    if(!hasMoves) {
        if(underCheck) return 1;
        else return 2;
    }
    if(underCheck) return 0;
    return -1;
}

// method to be called by python
void run_chess_algorithm(char* turnInput) {
        char *parsedInput = malloc(sizeof(char) * 10);
        understand(parsedInput, turnInput); // will try to convert input into standardized move notation (for this program, at least)

        printf("[Message] You said: %s\n", parsedInput);
        if(!validate_input(parsedInput)) return;
        if(!validate_move(parsedInput, turn)) {
            print_tts_message("Not a legal move!");
            return;
        }

        // if this code is reached, then the move is, on first glance, "legal" (minus checks and such)
        if(!move_piece_char(parsedInput, turn)) return;
        free(parsedInput);

        // if this code is reached, move completed and uploaded to board
        switch(analyze_board(turn)) {
        case 0: // check
            print_tts_message("Check!");
            break;
        case 1: // checkmate
            print_tts_message(turn == white ? "Checkmate, white wins!" : "Checkmate, black wins!");
            running = false;
            break;
        case 2: // stalemate
            print_tts_message("Stalemate.");
            running = false;
            break;
        }
        print_board();

        if(movesTillDraw <= 0) { // 50-move rule
            print_tts_message("50 move rule. Game is tied.");
            running = false;
        }

        turn = (turn == white ? black : white);
        promote_letter = 'q'; // reset to promoting to queen
}

int main(void) { // will have to mirror the main method in python, then call appropriate methods here
    while(true) {

        init_board(); // call from python
        print_board();

        while(running) {
            printf(turn == white ? "[MESSAGE]: White's turn:\n" : "[MESSAGE]: Black's turn:\n");
            char turnInput [50]; // how many letters do you need?
            gets(turnInput);

            run_chess_algorithm(turnInput);
        }
    }
    return 0;
}
