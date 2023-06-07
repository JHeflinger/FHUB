/**
 * utils.h - program helper functions for terminal output
 * author: Jason Heflinger
 * last modified: 6-6-2023
*/

// includes
#include <stdio.h>

// color identifier enum
enum COLORS {
    BLACK    = 0,
    RED      = 1,
    GREEN    = 2,
    YELLOW   = 3,
    BLUE     = 4,
    MAGENTA  = 5,
    CYAN     = 6,
    WHITE    = 7
};

// function declarations
void   setTextColor(int color);
void   setHighlight(int color);
void   setBoldText(void);
void   resetText(void);
void   getInput(char* buf, int len);

/**
 * Given a color ID (see @COLORS) changes following 
 * terminal text to be that color using ANSI escape codes
*/
void setTextColor(int color) {
    char ansi[6] = { 0 };
    ansi[0] = '\x1b'; 
    ansi[1] = '[';
    ansi[2] = '3';
    ansi[3] = color + 48; // add 48 to correct into an ASCII code
    ansi[4] = 'm';
    ansi[5] = '\0';
    printf("%s", ansi);
}

/**
 * Given a color ID (see @COLORS) changes following 
 * terminal highlight to be that color using ANSI escape codes
*/
void setHighlight(int color) {
    char ansi[6] = { 0 };
    ansi[0] = '\x1b'; 
    ansi[1] = '[';
    ansi[2] = '4';
    ansi[3] = color + 48; // add 48 to correct into an ASCII code
    ansi[4] = 'm';
    ansi[5] = '\0';
    printf("%s", ansi);
}

/**
 * Sets following terminal output to be bold using ANSI 
 * escape codes
*/
void setBoldText() {
    printf("\x1b[1m");
}

/**
 * Resets terminal colors back to normal using
 * ANSI escape codes
*/
void resetText() {
    printf("\x1b[0m");
}

/**
 * Given a buffer and the length of input to get,
 * will prompt the terminal to get an input and
 * copy it into the given buffer
*/
void getInput(char* buf, int len) {
    fgets(buf, len, stdin);
    for(int i = 0; i < len; i++)
       if (buf[i] == '\n')
            buf[i] = '\0';
}