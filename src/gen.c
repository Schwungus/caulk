#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

#define INDENT "\t"
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

static const char* fieldName(const char* field, const char* master) {
	if (master == NULL)
		return field;

	static char buf[512];
	size_t i = 0;

	for (; i < strlen(master); i++)
		buf[i] = master[i];

	buf[i++] = '_';
	buf[i++] = '_';

	for (size_t j = 0; j < strlen(field); j++, i++)
		buf[i] = field[j];

	buf[i] = '\0';
	return buf;
}

static const char* sanitizeType(const char* weee) {
	static char buf[512];
	size_t i = 0;
	for (; i < strlen(weee); i++)
		if (weee[i] == ':')
			buf[i] = '_';
		else if (weee[i] == '&')
			buf[i] = '*';
		else
			buf[i] = weee[i];
	buf[i] = '\0';
	return buf;
}

static void defineEnum(FILE* out, yyjson_val* enm, const char* parent) {
	const char* child = yyjson_get_str(yyjson_obj_get(enm, "enumname"));
	const char* name = fieldName(child, parent);

	fprintf(hOutput, "#ifndef __cplusplus\n");
	fprintf(hOutput, "typedef enum32_t %s;\n", name);
	fprintf(hOutput, "#endif\n");
}

static void fillEnum(FILE* out, yyjson_val* enm, const char* parent) {
	const char* child = yyjson_get_str(yyjson_obj_get(enm, "enumname"));
	const char* name = fieldName(child, parent);

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

static void writeEnums(yyjson_val* enums, const char* parent) {
	yyjson_arr_iter iter;
	yyjson_val* enm = NULL;

	yyjson_arr_iter_init(enums, &iter);

	while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
		defineEnum(hOutput, enm, parent);
		fillEnum(hOutput, enm, parent);
	}

	if (yyjson_get_len(enums))
		printl(hOutput, "\n");
}

static void writeDecl(const char* name, const char* type, bool private) {
	char* offset = NULL;

	type = sanitizeType(type);
	if ((offset = strstr(type, "(*)")) != NULL) {
		prints(hOutput, type, offset + 2 - type);
		fprintf(hOutput, "%s%s%s", private ? "__" : "", name, offset + 2);
	} else if ((offset = strstr(type, "[")) != NULL) {
		prints(hOutput, type, offset - 1 - type);
		fprintf(hOutput, " %s%s%s", private ? "__" : "", name, offset);
	} else {
		fprintf(hOutput, "%s", type);
		fprintf(hOutput, " %s%s", private ? "__" : "", name);
	}

	printl(hOutput, ";\n");
}

