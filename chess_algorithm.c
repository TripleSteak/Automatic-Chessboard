#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

// A standard chessboard is 8 x 8.
// Our board size has been extended to 10 x 10 to allow for captured pieces to be placed on the outer perimeter of the board.
// Playable board area is marked by the rectangle from (1, 1) to (8, 8)
#define BOARD_SIZE 10
#define BOARD_START 1

/*
 * DECLARATIONS FOR THE CHESSBOARD LOGIC!
 * Variables, constants, and functions that are needed to store the chessboard's physical state.
 */

const int PAWN_ID = 0;
const int KNIGHT_ID = 1;
const int BISHOP_ID = 2;
const int ROOK_ID = 3;
const int QUEEN_ID = 4;
const int KING_ID = 5;

const int WHITE = 0;
const int BLACK = 1;

struct piece {
    int pieceId; // PAWN_ID, KNIGHT_ID, etc.
    int colour; // WHITE or BLACK
    char letter;
};

// Standard pieces that can be found at the beginning of the game
const struct piece NULL_PIECE = {-1, -1};
const struct piece WHITE_PAWN = {PAWN_ID, WHITE, 'P'};
const struct piece WHITE_KNIGHT = {KNIGHT_ID, WHITE, 'N'};
const struct piece WHITE_BISHOP = {BISHOP_ID, WHITE, 'B'};
const struct piece WHITE_ROOK = {ROOK_ID, WHITE, 'R'};
const struct piece WHITE_QUEEN = {QUEEN_ID, WHITE, 'Q'};
const struct piece WHITE_KING = {KING_ID, WHITE, 'K'};
const struct piece BLACK_PAWN = {PAWN_ID, BLACK, 'P'};
const struct piece BLACK_KNIGHT = {KNIGHT_ID, BLACK, 'N'};
const struct piece BLACK_BISHOP = {BISHOP_ID, BLACK, 'B'};
const struct piece BLACK_ROOK = {ROOK_ID, BLACK, 'R'};
const struct piece BLACK_QUEEN = {QUEEN_ID, BLACK, 'Q'};
const struct piece BLACK_KING = {KING_ID, BLACK, 'K'};

// Promoted pawn variants that are not found at the beginning of the game
const struct piece WHITE_KNIGHT_P = {KNIGHT_ID, WHITE, 'N'};
const struct piece WHITE_BISHOP_P = {BISHOP_ID, WHITE, 'B'};
const struct piece WHITE_ROOK_P = {ROOK_ID, WHITE, 'R'};
const struct piece WHITE_QUEEN_P = {QUEEN_ID, WHITE, 'Q'};
const struct piece BLACK_KNIGHT_P = {KNIGHT_ID, BLACK, 'N'};
const struct piece BLACK_BISHOP_P = {BISHOP_ID, BLACK, 'B'};
const struct piece BLACK_ROOK_P = {ROOK_ID, BLACK, 'R'};
const struct piece BLACK_QUEEN_P = {QUEEN_ID, BLACK, 'Q'};

// (kingRank[colour], kingFile[colour]) locates the position of each player's king
// Ranks and files are given as an integer from [0, 8).
int kingRank [2];
int kingFile [2];

// Which file is available for en passant?
// enPassantFile[colour] means the given colour is capable of performing en passant in the given file
// A value of -1 indicates that no files are eligible for en passant
int enPassantFile [2];

// kingMoved[colour] and aRookMoved[colour] must both be false for given colour to perform queen-side castling
// kingMoved[colour] and hRookMoved[colour] must both be false for given colour to perform king-side castling
bool kingMoved [2];
bool aRookMoved [2];
bool hRookMoved [2];

/*
 * DECLARATIONS FOR PHYSICAL CHESSBOARD INTEGRATION
 * Variables, constants, and functions that are needed to properly link the physical chessboard components with the chess code
 */

// Motor position:
// (0,0) represents the bottom left corner of the 10x10 board (beyond A1)
// (10 * tileSize, 10 * tileSize) represents the top right corner (beyond H8)
float motorRow = 0;
float motorCol = 0;

// Stale move counter, which is reset every time a pawn is moved or a piece is captured.
// Due to the 50 move rule, will force a draw once movesTillDraw reaches 0.
// (50 moves per player = 100 "moves" total)
int movesTillDraw = 100;

// Colour corresponding to the player to move
int turn;

// Whether the game is currently running
bool isRunning;
bool is_running() { return isRunning; }

// Some of the letters that correspond to files on the chessboard aren't easily discernible by ear.
// Thus, our program uses words instead of letters for speech-to-text move commands.
const char *A_CODE = "apple";
const char *B_CODE = "banana";
const char *C_CODE = "cash";
const char *D_CODE = "donut";
const char *E_CODE = "eggplant";
const char *F_CODE = "falafel";
const char *G_CODE = "garlic";
const char *H_CODE = "hazelnut";

// Identifiers for communicating motor/magnet command types
const int MAGNET_TOGGLE = 0;
const int X_MOTOR_AXIS = 1;
const int Y_MOTOR_AXIS = 2;
const int BOTH_MOTOR_AXES = 3;

// How much further the motor moves than the required distance when travelling across the board
// This is needed because pieces are "dragged" by the electromagnet, resulting in pieces "positionally lagging" behind the electromagnet.
const float MOTOR_OVERFLOW = 0.45f;

struct next_command {
    int commandType; // MAGNET_TOGGLE, X_MOTOR_AXIS, etc.
    int i1; // First integer parameter; usage depends on command type
    float f1; // First float parameter; usage depends on command type
    float f2; // Second float parameter; usage depends on command type
};

// Commands are processed one by one after being inputted by the physical chessboard controller
// The queue will not realistically exceed 24 commands, so a fixed allocation is fine
struct next_command commandQueue [24];
int numCommandsInQueue = 0;

bool has_commands() {
    return numCommandsInQueue > 0;
}

int get_command_type() {
    return commandQueue[0].commandType;
}

// Pops the top command in the command queue
void go_next_command() {
    for(int i = 0; i < numCommandsInQueue - 1; i++) { commandQueue[i] = commandQueue[i + 1]; }
    numCommandsInQueue--;
}

// Command types that use an integer parameter only use one integer parameter
// Thus, this function also pops the top command from the queue
int get_int_command_value() {
    int value = commandQueue[0].i1;
    go_next_command();
    return value;
}

float get_float_command_value_a() {
    float value = commandQueue[0].f1;
    return value;
}

// Command types that use a float parameter only use two float parameters
// Thus, this function also pops the top command from the queue
float get_float_command_value_b() {
    float value = commandQueue[0].f2;
    go_next_command();
    return value;
}

