#include "Builder.h"

typedef union DirArg {
    char s[128];
    short int sd;
    long int ld;
    unsigned int u;
} DirArg;

static struct m{
    char id[10];
    unsigned short int opcode;
    __extension__ bool n_arg:1;
    __extension__ bool option:1;
}
mne[MAX_INSTR] =
{
    {     "LD" , 0x01 , true  , false },
    {     "LD-", 0x02 , true  , false },
    {     "LD|", 0x03 , true  , false },
    {    "LDmq", 0x0A , false , false },
    { "LDmq_mx", 0x09 , true  , false },
    {      "ST", 0x21 , true  , false },
    {     "JMP", 0x0D , true  , true  },
    {   "JUMP+", 0x0F , true  , true  },
    {     "ADD", 0x05 , true  , false },
    {    "ADD|", 0x07 , true  , false },
    {     "SUB", 0x06 , true  , false },
    {    "SUB|", 0x08 , true  , false },
    {     "MUL", 0x0B , true  , false },
    {     "DIF", 0x0C , true  , false },
    {     "LSH", 0x14 , false , false },
    {     "RSH", 0x15 , false , false },
    {  "STaddr", 0x12 , true  , true  }
};


static struct d{
    TypeDir dirType;
    char id[10];
    __extension__ unsigned short int n_arg:2;
    struct Arg{
        TypeArg type[4];
        long long int lowest;
        long long int highest;
    }arg[2];
}
dir[MAX_DIR] =
{
    {   SET ,  "set", 2, {
        {{SYM}                                         },
        {{HEX, DEC}         , 0           , 0x7FFFFFFF }
    }},
    {   ORG ,  "org", 1, {
        {{HEX, DEC}          , 0           , 0x3FF     }
    }},
    { ALIGN ,"align", 1, {
        {{DEC}              , 0           , 0x3FF      }
    }},
    { WFILL ,"wfill", 2, {
        {{     DEC          }, 1          , 0x3FF      },
        {{HEX, DEC, ROT, SYM}, -0x80000000, 0x7FFFFFFF }
    }},
    {  WORD , "word", 1, {
        {{HEX, DEC, ROT, SYM}, 0          , 0x7FFFFFFF }
    }}
};

bool end_line(FILE * src, String expr, unsigned int line){
    bool ok;
    fpos_t pos;
    String strarg = NULL;
    unsigned int el = 0;

    fgetpos(src, &pos);
    strarg = fgetword(src, &el ,&ok);
    /* Conseguiu pegar argumentos extras */
    if(el == 0){
        stderror(
            line,
            "Extra argument \"%s\" for %s\n",
            strarg,
            expr
        );
    }
    if(strarg){
        free(strarg);
    }
    fsetpos(src, &pos);
    return el > 0;
}
bool validate_arg_format(
    String str,
    TypeArg * t,
    bool  *instr,
    unsigned int line,
    HashT dict,
    bool second_time
){
    unsigned short int i;
    char th;
    unsigned int j, len;
    String aux;
    bool ok = true;
    for(len = 0; str[len]; len++);
    len++;
    aux = malloc(len*sizeof(char));
    for(j = 0; j < len; j++){
        aux[j] = str[j];
    }
    *t = 0;
    if(aux[0] == '"' && aux[strlen(aux) -1] == '"'){
        *instr = true;
        sscanf(aux, "\"%[^\"]\"", aux);

    }else{
        *instr = false;
    }
    if(aux[0] == '0' && aux[1] == 'x'){
        *t = HEX;
        for(i = 2; aux[i] && ok; i++){
            if(!hexadecimal(aux[i])){
                stderror(line, "%c is not a valid hexadecimal character\n", aux[i]);
                ok = false;
            }
        }
    }else if(between('0', aux[0], '9') || aux[0] == '-'){
        *t = DEC;
        for(i = 1; aux[i] && ok; i++){
            if(!decimal(aux[i])){
                stderror(line,"%c is not a valid decimal character\n",  aux[i]);
                ok = false;
            }
        }
    }else if(is_in_HashT(dict, aux, &th)){
        switch (th) {
            case 'S':
                *t = SYM;
                break;
            case 'L':
                *t = ROT;
                break;
            default:
                *t = 0;
                ok =  false;
                break;
        }
    }else{
        if(second_time){
            stderror(
                line,
                "Invalid argument: %s\n",
                aux
            );
        }
    }
    free(aux);
    return ok;
}