static void writeFields(yyjson_val* fields) {
	if (!yyjson_get_len(fields)) {
		printl(hOutput, INDENT "void* DUMMY;\n");
		return;
	}

	yyjson_arr_iter fld_iter;
	yyjson_arr_iter_init(fields, &fld_iter);

	yyjson_val* fld = NULL;
	while ((fld = yyjson_arr_iter_next(&fld_iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(fld, "fieldname"));
		const char* type = yyjson_get_str(yyjson_obj_get(fld, "fieldtype"));
		bool private = yyjson_get_bool(yyjson_obj_get(fld, "private"));
		printl(hOutput, INDENT);
		writeDecl(name, type, private);
	}
}

static void writeParams(yyjson_val* params) {
	yyjson_arr_iter arg_iter;
	yyjson_arr_iter_init(params, &arg_iter);

	yyjson_val* arg = NULL;
	while ((arg = yyjson_arr_iter_next(&arg_iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* type = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));
		fprintf(hOutput, "%s %s", sanitizeType(type), name);
		if (yyjson_arr_iter_has_next(&arg_iter))
			fprintf(hOutput, ", ");
	}
}

static void writeMethods(yyjson_val* methods) {
	yyjson_arr_iter met_iter;
	yyjson_arr_iter_init(methods, &met_iter);

	yyjson_val* met = NULL;
	while ((met = yyjson_arr_iter_next(&met_iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(met, "methodname_flat"));
		const char* returns = yyjson_get_str(yyjson_obj_get(met, "returntype"));
		fprintf(hOutput, "%s %s(", sanitizeType(returns), name);
		writeParams(yyjson_obj_get(met, "params"));
		fprintf(hOutput, ");\n");
	}
}

static void genConsts() {
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

static void genTypedefs() {
	writeEnums(yyjson_obj_get(ROOT_OBJ, "enums"), NULL);

	yyjson_arr_iter iter;
	yyjson_val* sources[] = {
	    yyjson_obj_get(ROOT_OBJ, "structs"),
	    yyjson_obj_get(ROOT_OBJ, "callback_structs"),
	    yyjson_obj_get(ROOT_OBJ, "interfaces"),
	};

	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_val* struc = NULL;
		yyjson_arr_iter_init(sources[i], &iter);

		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			const char* parent = yyjson_get_str(yyjson_obj_get(struc, i == 2 ? "classname" : "struct"));
			fprintf(hOutput, "typedef struct %s %s;\n", parent, parent);
			writeEnums(yyjson_obj_get(struc, "enums"), parent);
		}

		printl(hOutput, "\n");
	}

	yyjson_val* typedf = NULL;
	yyjson_val* typedefs = yyjson_obj_get(ROOT_OBJ, "typedefs");
	yyjson_arr_iter_init(typedefs, &iter);

	while ((typedf = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(typedf, "typedef"));
		const char* type = yyjson_get_str(yyjson_obj_get(typedf, "type"));
		printl(hOutput, "typedef ");
		writeDecl(name, type, false);
	}
	printl(hOutput, "\n");
}

static void writeStruct(yyjson_val* struc) {
	const char* name = yyjson_get_str(yyjson_obj_get(struc, "struct"));
	yyjson_val* fields = yyjson_obj_get(struc, "fields");

	fprintf(hOutput, "struct %s {\n", name);
	writeFields(fields);
	fprintf(hOutput, "};\n");

	yyjson_val* methods = yyjson_obj_get(struc, "methods");
	if (yyjson_get_len(methods)) {
		fprintf(hOutput, "\n");
		writeMethods(methods);
	}
}

static void genStructs() {
	yyjson_arr_iter iter;
	yyjson_val* sources[] = {
	    yyjson_obj_get(ROOT_OBJ, "structs"),
	    yyjson_obj_get(ROOT_OBJ, "callback_structs"),
	};

	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_val* struc = NULL;
		yyjson_arr_iter_init(sources[i], &iter);

		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			writeStruct(struc);
			printl(hOutput, "\n");
		}
	}
}

static void genInterfaces() {
	yyjson_val* intr = NULL;
	yyjson_val* interfaces = yyjson_obj_get(ROOT_OBJ, "interfaces");
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(interfaces, &iter);

	while ((intr = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(intr, "classname"));

		yyjson_val* fields = yyjson_obj_get(intr, "fields");
		fprintf(hOutput, "struct %s {\n", name);
		writeFields(fields);
		fprintf(hOutput, "};\n");

		yyjson_val* methods = yyjson_obj_get(intr, "methods");
		writeMethods(methods);

		printl(hOutput, "\n");
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
	fprintf(hOutput, "typedef void (*SteamAPIWarningMessageHook_t)(int, const char*);\n\n");
	fprintf(hOutput, "#ifndef __cplusplus\ntypedef uint8_t bool;\n#endif\n");

	fprintf(cOutput, "#include \"__gen.h\"\n\n");

	genConsts();
	genTypedefs();
	genStructs();
	genInterfaces();

	yyjson_doc_free(gDoc);
	fclose(hOutput);
	fclose(cOutput);

	return EXIT_SUCCESS;
}
