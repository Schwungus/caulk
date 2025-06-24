#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

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

    FILE *hOutput = fopen(argv[1], "wt"), *cOutput = fopen(argv[2], "wt");
    if (hOutput == NULL || cOutput == NULL)
        return EXIT_FAILURE;

    yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
    yyjson_read_err err;
    yyjson_doc* doc = yyjson_read_file(argv[3], flg, NULL, &err);
    if (doc == NULL)
        return EXIT_FAILURE;

    fprintf(hOutput, "#pragma once\n\n");
    fprintf(cOutput, "#include \"__gen.h\"\n\n");

    yyjson_val* obj = yyjson_doc_get_root(doc);
    yyjson_arr_iter iter;

    yyjson_val* struc = NULL;
    yyjson_val* structs = yyjson_obj_get(obj, "structs");
    yyjson_arr_iter_init(structs, &iter);
    while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(struc, "struct"));
        fprintf(hOutput, "typedef struct %s %s;\n", name, name);
    }
    printl(hOutput, "\n");

    struc = NULL;
    structs = yyjson_obj_get(obj, "callback_structs");
    yyjson_arr_iter_init(structs, &iter);
    while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(struc, "struct"));
        fprintf(hOutput, "typedef struct %s %s;\n", name, name);
    }
    printl(hOutput, "\n");

    yyjson_val* typedf = NULL;
    yyjson_val* typedefs = yyjson_obj_get(obj, "typedefs");
    yyjson_arr_iter_init(typedefs, &iter);
    while ((typedf = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(typedf, "typedef"));
        const char* type = yyjson_get_str(yyjson_obj_get(typedf, "type"));
        printl(hOutput, "typedef ");

        char* offset = NULL;
        if ((offset = strstr(type, "(*)")) != NULL) {
            prints(hOutput, type, offset + 2 - type);
            fprintf(hOutput, "%s%s", name, offset + 2);
        } else if ((offset = strstr(type, "[")) != NULL) {
            prints(hOutput, type, offset - 1 - type);
            fprintf(hOutput, " %s%s", name, offset);
        } else
            fprintf(hOutput, "%s %s", type, name);
        printl(hOutput, ";\n");
    }
    printl(hOutput, "\n");

    yyjson_val* enm = NULL;
    yyjson_val* enums = yyjson_obj_get(obj, "enums");
    yyjson_arr_iter_init(structs, &iter);
    while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(enm, "enumname"));
        fprintf(hOutput, "enum %s {\n", name);
    }
    printl(hOutput, "\n");

    struc = NULL;
    structs = yyjson_obj_get(obj, "structs");
    yyjson_arr_iter_init(structs, &iter);
    while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
        yyjson_obj_get(struc, "struct");
    }

    yyjson_doc_free(doc);
    fclose(hOutput);
    fclose(cOutput);

    return EXIT_SUCCESS;
}