void queue_command(int type, int i1, float f1, float f2) {
    if(numCommandsInQueue >= 24) return;

    struct next_command q_command = {type, i1, f1, f2};
    commandQueue[numCommandsInQueue] = q_command;
    numCommandsInQueue++;
}

/*
 * DECLARATIONS FOR TEXT-TO-SPEECH NARRATION!
 * Variables, constants, and functions that are needed to properly execute TTS directives.
 */

// Message to be spoken aloud using TTS
char *narration;

void set_tts(char *text) { narration = text; }

char *get_tts() {
    if(narration == NULL) return "";

    char *returnValue = narration;
    narration = NULL;
    return returnValue;
}

// Print a copy of TTS to the console as well for written record
void print_tts_message(char *message) {
    narration = message;
    printf("[MESSAGE] %s\n", message);
}

/*
 * PRIMARY CHESS LOGIC IMPLEMENTATION
 * Now that all (most of) the declarations are out of the way...
 */

// Rank-first array indexing -> in chess notation, [1-8][A-H]
struct piece *board [BOARD_SIZE][BOARD_SIZE];
struct piece *board_clone [BOARD_SIZE][BOARD_SIZE];

// used for motor pathfinding, since the virtual board will be updated but not the actual board
void clone_board() {
    for(int i = 0; i < BOARD_SIZE; i++) {
        for(int j = 0; j < BOARD_SIZE; j++) {
            board_clone[i][j] = board[i][j];
        }
    }
}

int other_colour(int colour) { return colour == WHITE ? BLACK : WHITE; }
bool piece_equal(struct piece *a, const struct piece *b) { return a->pieceId == b->pieceId && a->colour == b->colour; }

