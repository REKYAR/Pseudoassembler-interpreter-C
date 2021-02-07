// biblioteka https://pdcurses.org/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <curses.h>
#include <curspriv.h>
#include <panel.h>
#define N_LINES 3000
#define N_COLS 100

struct codepair {
    char name[2];
    char code[8];
};
struct typepair {
    char name[10];
    int code;
};
struct record {
    char etykieta[16];
    char order_code[2];
    int arg1;
    int arg2;
    char directive_typefield[16];
    int index_shift;
};
//tablica rozkazów(jednorazowy zapis w funkcji main następnie wyłącznie odczyt)
struct record optab[N_LINES];
//tablica typów(w przypadku zwiększenia ich ilości)
struct typepair typetable[] = {
        {"INTEGER", 4}
};
//tablica trzymająca 1. fazę parsowania używane w wielu funcjach programu(jednorazowy zapis w funkcji main następnie wyłącznie odczyt)
char  buffered_stage_1[N_LINES][5][25];
//wskaźnik do tablicy danych używanej w wielu funkcjach programu(wyłącznie w trakcie wykonania programu)
long* datasec;

//tablica wejścia(wczytanie wartości z pliku) używane w wielu funcjach programu(jednorazowy zapis w funkcji main następnie wyłącznie odczyt)
char inval[N_LINES][N_COLS];

//endmark, ord_section, ds - umożliwiają poruszanie się po tablicach rozkazów i danych wykorzystywane tam gdzie te tablice
int endmark = N_LINES - 1, ord_section = 0;
// tablica rejestrów(zerowana na początku programu)
int registers[16] = { 0 };

//kol0: etykieta(jeśli brak - pusty str)
//kol1: polecenie
//kol2: arg1
//kol3: arg2(opcjonalnie null)
//kol4: directive_typefield - jeśli nie dyrektywa - pusty str
char  buffered_stage_1[N_LINES][5][25];

//struktury biblioteki
WINDOW*dataw, *registersw, * ordersw, *program_statew, *infow, *psaw, *info2w, *opsignw;


/////////////////////////////////////////////////////////////////////////////////////////////
/*
KONIEC SEKCJI DANYCH
*/
char* calcshift_ord(int row) {
    //printf("%d\n", row);
    char a[12] = {0};
    char b[10] = {0};
    int shift = 0;
    int t_row = row;
    a[11] = '\0';
    b[9] = '\0';
    int i = 0;
    if (row != 0) {
        while (t_row != ord_section) {
            shift += get_byte_value(optab[t_row].order_code);
            t_row -= 1;
        }
    }
    
    if (shift != 0) {
        snprintf(b, 9,"%08X", shift);
        a[0] = b[0];
        a[1] = b[1];
        a[2] = ' ';
        a[3] = b[2];
        a[4] = b[3];
        a[5] = ' ';
        a[6] = b[4];
        a[7] = b[5];
        a[8] = ' ';
        a[9] = b[6];
        a[10] = b[7];
    }
    else {
        a[0] = '0';
        a[1] = '0';
        a[2] = ' ';
        a[3] = '0';
        a[4] = '0';
        a[5] = ' ';
        a[6] = '0';
        a[7] = '0';
        a[8] = ' ';
        a[9] = '0';
        a[10] = '0';
    }
    //printf(" %s\n", a);
    return a;
}

//in alpha beta aft opt(0-addition, 1-subtractrion) for additive operations
//out 1 if overflow occured
int check_oveflow_add(int a, int b,int af, int opt) {
    if (a > 0 && b > 0 && af<0 && opt==0) { return 1; }
    if (a < 0 && b < 0 && af > 0 && opt == 0) { return 1; }
    return 0;
}
//in alpha beta af for multiplication
//out 1 if overflow occured
int check_oveflow_mul(int a, int b, int af) {
    if (a > 0 && b > 0 && af < 0) { return 1; }
    if (a < 0 && b > 0 && af > 0) { return 1; }
    if (a > 0 && b < 0 && af > 0) { return 1; }
    return 0;
}
int hexchar_to_int(char a) {
    if (a == '0') { return 0; }
    if (a == '1') { return 1; }
    if (a == '2') { return 2; }
    if (a == '3') { return 3; }
    if (a == '4') { return 4; }
    if (a == '5') { return 5; }
    if (a == '6') { return 6; }
    if (a == '7') { return 7; }
    if (a == '8') { return 8; }
    if (a == '9') { return 9; }
    if (a == 'A') { return 10; }
    if (a == 'B') { return 11; }
    if (a == 'C') { return 12; }
    if (a == 'D') { return 13; }
    if (a == 'E') { return 14; }
    if (a == 'F') { return 15; }
}
char int_to_hexchar(int a) {
    if (a == 0) { return '0'; }
    if (a == 1) { return '1'; }
    if (a == 2) { return '2'; }
    if (a == 3) { return '3'; }
    if (a == 4) { return '4'; }
    if (a == 5) { return '5'; }
    if (a == 6) { return '6'; }
    if (a == 7) { return '7'; }
    if (a == 8) { return '8'; }
    if (a == 9) { return '9'; }
    if (a == 10) { return 'A'; }
    if (a == 11) { return 'B'; }
    if (a == 12) { return 'C'; }
    if (a == 13) { return 'D'; }
    if (a == 14) { return 'E'; }
    if (a == 15) { return 'F'; }
}
int process_hexdata(int sspc, char* indata, char mode) {
    char local_use_string4[8];
    long val;
    char* p;
    if (mode == 8) {
        if (strcmp(indata, "~~ ~~ ~~ ~~\n") == 0) {
            return rand();
        }
        else {
            local_use_string4[0] = inval[sspc][0];
            local_use_string4[1] = inval[sspc][1];
            local_use_string4[2] = inval[sspc][3];
            local_use_string4[3] = inval[sspc][4];
            local_use_string4[4] = inval[sspc][6];
            local_use_string4[5] = inval[sspc][7];
            local_use_string4[6] = inval[sspc][9];
            local_use_string4[7] = inval[sspc][10];
        }

        val = strtoul(local_use_string4, &p, 16);
        return val;
    }
    else if (mode == 4) {
        local_use_string4[0] = '0';
        local_use_string4[1] = '0';
        local_use_string4[2] = '0';
        local_use_string4[3] = '0';
        local_use_string4[4] = inval[sspc][6];
        local_use_string4[5] = inval[sspc][7];
        local_use_string4[6] = inval[sspc][9];
        local_use_string4[7] = inval[sspc][10];
        val = strtoul(local_use_string4, &p, 16);
        return val;
    }
}

int c_space(int ord_section) {
    int m = 0, i=0;
    for (i = 0; i < ord_section - 1; ++i) {
        if (optab[i].index_shift == -1) {
            m += optab[i].arg1;
        }
        else {
            m += 1;
        }
    }
    return m;
}
char is_line_empty(char* line) {
    int k = 0;
    for (k = 0; k < N_COLS; ++k) {
        if (line[k] != 0) { return 0; }
    }
    return 1;
}
//>in przesuniecie
//>out przesuniecie podzielone na 2/4 Bity
char* transform_shift_arr(int index_shift, char opcode, char* local_use_string4, char* local_use_string5, char* local_use_string2 , char* local_use_string3, char* local_use_string) {
    int l = 0;
    if (opcode == 1) {
        snprintf(local_use_string2, 5,"%04X ", index_shift);
        for (l = 0; l < 5; l++)
        {
            if (l == 2)
            {
                local_use_string[l] = 32;
                continue;
            }
            else if (l < 2)
            {
                local_use_string[l] = local_use_string2[l];
            }
            else if (l > 2)
            {
                local_use_string[l] = local_use_string2[l - 1];
            }

        }
        //printf("~~~~%s\n", local_use_string);
        return local_use_string;
        
    }if (opcode == 0)
    {
        snprintf(local_use_string4, 9,"%08X ", index_shift);
        //01 34 67 910
        local_use_string3[0] = local_use_string4[0];
        local_use_string3[1] = local_use_string4[1];
        local_use_string3[2] = 32;
        local_use_string3[3] = local_use_string4[2];
        local_use_string3[4] = local_use_string4[3];
        local_use_string3[5] = 32;
        local_use_string3[6] = local_use_string4[4];
        local_use_string3[7] = local_use_string4[5];
        local_use_string3[8] = 32;
        local_use_string3[9] = local_use_string4[6];
        local_use_string3[10] = local_use_string4[7];
        //printf("%c\n", local_use_string3[10]);
        //printf("ss: %s --- %s\n", local_use_string4, local_use_string3);
        //printf("~~~~%s\n", local_use_string3);
        return local_use_string3;
    }

}