bool validate_instr(String str, TypeInstr *t, unsigned int line){
    unsigned short int i;
    for(i = 0; i < MAX_INSTR && strcmp(str, mne[i].id); i++);
    if(i == MAX_INSTR){
        return false;
    }
    *t = i;
    return true;
}

bool interpret_instr(
    FILE * src,
    TypeInstr t,
    unsigned int line,
    MemMap map,
    HashT dict,
    bool second_time
){
    bool ok, instr = true;
    unsigned int el, arg;
    char c;
    String strarg;
    TypeArg ta;
    /*String aux;*/
    /* Se a instrução precisa de argumento */
    if(!between(LD, t, STaddr)){
        stderror(
            line,
            "Could not identify the instruction\n"
        );
    }
    if(mne[t].n_arg){
        el = 0;
        ok = true;
        strarg = fgetword(src, &el ,&ok);
                if(el > 0 || !ok){
            stderror(
                line,
                "Could not get a necessary argument to %s instruction\n",
                mne[t].id
            );
            return false;
        }
        ok = validate_arg_format(strarg, &ta, &instr, line, dict, second_time);
        if(!instr){
            stderror(
                line,
                "It is not formated as the argument of a instruction\n"
            );
            return false;
        }
        sscanf(strarg, "\"%[^\"]\"", strarg);
        if(ta == DEC){
            sscanf(strarg, "%u", &arg);
            if(!between(0, arg, MAX_MEM_MAP)){
                stderror(line, "Argument %d out of bounds\n", arg);
                ok = false;
            }
        }else if(ta == HEX){
            sscanf(strarg, "0x%X", &arg);
            if(!between(0, arg, MAX_MEM_MAP)){
                stderror(line, "Argument 0x%03X out of bounds\n", arg);
                ok = false;
            }
        }else if(ta == ROT || ta == SYM){
            if(get_HashT(dict, strarg, &arg, &c)){
                if(ta == SYM && c != 'S'){
                    stderror(
                        line,
                        "Argument %s shoud be a SYM but it is a %c\n",
                        strarg, c
                    );
                }else if(ta == ROT && c != 'L'){
                    stderror(
                        line,
                        "Argument %s shoud be a ROT but it is a %c\n",
                        strarg, c
                    );
                }else if(!char_in_string(c, "LS")){
                    stderror(
                        line,
                        "Invalid argument: %s\n",
                        strarg
                    );
                }
            }else{
                if(second_time){
                    stderror(
                        line,
                        "Invalid argument: %s\n",
                        strarg
                    );
                }
            }
        }

        free(strarg);
    }
    /* Verificar se não há argumentos extras */
    ok = ok ? end_line(src, mne[t].id, line) : false;
    ok = ok ? insert_instr_MemMap(map, mne[t].opcode, arg, line) : false;
    return ok;
}
void dir_set(String name, String arg1, unsigned int arg, HashT dict){
    put_HashT(dict, arg1, arg, 'S');
}

bool validate_label(String str, unsigned int line, HashT ht){
    bool resp = true;
    char c = ' ';
    unsigned int i;
    if(decimal(str[0])){
        stderror(line, "Erro, rotulo nao deve começar com número\n");
        return false;
    }
    for(i = 0; str[i] == ':'; i++){
        if(!(alphanumeric(str[i]) || str[i] == '_')){
            stderror(line, " %c is not a valid Label character\n", str[i]);
        }
    }
    if(is_in_HashT(ht, str, &c) && c != 'L'){
        resp = false;
    }
    return resp;
}

