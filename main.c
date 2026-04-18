/*
C COMPILER PIPELINE
Stages: Lexer -> Symbol Table -> Parser(AST) -> IR -> Binary
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
DATA TYPES & CONSTANTS
*/

#define MAX_TOKEN_LEN   64
#define MAX_TOKENS      256
#define MAX_SYMBOLS     64
#define MAX_AST_NODES   256
#define MAX_IR          256
#define MAX_CHILDREN    8

/*
Token kinds
*/
typedef enum{
    TOK_INT,        /* keyword: int   */
    TOK_RETURN,     /* keyword: return */
    TOK_MAIN,       /* keyword: main  */
    TOK_IDENT,      /* identifier     */
    TOK_NUMBER,     /* integer literal*/
    TOK_ASSIGN,     /* =              */
    TOK_PLUS,       /* +              */
    TOK_MINUS,      /* -              */
    TOK_STAR,       /* *              */
    TOK_SLASH,      /* /              */
    TOK_SEMI,       /* ;              */
    TOK_LPAREN,     /*(              */
    TOK_RPAREN,     /* )              */
    TOK_LBRACE,     /*{              */
    TOK_RBRACE,     /* }              */
    TOK_EOF
} TokenKind;

static const char *tok_name[] ={
    "INT","RETURN","MAIN","IDENT","NUMBER",
    "ASSIGN","PLUS","MINUS","STAR","SLASH",
    "SEMI","LPAREN","RPAREN","LBRACE","RBRACE","EOF"
};

typedef struct{
    TokenKind kind;
    char      text[MAX_TOKEN_LEN];
} Token;

/*  
Symbol table  
*/

typedef enum{SCOPE_GLOBAL, SCOPE_LOCAL} Scope;

typedef struct{
    char  name[MAX_TOKEN_LEN];
    char  type[16];   /* "int" only for now */
    Scope scope;
} Symbol;

/*  
AST node kinds  
*/
typedef enum{
    AST_PROGRAM,
    AST_FUNC_DEF,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_EXPR_BINOP,
    AST_IDENT,
    AST_NUMBER,
    AST_RETURN
} ASTKind;

static const char *ast_name[] ={
    "PROGRAM","FUNC_DEF","BLOCK","VAR_DECL",
    "ASSIGN","BINOP","IDENT","NUMBER","RETURN"
};

typedef struct ASTNode{
    ASTKind kind;
    char value[MAX_TOKEN_LEN]; /* literal text  */
    char op[4];               /* operator text */
    struct ASTNode *children[MAX_CHILDREN];
    int child_count;
} ASTNode;

/*  
3-address IR instruction  
*/
typedef struct{
    char dest[MAX_TOKEN_LEN];
    char src1[MAX_TOKEN_LEN];
    char op[4];
    char src2[MAX_TOKEN_LEN];  /* empty if unary/copy */
} IRInstr;

/* 
GLOBAL STATE
*/
static Token tokens[MAX_TOKENS];
static int token_count = 0;

static Symbol  symtab[MAX_SYMBOLS];
static int sym_count = 0;

static ASTNode ast_pool[MAX_AST_NODES];
static int ast_pool_idx = 0;

static IRInstr ir[MAX_IR];
static int  ir_count = 0;

static int temp_counter = 0; /* for t1, t2, … */

/* 
LEXER
*/