//>in nazwa rozkazu
//>out hex rozkazu
int get_hex_ord_code(char* rozkaz) {
    if (strcmp(rozkaz, "AR") == 0) {
        return 0x10;
    }
    else if (strcmp(rozkaz, "A") == 0) {
        return 0xD1;
    }
    else if (strcmp(rozkaz, "SR") == 0) {
        return 0x12;
    }
    else if (strcmp(rozkaz, "S") == 0) {
        return 0xD3;
    }
    else if (strcmp(rozkaz, "MR") == 0) {
        return 0x14;
    }
    else if (strcmp(rozkaz, "M") == 0) {
        return 0xD5;
    }
    else if (strcmp(rozkaz, "DR") == 0) {
        return 0x16;
    }
    else if (strcmp(rozkaz, "D") == 0) {
        return 0xD7;
    }
    else if (strcmp(rozkaz, "CR") == 0) {
        return 0x18;
    }
    else if (strcmp(rozkaz, "C") == 0) {
        return 0xD9;
    }
    else if (strcmp(rozkaz, "J") == 0) {
        return 0xE0;
    }
    else if (strcmp(rozkaz, "JZ") == 0) {
        return 0xE1;
    }
    else if (strcmp(rozkaz, "JP") == 0) {
        return 0xE2;
    }
    else if (strcmp(rozkaz, "JN") == 0) {
        return 0xE3;
    }
    else if (strcmp(rozkaz, "L") == 0) {
        return 0xF0;
    }
    else if (strcmp(rozkaz, "LR") == 0) {
        return 0x31;
    }
    else if (strcmp(rozkaz, "LA") == 0) {
        return 0xF2;
    }
    else if (strcmp(rozkaz, "ST") == 0) {
        return 0xF3;
    }
    return 0;
}
char* get_ord_name_from_hex(char* ordcode) {
    //printf("ordcode in: %s\n", ordcode);
    if (strcmp(ordcode, "10") == 0) {
        return "AR";
    }
    else if (strcmp(ordcode, "D1") == 0) {
        return "A";
    }
    else if (strcmp(ordcode, "12") == 0) {
        return "SR";
    }
    else if (strcmp(ordcode, "D3") == 0) {
        return "S";
    }
    else if (strcmp(ordcode, "14") == 0) {
        return "MR";
    }
    else if (strcmp(ordcode, "D5") == 0) {
        return "M";
    }
    else if (strcmp(ordcode, "16") == 0) {
        return "DR";
    }
    else if (strcmp(ordcode, "D7") == 0) {
        return "D";
    }
    else if (strcmp(ordcode, "18") == 0) {
        return "CR";
    }
    else if (strcmp(ordcode, "D9") == 0) {
        return "C";
    }
    else if (strcmp(ordcode, "E0") == 0) {
        return "J";
    }
    else if (strcmp(ordcode, "E1") == 0) {
        return "JZ";
    }
    else if (strcmp(ordcode, "E2") == 0) {
        return "JP";
    }
    else if (strcmp(ordcode, "E3") == 0) {
        return "JN";
    }
    else if (strcmp(ordcode, "F0") == 0) {
        return "L";
    }
    else if (strcmp(ordcode, "31") == 0) {
        return "LR";
    }
    else if (strcmp(ordcode, "F2") == 0) {
        return "LA";
    }
    else if (strcmp(ordcode, "F3") == 0) {
        return "ST";
    }
    return "ERR";
}
int getsign(int val) {
    if (val > 0) { return 1; }
    if (val == 0) { return 0; }
    if (val < 0) { return -1; }
}

//>in nazwa rozkazu
//>out wartość w bajtach rozkazu
int getsizeof_type(char* type) {
    if (strcmp(type, typetable[0].name) == 0) { return typetable[0].code; }
}
int get_byte_value(char* ordName) {
    //printf("%c\n",ordName);
    if (ordName[1] == 'R') {
        //printf("Z2\n");
        return 2;
    }
    return 4;
}
//> in etykieta
//> out adred w bajtach względem początku danej sekcji
int get_adresss(char* etykieta, int ord_section) {
    int shift_count = 0;
    int j = 0;
    for (j = 0; j < N_LINES; ++j) {
        if (j == ord_section) {
            shift_count = 0;
            if (strcmp(buffered_stage_1[j][0], etykieta) == 0) { return 0; }
            else { shift_count += get_byte_value(buffered_stage_1[j][1]); }
            continue;
        }
        if (strcmp(etykieta, buffered_stage_1[j][0]) == 0) {

            break;

        }
        if ((j < ord_section && isdigit(*buffered_stage_1[j][2]) == 0) || j > ord_section) {
            shift_count = shift_count + get_byte_value(buffered_stage_1[j][1]);
        }
        else {
            shift_count = shift_count + (getsizeof_type(buffered_stage_1[j][3]) * atoi(buffered_stage_1[j][2]));
        }
    }
    return shift_count;
}

// in > nazwa polecenia
//out > kod polecenia
char get_code(char* command, struct codepair* codetable) {
    int i = 0;
    int arrs = sizeof(codetable) / sizeof(codetable[0]);
    for (i = 0; i < arrs; i++)
    {
        if (strcmp(command, codetable[i].name)) {
            return *codetable[i].code;
        }
    }
    return -1;
}
//>> in line
void process_hexord(int f, char* indata, char* local_use_string5) {
    char a = (char)indata[3];
    char b = (char)indata[4];
    optab[f].arg1 = 0;
    optab[f].arg2 = 0;
    optab[f].index_shift = 0;
    local_use_string5[0] = indata[0];
    local_use_string5[1] = indata[1];
    strcpy(optab[f].order_code, get_ord_name_from_hex(local_use_string5));
    if (get_byte_value(optab[f].order_code) == 2) {
        optab[f].arg1 = hexchar_to_int(indata[3]);
        optab[f].arg2 = hexchar_to_int(indata[4]);
        return;
    }
    if (optab[f].order_code[0] == 'J') {
        optab[f].arg2 = hexchar_to_int(indata[4]);
        optab[f].arg1 = process_hexdata(f, indata, 4);
        return;
    }
    
    optab[f].arg1 = hexchar_to_int(a);
    optab[f].arg2 = hexchar_to_int(b);
    optab[f].index_shift = process_hexdata(f, indata, 4);

}

