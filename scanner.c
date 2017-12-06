#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
#include "display.h"
#include "allocator.h"

typedef struct {
    const char* name;
    size_t      length;
    TokenType   type;
} Keyword;

// The table of reserved words and their associated token types.
static Keyword keywords[] = {
    {"Begin",   5, TOKEN_BEGIN},
    {"End",     3, TOKEN_END},

    {"True",    4, TOKEN_TRUE},
    {"False",   5, TOKEN_FALSE},
    {"Null",    4, TOKEN_NULL},
    {"And",     3, TOKEN_AND},
    {"Or",      2, TOKEN_OR},
    {"Int",     3, TOKEN_INT},
    {"Float",   5, TOKEN_FLOAT},

    {"Set",     3, TOKEN_SET},
    {"Array",   5, TOKEN_ARRAY},
    {"Input",   5, TOKEN_INPUT},

    {"If",      2, TOKEN_IF},
    {"Then",    4, TOKEN_THEN},
    {"Else",    4, TOKEN_ELSE},
    {"EndIf",   5, TOKEN_ENDIF},

    {"For",     3, TOKEN_FOR},
    {"EndFor",  6, TOKEN_ENDFOR},

    {"While",   5, TOKEN_WHILE},
    {"Break",   5, TOKEN_BREAK},
    {"EndWhile",8, TOKEN_ENDWHILE},

    {"Do",      2, TOKEN_DO},
    {"EndDo",   5, TOKEN_ENDDO},

    {"Print",   5, TOKEN_PRINT},

    {"Routine", 7, TOKEN_ROUTINE},
    {"EndRoutine", 10, TOKEN_ENDROUTINE},
    {"Call",    4, TOKEN_CALL},
    {"Return",  6, TOKEN_RETURN},
    {"Foreign", 7, TOKEN_FOREIGN},

    {"Container", 9, TOKEN_CONTAINER},
    {"EndContainer", 12, TOKEN_ENDCONTAINER},

    // Sentinel to mark the end of the array.
    {NULL,      0, TOKEN_EOF}
};

typedef struct {
    const char* source;
    const char* tokenStart;
    const char* current;
    const char* fileName;
    int line;
} Scanner;

static Scanner scanner;
static int se = 0, fsp = 0;
static char **fileStack = NULL;

static void fstack_push(char *value){
    int i = 0;
    while(i < fsp){
        if(strcmp(fileStack[i], value) == 0)
            return;
        i++;
    }
    fileStack = (char **)reallocate(fileStack, sizeof(char) * ++fsp);
    fileStack[fsp - 1] = value;
}

static char* fstack_pop(){
    return fileStack[--fsp];
}

static int initScanner(const char* source, const char* file) {
    if(source == NULL)
        return 0;
    scanner.source = source;
    scanner.tokenStart = source;
    scanner.current = source;
    scanner.fileName = file;
    scanner.line = 1;
    return 1;
}

char* read_all(const char* s){
    FILE *source = fopen(s, "rb");
    if(source == NULL){
        err("Unable to open file : '%s'", s);
        se++;
        return NULL;
    }
    fseek(source, 0, SEEK_END);
    long fsize = ftell(source);
    fseek(source, 0, SEEK_SET);

    char *string = (char *)mallocate(fsize + 1);
    fread(string, fsize, 1, source);
    fclose(source);

    string[fsize] = 0;

    return string;
}

// Returns 1 if `c` is an English letter or underscore.
static int isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_';
}

// Returns 1 if `c` is a digit.
static int isDigit(char c) {
    return c >= '0' && c <= '9';
}

// Returns 1 if `c` is an English letter, underscore, or digit.
static int isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

static int isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static int match(char expected) {
    if (isAtEnd()) return 0;
    if (*scanner.current != expected) return 0;

    scanner.current++;
    return 1;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.tokenStart;
    token.length = (int)(scanner.current - scanner.tokenStart);
    token.fileName = scanner.fileName;
    token.line = scanner.line;

    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;

    return token;
}

static Token identifier() {
    while (isAlphaNumeric(peek())) advance();

    TokenType type = TOKEN_IDENTIFIER;

    // See if the identifier is a reserved word.
    size_t length = scanner.current - scanner.tokenStart;
    Keyword *keyword;
    for (keyword = keywords; keyword->name != NULL; keyword++) {
        if (length == keyword->length &&
                memcmp(scanner.tokenStart, keyword->name, length) == 0) {
            type = keyword->type;
            break;
        }
    }

    return makeToken(type);
}

static Token number() {
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the "."
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        else if(peek() == '\\' && peekNext() == '"')
            advance();
        advance();
    }

    // Unterminated string.
    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing ".
    advance();
    return makeToken(TOKEN_STRING);
}

static int skipEmptyLine(){
    short hasOtherChars = 0;
    const char *bak = scanner.current;
    while(!isAtEnd() && peek() != '\n'){
        if(peek() != ' ' && peek() != '\t' && peek() != '\r'){
            hasOtherChars = 1;
            break;
        }
        advance();
    }
    if(isAtEnd() && !hasOtherChars)
        return 0;
    if(!hasOtherChars){
        advance();
        scanner.line++;
        return 1;
    }
    else
        if(peek() == '/' && peekNext() == '/'){
            while(!isAtEnd() && peek() != '\n')
                advance();
            if(!isAtEnd()){
                scanner.line++;
                advance();
            }
            return 1;
        }
        else if(peek() == '/' && peekNext() == '*'){ 
            while(!(peek()=='*' && peekNext()=='/') && peek()!='\0'){
                if(peek() == '\n')
                    scanner.line++;
                advance();
            }
            if(!isAtEnd()){
                advance();
                advance();
                if(!isAtEnd() && peek() == '\n'){
                    advance();
                    scanner.line++;
                }
            }
        }
        else
            scanner.current = bak;
    }
    return 0;
}