/*
Tokenize the input string.  We scan character-by-character,
classify each token, and push it into the global tokens[].
*/
static void lex(const char *src){
    int i = 0;
    int len =(int)strlen(src);

    printf("STAGE 1 - LEXICAL ANALYSIS\n");

    while(i < len){
        /* skip whitespace */
        if(isspace((unsigned char)src[i])){i++; continue;}

        Token t;
        memset(&t, 0, sizeof t);

        /* --- identifiers/keywords --- */
        if(isalpha((unsigned char)src[i]) || src[i] == '_'){
            int start = i;
            while(i < len &&(isalnum((unsigned char)src[i]) || src[i] == '_'))
                i++;
            int tlen = i - start;
            if(tlen >= MAX_TOKEN_LEN) tlen = MAX_TOKEN_LEN - 1;
            strncpy(t.text, src + start, tlen);
            t.text[tlen] = '\0';

            if(strcmp(t.text, "int")    == 0) t.kind = TOK_INT;
            else if(strcmp(t.text, "return") == 0) t.kind = TOK_RETURN;
            else if(strcmp(t.text, "main")   == 0) t.kind = TOK_MAIN;
            else t.kind = TOK_IDENT;
        }
        /* integer literals  */
        else if(isdigit((unsigned char)src[i])){
            int start = i;
            while(i < len && isdigit((unsigned char)src[i])) i++;
            int tlen = i - start;
            if(tlen >= MAX_TOKEN_LEN) tlen = MAX_TOKEN_LEN - 1;
            strncpy(t.text, src + start, tlen);
            t.text[tlen] = '\0';
            t.kind = TOK_NUMBER;
        }
        /*  single-character tokens  */
        else{
            t.text[0] = src[i];
            t.text[1] = '\0';
            switch(src[i]){
                case '=': t.kind = TOK_ASSIGN; break;
                case '+': t.kind = TOK_PLUS;   break;
                case '-': t.kind = TOK_MINUS;  break;
                case '*': t.kind = TOK_STAR;   break;
                case '/': t.kind = TOK_SLASH;  break;
                case ';': t.kind = TOK_SEMI;   break;
                case '(': t.kind = TOK_LPAREN; break;
                case ')': t.kind = TOK_RPAREN; break;
                case '{': t.kind = TOK_LBRACE; break;
                case '}': t.kind = TOK_RBRACE; break;
                default:
                    fprintf(stderr, "Unknown char '%c'\n", src[i]);
                    i++; continue;
            }
            i++;
        }

        if(token_count < MAX_TOKENS)
            tokens[token_count++] = t;
    }

    /* sentinel */
    tokens[token_count].kind = TOK_EOF;
    strcpy(tokens[token_count].text, "<EOF>");

    /* print token list */
    printf("  %-4s  %-12s  %s\n", "No.", "Kind", "Text");
    printf("  %-4s  %-12s  %s\n", "---", "------------", "----");
    for(int j = 0; j < token_count; j++){
        printf("  %-4d  %-12s  \"%s\"\n",
               j, tok_name[tokens[j].kind], tokens[j].text);
    }
    printf("\n  Total tokens: %d\n", token_count);
}

/* 
SYMBOL TABLE HELPERS
*/

static Symbol *sym_lookup(const char *name){
    for(int i = 0; i < sym_count; i++)
        if(strcmp(symtab[i].name, name) == 0)
            return &symtab[i];
    return NULL;
}

static Symbol *sym_insert(const char *name, const char *type, Scope scope){
    if(sym_lookup(name)) return sym_lookup(name); /* already present */
    if(sym_count >= MAX_SYMBOLS){ fprintf(stderr,"Symbol table full\n"); return NULL; }
    Symbol *s = &symtab[sym_count++];
    strncpy(s->name, name, MAX_TOKEN_LEN-1);
    strncpy(s->type, type, 15);
    s->scope = scope;
    return s;
}

static void print_symtab(void){
    printf("STAGE 2 - SYMBOL TABLE\n");
    printf("  %-16s  %-8s  %s\n", "Name", "Type", "Scope");
    printf("  %-16s  %-8s  %s\n", "----------------","--------","------");
    for(int i = 0; i < sym_count; i++){
        printf("  %-16s  %-8s  %s\n",
               symtab[i].name, symtab[i].type,
               symtab[i].scope == SCOPE_GLOBAL ? "global" : "local");
    }
    if(sym_count == 0) printf(" (empty)\n");
    printf("\n  Total symbols: %d\n", sym_count);
}

/* 
RECURSIVE-DESCENT PARSER  ->  AST
*/

/* parser state */
static int pos = 0;   /* current position in tokens[] */

static Token *peek(void){ return &tokens[pos]; }
static Token *advance(void){ Token *t = &tokens[pos]; if(pos < token_count) pos++; return t; }

static Token *expect(TokenKind k){
    if(peek()->kind != k){
        fprintf(stderr, "Parse error: expected %s, got \"%s\"\n",
                tok_name[k], peek()->text);
        exit(1);
    }
    return advance();
}