// Returns true if the given char corresponds to a piece
bool is_piece_letter(char c) { return (c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k'); }

// Returns true if the given char corresponds to a rank or a file in chess notation
// The '$' wildcard character is used to denote "all rows" or "all files"
bool is_rank(char c) { return ((c >= 'a' && c <= 'h') || c == '$'); }
bool is_file(char c) { return ((c >= '1' && c <= '8') || c == '$'); }

int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

char *min_pointer(char *a, char *b) {
    if (a == NULL && b == NULL) return NULL;
    if (a == NULL) return b;
    if (b == NULL) return a;
    return a < b ? a : b;
}

bool prefix(char *pre, char *str) {
    if(pre == NULL || str == NULL) return false;
    return strncmp(pre, str, strlen(pre)) == 0;
}

// Needed by physical chessboard control code
int get_turn() { return turn; }
int get_white() { return WHITE; }

// Determines where the kings are on the board, and updates the local variables
void find_kings() {
    for(int rank = 0; rank < 8; rank++) {
        for(int file = 0; file < 8; file++) {
            if(board[BOARD_START + rank][BOARD_START + file]->pieceId == KING_ID) {
                if(board[BOARD_START + rank][BOARD_START + file]->colour == WHITE) {
                    kingRank[WHITE] = rank;
                    kingFile[WHITE] = file;
                } else {
                    kingRank[BLACK] = rank;
                    kingFile[BLACK] = file;
                }
            }
        }
    }
}

// Initializes board state at the beginning of the game
// Not the same as resetting the board! This function assumes that pieces are already placed in correct positions
// The physical chessboard is responsible for placing every piece in place before invoking this function
void init_board() {
    for(int rank = 0; rank < BOARD_SIZE; rank++) for(int file = 0; file < BOARD_SIZE; file++) board[rank][file] = &NULL_PIECE;
    for(int file = BOARD_START; file < BOARD_START + 8; file++) {
        board[BOARD_START + 1][file] = &WHITE_PAWN;
        board[BOARD_START + 6][file] = &BLACK_PAWN;
    }

    board[BOARD_START][BOARD_START] = board[BOARD_START][BOARD_START + 7] = &WHITE_ROOK;
    board[BOARD_START][BOARD_START + 1] = board[BOARD_START][BOARD_START + 6] = &WHITE_KNIGHT;
    board[BOARD_START][BOARD_START + 2] = board[BOARD_START][BOARD_START + 5] = &WHITE_BISHOP;
    board[BOARD_START][BOARD_START + 3] = &WHITE_QUEEN;
    board[BOARD_START][BOARD_START + 4] = &WHITE_KING;

    board[BOARD_START + 7][BOARD_START] = board[BOARD_START + 7][BOARD_START + 7] = &BLACK_ROOK;
    board[BOARD_START + 7][BOARD_START + 1] = board[BOARD_START + 7][BOARD_START + 6] = &BLACK_KNIGHT;
    board[BOARD_START + 7][BOARD_START + 2] = board[BOARD_START + 7][BOARD_START + 5] = &BLACK_BISHOP;
    board[BOARD_START + 7][BOARD_START + 3] = &BLACK_QUEEN;
    board[BOARD_START + 7][BOARD_START + 4] = &BLACK_KING

    find_kings();
    enPassantFile[WHITE] = enPassantFile[BLACK] = -1;
    kingMoved[WHITE] = kingMoved[BLACK] = aRookMoved[WHITE] = aRookMoved[BLACK] = hRookMoved[WHITE] = hRookMoved[BLACK] = false;

    turn = WHITE;
    isRunning = true;

    // Moves motors into place (ensure they're in the corner)
    motor_move_both(-50, -50);
    motorRow = 0;
    motorCol = 0;
}

// Lowercase for white pieces and uppercase for black pieces
// "onBoard" means whether the printed piece is on the actual 8x8 game board, as opposed to off to the side
char print_piece(struct piece *p, bool onBoard) {
    if(piece_equal(p, &NULL_PIECE)) return onBoard ? '_' : '.';

    int diff = p->colour == WHITE ? 32 : 0; // 32 is the difference between ASCII's 'a' and "A" characters
    return p->letter + diff;
}

// Useful for debugging the algorithm; not relevant in final program
void print_board() {
    printf("[BOARD]\n");
    for(int rank = BOARD_SIZE - 1; rank >= 0; rank--) {
        printf("[BOARD]     ");
        for(int file = 0; file < BOARD_SIZE; file++) {
            printf("%c ", print_piece(board[rank][file], rank >= BOARD_START && rank < BOARD_START + 8 && file >= BOARD_START && file < BOARD_START + 8));
        }
        printf("\n");
    }
    printf("[BOARD]\n");
}

void print_clone_board() {
    printf("[CLONE]\n");
    for(int rank = BOARD_SIZE - 1; rank >= 0; rank--) {
        printf("[CLONE]     ");
        for(int file = 0; file < BOARD_SIZE; file++) {
            printf("%c ", print_piece(board_clone[rank][file], rank >= BOARD_START && rank < BOARD_START + 8 && file >= BOARD_START && file < BOARD_START + 8));
        }
        printf("\n");
    }
    printf("[CLONE]\n");
}

// Returns the first empty spot on the chess board (useful for deciding where to put captured pieces)
// "white" determines if we start searching for positions from white's side
int first_empty_spot(bool white) {
    for(int rank = 0; rank < BOARD_SIZE; rank++) {
        for(int file = 0; file < BOARD_SIZE; file++) {
            if(piece_equal(board[white ? rank : BOARD_SIZE - rank - 1][file], &NULL_PIECE)) {
                // Return the empty position as one encoded integer
                return (white ? rank : BOARD_SIZE - rank - 1) * BOARD_SIZE + file;
            }
        }
    }
}

// "parsed" represents the char array to fill with the translated content
// "input" represents the raw input from the user
// For simplicity's sake, we're going to force the player to say the word "pawn" before moving a pawn
// AVOID USING THE WORD "TO" AS A CONJUNCTION!
void understand(char *parsed, char *input) {
    // Turn everything to lowercase
    for(int i = 0; i < strlen(input); i++) {
        if(input[i] >= 'A' & input[i] <= 'Z') input[i] += 32;
    }

    // Locate the first occurrence of a word that could correspond to the name of a piece
    // We've included similar words to capture speech-to-text errors (i.e., "pond" sounds like "pawn")
    char *pieceToMove = min_pointer(strstr(input, "pawn"), strstr(input, "pine"));
    pieceToMove = min_pointer(strstr(input, "pond"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "pain"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "paun"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "night"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "horse"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "bishop"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "rook"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "queen"), pieceToMove);
    pieceToMove = min_pointer(strstr(input, "king"), pieceToMove);

    // Look for the term "castle", which determines whether we should try to castle
    char *castle = strstr(input, "castle");
    if(castle != NULL) {
        if(prefix("queen", pieceToMove)) strcpy(parsed, "o-o-o");
        else if(prefix("king", pieceToMove)) strcpy(parsed, "o-o");
    } else if (pieceToMove != NULL) {
        int letterIndices [2] = {-1, -1}; // Files are letters!
        int numberIndices [2] = {-1, -1}; // Ranks are numbers!
        int letterIndexCnt = 0, numberIndexCnt = 0, totalIndexCnt = 2; // Keep track of which array slot to fill

        for(int i = 0; i < strlen(input); i++) {
            // Search for occurrences of terms that could be meant to denote files
            if(prefix(A_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(B_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(C_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(D_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(E_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(F_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(G_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;
            else if(prefix(H_CODE, input + i) && letterIndexCnt < totalIndexCnt) letterIndices[letterIndexCnt++] = i;

            // Search for occurrences of terms that could be meant to denote ranks
            else if(numberIndexCnt < totalIndexCnt && (prefix("1", input + i) || prefix("one", input + i) || prefix("won", input + i))) numberIndices[numberIndexCnt++] = 1;
            else if(numberIndexCnt < totalIndexCnt && (prefix("2", input + i) || prefix("two", input + i) || prefix("too", input + i) || prefix("to", input + i))) numberIndices[numberIndexCnt++] = 2;
            else if(numberIndexCnt < totalIndexCnt && (prefix("3", input + i) || prefix("three", input + i))) numberIndices[numberIndexCnt++] = 3;
            else if(numberIndexCnt < totalIndexCnt && (prefix("4", input + i) || prefix("four", input + i) || prefix("for", input + i))) numberIndices[numberIndexCnt++] = 4;
            else if(numberIndexCnt < totalIndexCnt && (prefix("5", input + i) || prefix("five", input + i))) numberIndices[numberIndexCnt++] = 5;
            else if(numberIndexCnt < totalIndexCnt && (prefix("6", input + i) || prefix("six", input + i) || prefix("stick", input + i))) numberIndices[numberIndexCnt++] = 6;
            else if(numberIndexCnt < totalIndexCnt && (prefix("7", input + i) || prefix("seven", input + i))) numberIndices[numberIndexCnt++] = 7;
            else if(numberIndexCnt < totalIndexCnt && (prefix("8", input + i) || prefix("eight", input + i) || prefix("ate", input + i))) numberIndices[numberIndexCnt++] = 8;
        }

        // Update parsed file and rank information
        if(letterIndexCnt > 0 && numberIndexCnt > 0) {
            char newParsed [6];
            newParsed[0] = (prefix("horse", pieceToMove) ? 'n' : pieceToMove[0]);
            newParsed[1] = letterIndexCnt > 1 ? input[letterIndices[0]] : '$';
            newParsed[2] = numberIndexCnt > 1 ? numberIndices[0] + '0' : '$';
            newParsed[3] = letterIndexCnt > 1 ? input[letterIndices[1]] : input[letterIndices[0]];
            newParsed[4] = numberIndexCnt > 1 ? numberIndices[1] + '0' : numberIndices[0] + '0';
            newParsed[5] = '\0';
            strcpy(parsed, newParsed);

            // Check to see if the player wants to promote to a certain piece
            if(newParsed[0] == 'p') {
                if(strstr(input, "queen") != NULL) promote_letter = 'q';
                else if(strstr(input, "rook") != NULL) promote_letter = 'r';
                else if(strstr(input, "bishop") != NULL) promote_letter = 'b';
                else if(strstr(input, "night") != NULL || strstr(input, "horse") != NULL) promote_letter = 'n';
            }
        } else strcpy(parsed, "");
    } else strcpy(parsed, "");
}

// Move inputs must be grammatically valid, with no capitals
// Inputs could take on the form "ra1h8" for moves, or "o-o"/"o-o-o" for castling
bool validate_input(char *input) {
    if(!strcmp(input, "o-o") || !strcmp(input, "o-o-o")) return true;

    bool flag;
    if(strlen(input) < 5) flag = false;
    else flag = is_piece_letter(input[0]) && is_rank(input[1]) && is_file(input[2]) && is_rank(input[3]) && is_file(input[4]);
    return flag;
}

// Checks whether the piece at [srcRank][srcFile] is legally allowed to move to [destRank][destFile]
// Rank and column parameters are given within the interval [0, 8).
bool legal_move(int srcRank, int srcFile, int destRank, int destFile) {
    struct piece *srcPiece = board[BOARD_START + srcRank][BOARD_START + srcFile];
    struct piece *destPiece = board[BOARD_START + destRank][BOARD_START + destFile];

    if(destPiece->colour == srcPiece->colour || (srcRank == destRank && srcFile == destFile)) return false; // Same colour or same tile
    bool isCapture = !piece_equal(destPiece, &NULL_PIECE); // Whether this move is considered a capture

    if(srcPiece->pieceId == PAWN_ID) {
        if(enPassantFile[other_colour(srcPiece->colour)] == destFile && abs(srcFile - destFile) == 1 && ((srcPiece->colour == white && destRank - srcRank == 1)
            || (srcPiece->colour == black && srcRank - destRank == 1)) && !isCapture && board[BOARD_START + srcRank][BOARD_START + destFile]->pieceId == PAWN_ID) { // En passant
            return true;
        }
        if(srcFile == destFile && !isCapture) { // Advance forwards, with no capture
            if((srcPiece->colour == white && destRank <= srcRank) || (srcPiece->colour == black && destRank >= srcRank)) return false; // No moving backwards!

            int distMoved = abs(srcRank - destRank);
            if(distMoved == 2 && ((srcPiece->colour == white && srcRank == 1) || (srcPiece->colour == black && srcRank == 6))) return true; // Two square pawn advance from start
            else if(distMoved == 1) return true; // One square advance
        } else if(abs(srcFile - destFile) == 1) { // Diagonal capture
            if(((srcPiece->colour == white && destRank - srcRank == 1) || (srcPiece->colour == black && srcRank - destRank == 1)) && isCapture) return true; // Diagonal capture on pawn
        }
        return false;
    }

    int deltaRow = abs(srcRank - destRank);
    int deltaCol = abs(srcFile - destFile);

    if(srcPiece->pieceId == KNIGHT_ID) { // Can move 2 in one direction and 1 perpendicular
        if((deltaRow == 2 && deltaCol == 1) || (deltaRow == 1 && deltaCol == 2)) return true;
    }

    if(srcPiece->pieceId == BISHOP_ID || srcPiece->pieceId == QUEEN_ID) { // Can move diagonally
        if(deltaRow == deltaCol) {
            int smallerRow = min(srcRank, destRank) + 1;
            int largerRow = max(srcRank, destRank) - 1;
            int smallerCol = min(srcFile, destFile) + 1;
            int largerCol = max(srcFile, destFile) - 1;

            if((destRank > srcRank && destFile > srcFile) || (destRank < srcRank && destFile < srcFile)) { // bottom-left to top-right diagonal
                for(int i = 0; i + smallerCol <= largerCol; i++) {
                    if(!piece_equal(board[BOARD_START + smallerRow + i][BOARD_START + smallerCol + i], &NULL_PIECE)) return false; // obstruction
                }
            } else for(int i = 0; i + smallerCol <= largerCol; i++) {
                if(!piece_equal(board[BOARD_START + smallerRow + i][BOARD_START + largerCol - i], &NULL_PIECE)) return false; // obstruction
            }
            return true;
        }
    }

    if(srcPiece->pieceId == ROOK_ID || srcPiece->pieceId == QUEEN_ID) { // Can move parallel to either edge of the chessboard
        if(deltaRow == 0) {
            int smallerCol = min(srcFile, destFile) + 1;
            int largerCol = max(srcFile, destFile) - 1;

            for(int col = smallerCol; col <= largerCol; col++) {
                if(!piece_equal(board[BOARD_START + srcRank][BOARD_START + col], &NULL_PIECE)) return false;
            }
            return true;
        } else if (deltaCol == 0) {
            int smallerRow = min(srcRank, destRank) + 1;
            int largerRow = max(srcRank, destRank) - 1;

            for(int row = smallerRow; row <= largerRow; row++) {
                if(!piece_equal(board[BOARD_START + row][BOARD_START + srcFile], &NULL_PIECE)) return false;
            }
            return true;
        }
    }

    if(srcPiece->pieceId == KING_ID) { // Can move in any direction as the queen, but only one tile
        if(((deltaRow == 0 || deltaCol == 0) || (deltaRow == deltaCol)) && (deltaRow <= 1 && deltaCol <= 1)) return true;
    }

    return false;
}

// Colour represents the colour BEING attacked
// Rank and file given within interval [0, 8)
bool tile_attacked(int targetRank, int targetFile, int colour) {
    for(int rank = 0; rank < 8; rank++) {
        for(int file = 0; file < 8; file++) {
            if(board[BOARD_START + rank][BOARD_START + file]->colour != colour && legal_move(rank, file, targetRank, targetFile)) return true;
        }
    }
    return false;
}

// Whether queen-side castling is legal
bool legal_castle_queenside(int colour) {
    int targetRank = (colour == WHITE ? 0 : 7);
    if(!(tile_attacked(targetRank, 2, colour) || tile_attacked(targetRank, 3, colour) || tile_attacked(targetRank, 4, colour))) { // Tiles king would pass through can't be under attack
        if(!(aRookMoved[colour] || kingMoved[colour])) { // Pieces already moved
            if(piece_equal(board[BOARD_START + targetRank][BOARD_START + 1], &NULL_PIECE) && piece_equal(board[BOARD_START + targetRank][BOARD_START + 2], &NULL_PIECE) && piece_equal(board[BOARD_START + targetRank][BOARD_START + 3], &NULL_PIECE)) { // No obstructing pieces
                return true;
            }
        }
    }
    print_tts_message("Can't castle now.\n");
    return false;
}

// Whether king-side castling is legal
bool legal_castle_kingside(int colour) {
    int targetRank = (colour == WHITE ? 0 : 7);
    if(!(tile_attacked(targetRank, 4, colour) || tile_attacked(targetRank, 5, colour) || tile_attacked(targetRank, 6, colour))) { // Tiles king would pass through can't be under attack
        if(!(hRookMoved[colour] || kingMoved[colour])) { // Pieces already moved
            if(piece_equal(board[BOARD_START + targetRank][BOARD_START + 5], &NULL_PIECE) && piece_equal(board[BOARD_START + targetRank][BOARD_START + 6], &NULL_PIECE)) { // No obstructing pieces
                return true;
            }
        }
    }
    print_tts_message("Can't castle now.\n");
    return false;
}

// See validate_input() for details (format should be "ra1h3")
// This function ensures that notation is correct (identified the correct piece, etc.)
bool validate_move(char *input, int turn) {
    // Handle castling separately
    if(!strcmp(input, "o-o")) return legal_castle_kingside(turn);
    else if(!strcmp(input, "o-o-o")) return legal_castle_queenside(turn);

    if(input[1] == '$') { // Variable file
        for(int i = 0; i < 8; i++) { // Test all files
            input[1] = 'a' + i;
            if(validate_move(input, turn)) return true;
        }
        return false;
    } else if (input[2] == '$') { // Variable rank, file fixed
        for(int i = 0; i < 8; i++) { // Test all ranks
            input[2] = '1' + i;
            if(validate_move(input, turn)) return true;
        }
        input[2] = '$'; // Reset, in case further files need the rank iteration again
        return false;
    }

    struct piece *toMove = board[BOARD_START + input[2] - '1'][BOARD_START + input[1] - 'a'];
    if(toMove->colour == turn && toMove->letter + 32 == input[0] && legal_move(input[2] - '1', input[1] - 'a', input[4] - '1', input[3] - 'a')) return true;
    return false;
}

// Returns true if the given colour's king is currently under threat
bool under_check(int colour) {
    return tile_attacked(kingRank[colour], kingFile[colour], colour);
}

// Given the current board state, determines if the given colour has a valid move
bool has_valid_move(int colour) {
    bool moveAvailable = false; // Whether a valid move exists
    bool continueCheckmateCheck = true; // Whether to keep going with the following giga-loop

    // Disgusting loop (sorry!)
    for(int srcRank = 0; srcRank < 8 && continueCheckmateCheck; srcRank++) {
        for(int srcFile = 0; srcFile < 8 && continueCheckmateCheck; srcFile++) {
            for(int destRank = 0; destRank < 8 && continueCheckmateCheck; destRank++) {
                for(int destFile = 0; destFile < 8 && continueCheckmateCheck; destFile++) {
                    if(srcRank == destRank && srcFile == destFile) continue; // Same tile
                    if(board[BOARD_START + srcRank][BOARD_START + srcFile]->colour != colour) continue; // Opponent piece or open tile

                    if(legal_move(srcRank, srcFile, destRank, destFile)) { // Legal move, begin simulation
                        struct piece *dest = board[BOARD_START + destRank][BOARD_START + destFile];
                        struct piece *src = board[BOARD_START + srcRank][BOARD_START + srcFile];

                        board[BOARD_START + destRank][BOARD_START + destFile] = src;
                        board[BOARD_START + srcRank][BOARD_START + srcFile] = &NULL_PIECE;
                        find_kings();

                        if(!under_check(colour)) { // This move does not result in staying/getting checked, so it is valid
                            moveAvailable = true;
                            continueCheckmateCheck = false;
                        }

                        board[BOARD_START + destRank][BOARD_START + destFile] = dest;
                        board[BOARD_START + srcRank][BOARD_START + srcFile] = src;
                        find_kings();
                    }
                }
            }
        }
    }

    return moveAvailable;
}

// Motor will move the given distance ACROSS rows
void motor_move_row(float delta) {
    motorRow += delta;
    queue_command(X_MOTOR_AXIS, 0, 0, delta);
    printf("[DEBUG] $MOTOR$ moved in Y: %f tiles\n", delta);
}

// Motor will move the given distance ALONG rows
void motor_move_col(float delta) {
    motorCol += delta;
    queue_command(Y_MOTOR_AXIS, 0, 0, delta);
    printf("[DEBUG] $MOTOR$ moved in X: %f tiles\n", delta);
}

void motor_move_both(float deltaX, float deltaY, bool withOverflow) {
    motorRow += deltaX;
    motorCol += deltaY;

    queue_command(BOTH_MOTOR_AXES, 0, deltaX, deltaY);
    printf("[DEBUG] $MOTOR$ moved in two axes: %f, %f tiles\n", deltaX, deltaY);
}

// Our motors aren't precise enough, so we'll recalibrate the motor in between movements (call at the end of command chain)
void motor_reset() {
    if(motorRow != 0 || motorCol != 0)
        motor_move_both(-motorRow, -motorCol, false);
}

// Turns the electromagnet on/off
void toggle_magnet(bool state) {
    int toggle = state ? 1 : 0;
    queue_command(MAGNET_TOGGLE, toggle, 0, 0);
    printf(state ? "[DEBUG] $MAGNET$: ON\n" : "[DEBUG] $MAGNET$: OFF\n");
}

// Identify all the tiles that can be reached from this tile
// All tiles that can be reached from this tile end up in the "visited" parameter
// The "visited" parameter can take in any integer, representing the "number of steps" needed to reach a certain tile
// This function is needed to figure out how to move pieces in cramped areas, where other pieces may need to be physically moved aside first.
void initial_spread(int *visited, int startRank, int startFile, int n) {
    if(startRank < 0 || startFile < 0 || startRank >= BOARD_SIZE || startFile >= BOARD_SIZE) return; // Out of bounds
    if(visited[startRank * BOARD_SIZE + startFile] != 0) return; // Already checked

    visited[startRank * BOARD_SIZE + startFile] = n;

    // Check traversable tiles
    if(startRank > 0 && piece_equal(board_clone[startRank - 1][startFile], &NULL_PIECE)) initial_spread(visited, startRank - 1, startFile, n);
    if(startRank < BOARD_SIZE - 1 && piece_equal(board_clone[startRank + 1][startFile], &NULL_PIECE)) initial_spread(visited, startRank + 1, startFile. n);
    if(startFile > 0 && piece_equal(board_clone[startRank][startFile - 1], &NULL_PIECE)) initial_spread(visited, startRank, startFile - 1, n);
    if(startFile < BOARD_SIZE - 1 && piece_equal(board_clone[startRank][startFile + 1], &NULL_PIECE)) initial_spread(visited, startRank, startFile + 1, n);
}

// Once we know which tiles can be accessed immediately, populate remaining tiles with higher integers
// By the end, the "visited" parameter can be interpreted as follows: (the int on each tile - 1) = the number of pieces needed to be moved out of the way to get here
// This function is needed to figure out how to move pieces in cramped areas, where other pieces may need to be physically moved aside first.
void further_spread(int *visited, int n, int startRank, int startFile) {
    bool containsZeros = false;
    for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) if(visited[i * BOARD_SIZE + j] == 0) containsZeros = true;

    if(containsZeros) {
        for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
            if(i == startRank && j == startFile) continue; // Don't touch starting square
            if(visited[i * BOARD_SIZE + j] != 0) continue; // This tile has already been calculated

            bool neighbouringAccessPoint = false; // A neighbouring (n - 1) will allow this tile to be a (n)
            if(i > 0 && visited[(i - 1) * BOARD_SIZE + j] == n - 1) neighbouringAccessPoint = true;
            if(i < BOARD_SIZE - 1 && visited[(i + 1) * BOARD_SIZE + j] == n - 1) neighbouringAccessPoint = true;
            if(j > 0 && visited[i * BOARD_SIZE + j - 1] == n - 1) neighbouringAccessPoint = true;
            if(j < BOARD_SIZE - 1 && visited[i * BOARD_SIZE + j + 1] == n - 1) neighbouringAccessPoint = true;
            if(neighbouringAccessPoint) initial_spread(visited, i, j, n); // Finds adjacent tiles
        }
        further_spread(visited, n+1, startRank, startFile); // Keep going until no zeros left
    }
}

// Returns the number of tiles on the path, minus one
// In cramped areas, other pieces may need to be physically moved aside for a target piece to be moved.
// This function helps the "other pieces" find their way back to their original spots.
int find_path_back(bool *path, int *initial_reach, int *paths, int startRank, int startFile, int destRank, int destFile) {
    bool visited [BOARD_SIZE][BOARD_SIZE] = {0};
    int minDists [BOARD_SIZE][BOARD_SIZE];
    int ref [BOARD_SIZE][BOARD_SIZE]; // Points to the tile from which the path came
    for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
        minDists[i][j] = 10000;
        ref[i][j] = -1;
    }

    minDists[startRank][startFile] = 0;
    int pq [128]; // Custom priority queue LOL
    pq[0] = startRank * BOARD_SIZE + startFile;
    visited[startRank][startFile] = true;
    int *pqStart = pq;
    int pqLen = 1;

    while(pqLen != 0) {
        int curRow = pqStart[0] / BOARD_SIZE;
        int curCol = pqStart[0] % BOARD_SIZE;
        pqStart++;
        pqLen--;

        bool curIsPiece = !piece_equal(board_clone[curRow][curCol], &NULL_PIECE);

        int nextRows [4] = {curRow - 1, curRow + 1, curRow, curRow};
        int nextCols [4] = {curCol, curCol, curCol - 1, curCol + 1};
        for(int i = 0; i < 4; i++) {
            if(nextRows[i] < 0 || nextRows[i] >= BOARD_SIZE || nextCols[i] < 0 || nextCols[i] >= BOARD_SIZE) continue; // Off the board
            if(visited[nextRows[i]][nextCols[i]]) continue; // No need to see again

            // Two cases:
            //  1) The neighbour tile is one lower on the initial reach, meaning we're one step closer
            //  2) The neighbour tile is the same, but this tile doesn't have a piece (we don't need to descend)
            if((initial_reach[nextRows[i] * BOARD_SIZE + nextCols[i]] < initial_reach[curRow * BOARD_SIZE + curCol])
                || (!curIsPiece && initial_reach[nextRows[i] * BOARD_SIZE + nextCols[i]] == initial_reach[curRow * BOARD_SIZE + curCol])) {
                visited[nextRows[i]][nextCols[i]] = true;
                minDists[nextRows[i]][nextCols[i]] = minDists[curRow][curCol] + 1;
                ref[nextRows[i]][nextCols[i]] = curRow * BOARD_SIZE + curCol;
                pqStart[pqLen] = nextRows[i] * BOARD_SIZE + nextCols[i];
                pqLen++;
            }
        }
    }

    int trackRank = destRank, trackFile = destFile;
    int counter = 0;
    while(ref[trackRank][trackFile] != -1) {
        paths[counter] = trackRank * BOARD_SIZE + trackFile;
        path[trackRank * BOARD_SIZE + trackFile] = true;
        int newTrackRow = ref[trackRank][trackFile] / BOARD_SIZE;
        trackFile = ref[trackRank][trackFile] % BOARD_SIZE;
        trackRank = newTrackRow;
        counter++;
    }
    path[startRank * BOARD_SIZE + startFile] = true;
    paths[counter] = startRank * BOARD_SIZE + startFile;
    path[destRank * BOARD_SIZE + destFile] = false; // Remove the piece that needs to be moved from path

    return minDists[destRank][destFile];
}

// All values are given in terms of full board dimensions
// Returns the length of the path, excluding the starting piece
int min_disruption(bool *path, int *paths, int startRank, int startFile, int endRank, int endFile) {
    int initial_reach [BOARD_SIZE][BOARD_SIZE] = {0};
    initial_spread(initial_reach, startRank, startFile);
    further_spread(initial_reach, 2, startRank, startFile);
    initial_reach[startRank][startFile] = 1;

    return find_path_back(path, initial_reach, paths, endRank, endFile, startRank, startFile); // Go backwards
}

// Calculate closest exits
void closest_exit(int *pathExitsOrdered, int *closestExits, int length) {
    for(int i = 1; i < length; i++) closestExits[i] = -1;
    for(int i = 1; i < length; i++) if(pathExitsOrdered[i] != 0) closestExits[i] = i; // The closest exit is the current tile
    while(true) {
        bool flag = true; // Whether we can terminate
        for(int i = 1; i < length; i++) {
            if(closestExits[i] == -1) { // No exit found yet
                flag = false;
                if(i > 1 && closestExits[i - 1] != -1) closestExits[i] = closestExits[i - 1];
                else if(i < length - 1 && closestExits[i + 1] != -1) closestExits[i] = closestExits[i + 1];
            }
        }
        if(flag) break;
    }
}

// Used to move pieces in cramped areas. The general strategy is:
// Remove obstacles along the path until there are none left, then move the main piece, then put everything back

// paths: An ordered array of path coordinates, from start to end
// closestExit: The closest exit to each square on the path, in the order of paths
// srcRank...destFile: For the final target piece movement
// exitDist: Distance currently being evacuated, items closest to exit go first (to ensure no collision)
void clear_path(bool *path, int *paths, int *pathExitsOrdered, int *closestExit, int length, int srcRank, int srcFile, int destRank, int destFile, int exitDist) {
    closest_exit(pathExitsOrdered, closestExit, length);

    bool pieceNeedsMove = false;
    for(int i = 1; i < length; i++) {
        if(!piece_equal(board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE], &NULL_PIECE)) {
            pieceNeedsMove = true;
            if(abs(closestExit[i] - i) == exitDist) { // Exit this item
                // Run direct motor command to move along path tiles one by one until exit is reached
                // Use motor to exit to any valid exit
                // Move motor to the piece's starting position, turn on magnet
                motor_move_both(paths[i] / BOARD_SIZE - motorRow, paths[i] % BOARD_SIZE - motorCol, false);
                toggle_magnet(true);

                int curPathPos = i;
                while(curPathPos != closestExit[i]) { // Move to the exit slot
                    int nextPathPos = (curPathPos > closestExit ? curPathPos-- : curPathPos++);
                    motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // Move to next path tile
                }

                int exitRow = -1, exitCol = -1;
                int nextRows [4] = {(paths[curPathPos] / BOARD_SIZE) - 1, (paths[curPathPos] / BOARD_SIZE) + 1, (paths[curPathPos] / BOARD_SIZE), (paths[curPathPos] / BOARD_SIZE)};
                int nextCols [4] = {(paths[curPathPos] % BOARD_SIZE), (paths[curPathPos] % BOARD_SIZE), (paths[curPathPos] % BOARD_SIZE) - 1, (paths[curPathPos] % BOARD_SIZE) + 1};
                for(int k = 0; k < 4; k++) {
                    if(nextRows[k] < 0 || nextRows[k] >= BOARD_SIZE || nextCols[k] < 0 || nextCols[k] >= BOARD_SIZE) continue; // Off the board
                    if(path[nextRows[k] * BOARD_SIZE + nextCols[k]]) continue; // Still on the path, isn't an exit;
                    if(piece_equal(board_clone[nextRows[k]][nextCols[k]], &NULL_PIECE)) { // Exit found
                        exitRow = nextRows[k];
                        exitCol = nextCols[k];
                        break;
                    }
                }
                if(exitRow == -1 || exitCol == -1) {
                    // Unhandled error, needs further implementation
                    printf("WARNING HELP HELP WHAT IS GOING ON?\n");
                }

                motor_move_both(exitRow - (paths[curPathPos] / BOARD_SIZE), exitCol - (paths[curPathPos] % BOARD_SIZE), true); // Move the item onto the exit tile
                toggle_magnet(false); // Turn off magnet
                pathExitsOrdered[curPathPos]--; // One less path here!

                board_clone[exitRow][exitCol] = board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE]; // Change board clone to match
                board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE] = &NULL_PIECE;
                closest_exit(pathExitsOrdered, closestExit, length); // Recalculate closest exits

                // Perform recursive call, then move the piece back into original place
                clear_path(path, paths, pathExitsOrdered, closestExit, length, srcRank, srcFile, destRank, destFile, exitDist);

                // Use motor to move the piece back
                motor_move_both(exitRow - motorRow, exitCol - motorCol, false);
                toggle_magnet(true);
                motor_move_both((paths[curPathPos] / BOARD_SIZE) - exitRow, (paths[curPathPos] % BOARD_SIZE) - exitCol, true); // Move the piece back onto the path
                while(curPathPos != i) { // Move from exit slot back to original slot
                    int nextPathPos = (curPathPos > i ? curPathPos-- : curPathPos++);
                    motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // Move to next path tile
                }
                toggle_magnet(false);
                board_clone[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE] = board_clone[exitRow][exitCol]; // Change board clone to match
                board_clone[exitRow][exitCol] = &NULL_PIECE;
                return; // Piece has been moved back, so we can return
            }
        }
    }

    // Check if the list is empty, if so run base case (move the actual target piece)
    // If the list is not empty, increment exitDist and run the function again
    if(pieceNeedsMove) {
        // No pieces are able to be moved with given nearest exit length, so we will accumulate it by 1
        clear_path(path, paths, pathExitsOrdered, closestExit, length, srcRank, srcFile, destRank, destFile, exitDist + 1);
        return;
    } else {
        // Base case; set up motor to move piece
        motor_move_both(srcRank - motorRow, srcFile - motorCol, false);
        toggle_magnet(true);

        for(int curPathPos = 0; curPathPos < length; curPathPos++) {
            int nextPathPos = curPathPos + 1;
            motor_move_both((paths[nextPathPos] / BOARD_SIZE) - (paths[curPathPos] / BOARD_SIZE), (paths[nextPathPos] % BOARD_SIZE) - (paths[curPathPos] % BOARD_SIZE), true); // move to next path tile
        }

        toggle_magnet(false); // WE'RE DONE!
        return;
    }
}

// Motor will move a piece from the given src to the given dest
// direct: whether to go there in a straight line, or move obstructing pieces first
//
// Note that moving pieces between tiles does not work given our hardware constraints.
// The magnet is powerful enough that it will start dragging along other adjacent pieces.
void motor_instruct(int srcRank, int srcFile, int destRank, int destFile, bool direct) {
    if(direct) {
        motor_move_both(srcRank - motorRow, srcFile - motorCol, false); // Move to source position
        toggle_magnet(true); // Turn on electromagnet
        motor_move_both(destRank - srcRank, destFile - srcFile, true); // Move at once, as the crow flies
        toggle_magnet(false); // Turn magnet off
    } else { // Move along lines
        bool path [BOARD_SIZE][BOARD_SIZE] = {0}; // All set to false, set tiles to true when they're on the path
        int paths [64] = {0}; // Ordered list of paths

        int length = min_disruption(path, paths, srcRank, srcFile, destRank, destFile); // Does min dist calculations, dijkstra's, and draws the final path onto the path array

        int pathExits [BOARD_SIZE][BOARD_SIZE] = {0}; // Exits from each tile
        for(int i = 0; i < BOARD_SIZE; i++) for(int j = 0; j < BOARD_SIZE; j++) {
            if(path[i][j]) { // On the path
                int nextRows [4] = {i - 1, i + 1, i, i};
                int nextCols [4] = {j, j, j - 1, j + 1};
                for(int k = 0; k < 4; k++) {
                    if(nextRows[k] < 0 || nextRows[k] >= BOARD_SIZE || nextCols[k] < 0 || nextCols[k] >= BOARD_SIZE) continue; // Off the board
                    if(path[nextRows[k]][nextCols[k]]) continue; // Still on the path, isn't an exit
                    if(piece_equal(board_clone[nextRows[k]][nextCols[k]], &NULL_PIECE)) {
                        pathExits[i][j]++;
                    }
                }
            }
        }

        int pathExitsOrdered [BOARD_SIZE * BOARD_SIZE]; // Ignore position 0 because it's the start
        for(int i = 1; i < length; i++) pathExitsOrdered[i] = pathExits[paths[i] / BOARD_SIZE][paths[i] % BOARD_SIZE];
        int closestExits [BOARD_SIZE * BOARD_SIZE]; // Ignore position 0 because it's the start

        clear_path(path, paths, pathExitsOrdered, closestExits, length, srcRank, srcFile, destRank, destFile, 0);
    }
}

// Deposits the captured piece onto the perimeter of the chessboard
// "pieceRow" and "pieceCol" are given within intervals [0, 10)
void deposit_captured(int pieceRow, int pieceCol, int colour, struct piece *captured) {
    int depositSpot = first_empty_spot(colour); // Put the piece on the opposite colour's side

    board[depositSpot / BOARD_SIZE][depositSpot % BOARD_SIZE] = captured;
    motor_instruct(pieceRow, pieceCol, depositSpot / BOARD_SIZE, depositSpot % BOARD_SIZE, false); // Call motor instructions to move piece off
}

// Returns true if the move succeeds (if king is left open, will revert)
// Move should already have been deemed "legal" by standard rules (movement, captures, NOT including check, etc.)
// All rows and columns are given within intervals [0, 10)
bool move_piece(int srcRow, int srcCol, int destRow, int destCol, int turn) {
    struct piece *dest = board[destRow][destCol];
    struct piece *src = board[srcRow][srcCol];
    struct piece *adj = board[srcRow][destCol]; // Potential en passant victim
    clone_board();

    // Move piece
    board[destRow][destCol] = src;
    board[srcRow][srcCol] = &NULL_PIECE;
    if(src->pieceId == PAWN_ID && abs(srcRow - destRow) == 1 && abs(srcCol - destCol) == 1) { // Diagonal pawn move, is it a capture or en passant?
        if(piece_equal(dest, &NULL_PIECE)) { // No piece on the diagonal, therefore en passant
            board[srcRow][destCol] = &NULL_PIECE;
        }
    }
    find_kings();

    if(under_check(turn)) { // King vulnerable, move piece back
        board[destRow][destCol] = dest;
        board[srcRow][srcCol] = src;
        board[srcRow][destCol] = adj;
        find_kings();

        print_tts_message("Not a legal move, you will be under check!");
        return false;
    }

    if(src->pieceId == PAWN_ID && abs(srcRow - destRow) == 1 && abs(srcCol - destCol) == 1) { // Diagonal pawn move, is it a capture or en passant?
        if(piece_equal(dest, &NULL_PIECE)) { // No piece on the diagonal, therefore en passant
            deposit_captured(srcRow, destCol, other_colour(turn), adj); // Move the captured pawn into the captured pieces area
            printf("en passant\n");
        }
    }
    if(!piece_equal(dest, &NULL_PIECE)) // Something was in the dest spot, meaning it was captured
        deposit_captured(destRow, destCol, other_colour(turn), dest);

    if(src->pieceId == PAWN_ID && abs(srcRow - destRow) == 2) // Pawn double advance; check for en passant opportunity
        enPassantFile[turn] = srcCol - BOARD_START;
    else enPassantFile[turn] = -1; // Reset en passant

    if(src->pieceId == PAWN_ID && (destRow == BOARD_START || destRow == BOARD_START + 7)) { // Prompt promotion
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

    // Motor instructions
    motor_instruct(srcRow, srcCol, destRow, destCol, board[destRow][destCol]->pieceId != knight); // Only the knight moves indirectly

    // Check if castling is still legal
    kingMoved[white] = kingMoved[white] || !piece_equal(board[BOARD_START][BOARD_START + 4], &white_king);
    aRookMoved[white] = aRookMoved[white] || !piece_equal(board[BOARD_START][BOARD_START], &white_rook);
    hRookMoved[white] = hRookMoved[white] || !piece_equal(board[BOARD_START][BOARD_START + 7], &white_rook);
    kingMoved[black] = kingMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START + 4], &black_king);
    aRookMoved[black] = aRookMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START], &black_rook);
    hRookMoved[black] = hRookMoved[black] || !piece_equal(board[BOARD_START + 7][BOARD_START + 7], &black_rook);

    // Check 50 move rule
    if(src->pieceId == PAWN_ID || !piece_equal(dest, &NULL_PIECE)) movesTillDraw = 100; // Pawn moved or capture happened, reset
    else movesTillDraw--;

    return true;
}

// ASSUMES that legality check for castling has already been made
bool move_castle(int colour, bool kingSide) {
    int targetFile = (colour == white ? 0 : 7);
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 7 : 0)] = &NULL_PIECE; // Remove rook
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 5 : 3)] = (colour == white ? &white_rook : &black_rook); // Reposition rook
    clone_board(); // Clone board after rook is moved, which is always a guaranteed straight line with no interruptions

    board[BOARD_START + targetFile][BOARD_START + 4] = &NULL_PIECE; // Remove king
    board[BOARD_START + targetFile][BOARD_START + (kingSide ? 6 : 2)] = (colour == white ? &white_king : &black_king); // Reposition king

    // Appropriate motor commands (move rook first)
    motor_instruct(BOARD_START + targetFile, BOARD_START + (kingSide ? 7 : 0), BOARD_START + targetFile, BOARD_START + (kingSide ? 5 : 3), true);
    motor_instruct(BOARD_START + targetFile, BOARD_START + 4, BOARD_START + targetFile, BOARD_START + (kingSide ? 6 : 2), false);
    return true; // Always succeeds because legality is presumed to have already been checked
}

