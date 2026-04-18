/* I need help here, to make the approach and process for a better general case. There are many edge cases not taken care of here
It would really help if you could atleast list them out aand even better if you solve them
Always open to new suggestions
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKEN_LEN   64
#define MAX_TOKENS      256
#define MAX_SYMBOLS     64
#define MAX_AST_NODES   256
#define MAX_IR          256
#define MAX_CHILDREN    8

typedef enum{
    TOK_INT,
    TOK_RETURN,
    TOK_MAIN,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_SEMI,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
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

typedef enum{SCOPE_GLOBAL, SCOPE_LOCAL} Scope;

typedef struct{
    char  name[MAX_TOKEN_LEN];
    char  type[16];
    Scope scope;
} Symbol;

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
    char value[MAX_TOKEN_LEN];
    char op[4];
    struct ASTNode *children[MAX_CHILDREN];
    int child_count;
} ASTNode;

typedef struct{
    char dest[MAX_TOKEN_LEN];
    char src1[MAX_TOKEN_LEN];
    char op[4];
    char src2[MAX_TOKEN_LEN];
} IRInstr;

static Token tokens[MAX_TOKENS];
static int token_count = 0;

static Symbol  symtab[MAX_SYMBOLS];
static int sym_count = 0;

static ASTNode ast_pool[MAX_AST_NODES];
static int ast_pool_idx = 0;

static IRInstr ir[MAX_IR];
static int  ir_count = 0;

static int temp_counter = 0;

static void lex(const char *src){
    int i = 0;
    int len =(int)strlen(src);

    printf("STAGE 1 - LEXICAL ANALYSIS\n");

    while(i < len){
        if(isspace((unsigned char)src[i])){i++; continue;}

        Token t;
        memset(&t, 0, sizeof t);

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
        else if(isdigit((unsigned char)src[i])){
            int start = i;
            while(i < len && isdigit((unsigned char)src[i])) i++;
            int tlen = i - start;
            if(tlen >= MAX_TOKEN_LEN) tlen = MAX_TOKEN_LEN - 1;
            strncpy(t.text, src + start, tlen);
            t.text[tlen] = '\0';
            t.kind = TOK_NUMBER;
        }
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

    tokens[token_count].kind = TOK_EOF;
    strcpy(tokens[token_count].text, "<EOF>");

    printf("  %-4s  %-12s  %s\n", "No.", "Kind", "Text");
    printf("  %-4s  %-12s  %s\n", "---", "------------", "----");
    for(int j = 0; j < token_count; j++){
        printf("  %-4d  %-12s  \"%s\"\n",
               j, tok_name[tokens[j].kind], tokens[j].text);
    }
    printf("\n  Total tokens: %d\n", token_count);
}

static Symbol *sym_lookup(const char *name){
    for(int i = 0; i < sym_count; i++)
        if(strcmp(symtab[i].name, name) == 0)
            return &symtab[i];
    return NULL;
}

static Symbol *sym_insert(const char *name, const char *type, Scope scope){
    if(sym_lookup(name)) return sym_lookup(name);
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

static int pos = 0;

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

static ASTNode *parse_stmt(Scope scope);

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

static ASTNode *parse_stmt(Scope scope){
    Token *t = peek();

    if(t->kind == TOK_INT){
        advance();
        Token *name = expect(TOK_IDENT);
        expect(TOK_SEMI);
        sym_insert(name->text, "int", scope);
        ASTNode *n = new_node(AST_VAR_DECL);
        strcpy(n->value, name->text);
        return n;
    }

    if(t->kind == TOK_RETURN){
        advance();
        ASTNode *n = new_node(AST_RETURN);
        ASTNode *expr = parse_expr();
        add_child(n, expr);
        expect(TOK_SEMI);
        return n;
    }

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

static ASTNode *parse_block(Scope scope){
    expect(TOK_LBRACE);
    ASTNode *block = new_node(AST_BLOCK);
    while(peek()->kind != TOK_RBRACE && peek()->kind != TOK_EOF)
        add_child(block, parse_stmt(scope));
    expect(TOK_RBRACE);
    return block;
}

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

static ASTNode *parse(void){
    ASTNode *prog = new_node(AST_PROGRAM);
    while(peek()->kind != TOK_EOF)
        add_child(prog, parse_func());
    return prog;
}

static void print_ast(ASTNode *n, int depth){
    for(int i = 0; i < depth; i++) printf(i == depth-1 ? "  ├─ " : "  │  ");
    if(depth == 0) printf("  ");

    printf("[%s]", ast_name[n->kind]);
    if(n->value[0]) printf("  \"%s\"", n->value);
    if(n->op[0])    printf("  op='%s'", n->op);
    printf("\n");

    for(int i = 0; i < n->child_count; i++)
        print_ast(n->children[i], depth + 1);
}

static char *new_temp(char *buf){
    sprintf(buf, "t%d", ++temp_counter);
    return buf;
}

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
    if(n->kind == AST_VAR_DECL) return;

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
        lower_stmt(n->children[0]);
        return;
    }

    if(n->kind == AST_PROGRAM){
        for(int i = 0; i < n->child_count; i++)
            lower_stmt(n->children[i]);
        return;
    }
}

static void print_ir(void){
    printf("STAGE 4 - INTERMEDIATE REPRESENTATION\n");
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
 * STAGE 5 - x86-64 ASSEMBLY GENERATION (AT&T syntax)
 *
 * Strategy:
 *   - All local variables and temporaries are mapped to 4-byte slots on the stack.
 *   - %rbp is the frame base pointer; offsets are negative from %rbp.
 *   - For binary ops: load src1 into %eax, load src2 into %ecx, apply op, store result.
 *   - Division uses idivl: dividend in %eax, sign-extended into %edx:%eax via cltd,
 *     divisor in %ecx; quotient lands in %eax.
 *   - Return value is placed in %eax, which is the System V ABI convention.
 */