static ASTNode *new_node(ASTKind kind){
    if(ast_pool_idx >= MAX_AST_NODES){ fprintf(stderr,"AST pool full\n"); exit(1); }
    ASTNode *n = &ast_pool[ast_pool_idx++];
    memset(n, 0, sizeof *n);
    n->kind = kind;
    return n;
}

static void add_child(ASTNode *parent, ASTNode *child){
    if(parent->child_count < MAX_CHILDREN)
        parent->children[parent->child_count++] = child;
}

/* forward declaration */
static ASTNode *parse_stmt(Scope scope);

/*
expression  :=  term(('+' | '-') term )*
term        :=  factor(('*' | '/') factor )*
factor      :=  IDENT | NUMBER
*/
static ASTNode *parse_factor(void){
    Token *t = peek();
    if(t->kind == TOK_IDENT){
        advance();
        ASTNode *n = new_node(AST_IDENT);
        strcpy(n->value, t->text);
        return n;
    }
    if(t->kind == TOK_NUMBER){
        advance();
        ASTNode *n = new_node(AST_NUMBER);
        strcpy(n->value, t->text);
        return n;
    }
    fprintf(stderr, "Expected factor, got \"%s\"\n", t->text);
    exit(1);
}

static ASTNode *parse_term(void){
    ASTNode *left = parse_factor();
    while(peek()->kind == TOK_STAR || peek()->kind == TOK_SLASH){
        Token *op = advance();
        ASTNode *right = parse_factor();
        ASTNode *n = new_node(AST_EXPR_BINOP);
        strcpy(n->op, op->text);
        add_child(n, left);
        add_child(n, right);
        left = n;
    }
    return left;
}

static ASTNode *parse_expr(void){
    ASTNode *left = parse_term();
    while(peek()->kind == TOK_PLUS || peek()->kind == TOK_MINUS){
        Token *op = advance();
        ASTNode *right = parse_term();
        ASTNode *n = new_node(AST_EXPR_BINOP);
        strcpy(n->op, op->text);
        add_child(n, left);
        add_child(n, right);
        left = n;
    }
    return left;
}

/*
statement  :=  var_decl | assignment | return_stmt
var_decl   :=  'int' IDENT ';'
assignment :=  IDENT '=' expr ';'
return_stmt:=  'return' expr ';'
*/
static ASTNode *parse_stmt(Scope scope){
    Token *t = peek();

    /* var_decl: int x; */
    if(t->kind == TOK_INT){
        advance();
        Token *name = expect(TOK_IDENT);
        expect(TOK_SEMI);
        sym_insert(name->text, "int", scope);
        ASTNode *n = new_node(AST_VAR_DECL);
        strcpy(n->value, name->text);
        return n;
    }

    /* return expr; */
    if(t->kind == TOK_RETURN){
        advance();
        ASTNode *n = new_node(AST_RETURN);
        ASTNode *expr = parse_expr();
        add_child(n, expr);
        expect(TOK_SEMI);
        return n;
    }

    /* assignment: IDENT = expr; */
    if(t->kind == TOK_IDENT){
        Token *name = advance();
        expect(TOK_ASSIGN);
        ASTNode *expr = parse_expr();
        expect(TOK_SEMI);
        ASTNode *n = new_node(AST_ASSIGN);
        strcpy(n->value, name->text);
        add_child(n, expr);
        return n;
    }

    fprintf(stderr, "Unexpected token \"%s\" in statement\n", t->text);
    exit(1);
}

/*
block  :=  '{' stmt* '}'
*/
static ASTNode *parse_block(Scope scope){
    expect(TOK_LBRACE);
    ASTNode *block = new_node(AST_BLOCK);
    while(peek()->kind != TOK_RBRACE && peek()->kind != TOK_EOF)
        add_child(block, parse_stmt(scope));
    expect(TOK_RBRACE);
    return block;
}