// Calls move_piece, except takes string sequence as input
bool move_piece_char(char *input, int turn) {
    // Handle castling separately
    if(!strcmp(input, "o-o")) return move_castle(turn, true);
    else if(!strcmp(input, "o-o-o")) return move_castle(turn, false);

    return move_piece(BOARD_START + input[2] - '1', BOARD_START + input[1] - 'a', BOARD_START + input[4] - '1', BOARD_START + input[3] - 'a', turn);
}

// Colour represents the player that JUST made a move
// Returns 0 for check, 1 for checkmate, 2 for stalemate, -1 for none of the above
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

// Method to be called by the main physical chessboard controller
void run_chess_algorithm(char* turnInput) {
        char *parsedInput = malloc(sizeof(char) * 10);
        understand(parsedInput, turnInput); // Will try to convert input into standardized move notation (for this program, at least)

        printf("[Message] You said: %s\n", parsedInput);
        if(!validate_input(parsedInput)) return;
        if(!validate_move(parsedInput, turn)) {
            print_tts_message("Not a legal move!");
            return;
        }

        // If this code is reached, then the move is, on first glance, "legal" (minus checks and such)
        if(!move_piece_char(parsedInput, turn)) return;
        free(parsedInput);

        // If this code is reached, move completed and uploaded to board
        switch(analyze_board(turn)) {
        case 0: // Check
            print_tts_message("Check!");
            break;
        case 1: // Checkmate
            print_tts_message(turn == white ? "Checkmate, white wins!" : "Checkmate, black wins!");
            isRunning = false;
            break;
        case 2: // Stalemate
            print_tts_message("Stalemate.");
            isRunning = false;
            break;
        }
        print_board();

        if(movesTillDraw <= 0) { // 50-move rule
            print_tts_message("50 move rule. Game is tied.");
            isRunning = false;
        }

        turn = (turn == white ? black : white);
        promote_letter = 'q'; // Reset to promoting to queen
}
