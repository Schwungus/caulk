#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

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
    fprintf(hOutput, "#include <stdint.h>\n\n");

    fprintf(cOutput, "#include \"__gen.h\"\n\n");

    yyjson_val* obj = yyjson_doc_get_root(doc);
    yyjson_arr_iter iter;

    yyjson_val* cnst = NULL;
    yyjson_val* consts = yyjson_obj_get(obj, "consts");
    yyjson_arr_iter_init(consts, &iter);
    while ((cnst = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(cnst, "constname"));
        const char* type = yyjson_get_str(yyjson_obj_get(cnst, "consttype"));
        const char* value = yyjson_get_str(yyjson_obj_get(cnst, "constval"));
        fprintf(hOutput, "#define %s ((%s)(%s))\n", name, type, value);
    }
    printl(hOutput, "\n");

    fprintf(hOutput, "typedef uint32_t enum32_t;\n\n");

    yyjson_val* enm = NULL;
    yyjson_val* enums = yyjson_obj_get(obj, "enums");
    yyjson_arr_iter_init(enums, &iter);
    while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
        const char* name = yyjson_get_str(yyjson_obj_get(enm, "enumname"));

        fprintf(hOutput, "#ifndef __cplusplus\n");
        fprintf(hOutput, "typedef enum32_t %s;\n", name);
        fprintf(hOutput, "#endif\n");

        fprintf(hOutput, "enum %s {\n", name);

        yyjson_val* val = NULL;
        yyjson_val* values = yyjson_obj_get(enm, "values");
        yyjson_arr_iter val_iter;
        yyjson_arr_iter_init(values, &val_iter);
        while ((val = yyjson_arr_iter_next(&val_iter)) != NULL) {
            name = yyjson_get_str(yyjson_obj_get(val, "name"));
            const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
            fprintf(hOutput, "    %s = %s,\n", name, value);
        }

        fprintf(hOutput, "};\n\n");
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

    yyjson_val* struc = NULL;
    yyjson_val* structs = yyjson_obj_get(obj, "callback_structs");
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

        yyjson_val* enums = yyjson_obj_get(struc, "enums");
        if (enums != NULL) {
            yyjson_val* enm = NULL;
            yyjson_arr_iter enm_iter;
            yyjson_arr_iter_init(enums, &enm_iter);
            while ((enm = yyjson_arr_iter_next(&enm_iter)) != NULL) {
                const char* enum_name = yyjson_get_str(yyjson_obj_get(enm, "enumname"));

                fprintf(hOutput, "#ifndef __cplusplus\n");
                fprintf(hOutput, "typedef enum32_t %s__%s;\n", name, enum_name);
                fprintf(hOutput, "#endif\n");
                fprintf(hOutput, "enum %s__%s {\n", name, enum_name);

                yyjson_val* val = NULL;
                yyjson_arr_iter val_iter;
                yyjson_arr_iter_init(yyjson_obj_get(enm, "values"), &val_iter);
                while ((val = yyjson_arr_iter_next(&val_iter)) != NULL) {
                    const char* val_name = yyjson_get_str(yyjson_obj_get(val, "name"));
                    const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
                    fprintf(hOutput, "    %s = %s,\n", val_name, value);
                }

                fprintf(hOutput, "};\n");
            }
        }

        yyjson_val* fields = yyjson_obj_get(struc, "fields");
        if (fields == NULL || yyjson_arr_size(fields) == 0) {
            fprintf(hOutput, "typedef struct %s %s;\n", name, name);
        } else {
            if (enums != NULL)
                fprintf(hOutput, "\n");
            fprintf(hOutput, "typedef struct %s {\n", name);

            yyjson_val* fld = NULL;
            yyjson_arr_iter fld_iter;
            yyjson_arr_iter_init(fields, &fld_iter);
            while ((fld = yyjson_arr_iter_next(&fld_iter)) != NULL) {
                char field_type[1024];
                strcpy_s(field_type, sizeof(field_type), yyjson_get_str(yyjson_obj_get(fld, "fieldtype")));

                char* pos = strrchr(field_type, ':');
                while (pos != NULL) {
                    *pos = '_';
                    pos = strrchr(field_type, ':');
                }

                const char* field_name = yyjson_get_str(yyjson_obj_get(fld, "fieldname"));
                fprintf(hOutput, "    %s %s;\n", field_type, field_name);
            }

            fprintf(hOutput, "} %s;\n", name);
        }

        if (enums != NULL || fields != NULL)
            fprintf(hOutput, "\n");
    }

    yyjson_doc_free(doc);
    fclose(hOutput);
    fclose(cOutput);

    return EXIT_SUCCESS;
}