/* variable-to-stack-offset table */
#define MAX_VARS 128
static struct { char name[MAX_TOKEN_LEN]; int offset; } var_map[MAX_VARS];
static int var_map_count = 0;
static int next_offset   = 0; /* grows downward, stored as positive, used as negative */

/* look up or allocate a stack slot for a name */
static int get_offset(const char *name){
    for(int i = 0; i < var_map_count; i++)
        if(strcmp(var_map[i].name, name) == 0)
            return var_map[i].offset;
    next_offset += 4;
    var_map[var_map_count].offset = next_offset;
    strncpy(var_map[var_map_count].name, name, MAX_TOKEN_LEN - 1);
    var_map_count++;
    return next_offset;
}

/* true if the string is a plain integer literal */
static int is_number(const char *s){
    if(!s || !*s) return 0;
    for(int i = 0; s[i]; i++)
        if(!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

/* emit: load operand into a register (%eax or %ecx) */
static void emit_load(const char *operand, const char *reg){
    if(is_number(operand))
        printf("    movl    $%s, %%%s\n", operand, reg);
    else
        printf("    movl    -%d(%%rbp), %%%s\n", get_offset(operand), reg);
}

static void emit_asm(void){
    printf("STAGE 5 - ASSEMBLY GENERATION (x86-64 AT&T)\n\n");

    /* calculate total frame size needed: one slot per unique var/temp + 16-byte align */
    int frame_vars = ir_count + 8; /* rough upper bound */
    int frame_size = frame_vars * 4;
    if(frame_size % 16 != 0) frame_size += 16 - (frame_size % 16);

    /* function prologue */
    printf("    .text\n");
    printf("    .globl  main\n");
    printf("main:\n");
    printf("    pushq   %%rbp\n");
    printf("    movq    %%rsp, %%rbp\n");
    printf("    subq    $%d, %%rsp\n\n", frame_size);

    for(int i = 0; i < ir_count; i++){
        IRInstr *ins = &ir[i];
        printf("    # IR[%d]: ", i);
        if(ins->op[0] && ins->src2[0])
            printf("%s = %s %s %s\n", ins->dest, ins->src1, ins->op, ins->src2);
        else
            printf("%s = %s\n", ins->dest, ins->src1);

        if(strcmp(ins->dest, "ret") == 0){
            /* return: load return value into %eax, then epilogue */
            emit_load(ins->src1, "eax");
            printf("    movq    %%rbp, %%rsp\n");
            printf("    popq    %%rbp\n");
            printf("    ret\n");
        }
        else if(!ins->op[0]){
            /* simple copy: src1 -> dest */
            emit_load(ins->src1, "eax");
            printf("    movl    %%eax, -%d(%%rbp)\n\n", get_offset(ins->dest));
        }
        else{
            /* binary operation */
            emit_load(ins->src1, "eax");
            emit_load(ins->src2, "ecx");

            if(strcmp(ins->op, "+") == 0){
                printf("    addl    %%ecx, %%eax\n");
            }
            else if(strcmp(ins->op, "-") == 0){
                printf("    subl    %%ecx, %%eax\n");
            }
            else if(strcmp(ins->op, "*") == 0){
                printf("    imull   %%ecx, %%eax\n");
            }
            else if(strcmp(ins->op, "/") == 0){
                /* idivl requires %edx:%eax as dividend; cltd sign-extends %eax into %edx */
                printf("    cltd\n");
                printf("    idivl   %%ecx\n");
            }

            printf("    movl    %%eax, -%d(%%rbp)\n\n", get_offset(ins->dest));
        }
    }

    printf("\n  Assembly generation complete.\n");
}

int main(int argc, char *argv[]){
    const char *src =(argc > 1) ? argv[1]
        : "int main(){ int x; int y; y = 5; x = y + 2; return x; }";

    printf("  C COMPILER PIPELINE\n");
    printf("\n");
    printf("  Input source:\n  > %s\n", src);

    lex(src);

    ASTNode *root = parse();

    print_symtab();

    printf("STAGE 3 - ABSTRACT SYNTAX TREE\n");
    print_ast(root, 0);

    lower_stmt(root);
    print_ir();

    emit_asm();

    printf("yup, thats how you do it. - Aaryan Singh\n");
    return 0;
}