bool validate_dir(String str, TypeDir * t, unsigned int line){
    unsigned short int i;
    sscanf(str, ".%s", str);
    for(i = 0; i < MAX_DIR && strcmp(str, dir[i].id); i++);
    if(i == MAX_DIR){
        stderror(line, "[.%s] is not a valid directive\n", str);
        return false;
    }
    *t = i;
    return true;
}
bool threat_dir(
    TypeDir t,
    DirArg arg[],
    unsigned int line,
    HashT ht,
    MemMap m
){
    bool ok = true;
    unsigned int i;
    switch(t){
        case SET:
            put_HashT(ht, arg[0].s, arg[1].u, 'S');
            break;

        case ORG:
            m->pos = arg[0].u*2;
            break;

        case ALIGN:
            m->pos >>= 1;
            m->pos = ((m->pos)/arg[0].u)*arg[0].u;
            m->pos <<= 1;
            break;

        case WORD:
            if(m->pos % 2 == 1){
                stderror(line, "Trying to put a word in the right\n");
                ok = false;
            }else if(m->pos/2 + 1 >= IAS_MAX_LINE_NUMBER){
                stderror(line, "Wfill overflows the available memory\n");
                ok = false;
            }
            ok = ok ? insert_word_MemMap(m, arg[0].u, line) : false;
            break;

        case WFILL:
            if(m->pos % 2 == 1){
                stderror(line, "Trying to wfill from the right");
            }if(m->pos/2 + arg[0].u >= IAS_MAX_LINE_NUMBER){
                stderror(line, "Wfill overflows the available memory\n");
            }
            for(i = 0; i < arg[0].u; i++){
                ok = ok ? insert_word_MemMap(m, arg[1].ld, line) : false;
            }
            break;

        case NONE:
            return false;
            break;
    }
    return ok;
}
bool interpret_dir(
    FILE * src,
    TypeDir t,
    unsigned int line,
    MemMap map,
    HashT dict,
    bool second_time
){
    unsigned short int i, j;
    bool ok, instr = true, arg_flag;
    unsigned int el;
    DirArg arg[2];
    char c;
    String strarg;
    TypeArg ta = NONE;
    /*String aux;*/
    /* Se a instrução precisa de argumento */
    if(!between(SET ,t, WORD)){
        stderror(
            line,
            "Could not identify the directive\n"
        );
    }
    for(i = 0, el = 0, ok = true; i < dir[t].n_arg && ok; i++){
        strarg = fgetword(src, &el ,&ok);
        if(el > 0 || !ok){
            stderror(
                line,
                "Could not get a necessary argument to %s directive\n",
                dir[t].id
            );
            return false;
        }
        ok = validate_arg_format(
            strarg,
            &ta,
            &instr,
            line,
            dict,
            second_time
        );
        if(instr){
            stderror(
                line,
                "It is formated as the argument of a instruction, but %s is a directive\n",
                dir[t].id
            );
            return false;
        }
        arg_flag = false;
        /* arg_flag = ta in dir[t].arg[i].type*/
        for(j = 0; j < 4 && !arg_flag && ok; j++){
            if(ta == dir[t].arg[i].type[j]){
                arg_flag = true;
                if(ta == DEC){
                    sscanf(strarg, "%ld", &arg[i].ld);
                    if(!between(
                        dir[t].arg[i].lowest,
                        arg[i].ld,
                        dir[t].arg[i].highest
                    )){
                        stderror(
                            line,
                            "Argument %ld out of bounds\n",
                            arg[i].ld
                        );
                        ok = false;
                    }
                }else if(ta == HEX){
                    sscanf(strarg, "0x%X", &arg[i].u);
                    if(!between(
                        dir[t].arg[i].lowest,
                        arg[i].u, dir[t].arg[i].highest
                    )){
                        stderror(
                            line,
                            "Argument 0x%010X out of bounds\n",
                            arg[i].u
                        );
                        ok = false;
                    }
                }
                else if(ta == ROT || ta == SYM){
                    if(get_HashT(dict, strarg, &arg[i].u, &c)){
                        if(ta == SYM && c != 'S'){
                            stderror(
                                line,
                                "Argument %s shoud be a SYM but it is a %c\n",
                                strarg, c
                            );
                        }else if(ta == ROT && c != 'L'){
                            stderror(
                                line,
                                "Argument %s shoud be a ROT but it is a %c\n",
                                strarg, c
                            );
                        }else if(c == 'L' && t == SET && i == 0){
                            stderror(
                                line,
                                "Redeclaration of %s\n",
                                strarg
                            );
                        }else if(!char_in_string(c, "LS")){
                            stderror(
                                line,
                                "Invalid argument: %s\n",
                                strarg
                            );
                        }
                    }else if(t == SET && i == 0){
                            strcpy(arg[i].s, strarg);
                    }else{
                        if(second_time){
                            stderror(
                                line,
                                "Invalid argument: %s\n",
                                strarg
                            );
                        }
                    }
                    strcpy(arg[i].s, strarg);
                }
            }
        }
        free(strarg);
    }
    /* Verificar se não há argumentos extras */
    ok = ok ? end_line(src, dir[t].id, line) : false;
    ok = ok ? threat_dir(t, arg, line, dict, map) : false;
    return ok;
}