static int startsWith(const char *source, const char *predicate){
    size_t slen = strlen(source), plen = strlen(predicate), i = 0;
    if(slen < plen)
        return 0;

    while(i < plen){
        if(source[i] != predicate[i])
            return 0;
        i++;
    }

    return 1;
}

static char* str_part(const char* source, int start){
    int i = 0;
    char *part = NULL;
    while(source[start + i] != '\n' && source[start + i] != '\r' && source[start + i] != '\0'){
        //        printf(debug("Current char : [%c] i : %d"), source[start+i], i);
        part = (char *)reallocate(part, sizeof(char) * (i + 1));
        part[i] = source[start + i];
        i++;
    }

    i++;
    part = (char *)reallocate(part, sizeof(char) * i);
    part[i - 1] = '\0';

    //    printf(debug("Parted for inclusion : [%s]"), part);

    return part;
}

static int checkInclude(){
    if(!startsWith(scanner.current, "Include "))
        return 0;
    //printf(debug("Found include statement!"));
    fstack_push(str_part(scanner.current, 8));
    while(peek() != '\n' && !isAtEnd())
        advance();
    if(!isAtEnd())
        advance();
    return 1;
}

static Token scanToken() {

    // The next token starts with the current character.
    scanner.tokenStart = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case ' ': {
                      int count = 1;
                      while(peek() == ' ' && count < 4){
                          count++;
                          advance();
                      }
                      if(count == 4){
                          return makeToken(TOKEN_INDENT);
                      }
                      else
                          return scanToken(); // Ignore all other spaces
                  }
        case '\r':
                  if(peek() == '\n'){
                      advance();
                      scanner.line++;
                      return makeToken(TOKEN_NEWLINE);
                  }
                  return scanToken(); // Ignore \r
        case '\n':
                  scanner.line++;
                  return makeToken(TOKEN_NEWLINE);
        case '\t': return makeToken(TOKEN_INDENT);
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_SQUARE);
        case ']': return makeToken(TOKEN_RIGHT_SQUARE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ':': return makeToken(TOKEN_COLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': 
                  if(match('/')){
                      while(peek()!='\n' && peek()!='\0')
                          advance();
                      return scanToken();
                  }
                  else if(match('*')){
                      while(!(peek()=='*' && peekNext()=='/') && peek()!='\0'){
                          if(peek() == '\n')
                              scanner.line++;
                          advance();
                      }
                      if(!isAtEnd()){
                          advance();
                          advance();
                      }
                      return scanToken();
                  }
                  return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '%': return makeToken(TOKEN_PERCEN);
        case '^': return makeToken(TOKEN_CARET);
        case '!':
                  if (match('=')) return makeToken(TOKEN_BANG_EQUAL);
                  return makeToken(TOKEN_BANG);

        case '=':
                  if (match('=')) return makeToken(TOKEN_EQUAL_EQUAL);
                  return makeToken(TOKEN_EQUAL);

        case '<':
                  if (match('=')) return makeToken(TOKEN_LESS_EQUAL);
                  return makeToken(TOKEN_LESS);

        case '>':
                  if (match('=')) return makeToken(TOKEN_GREATER_EQUAL);
                  return makeToken(TOKEN_GREATER);

        case '"': return string();
        default:
                  lnerr("Unexpected character %c!", (Token){TOKEN_EOF, NULL, 0, scanner.line, scanner.fileName}, c);
                  se++;
                  return makeToken(TOKEN_EOF);
    }
}

static TokenList *newList(Token t){ 
    TokenList *now = (TokenList *)mallocate(sizeof(TokenList));
    now->value = t;
    now->next = NULL;
    return now;
}

static void insertList(TokenList **head, TokenList **prev, TokenList *toInsert){
    if(*head == NULL)
        (*head) = toInsert;
    else
        (*prev)->next = toInsert;

    (*prev) = toInsert;
}

static int scanfile(){
    char *file = fstack_pop();
    return initScanner(read_all(file), file);
}

TokenList* scanTokens(char *file){
    TokenList *ret = NULL, *head = NULL;
    Token t;
    fstack_push(file);
    while(fsp != 0){
        if(!scanfile())
            return NULL;
        while(skipEmptyLine() || checkInclude());
        while((t = scanToken()).type != TOKEN_EOF){
            insertList(&head, &ret, newList(t));
            if(t.type == TOKEN_NEWLINE){
                while(skipEmptyLine() || checkInclude());
            }
        }
    }

    insertList(&head, &ret, newList(makeToken(TOKEN_EOF)));

    return head;
}

void printList(TokenList *list){
    if(list == NULL){
        printf("\n[Error] Empty list!");
        return;
    }
    printf("\n");
    while(list->value.type != TOKEN_EOF){
        printf("%s ", tokenNames[list->value.type]);
        if(list->value.type == TOKEN_NEWLINE)
            printf("\n") ;
        else if(list->value.type == TOKEN_ERROR){
            printf("\n[Error] %s\n", list->value.start);
        }
        list = list->next; 
    }
    printf("TOKEN_EOF\n");
}

void freeList(TokenList *list){
    while(list != NULL){
        TokenList *bak = list->next;
        memfree(list);
        list = bak;
    }
}

int hasScanErrors(){
    return se;
}