/*
func_def  :=  'int' 'main' '(' ')' block
*/
static ASTNode *parse_func(void){
    expect(TOK_INT);
    Token *name = expect(TOK_MAIN);
    expect(TOK_LPAREN);
    expect(TOK_RPAREN);
    sym_insert(name->text, "int", SCOPE_GLOBAL);
    ASTNode *n = new_node(AST_FUNC_DEF);
    strcpy(n->value, name->text);
    add_child(n, parse_block(SCOPE_LOCAL));
    return n;
}
/*
program  :=  func_def*
*/
static ASTNode *parse(void){
    ASTNode *prog = new_node(AST_PROGRAM);
    while(peek()->kind != TOK_EOF)
        add_child(prog, parse_func());
    return prog;
}

/* pretty-print the AST  */
static void print_ast(ASTNode *n, int depth){
    for(int i = 0; i < depth; i++) printf(i == depth-1 ? "  ├─ " : "  │  ");
    /* root node has no indent line */
    if(depth == 0) printf("  ");

    printf("[%s]", ast_name[n->kind]);
    if(n->value[0]) printf("  \"%s\"", n->value);
    if(n->op[0])    printf("  op='%s'", n->op);
    printf("\n");

    for(int i = 0; i < n->child_count; i++)
        print_ast(n->children[i], depth + 1);
}

/* 
IR GENERATION (3-address code)
*/

static char *new_temp(char *buf){
    sprintf(buf, "t%d", ++temp_counter);
    return buf;
}

/*
Recursively lower an expression node.
Returns the name of the variable/temp holding the result.
*/
static void lower_expr(ASTNode *n, char *result){
    if(n->kind == AST_NUMBER || n->kind == AST_IDENT){
        strcpy(result, n->value);
        return;
    }
    if(n->kind == AST_EXPR_BINOP){
        char l[MAX_TOKEN_LEN], r[MAX_TOKEN_LEN];
        lower_expr(n->children[0], l);
        lower_expr(n->children[1], r);
        new_temp(result);
        IRInstr *ins = &ir[ir_count++];
        strcpy(ins->dest, result);
        strcpy(ins->src1, l);
        strcpy(ins->op,   n->op);
        strcpy(ins->src2, r);
        return;
    }
    fprintf(stderr,"IR: unexpected expr node %d\n", n->kind);
    exit(1);
}

static void lower_stmt(ASTNode *n){
    if(n->kind == AST_VAR_DECL) return; /* declarations produce no IR */

    if(n->kind == AST_ASSIGN){
        char rhs[MAX_TOKEN_LEN];
        lower_expr(n->children[0], rhs);
        IRInstr *ins = &ir[ir_count++];
        strcpy(ins->dest, n->value);
        strcpy(ins->src1, rhs);
        ins->op[0] = '\0';
        ins->src2[0] = '\0';
        return;
    }

    if(n->kind == AST_RETURN){
        char rhs[MAX_TOKEN_LEN];
        lower_expr(n->children[0], rhs);
        IRInstr *ins = &ir[ir_count++];
        strcpy(ins->dest, "ret");
        strcpy(ins->src1, rhs);
        ins->op[0] = '\0';
        ins->src2[0] = '\0';
        return;
    }

    if(n->kind == AST_BLOCK){
        for(int i = 0; i < n->child_count; i++)
            lower_stmt(n->children[i]);
        return;
    }

    if(n->kind == AST_FUNC_DEF){
        lower_stmt(n->children[0]); /* recurse into block */
        return;
    }

    if(n->kind == AST_PROGRAM){
        for(int i = 0; i < n->child_count; i++)
            lower_stmt(n->children[i]);
        return;
    }
}

static void print_ir(void){
    printf("STAGE 4 — INTERMEDIATE REPRESENTATION\n");
    printf("  3-Address Code:\n\n");
    for(int i = 0; i < ir_count; i++){
        IRInstr *ins = &ir[i];
        if(ins->op[0] && ins->src2[0])
            printf("    %s  =  %s  %s  %s\n",
                   ins->dest, ins->src1, ins->op, ins->src2);
        else
            printf("    %s  =  %s\n", ins->dest, ins->src1);
    }
    printf("\n  Total IR instructions: %d\n", ir_count);
}