bool first_pass(FILE * src, FILE * out, HashT dict, MemMap map){
    /* Retorno da função */
    bool ok = true;
    unsigned int
    m_pos = 0,  /* posição de montagem */
    line = 0,  /* linha de entrada */
    len, /* tamanho da string */
    wpl = 0, /*palavras por linha*/
    el = 0; /* Conta \n's */
    bool instr,
    nef = true; /* indica se o arquivo de entrada ja chegou ao fim */
    String w;

    union Type{
        TypeDir   d;
        TypeInstr i;
        TypeArg   a;
    }t;

    rewind(src);
    for(
        nef = true, el = 0, w = NULL;
        ok && nef;
        w = fgetword(src, &el, &nef), line += el
    ){
        if(!w) {
            continue;
        }
        wpl = (el == 0)? (wpl + 1) : 0;
        printf("%d: %s\n", wpl+1, w);

        len = strlen(w);

        if(seems_directive(w)){
            ok = validate_dir(w, &t.d, line);
            ok = ok ? interpret_dir(src, t.i, line, map, dict, false) : false;
        }else if(seems_label(w)){
            ok = validate_label(w, line, dict);
            sscanf(w, "%[^:]:", w);
            put_HashT(dict, w, map->pos, 'L');
            print_HashT(dict);
            printf("\n");
        }else if(seems_argument(w)){
            ok = validate_arg_format(w, &t.a, &instr, line, dict, false);
        /* Ainda pode ser argumento de diretiva ou instrução */
        }else{
            ok = validate_instr(w, &t.i, line);
            ok = ok ? interpret_instr(src, t.i, line, map, dict, false) : false;
            if (ok){
                m_pos++;
            }
        }
        free(w);
    }
    return ok;

}


bool second_pass(FILE * src, FILE * out, HashT dict, MemMap map){
    /* Retorno da função */
    bool ok = true;
    unsigned int
    m_pos = 0,  /* posição de montagem */
    line = 0,  /* linha de entrada */
    len, /* tamanho da string */
    wpl = 0, /*palavras por linha*/
    el = 0; /* Conta \n's */
    bool instr, /* checar se eh uma instrução */
    nef = false; /* indica se o arquivo de entrada ja chegou ao fim */
    String w;

    union Type{
        TypeDir   d;
        TypeInstr i;
        TypeArg   a;
    }t;
    rewind(src);
    for(
        line = 0, ok = true, nef = true, el = 0, w = NULL;
        ok && nef;
        w = ok? fgetword(src, &el, &nef) : NULL, line += el
    ){
        if(!w) {
            continue;
        }
        wpl = (el == 0)? (wpl + 1) : 0;

        len = strlen(w);

        if(seems_directive(w)){
            ok = validate_dir(w, &t.d, line);
            ok = ok ? interpret_dir(src, t.i, line, map, dict, true) : false;
        }else if(seems_label(w)){
            ok = validate_label(w, line, dict);
        }else if(seems_argument(w)){
            ok = validate_arg_format(w, &t.a, &instr, line, dict, true);
        /* Ainda pode ser argumento de diretiva ou instrução */
        }else{
            ok = validate_instr(w, &t.i, line);
            ok = ok ? interpret_instr(src, t.i, line, map, dict, true) : false;
            if (ok){
                m_pos++;
            }
        }
        free(w);
        w = NULL;
    }
    return ok;
}


bool build(FILE * src, FILE * out) {
    bool ok = true;
    MemMap map = new_MemMap();
    HashT dict = new_HashT();
    cfprintf(stdout, 92,"Começando Primeira Montagem\n");
    ok = first_pass(src, out, dict, map);
    cfprintf(stdout, 92, "Terminada primeira montagem\n");
    print_HashT(dict);
    fprint_MemMap(stdout, map);
    printf("\n");

    free_MemMap(&map);
    map = new_MemMap();
    cfprintf(stdout, 92, "Começando Segunda Montagem\n");
    ok = ok? second_pass(src, out, dict, map): ok;
    printf("\n");
    print_HashT(dict);
    fprint_MemMap(stdout, map);
    printf("\n");

    free_MemMap(&map);
    free_HashT(&dict);
    return ok;
}
