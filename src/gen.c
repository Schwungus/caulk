#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_MAX (2048)
#define ARGS_MAX (128)

static void prints(FILE* file, const char* str, size_t len) {
    for (size_t i = 0; i < len; i++)
        fputc(str[i], file);
}
static void printl(FILE* file, const char* str) {
    prints(file, str, strlen(str));
}

struct arg {
    const char* name;
    size_t len;
};

int main(int argc, char* argv[]) {
    if (argc != 4)
        return EXIT_FAILURE;

    FILE *input = fopen(argv[3], "rt"), *hOutput = fopen(argv[1], "wt"), *cOutput = fopen(argv[2], "wt");
    if (input == NULL || hOutput == NULL || cOutput == NULL)
        return EXIT_FAILURE;

    fprintf(hOutput, "#pragma once\n\n");
    fprintf(hOutput, "#include \"caulkSteamTypes.h\"\n\n");

    fprintf(cOutput, "#include \"caulkSteamTypes.h\"\n\n");

    char line[BUF_MAX];
    while (fgets(line, BUF_MAX, input) != NULL) {
        if (strstr(line, "S_API ") == NULL)
            continue;
        const char* sub = line;

        while (*++sub != ' ') {}
        const char* typeStart = sub + 1;

        while (*++sub != '(') {}
        const char *typeAndNameEnd = sub - 1, *argsStart = sub;

        while (*++sub != '\n') {}
        const char* argsEnd = sub - 2;

        sub = typeAndNameEnd;
        while (*--sub != ' ') {}

        const char *name = sub + 1, *type = typeStart, *args = argsStart;
        size_t nameLen = typeAndNameEnd - name + 1, typeLen = name - type - 1, argsLen = argsEnd - argsStart + 1;

        for (char* c = (char*)argsStart; c != argsEnd; c++)
            if (*c == '&')
                *c = '*';

        prints(hOutput, type, typeLen);
        printl(hOutput, " caulk__");
        prints(hOutput, name, nameLen);
        prints(hOutput, args, argsLen);
        printl(hOutput, ";\n");

        struct arg parsedArgs[ARGS_MAX];
        for (size_t i = 0; i < ARGS_MAX; i++)
            parsedArgs[i].name = NULL;

        const char* curArg = argsStart + 1;
        size_t argsParsed = 0;
        for (;;) {
            while (*curArg == ' ' || *curArg == ',')
                curArg++;
            if (*curArg == ')')
                break;

            const char* argEnd = curArg;
            while (*argEnd != ',' && *argEnd != ')')
                argEnd++;
            if (*argEnd == ')')
                argEnd--;

            curArg = argEnd;
            while (*--curArg == ' ') {}
            while (*--curArg != ' ') {}

            parsedArgs[argsParsed].name = curArg + 1;
            parsedArgs[argsParsed].len = argEnd - parsedArgs[argsParsed].name;

            argsParsed++;
            curArg = argEnd;
        }

        prints(cOutput, type, typeLen);
        printl(cOutput, " caulk__");
        prints(cOutput, name, nameLen);
        prints(cOutput, args, argsLen);
        printl(cOutput, " {\n\treturn ");
        prints(cOutput, name, nameLen);
        printl(cOutput, "(");
        for (size_t i = 0; i < argsParsed; i++) {
            prints(cOutput, parsedArgs[i].name, parsedArgs[i].len);
            if (i != argsParsed - 1)
                printl(cOutput, ", ");
        }
        printl(cOutput, ");\n");
        printl(cOutput, "}\n\n");
    }

    fclose(input);
    fclose(hOutput);
    fclose(cOutput);

    return EXIT_SUCCESS;
}
