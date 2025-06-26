#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

#define BUF_MAX (2048)
#define ARGS_MAX (128)
#define INDENT "    "

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

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

static FILE *hOutput = NULL, *cOutput = NULL;
static yyjson_doc* gDoc;
#define ROOT_OBJ (yyjson_doc_get_root(gDoc))

static const char* uwu(const char* child, const char* parent) {
    if (parent == NULL)
        return child;

    static char buf[512];
    size_t i = 0;

    for (; i < strlen(parent); i++)
        buf[i] = parent[i];

    buf[i++] = '_';
    buf[i++] = '_';

    for (size_t j = 0; j < strlen(child); j++, i++)
        buf[i] = child[j];

    buf[i] = '\0';
    return buf;
}

static const char* owo(const char* weee) {
    static char buf[512];
    size_t i = 0;
    for (; i < strlen(weee); i++)
        buf[i] = weee[i] == ':' ? '_' : weee[i];
    buf[i] = '\0';
    return buf;
}

static void precumEnum(FILE* out, yyjson_val* enm, const char* parent) {
    const char* child = yyjson_get_str(yyjson_obj_get(enm, "enumname"));
    const char* name = uwu(child, parent);

    fprintf(hOutput, "#ifndef __cplusplus\n");
    fprintf(hOutput, "typedef enum32_t %s;\n", name);
    fprintf(hOutput, "#endif\n");
}

static void ejacEnum(FILE* out, yyjson_val* enm, const char* parent) {
    const char* child = yyjson_get_str(yyjson_obj_get(enm, "enumname"));
    const char* name = uwu(child, parent);

    fprintf(out, "enum %s {\n", name);

    yyjson_val* val = NULL;
    yyjson_val* values = yyjson_obj_get(enm, "values");
    yyjson_arr_iter val_iter;
    yyjson_arr_iter_init(values, &val_iter);
    while ((val = yyjson_arr_iter_next(&val_iter)) != NULL) {
        name = yyjson_get_str(yyjson_obj_get(val, "name"));
        const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
        fprintf(out, INDENT "%s = %s,\n", name, value);
    }

    fprintf(out, "};\n\n");
}

static void impregEnums(yyjson_val* enums, const char* parent) {
    if (!yyjson_get_len(enums))
        return;

    yyjson_arr_iter iter;
    yyjson_val* enm = NULL;

    yyjson_arr_iter_init(enums, &iter);

    while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
        precumEnum(hOutput, enm, parent);
        ejacEnum(hOutput, enm, parent);
    }

    printl(hOutput, "\n");
}

static void spitField(const char* name, const char* type) {
    char* offset = NULL;

    type = owo(type);
    if ((offset = strstr(type, "(*)")) != NULL) {
        prints(hOutput, type, offset + 2 - type);
        fprintf(hOutput, "%s%s", name, offset + 2);
    } else if ((offset = strstr(type, "[")) != NULL) {
        prints(hOutput, type, offset - 1 - type);
        fprintf(hOutput, " %s%s", name, offset);
    } else {
        fprintf(hOutput, "%s", type);
        fprintf(hOutput, " %s", name);
    }

    printl(hOutput, ";\n");
}

static void constipate() {
    yyjson_arr_iter iter;

    yyjson_val* cnst = NULL;
    yyjson_val* consts = yyjson_obj_get(yyjson_doc_get_root(gDoc), "consts");
    yyjson_arr_iter_init(consts, &iter);

    while ((cnst = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(cnst, "constname"));
        const char* type = yyjson_get_str(yyjson_obj_get(cnst, "consttype"));
        const char* value = yyjson_get_str(yyjson_obj_get(cnst, "constval"));
        fprintf(hOutput, "#define %s ((%s)(%s))\n", name, type, value);
    }

    printl(hOutput, "\n");
}

static void precum() {
    impregEnums(yyjson_obj_get(ROOT_OBJ, "enums"), NULL);

    yyjson_arr_iter iter;
    yyjson_val* sources[] = {
        yyjson_obj_get(ROOT_OBJ, "structs"),
        yyjson_obj_get(ROOT_OBJ, "callback_structs"),
    };

    for (size_t i = 0; i < LENGTH(sources); i++) {
        yyjson_val* struc = NULL;
        yyjson_arr_iter_init(sources[i], &iter);

        while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
            const char* parent = yyjson_get_str(yyjson_obj_get(struc, "struct"));
            fprintf(hOutput, "typedef struct %s %s;\n", parent, parent);
            impregEnums(yyjson_obj_get(struc, "enums"), parent);
        }
    }
    printl(hOutput, "\n");

    yyjson_val* typedf = NULL;
    yyjson_val* typedefs = yyjson_obj_get(ROOT_OBJ, "typedefs");
    yyjson_arr_iter_init(typedefs, &iter);

    while ((typedf = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(typedf, "typedef"));
        const char* type = yyjson_get_str(yyjson_obj_get(typedf, "type"));
        printl(hOutput, "typedef ");
        spitField(name, type);
    }
    printl(hOutput, "\n");
}

static void strunc() {
    yyjson_arr_iter iter;
    yyjson_val* sources[] = {
        yyjson_obj_get(ROOT_OBJ, "structs"),
        yyjson_obj_get(ROOT_OBJ, "callback_structs"),
    };

    for (size_t i = 0; i < LENGTH(sources); i++) {
        yyjson_val* struc = NULL;
        yyjson_arr_iter_init(sources[i], &iter);

        while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
            const char* name = yyjson_get_str(yyjson_obj_get(struc, "struct"));

            yyjson_val* fld = NULL;
            yyjson_val* fields = yyjson_obj_get(struc, "fields");

            yyjson_arr_iter fld_iter;
            yyjson_arr_iter_init(fields, &fld_iter);

            fprintf(hOutput, "struct %s {\n", name);
            if (yyjson_get_len(fields))
                while ((fld = yyjson_arr_iter_next(&fld_iter)) != NULL) {
                    const char* name = yyjson_get_str(yyjson_obj_get(fld, "fieldname"));
                    const char* type = yyjson_get_str(yyjson_obj_get(fld, "fieldtype"));
                    printl(hOutput, INDENT);
                    spitField(name, type);
                }
            else
                fprintf(hOutput, INDENT "void* DUMMY;\n");
            fprintf(hOutput, "};\n");

            printl(hOutput, "\n");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4)
        return EXIT_FAILURE;

    hOutput = fopen(argv[1], "wt");
    cOutput = fopen(argv[2], "wt");
    if (hOutput == NULL || cOutput == NULL)
        return EXIT_FAILURE;

    yyjson_arr_iter iter;
    yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
    yyjson_read_err err;

    gDoc = yyjson_read_file(argv[3], flg, NULL, &err);
    if (gDoc == NULL)
        return EXIT_FAILURE;

    fprintf(hOutput, "#pragma once\n\n");
    fprintf(hOutput, "#include <stdint.h>\n\n");

    fprintf(hOutput, "typedef uint32_t enum32_t;\n");
    fprintf(hOutput, "typedef enum32_t SteamInputActionEvent_t__AnalogAction_t;\n"); // :(
    fprintf(hOutput, "typedef uint64_t CSteamID, CGameID;\n");
    fprintf(hOutput, "typedef uint8_t bool;\n\n");

    fprintf(cOutput, "#include \"__gen.h\"\n\n");

    constipate();
    precum();
    strunc();

    yyjson_doc_free(gDoc);
    fclose(hOutput);
    fclose(cOutput);

    return EXIT_SUCCESS;
}