//in przesuniecie, adres rejestru w którym adres do startu(default 14- sekcaj komend, 15- sekcja danych), tryb(0-sekcja rozkazow, inne sekcja danych)
//out nr wiersza
int row_from_shift(int shift, int start_addr, int mode) {
    //7,5,0(shift)
    int c=0;
    int row_cnt=0;
    if (mode == 0) {
        row_cnt = ord_section;
        c = shift;
        if (c == 0) { return ord_section; }
        row_cnt += 1;
        c -= get_byte_value(optab[row_cnt - 1].order_code);
        while (c > 0) {
            //printf("%d<-%d | %d \n", get_byte_value(optab[row_cnt - 1].order_code), row_cnt + 1, c);
            row_cnt += 1;
            c -= get_byte_value(optab[row_cnt - 1].order_code);
        }
        //printf("returned index: %d \n", row_cnt);
        //printf("value at index %d \n", datasec[row_cnt]);
        return row_cnt;
    }
    else {
        if (start_addr != 15) { row_cnt = registers[start_addr] / 4; }
        else { row_cnt = 0; }
        //printf("row cout: %d\n", row_cnt);
        c = shift / 4;
        while (c > 0) {
            row_cnt += 1;
            c -= 1;
        }
        //printf("returned index: %d \n", row_cnt);
        //printf("value at index %d \n", datasec[row_cnt]);
        return row_cnt;
    }
}


int execute(int crrs, int* sfd, int* overflow,int ds,int *compres) {
    if (sfd != NULL) {
        *sfd = crrs;
    }
    if (overflow != NULL) {
        *overflow = 0;
    }
    int afterop = 0, alpha = 0, beta = 0;
    if (strcmp(optab[crrs].order_code, "A") == 0) {
        if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) < 0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) > ds - 1)
        {
            *overflow = 1;
        }
        alpha = registers[optab[crrs].arg1];
        beta = datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] + datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        afterop = registers[optab[crrs].arg1];
        *compres = getsign(registers[optab[crrs].arg1]);

        if (check_oveflow_add(alpha, beta, afterop, 0) == 1)
        {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "AR") == 0) {
        alpha = registers[optab[crrs].arg1];
        beta = registers[optab[crrs].arg2];
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] + registers[optab[crrs].arg2];
        afterop = registers[optab[crrs].arg1];
        *compres = getsign(registers[optab[crrs].arg1]);
        if (check_oveflow_add(alpha, beta, afterop, 0) == 1)
        {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "S") == 0) {
        if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) < 0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) > ds - 1)
        {
            *overflow = 1;
        }
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] - datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        *compres = getsign(registers[optab[crrs].arg1]);

        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "SR") == 0) {
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] - registers[optab[crrs].arg2];
        *compres = getsign(registers[optab[crrs].arg1]);

        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "M") == 0) {
        if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) < 0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) > ds - 1)
        {
            *overflow = 1;
        }
        alpha = registers[optab[crrs].arg1];
        beta = datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] * datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        afterop = registers[optab[crrs].arg1];
        *compres = getsign(registers[optab[crrs].arg1]);
        if (check_oveflow_mul(alpha, beta, afterop) == 1) {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "MR") == 0) {
        alpha = registers[optab[crrs].arg1];
        beta = registers[optab[crrs].arg2];
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] * registers[optab[crrs].arg2];
        afterop = registers[optab[crrs].arg1];
        *compres = getsign(registers[optab[crrs].arg1]);
        *overflow = 0;
        if (check_oveflow_mul(alpha, beta, afterop == 1)) {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "J") == 0) {
        crrs = row_from_shift(optab[crrs].arg1, optab[crrs].arg2, 0);
        if (crrs<ord_section || crrs>endmark)
        {
            *overflow = 1;
        }
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "JZ") == 0) {
        if (*compres == 0) {
            crrs = row_from_shift(optab[crrs].arg1, optab[crrs].arg2, 0);
            return crrs;
        }
        if (crrs<ord_section || crrs>endmark)
        {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "JP") == 0) {
        if (*compres == 1) {
            crrs = row_from_shift(optab[crrs].arg1, optab[crrs].arg2, 0);
            return crrs;
        }
        if (crrs<ord_section || crrs>endmark)
        {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "JN") == 0) {
        //printf("cache: %d\n", *compres);
        if (*compres == -1) {
            crrs = row_from_shift(optab[crrs].arg1, optab[crrs].arg2, 0);
            return crrs;
        }
        if (crrs<ord_section || crrs>endmark)
        {
            *overflow = 1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "L") == 0) {
        if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) < 0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) > ds-1){ 
            *overflow = 1;
        }
        registers[optab[crrs].arg1] = datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "LA") == 0) {
        registers[optab[crrs].arg1] = optab[crrs].index_shift;
        crrs += 1;
        return crrs;
    }

    else if (strcmp(optab[crrs].order_code, "ST") == 0) {
    if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)<0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)> ds - 1)
    {
        *overflow = 1;
    }
        datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)] = registers[optab[crrs].arg1];
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "C") == 0) {
    if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)<0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)> ds - 1)
    {
        *overflow = 1;
    }
        if (registers[optab[crrs].arg1] == datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)]) {
            *compres = 0;
        }
        else if (registers[optab[crrs].arg1] > datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)]) {
            *compres = 1;
        }
        else {
            *compres = -1;
        }
        *overflow = 0;
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "CR") == 0) {
        if (registers[optab[crrs].arg1] == registers[optab[crrs].arg2]) {
            *compres = 0;
        }
        else if (registers[optab[crrs].arg1] > registers[optab[crrs].arg2]) {
            *compres = 1;
        }
        else {
            *compres = -1;
        }
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "D") == 0) {
    if (row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)<0 || row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1) > ds-1)
    {
        *overflow = 1;
    }
    if (datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)]==0)
    {
        *overflow = 1;
    }
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] / datasec[row_from_shift(optab[crrs].index_shift, optab[crrs].arg2, 1)];
        *compres = getsign(registers[optab[crrs].arg1]);
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "DR") == 0) {
    if (registers[optab[crrs].arg2]==0)
    {
        *overflow = 1;
    }
        registers[optab[crrs].arg1] = registers[optab[crrs].arg1] / registers[optab[crrs].arg2];
        *compres = getsign(registers[optab[crrs].arg1]);
        crrs += 1;
        return crrs;
    }
    else if (strcmp(optab[crrs].order_code, "LR") == 0) {
        registers[optab[crrs].arg1] = registers[optab[crrs].arg2];
        crrs += 1;
        return crrs;
    }
    else {
        printf("ARGUMENT %s NIE OBSLUGIWANY\n", optab[crrs].order_code);
        crrs += 1;
        return crrs;
    }
}