/*
BINARY ENCODING
 *  Custom 8-bit opcode scheme:
 *    DECL_INT   0x01 (00000001)  declare int variable
 *    COPY       0x02 (00000010)  dest = src
 *    ADD        0x03 (00000011)  dest = src1 + src2
 *    SUB        0x04 (00000100)  dest = src1 - src2
 *    MUL        0x05 (00000101)  dest = src1 * src2
 *    DIV        0x06 (00000110)  dest = src1 / src2
 *    RET        0x07 (00000111)  return src
 *
 *  Each operand encoded as one of:
 *    TYPE_IDENT  0x0A (00001010) followed by name bytes
 *    TYPE_NUM    0x0B (00001011) followed by value bytes
 *    TERM        0x00 (00000000) string terminator
 *
 *  Output: space-separated 8-bit binary strings per field.
*/

/* Map single character to 8-bit binary string */
static void byte_to_bin(unsigned char byte, char *buf){
    for(int i = 7; i >= 0; i--)
        buf[7-i] =((byte >> i) & 1) ? '1' : '0';
    buf[8] = '\0';
}

static void emit_bin_byte(unsigned char b){
    char buf[9];
    byte_to_bin(b, buf);
    printf("%s ", buf);
}

static void emit_bin_string(const char *s){
    while(*s){
        emit_bin_byte((unsigned char)*s);
        s++;
    }
    emit_bin_byte(0x00); /* null terminator */
}

static void encode_binary(void){
    printf("STAGE 5 — BINARY ENCODING\n");
    printf("  Encoding scheme:\n");
    printf("    OPCODE: DECL_INT=00000001  COPY=00000010\n");
    printf("            ADD=00000011  SUB=00000100\n");
    printf("            MUL=00000101  DIV=00000110  RET=00000111\n");
    printf("    OPERAND PREFIX: IDENT=00001010  NUM=00001011\n");
    printf("    TERMINATOR: 00000000\n\n");

    for(int i = 0; i < ir_count; i++){
        IRInstr *ins = &ir[i];
        printf("  [%d] ", i);

        /* determine opcode */
        unsigned char opcode;
        if(strcmp(ins->dest, "ret") == 0)      opcode = 0x07;
        else if(!ins->op[0])                    opcode = 0x02; /* COPY */
        else if(strcmp(ins->op, "+") == 0)      opcode = 0x03;
        else if(strcmp(ins->op, "-") == 0)      opcode = 0x04;
        else if(strcmp(ins->op, "*") == 0)      opcode = 0x05;
        else if(strcmp(ins->op, "/") == 0)      opcode = 0x06;
        else                                     opcode = 0x02;

        emit_bin_byte(opcode);

        /* encode dest */
        int dest_is_num =(ins->dest[0] >= '0' && ins->dest[0] <= '9');
        emit_bin_byte(dest_is_num ? 0x0B : 0x0A);
        emit_bin_string(ins->dest);

        /* encode src1 */
        int s1_is_num =(ins->src1[0] >= '0' && ins->src1[0] <= '9');
        emit_bin_byte(s1_is_num ? 0x0B : 0x0A);
        emit_bin_string(ins->src1);

        /* encode src2(if present) */
        if(ins->src2[0]){
            int s2_is_num =(ins->src2[0] >= '0' && ins->src2[0] <= '9');
            emit_bin_byte(s2_is_num ? 0x0B : 0x0A);
            emit_bin_string(ins->src2);
        }

        printf("\n");
    }
    printf("\n  Binary encoding complete.\n");
}

/*
MAIN
*/
int main(int argc, char *argv[]){
    /* default program or accept CLI argument */
    const char *src =(argc > 1) ? argv[1]
        : "int main(){ int x; int y; y = 5; x = y + 2; return x; }";

    printf("  C COMPILER PIPELINE\n");
    printf("\n");
    printf("  Input source:\n  > %s\n", src);

    /* Stage 1: Lex */
    lex(src);

    /* Stage 3: Parse -> also fills symbol table(stage 2) */
    ASTNode *root = parse();

    /* Stage 2: print symbol table(filled during parse) */
    print_symtab();

    /* Stage 3 continued: print AST */
    printf("sTAGE 3 — ABSTRACT SYNTAX TREE\n");
    print_ast(root, 0);

    /* Stage 4: IR */
    lower_stmt(root);
    print_ir();

    /* Stage 5: Binary */
    encode_binary();

    printf("yup, thats how you do it. - Aaryan Singh\n");
    return 0;
}