void printstate(int* sfd, char* local_use_string4, char* local_use_string5, char* local_use_string2, char* local_use_string3, char* local_use_string, int ds, int* compres, int crr_st, int u_ord, int l_ord, int u_dat, int l_dat, WINDOW* crrc, char is_psa, int u_psa, int l_psa) {
    int crrs_i = 1;
    unsigned int curr_state = *sfd;
    int o, p = *sfd, pindex, oindex, c;
    char psr[24] = { "0" }, shf[12];
    //printf("%d  - %d - %d - %d\n", u_ord, l_ord, u_dat, l_dat);
    psr[23] = '\0';
    shf[11] = '\0';
    start_color();
    //ord line obecna linijka sfd prdze obecna linijka
    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_WHITE, COLOR_GREEN);
    init_pair(4, COLOR_MAGENTA, COLOR_CYAN);
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);

    if (is_psa == 1) {
        if (endmark >= 30) {
            psaw = newwin(32, 37, 1, 89);
            cbreak();
        }
        else { psaw = newwin(endmark + 2, 37, 1, 89); }
    }
    //refresh();
    if (endmark - ord_section >= 30) {
        ordersw = newwin(32, 21, 1, 1);
        cbreak();
    }
    else { ordersw = newwin(endmark - ord_section + 2, 21, 1, 1); }
    registersw = newwin(18, 19, 1, 22);
    if (ds >= 30) {
        dataw = newwin(32, 21, 1, 41);
        cbreak();
    }
    else { dataw = newwin(ds + 2, 21, 1, 41); }
    program_statew = newwin(3, 27, 1, 62);
    opsignw = newwin(3, 27, 4, 62);
    infow = newwin(1, 100, 0, 0);
    info2w = newwin(23, 27, 7, 62);
    refresh();
    //wypisywanie rozkazow
    
    //obecna linia w oknie
    crrs_i = 1;
    for (o = l_ord; o < u_ord; o++) {
        mvwprintw(ordersw, crrs_i, 2, "%04d ", o - ord_section);
        mvwprintw(ordersw, crrs_i, 6, ":");
        mvwprintw(ordersw, crrs_i, 8, "%X ", get_hex_ord_code(optab[o].order_code));
        wattron(ordersw, COLOR_PAIR(4));
        mvwprintw(ordersw, *sfd - l_ord + 1, 8, "%X", get_hex_ord_code(optab[*sfd].order_code));
        wattroff(ordersw, COLOR_PAIR(4));
        if (crr_st != endmark) {
            wattron(ordersw, COLOR_PAIR(5));
            mvwprintw(ordersw, crr_st - l_ord + 1, 8, "%X", get_hex_ord_code(optab[crr_st].order_code));
            wattroff(ordersw, COLOR_PAIR(5));
            mvwprintw(ordersw, crr_st - l_ord + 1, 10, " ");
        }

        if (o == *sfd && !(strcmp(optab[o].order_code, "J") == 0 || strcmp(optab[o].order_code, "JZ") == 0 || strcmp(optab[o].order_code, "JN") == 0 || strcmp(optab[o].order_code, "JP") == 0))
        {
            //obsluga rozkazow rejestr rejestr
            if (get_byte_value(optab[*sfd].order_code) == 2) {
                wattron(ordersw, COLOR_PAIR(2));
                mvwprintw(ordersw, crrs_i, 11, "%c", int_to_hexchar(optab[o].arg1));
                wattroff(ordersw, COLOR_PAIR(2));
                wattron(ordersw, COLOR_PAIR(3));
                mvwprintw(ordersw, crrs_i, 12, "%c", int_to_hexchar(optab[o].arg2));
                wattroff(ordersw, COLOR_PAIR(3));
                crrs_i += 1;
                continue;
            }

            //obsluga rozkazow rejestr pamiec
            if (!(strcmp(optab[o].order_code, "J") == 0 || strcmp(optab[o].order_code, "JZ") == 0 || strcmp(optab[o].order_code, "JN") == 0 || strcmp(optab[o].order_code, "JP") == 0)) {
                //mvwprintw(ordersw, crrs_i, 2, get_hex_ord_code(optab[*sfd].order_code));
                wattron(ordersw, COLOR_PAIR(2));
                mvwprintw(ordersw, crrs_i, 11, "%c", int_to_hexchar(optab[o].arg1));
                wattroff(ordersw, COLOR_PAIR(2));
                wattron(ordersw, COLOR_PAIR(1));
                mvwprintw(ordersw, crrs_i, 12, "%c", int_to_hexchar(optab[o].arg2));
                mvwprintw(ordersw, crrs_i, 14, transform_shift_arr(optab[o].index_shift, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                wattroff(ordersw, COLOR_PAIR(1));
                crrs_i += 1;
                continue;
            }
        }

        // wypisywnie całości bloku rozkazów
        else {
            //obsluga rozkazow rejestr rejestr
            if (get_byte_value(optab[o].order_code) == 2) {
                mvwprintw(ordersw, crrs_i, 11, "%c", int_to_hexchar(optab[o].arg1));
                mvwprintw(ordersw, crrs_i, 12, "%c", int_to_hexchar(optab[o].arg2));
                crrs_i += 1;
                continue;
            }
            //obsluga instrukcji typu jump
            if (strcmp(optab[o].order_code, "J") == 0 || strcmp(optab[o].order_code, "JZ") == 0 || strcmp(optab[o].order_code, "JN") == 0 || strcmp(optab[o].order_code, "JP") == 0) {
                mvwprintw(ordersw, crrs_i, 11, "0");
                mvwprintw(ordersw, crrs_i, 12, "%c", int_to_hexchar(optab[o].arg2));
                mvwprintw(ordersw, crrs_i, 14, transform_shift_arr(optab[o].arg1, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                crrs_i += 1;
                continue;
            }

            //obsluga rozkazow rejestr pamiec
            mvwprintw(ordersw, crrs_i, 11, "%c", int_to_hexchar(optab[o].arg1));
            mvwprintw(ordersw, crrs_i, 12, "%c", int_to_hexchar(optab[o].arg2));
            mvwprintw(ordersw, crrs_i, 14, transform_shift_arr(optab[o].index_shift, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
            crrs_i += 1;
            continue;
        }
    }
    //wypisywanie instrukcji jump gdy jest obecnie wykonywana
    if (strcmp(optab[*sfd].order_code, "J") == 0 || strcmp(optab[*sfd].order_code, "JZ") == 0 || strcmp(optab[*sfd].order_code, "JN") == 0 || strcmp(optab[*sfd].order_code, "JP") == 0) {
        mvwprintw(ordersw, *sfd + 1 - l_ord, 11, "0");
        wattron(ordersw, COLOR_PAIR(3));
        mvwprintw(ordersw, *sfd + 1 - l_ord, 12, "%c", int_to_hexchar(optab[*sfd].arg2));
        mvwprintw(ordersw, *sfd + 1 - l_ord, 14, transform_shift_arr(optab[*sfd].arg1, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
        wattroff(ordersw, COLOR_PAIR(3));
        pindex = row_from_shift(optab[*sfd].arg1, optab[*sfd].arg2, 0) - l_ord + 1;
        oindex = row_from_shift(optab[*sfd].arg1, optab[*sfd].arg2, 0);
        //printf("oindex: %d\n", oindex);
        //printf("prindex: %d\n", pindex);
        //obsluga rozkazow rejestr rejestr
        if (get_byte_value(optab[oindex].order_code) == 2) {
            wattron(ordersw, COLOR_PAIR(3));
            mvwprintw(ordersw, pindex, 11, "%c", int_to_hexchar(optab[oindex].arg1));
            mvwprintw(ordersw, pindex, 12, "%c", int_to_hexchar(optab[oindex].arg2));
            wattroff(ordersw, COLOR_PAIR(3));
        }
        //obsluga instrukcji typu jump
        else if (strcmp(optab[oindex].order_code, "J") == 0 || strcmp(optab[oindex].order_code, "JZ") == 0 || strcmp(optab[oindex].order_code, "JN") == 0 || strcmp(optab[oindex].order_code, "JP") == 0) {
            wattron(ordersw, COLOR_PAIR(3));
            mvwprintw(ordersw, pindex, 11, "0");
            mvwprintw(ordersw, pindex, 12, "%c", int_to_hexchar(optab[oindex].arg2));
            mvwprintw(ordersw, pindex, 14, transform_shift_arr(optab[oindex].arg1, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
            wattroff(ordersw, COLOR_PAIR(3));
        }
        else {
            //obsluga rozkazow rejestr pamiec
            wattron(ordersw, COLOR_PAIR(3));
            mvwprintw(ordersw, pindex, 11, "%c", int_to_hexchar(optab[oindex].arg1));
            mvwprintw(ordersw, pindex, 12, "%c", int_to_hexchar(optab[oindex].arg2));
            mvwprintw(ordersw, pindex, 14, transform_shift_arr(optab[oindex].index_shift, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
            wattroff(ordersw, COLOR_PAIR(3));
        }
    }

    //wypisywanie rejestrow
    for (o = 0; o < 16; o++) {
        mvwprintw(registersw, o + 1, 2, "%02d",o);
        mvwprintw(registersw, o + 1, 4, ":");
        mvwprintw(registersw, o + 1, 6, transform_shift_arr(registers[o], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
    }
    //nadpisanie koloru rejestru 1( i czasem 2)
    if ((strcmp(optab[*sfd].order_code, "J") == 0 || strcmp(optab[*sfd].order_code, "JZ") == 0 || strcmp(optab[*sfd].order_code, "JN") == 0 || strcmp(optab[*sfd].order_code, "JP") == 0) == 0) {
        wattron(registersw, COLOR_PAIR(2));
        mvwprintw(registersw, optab[*sfd].arg1 + 1, 6, transform_shift_arr(registers[optab[*sfd].arg1], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
        wattroff(registersw, COLOR_PAIR(2));
        if (get_byte_value(optab[*sfd].order_code) == 2) {
            wattron(registersw, COLOR_PAIR(3));
            mvwprintw(registersw, optab[*sfd].arg2 + 1, 6, transform_shift_arr(registers[optab[*sfd].arg2], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
            wattroff(registersw, COLOR_PAIR(3));
        }
    }
    crrs_i = 1;
    //wypisywanie danych
    for (o = l_dat; o < u_dat; o++) {
        mvwprintw(dataw, crrs_i, 2, "%04d ", o);
        mvwprintw(dataw, crrs_i, 6, ":");
        mvwprintw(dataw, crrs_i, 8, transform_shift_arr(datasec[o], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
        crrs_i += 1;
    }
    if (get_byte_value(optab[*sfd].order_code) == 4 && ((strcmp(optab[*sfd].order_code, "J") == 0 || strcmp(optab[*sfd].order_code, "JZ") == 0 || strcmp(optab[*sfd].order_code, "JN") == 0 || strcmp(optab[*sfd].order_code, "JP") == 0) == 0)) {
        wattron(dataw, COLOR_PAIR(1));
        mvwprintw(dataw, row_from_shift(optab[*sfd].index_shift, optab[*sfd].arg2, 1) + 1 - l_dat, 8, transform_shift_arr(datasec[row_from_shift(optab[*sfd].index_shift, optab[*sfd].arg2, 1)], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
        wattroff(dataw, COLOR_PAIR(1));
    }

    //rejestr stanu programu(budowa do wypisania)
    for (o = 0; o < 23; o++) {
        //psr[o] = '0';

        if ((o + 1) % 3 == 0) { psr[o] = 32; }
        else { psr[o] = '0'; }

    }
    strcpy(shf, calcshift_ord(crr_st));
    for (o = 12; o < 23; o++) { psr[o] = shf[o - 12]; }
    if (*compres == 1) { psr[1] = '1'; }
    else if (*compres == -1) { psr[0] = '1'; }

    // rejestr stanu programu wypisywanie
    for (o = 0; o < 23; o++) {
        if (o > 11) { wattron(program_statew, COLOR_PAIR(5)); }
        mvwprintw(program_statew, 1, 2 + o, "%c", psr[o]);
        if (o > 11) { wattroff(program_statew, COLOR_PAIR(5)); }
    }
    if (psr[0] == '1' && psr[1] == '1') {
        wattron(program_statew, COLOR_PAIR(4));
        mvwprintw(program_statew, 1, 2, "%c", psr[0]);
        mvwprintw(program_statew, 1, 3, "%c ", psr[1]);
        wattroff(program_statew, COLOR_PAIR(4));
    }

    //wypisywanie pleudoassemblera (jesli jest)
    crrs_i = 1;
    //printf("%d --- %d\n", l_psa, u_psa);
    for (o = l_psa; o < u_psa; o++) {
        mvwprintw(psaw, crrs_i, 2, "%s ", buffered_stage_1[o][0]);
        mvwprintw(psaw, crrs_i, 13, "%s ", buffered_stage_1[o][1]);
        if (o < ord_section - 1)
        {
            mvwprintw(psaw, crrs_i, 16, "%s", buffered_stage_1[o][2]);
            if (strcmp(buffered_stage_1[o][2], "INTEGER") == 0) {
                mvwprintw(psaw, crrs_i, 23, "(%s)", buffered_stage_1[o][3]);
            }
            else {
                mvwprintw(psaw, crrs_i, 17 + strlen(buffered_stage_1[o][2]), "* INTEGER", buffered_stage_1[o][2]);
            }
        }
        else if (o >= ord_section) {
            mvwprintw(psaw, crrs_i, 16, "%s", buffered_stage_1[o][2]);
            if (strcmp(buffered_stage_1[o][3], "\0") != 0) {
                mvwprintw(psaw, crrs_i, 16 + strlen(buffered_stage_1[o][2]), ", %s", buffered_stage_1[o][3]);
            }
            if (strcmp(buffered_stage_1[o][4], "\0") != 0) {
                mvwprintw(psaw, crrs_i, 18 + strlen(buffered_stage_1[o][3]) + strlen(buffered_stage_1[o][2]), "(%s)", buffered_stage_1[o][4]);
            }

        }
        crrs_i += 1;
    }
    //wypisywanie podświetlonej linii pleudoassemblera
    wattron(psaw, COLOR_PAIR(4));
    if (strcmp(buffered_stage_1[*sfd][0], "\0") != 0) {
        mvwprintw(psaw, *sfd + -l_psa + 1, 2, "%s ", buffered_stage_1[*sfd][0]);
    }
    mvwprintw(psaw, *sfd - l_psa + 1, 13, "%s ", buffered_stage_1[*sfd][1]);
    mvwprintw(psaw, *sfd - l_psa + 1, 16, "%s", buffered_stage_1[*sfd][2]);
    if (strcmp(buffered_stage_1[*sfd][3], "\0") != 0 && !(strcmp(optab[*sfd].order_code, "J") == 0 || strcmp(optab[*sfd].order_code, "JZ") == 0 || strcmp(optab[*sfd].order_code, "JN") == 0 || strcmp(optab[*sfd].order_code, "JP") == 0)) {
        mvwprintw(psaw, *sfd - l_psa + 1, 16 + strlen(buffered_stage_1[*sfd][2]), ", %s", buffered_stage_1[*sfd][3]);
    }
    if (strcmp(buffered_stage_1[*sfd][4], "\0") != 0) {
        wattron(psaw, COLOR_PAIR(4));
        mvwprintw(psaw, *sfd + 1 - l_psa, 18 + strlen(buffered_stage_1[*sfd][3]) + strlen(buffered_stage_1[*sfd][2]), "(%s)", buffered_stage_1[*sfd][4]);
    }
    wattroff(psaw, COLOR_PAIR(4));
    wattron(psaw, COLOR_PAIR(5));
    if (strcmp(buffered_stage_1[crr_st][0], "\0") != 0) {
        mvwprintw(psaw, crr_st + -l_psa + 1, 2, "%s ", buffered_stage_1[crr_st][0]);
    }
    mvwprintw(psaw, crr_st - l_psa + 1, 13, "%s ", buffered_stage_1[crr_st][1]);
    mvwprintw(psaw, crr_st - l_psa + 1, 16, "%s", buffered_stage_1[crr_st][2]);
    if (strcmp(buffered_stage_1[crr_st][3], "\0") != 0 && !(strcmp(optab[crr_st].order_code, "J") == 0 || strcmp(optab[crr_st].order_code, "JZ") == 0 || strcmp(optab[crr_st].order_code, "JN") == 0 || strcmp(optab[crr_st].order_code, "JP") == 0)) {
        mvwprintw(psaw, crr_st - l_psa + 1, 16 + strlen(buffered_stage_1[crr_st][2]), ", %s", buffered_stage_1[crr_st][3]);
    }
    if (strcmp(buffered_stage_1[crr_st][4], "\0") != 0) {
        wattron(psaw, COLOR_PAIR(5));
        mvwprintw(psaw, crr_st + 1 - l_psa, 18 + strlen(buffered_stage_1[crr_st][3]) + strlen(buffered_stage_1[crr_st][2]), "(%s)", buffered_stage_1[crr_st][4]);
    }
    wattroff(psaw, COLOR_PAIR(5));

    //wypelnianie okna znaku ostatniej operacji
    mvwprintw(opsignw, 1, 2, "%s ","LAST OPERATION SIGN:");
    mvwprintw(opsignw, 1, 23, "%c%c ", psr[0],psr[1]);

    //wypełnianie okna informacji
    mvwprintw(info2w, 1, 2, "%s","USAGE INFO");
    mvwprintw(info2w, 2, 1, "%s", "-------------------------");
    mvwprintw(info2w, 3, 2, "%s", "MOVEMENT");
    mvwprintw(info2w, 4, 2, "%s", "x-execute next line");
    mvwprintw(info2w, 5, 2, "%s", "w,s-move up,down ORDERS");
    mvwprintw(info2w, 6, 2, "%s", "e,d-move up,down DATA");
    mvwprintw(info2w, 7, 2, "%s", "r,f-move up,down PSA");
    mvwprintw(info2w, 8, 2, "%s", "q-quit debug mode");
    mvwprintw(info2w, 9, 1, "%s", "-------------------------");
    mvwprintw(info2w, 10, 2, "%s", "COLOURS");
    wattron(info2w, COLOR_PAIR(4));
    mvwprintw(info2w, 11, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(4));
    mvwprintw(info2w, 11, 7, "%s", "marks current line");
    wattron(info2w, COLOR_PAIR(5));
    mvwprintw(info2w, 12, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(5));
    mvwprintw(info2w, 12, 7, "%s", "marks next line");
    wattron(info2w, COLOR_PAIR(1));
    mvwprintw(info2w, 13, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(1));
    mvwprintw(info2w, 13, 7, "%s", "marks DATA use");
    wattron(info2w, COLOR_PAIR(2));
    mvwprintw(info2w, 14, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(2));
    mvwprintw(info2w, 14, 7, "%s", "marks REGISTERS use");
    mvwprintw(info2w, 15, 2, "%s", "(1st halfbyte)");
    wattron(info2w, COLOR_PAIR(3));
    mvwprintw(info2w, 16, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(3));
    mvwprintw(info2w, 16, 7, "%s", "marks REGISTERS use");
    mvwprintw(info2w, 17, 2, "%s", "(2nd halfbyte) or both");
    mvwprintw(info2w, 18, 2, "%s", "if they are the same");
    wattron(info2w, COLOR_PAIR(3));
    mvwprintw(info2w, 19, 2, "%s", "THIS");
    wattroff(info2w, COLOR_PAIR(3));
    mvwprintw(info2w, 19, 7, "%s", "too marks potential");
    mvwprintw(info2w, 20, 2, "%s", "jump line for JUMP ");
    mvwprintw(info2w, 21, 2, "%s", "type instructions");
    //wypisywanie podpisów okien
    mvwprintw(infow, 0, 2, "%s ", "ORDERS");
    mvwprintw(infow, 0, 23, "%s ", "REGISTERS");
    mvwprintw(infow, 0, 42, "%s ", "DATA");
    mvwprintw(infow, 0, 63, "%s ", "PROGRAM STATE REGISTER");
    if (is_psa == 1) {
        mvwprintw(infow, 0, 90, "%s ", "PSA CODE");
    }
    box(opsignw, 0, 0);
    box(ordersw, 0, 0);
    if (crrc == ordersw) {
        wattron(ordersw, COLOR_PAIR(6));
        box(ordersw, 0, 0);
        wattroff(ordersw, COLOR_PAIR(6));

    }
    box(registersw, 0, 0);
    box(dataw, 0, 0);
    if (crrc == dataw) {
        wattron(dataw, COLOR_PAIR(6));
        box(dataw, 0, 0);
        wattroff(dataw, COLOR_PAIR(6));

    }
    box(program_statew, 0, 0);
    box(psaw, 0, 0);
    box(info2w, 0, 0);
    wrefresh(psaw);
    wrefresh(opsignw);
    wrefresh(ordersw);
    wrefresh(registersw);
    wrefresh(dataw);
    wrefresh(program_statew);
    wrefresh(infow);
    wrefresh(info2w);
    return;
}
int main(int argc, char* argv[]) {
    WINDOW* crrw = ordersw;
    //sfd linijka przed wykonaniem rozkazu
    int *sfd = calloc(1, sizeof(int));
    int *overflow = calloc(1, sizeof(char));
    int compres = calloc(1, sizeof(int));
    FILE* infile= NULL, * ofile, * varfile;
    char temporary_input_string[N_LINES] = "";
    unsigned int curr_state = 0;
    int c = 0, i = 0, j = 0, k = 0, l = 0, m = 0, n = 0, u = 0, shift_count = 0, shiftval = 0, default_data_section_reg = 15,
        default_operation_section_reg = 14, saveline = 0, surpl = 0, osurpl = 0, ds = 0, row_cnt = 0, alpha = 0, beta = 0, afterop = 0, nxord = 0, crrs_i = 0, u_ord, l_ord, u_dat, l_dat, u_psa, l_psa, d=0;
    char isfset = 0, sflg = 0, argerr = 0, uam = 0, previous_operation_sign = 0, line_used = 0, a, b, is_psa=1;
    char fpath[100], local_use_string[6] = { 0 }, local_use_string2[5] = { 0 }, local_use_string3[12] = { 0 }, local_use_string4[9] = { 0 }, local_use_string5[3] = {0}, tpsr[12];
    local_use_string4[8] = '\0';
    local_use_string2[3] = '\0';
    local_use_string5[2] = '\0';
    srand(time(NULL));
    do {
        if (argc == 0 && sflg == 0) {
            printf("podany plik nie zostal odnazeziony prosze wprowadzic poprawna sciezke\n");
            sflg = 1;
            continue;
        }
        else {
            if (sflg == 1) {
                printf("podany plik nie zostal odnazeziony prosze wprowadzic poprawna sciezke\n");
                scanf("%s", fpath);
            }
            //wczytywanie sciezki z argumentow
            if (sflg == 0 && argc > 1) {
                strcpy(fpath, argv[1]);
            }
            infile = fopen(fpath, "r");
            sflg = 1;
            printf("Entered Name: %s\n", fpath);
        }
    } while (infile == NULL);
    //zerowanie tablicy inval
    for (i = 0; i < N_LINES; i++) { strcpy(inval[i], temporary_input_string); }
    //wczytywanie wejścia tekstowego do tablicy inval
    i = 0;
    while (fgets(temporary_input_string, sizeof temporary_input_string, infile) != NULL) {
        strcpy(inval[i], temporary_input_string);
        i += 1;
    }
    fclose(infile);


    //////////////////////////////////////////////////////////////////////////////////////////
    /*
    PARSOWANIE
    */


    if (argc <= 2 || ((argc == 3 || argc == 4) && strcmp(argv[2], "psa_code") == 0)) {
        
        for (i = 0; i < N_LINES; i++)
        {
            uam = 1;
            line_used = 0;
            if (is_line_empty(inval[i]) && is_line_empty(inval[i + 1]) || is_line_empty(inval[saveline]) && is_line_empty(inval[saveline + 1])) {
                endmark = i;
                break;
            }
            //wykrywalnie linii z samym komentarzem
            if (inval[i][0] == 47 && inval[i][1] == 47) {
                surpl += 1;
                if (isfset == 0) {
                    osurpl += 1;
                }
                continue;
            }
            //znajdowanie pustej linii miedzy sekcjami
            if (isfset == 0 && inval[i][0] == '\n' || (inval[i][0] == '\n' && inval[i][1] == 13)) {
                ord_section = i + 1;
                isfset = 1;
                strcpy(buffered_stage_1[saveline][0], "");
                saveline += 1;
                continue;
            }
            //k -index słowa
            k = 0;
            // l inedx litery w słowie
            l = 0;
            if (inval[i][0] == 32) {
                k += 1;
                uam = 0;
            }
            for (j = 0; j < N_COLS; j++){
                if (j == N_COLS - 1 && inval[i][j] == 0) {
                    continue;
                }
                //eliminacja komentarzy
                if (j != 99 && inval[i][j + 1] == 47 && inval[i][j] == 47) {

                    j = N_LINES - 1;
                    continue;
                }
                //omijanie pustych znaków
                if (((j != 0 && (inval[i][j - 1] != 32 && inval[i][j - 1] != 44) && (inval[i][j - 1] != 32 && inval[i][j - 1] != 42) && inval[i][j] == 32) || inval[i][j] == 9 || inval[i][j] == 44 || inval[i][j] == 40 || inval[i][j] == 42) && uam == 1) {
                    k = k + 1;
                    l = 0;
                    c += 1;
                    uam = 0;
                }
                //wczytywanie do tablicy buffered_stage_1
                if ((inval[i][j] > 0 && inval[i][j] != 32 && inval[i][j] != 42 && inval[i][j] != 40 && inval[i][j] != 41 && inval[i][j] != 10 && inval[i][j] != 44) && ((inval[i][j] >= 48 && inval[i][j] <= 57) || (inval[i][j] >= 65 && inval[i][j] <= 90) || (inval[i][j] >= 97 && inval[i][j] <= 122)) || inval[i][j]==45) {
                    uam = 1;
                    buffered_stage_1[saveline][k][l] = inval[i][j];
                    l += 1;
                    line_used = 1;
                }

            }
            if (line_used) {
                saveline += 1;
            }
        }


        //korekta o linie z samym komentarzem
        endmark = endmark - surpl;
        ord_section = ord_section - osurpl;
        //printf("ORD SECTION: %d\n", ord_section);

        //parsowanie cz2
        for (i = 0; i < endmark; i++) {
            optab[i].arg1 = 0;
            optab[i].arg2 = 0;
            if (i == ord_section - 1) { continue; }
            if (strcmp(buffered_stage_1[i][1], "") == 0){
                break;
            }

            //przetwarzanie etykiet
            strcpy(optab[i].etykieta, buffered_stage_1[i][0]);
            strcpy(optab[i].order_code, buffered_stage_1[i][1]);
            //obsługa sekcji danych
            if (i < (ord_section - 1))
            {
                if (strcmp(buffered_stage_1[i][2], "INTEGER") == 0)
                {
                    optab[i].index_shift = 0;
                    strcpy(optab[i].directive_typefield, buffered_stage_1[i][2]);
                    if (strcmp(buffered_stage_1[i][1], "DC") == 0)
                    {

                        optab[i].arg1 = atoi(buffered_stage_1[i][3]);
                    }
                }
                else {
                    optab[i].index_shift = -1;
                    strcpy(optab[i].directive_typefield, buffered_stage_1[i][3]);
                    if (strcmp(buffered_stage_1[i][1], "DC") == 0)
                    {
                        optab[i].arg2 = atoi(buffered_stage_1[i][4]);
                    }
                    optab[i].arg1 = atoi(buffered_stage_1[i][2]);
                }
                continue;
            }
            //obsługa sekcji rozkazów


            //obsługa rozkazów na rejestrach
            if (get_byte_value(optab[i].order_code) == 2)
            {
                optab[i].arg1 = atoi(buffered_stage_1[i][2]);
                optab[i].arg2 = atoi(buffered_stage_1[i][3]);
                continue;
            }
            k = 0;
            //wczytywanie przesuniec w rozkazach
            if (isdigit(buffered_stage_1[i][3][0]) == 0 && optab->etykieta != 0)
            {
                k = 1;
                if (buffered_stage_1[i][1][0] == 'J')
                {
                    shiftval = get_adresss(buffered_stage_1[i][2], ord_section);
                }
                else {
                    shiftval = get_adresss(buffered_stage_1[i][3], ord_section);
                }

            }
            else {
                shiftval = atoi(buffered_stage_1[i][3]);
            }
            //obsługa jump
            if (buffered_stage_1[i][1][0] == 'J') {

                optab[i].arg1 = shiftval;
                if (k == 1) {
                    optab[i].arg2 = default_operation_section_reg;
                }
                else
                {
                    optab[i].arg2 = atoi(buffered_stage_1[i][4]);
                }


            }
            else {
                //obsługa pozostałych funkcji
                optab[i].arg1 = atoi(buffered_stage_1[i][2]);
                optab[i].index_shift = shiftval;
                if (k == 1) {
                    optab[i].arg2 = default_data_section_reg;
                }
                else
                {
                    optab[i].arg2 = atoi(buffered_stage_1[i][4]);
                }
            }
        }

        //wyliczanie wielkości sekcji danych w bitach
        ds = c_space(ord_section);
        datasec = (long*)calloc(ds, sizeof(long));
        m = 0;
        //wyliczanie wielkości sekcji rozkazów w bitach
        n = endmark - ord_section - 1;



        //tworzenie heksadecymalnego pliku programu
        //printf("ORD SECTION FILE CREATION: %d\n", ord_section);
        ofile = fopen("output.txt", "w");
        if (ofile)
        {
            for (i = 0; i < endmark; i++)
            {
                //obsluga pustej linii
                if (i == ord_section - 1) {
                    fprintf(ofile, "\n");
                    continue;
                }
                //obsluga wpisywania sekcji danych do pliku i wpisywania sekcji danych do tablicy danych
                if (i < ord_section) {
                    if (optab[i].index_shift == -1) {
                        if (strcmp(optab[i].order_code, "DS") == 0) {
                            for (j = 0; j < optab[i].arg1; j++) {
                                fprintf(ofile, "~~ ~~ ~~ ~~\n");
                                datasec[m] = rand();
                                m += 1;
                            }
                        }
                        else {
                            for (j = 0; j < optab[i].arg1; j++) {
                                fprintf(ofile, "%s\n", transform_shift_arr(optab[i].arg1, 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                                datasec[m] = optab[i].arg1;
                                m += 1;
                            }
                        }
                    }
                    else {
                        if (strcmp(optab[i].order_code, "DS") == 0) {
                            fprintf(ofile, "~~ ~~ ~~ ~~\n");
                            datasec[m] = rand();
                            m += 1;
                        }
                        else {
                            fprintf(ofile, "%s\n", transform_shift_arr(optab[i].arg1, 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                            datasec[m] = optab[i].arg1;
                            m += 1;
                        }
                    }
                    continue;
                }
                //obsluga sekcji rozkazow
                if (i >= ord_section) {
                    n += 1;
                    //obsluga 2B
                    if (get_byte_value(optab[i].order_code) == 2) {
                        fprintf(ofile, "%X ", get_hex_ord_code(optab[i].order_code));
                        fprintf(ofile, "%X", optab[i].arg1);
                        fprintf(ofile, "%X\n", optab[i].arg2);
                    }
                    else {
                        //obsluga 4B
                        fprintf(ofile, "%X ", get_hex_ord_code(optab[i].order_code));
                        //obsluga instrukcji jump
                        if (strcmp(optab[i].order_code, "J") == 0 || strcmp(optab[i].order_code, "JZ") == 0 || strcmp(optab[i].order_code, "JN") == 0 || strcmp(optab[i].order_code, "JP") == 0) {
                            fprintf(ofile, "0");
                            fprintf(ofile, "%X ", optab[i].arg2);
                            fprintf(ofile, "%s\n", transform_shift_arr(optab[i].arg1, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                        }
                        else {

                            //obsluga innych instrukcji rejestr pamiec
                            fprintf(ofile, "%X", optab[i].arg1);
                            fprintf(ofile, "%X ", optab[i].arg2);
                            fprintf(ofile, "%s\n", transform_shift_arr(optab[i].index_shift, 1, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
                        }

                    }
                }
            }
            fclose(ofile);
        }
        else
        {
            printf("nie mozna bylo otworzyc pliku\n");
            return 1;
        }
    }
    else if ((argc == 3 || argc == 4) && strcmp(argv[2], "msck_code") == 0) {
        is_psa = 0;
        sflg = 1;
        ds = 0;
        n = 0;
        // parsowanie pliku maszynowego
        for (i = 0; i < N_LINES; i++) {
            if (inval[i][0] == 0) {
                endmark = i;
                break;
            }
            else {
                if (inval[i][0] == 10) {
                    ord_section = i + 1;
                    sflg = 0;
                    continue;
                }
                if (sflg) {
                    ds += 1;
                }
                else { n += 1; }
            }
        }
        datasec = (long*)calloc(ds, sizeof(long));
        for (i = 0; i < endmark; i++) {
            if (i < ord_section - 1) {
                datasec[i] = process_hexdata(i, inval[i], 8);
            }
            else if (i >= ord_section) {
                process_hexord(i, inval[i], local_use_string5);
            }
        }

    }
    else {
        printf("CRITICAL ERROR WHILE PARSING PARAMETERS");
        return 2;
    }
    //for (i = 0; i < ds; ++i) {
    //    printf("%d\n", datasec[i]);
    //}
    //for (i = 0; i < endmark; ++i) {
    //    printf("%d>>", i);
    //    printf("%s: ", optab[i].etykieta);
    //    printf("%s: ", optab[i].order_code);
    //    printf("%s: ", optab[i].directive_typefield);
    //    printf("%d: ", optab[i].arg1);
    //    printf("%d: ", optab[i].arg2);
    //    printf("%d: ", optab[i].index_shift);
    //    printf("\n");
    //}
    //printf("%d: ",c);
    //for (i = 0; i < endmark; i++)
    //{
    //    printf("\n");
    //    printf("%d: /", i);
    //    for (k = 0; k < 5; k++)
    //    {
    //        printf("%s/", buffered_stage_1[i][k]);
    //    }
    //   printf("\n");
    //    printf("-------");
    //}
    ////////////////////////////////////////////////////////////////////////////////////////
    /*
    EGZEKUCJA PROGRAMU
    */
    if (argc == 4 && strcmp(argv[3], "debug") == 0) {
        initscr();
        noecho();
        curs_set(0);
        //ustawianie ogranicznikow do wypisania <l, u)
        u_ord=endmark;
        l_ord=ord_section;
        u_dat=ds;
        l_dat=0;
        u_psa = endmark;
        l_psa = 0;
        if (u_dat >= 30) {
            u_dat = 30;
        }
        if (endmark - ord_section >= 30) {
            u_ord = l_ord+30;
        }
        if (endmark >= 30) {
            u_psa = 30;
        }
        curr_state = ord_section;
        printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord,  l_ord,  u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
        
        while (1) {
            if (curr_state == endmark || curr_state == -1) { break; }
            curr_state = execute(curr_state, sfd, overflow, ds, compres);
            //obsługa błędu
            if (*overflow == 1)
            {
                printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                printf("PRZERWANIE WYKONANIA PROGRAMU, BLAD PODCZAS WYKONYWANIA PROGRAMU, LINIJKA :%d \n", *sfd);
                getch();
                exit(1);
            }
            printstate(sfd, local_use_string4,  local_use_string5,  local_use_string2,  local_use_string3,  local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
            // POCZATEK PORUSZANIA
            
            while (d==0)
            {
                c = getch();
                if (c != 120)
                {
                    while (c != 120) {
                        c = 0;
                        c = wgetch(stdscr);
                        if (c == 113)
                        {
                            d = 1;
                            break;
                        }
                        //printf("(%d)\n", c);
                        if (c == 115) {
                            // poruszanie w dol rozkazy
                            if (u_ord < endmark) {
                                l_ord += 1;
                                u_ord += 1;
                            }
                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);

                        }
                        else if (c == 119) {
                            // poruszanie w gore rozkazy
                            if (l_ord > ord_section) {
                                l_ord -= 1;
                                u_ord -= 1;
                            }
                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                        }
                        else if (c == 101) {
                            // poruszanie w gore dane
                            if (l_dat > 0) {
                                l_dat -= 1;
                                u_dat -= 1;
                            }

                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                        }
                        else if (c == 100) {
                            // poruszanie w dol dane
                            if (u_dat < ds) {
                                //printf("bazinga\n");
                                l_dat += 1;
                                u_dat += 1;
                            }
                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                        }
                        else if (c == 114) {
                            //printf("psa_up\n");
                            //poruszanie gora psa
                            if (l_psa > 0) {

                                l_psa -= 1;
                                u_psa -= 1;
                            }
                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                        }
                        else if (c == 102) {
                            //printf("psa_down\n");
                            //poruszanie dol psa
                            if (u_psa < endmark) {
                                l_psa += 1;
                                u_psa += 1;
                            }
                            printstate(sfd, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string, ds, compres, curr_state, u_ord, l_ord, u_dat, l_dat, crrw, is_psa, u_psa, l_psa);
                        }
                    }
                }
                else
                {
                    break;
                }
                // KONIEC PORUSZANIA
                //printf("\n::nxs::\n");
            }
            
        }
        if (d!=1)
        {
            getch();
        }
        endwin();
    }else if (argc < 4) {
        curr_state = ord_section;
        while (1) {
            if (curr_state == endmark || curr_state == -1) { break; }
            curr_state = execute(curr_state, sfd, overflow, ds, compres);
            if (*overflow==1)
            {
                printf("PRZERWANIE WYKONANIA PROGRAMU, BLAD PODCZAS WYKONYWANIA PROGRAMU, LINIJKA :%d \n", curr_state);
                exit(1);
            }
        }
    }else {
        printf("CRITICAL ERROR WHILE PARSING PARAMETERS");
        return 1;
    }
    ///////////////////////////////////////////////////////////////////////////////////////////
    /*
    ZAPIS WYNIKU DO PLIKU
    */
    varfile = fopen("var.txt", "w");
    for (i = 0; i < ds; ++i) {
        fprintf(varfile, "%s\n", transform_shift_arr(datasec[i], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
    }
    fprintf(varfile, "\n");
    for (i = 0; i < 15; ++i) {
        fprintf(varfile, "%s\n", transform_shift_arr(registers[i], 0, local_use_string4, local_use_string5, local_use_string2, local_use_string3, local_use_string));
    }
    printf("\nexec_end");
    return 0;

}