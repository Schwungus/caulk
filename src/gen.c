#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yyjson.h"

#define INDENT "\t"
#define THIS "__THIS"
#define NS_PREFIX "caulk::"
#define METHOD_PREFIX "caulk_"
#define RESULT "__RESULT"

#define LENGTH(expr) (sizeof((expr)) / sizeof(*(expr)))

static void fprint0(FILE* file, const char* str, size_t len) {
	for (size_t i = 0; i < len; i++)
		fputc(str[i], file);
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

static const char* prefixUserType(const char* type) {
	static char buf[1024] = {0};
	static const char* noops[] = {"unsigned ", "int ", "intptr", "int32", "int64",	"int ",
				      "char",	   "void", "bool",   "float", "double", "size_t"};

	if (!strcmp(type, "int"))
		goto noop;
	for (size_t i = 0; i < LENGTH(noops); i++)
		if (strstr(type, noops[i]))
			goto noop;

	char* dest = buf;
	if (strstr(type, "const ") != NULL) {
		strcpy(buf, "const ");
		dest = buf + strlen("const ");
		type += strlen("const ");
	}
	strcpy(dest, NS_PREFIX);
	dest += strlen(NS_PREFIX);
	strcpy(dest, type);

	return buf;

noop:
	strcpy(buf, type);
	return buf;
}

static void defineEnum(yyjson_val* enm, const char* master) {
	const char* name = fieldName(yyjson_get_str(yyjson_obj_get(enm, "enumname")), master);
	fprintf(hOutput, "#ifndef __cplusplus\n");
	fprintf(hOutput, "typedef enum32_t %s;\n", name);
	fprintf(hOutput, "#endif\n");
}

static void fillEnum(yyjson_val* enm, const char* master) {
	const char* name = fieldName(yyjson_get_str(yyjson_obj_get(enm, "enumname")), master);
	fprintf(hOutput, "enum %s {\n", name);

	yyjson_val* val = NULL;
	yyjson_val* values = yyjson_obj_get(enm, "values");
	yyjson_arr_iter val_iter;
	yyjson_arr_iter_init(values, &val_iter);
	while ((val = yyjson_arr_iter_next(&val_iter)) != NULL) {
		name = yyjson_get_str(yyjson_obj_get(val, "name"));
		const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
		fprintf(hOutput, INDENT "%s = %s,\n", name, value);
	}

	fprintf(hOutput, "};\n\n");
}

static void genEnums(yyjson_val* enums, const char* master) {
	yyjson_arr_iter iter;
	yyjson_val* enm = NULL;

	yyjson_arr_iter_init(enums, &iter);

	while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
		defineEnum(enm, master);
		fillEnum(enm, master);
	}

	if (yyjson_get_len(enums))
		fprintf(hOutput, "\n");
}

static void writeDecl(FILE* out, const char* name, const char* type, bool private) {
	char* offset = NULL;
	if ((offset = strstr(type, "(*)")) != NULL) {
		fprint0(out, type, offset + 2 - type);
		fprintf(out, "%s%s%s", private ? "__" : "", name, offset + 2);
	} else if ((offset = strstr(type, "[")) != NULL) {
		fprint0(out, type, offset - 1 - type);
		fprintf(out, " %s%s%s", private ? "__" : "", name, offset);
	} else {
		fprintf(out, "%s", type);
		fprintf(out, " %s%s", private ? "__" : "", name);
	}
}

static void writeFields(FILE* out, yyjson_val* fields) {
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(fields, &iter);

	yyjson_val* field = NULL;
	while ((field = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(field, "fieldname"));
		const char* type = yyjson_get_str(yyjson_obj_get(field, "fieldtype"));
		bool private = yyjson_get_bool(yyjson_obj_get(field, "private"));
		fprintf(out, INDENT);
		writeDecl(out, name, sanitizeType(type), private);
		fprintf(out, ";\n");
	}
}

static void writeParams(FILE* out, yyjson_val* params) {
	yyjson_arr_iter arg_iter;
	yyjson_arr_iter_init(params, &arg_iter);

	yyjson_val* arg = NULL;
	while ((arg = yyjson_arr_iter_next(&arg_iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* type0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		char type[1024];
		strcpy(type, sanitizeType(type0));
		if (out == cOutput)
			strcpy(type, prefixUserType(type));

		fprintf(out, "%s %s", type, name);
		if (yyjson_arr_iter_has_next(&arg_iter))
			fprintf(out, ", ");
	}
}

static const char* structName(yyjson_val* type) {
	const char* master = yyjson_get_str(yyjson_obj_get(type, "struct"));
	if (master == NULL)
		master = yyjson_get_str(yyjson_obj_get(type, "classname"));
	return master;
}

static int isConstructor(yyjson_val* met) {
	return strstr(yyjson_get_str(yyjson_obj_get(met, "methodname_flat")), "Construct") != NULL;
}

static const char* normalizeMethodName(yyjson_val* method) {
	const char* metName = yyjson_get_str(yyjson_obj_get(method, "methodname_flat"));

	static char buf[1024];
	strcpy(buf, METHOD_PREFIX);
	strcpy(buf + strlen(METHOD_PREFIX), metName + strlen("SteamAPI_"));
	return buf;
}

static void writeMethodSign(FILE* out, yyjson_val* tMaster, yyjson_val* method) {
	const char* metName = normalizeMethodName(method);
	char deezType[1024] = {0}, deezPtr[1024] = {0}, retType[1024] = {0};

	strcpy(deezType, structName(tMaster));
	if (isConstructor(method))
		strcpy(retType, deezType);
	else
		strcpy(retType, yyjson_get_str(yyjson_obj_get(method, "returntype")));

	if (out == cOutput) {
		strcpy(deezType, prefixUserType(deezType));
		strcpy(retType, prefixUserType(retType));
	}

	strcpy(deezPtr, deezType);
	size_t i = strlen(deezPtr);
	deezPtr[i++] = '*';
	deezPtr[i++] = '\0';

	fprintf(out, "%s %s(", retType, metName);
	writeDecl(out, THIS, deezPtr, false);

	yyjson_val* params = yyjson_obj_get(method, "params");
	if (yyjson_get_len(params))
		fprintf(out, ", ");
	writeParams(out, params);

	fprintf(out, ")");
}

static void genWrapper(yyjson_val* tMaster, yyjson_val* met) {
	const char* mastName = structName(tMaster);
	const char* metName = yyjson_get_str(yyjson_obj_get(met, "methodname"));
	const char* metType = yyjson_get_str(yyjson_obj_get(met, "returntype"));

	writeMethodSign(hOutput, tMaster, met);
	fprintf(hOutput, ";\n");

	writeMethodSign(cOutput, tMaster, met);
	fprintf(cOutput, " {\n");

	yyjson_val* params = yyjson_obj_get(met, "params");
	if (isConstructor(met))
		metType = mastName;

	yyjson_val* arg = NULL;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(params, &iter);

	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		char pType[1024] = {0};
		strcpy(pType, prefixUserType(sanitizeType(pType0)));

		fprintf(cOutput, INDENT "%s* __%s = &%s;\n", pType, pName, pName);
	}

	yyjson_arr_iter_init(params, &iter);
	size_t count = yyjson_get_len(params), idx = 0;
	int retVoid = !strcmp(metType, "void");

	if (retVoid)
		fprintf(cOutput, INDENT);
	else
		fprintf(cOutput, INDENT "%s " RESULT " = ", metType);

	if (isConstructor(met))
		fprintf(cOutput, "%s(", metType);
	else
		fprintf(cOutput, "reinterpret_cast<%s*>(" THIS ")->%s(", mastName, metName);

	if (count)
		fprintf(cOutput, "\n");

	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		char pType[1024] = {0};
		strcpy(pType, sanitizeType(pType0));

		fprintf(cOutput, INDENT INDENT);
		for (size_t i = 0; i < strlen(pType0); i++)
			if (pType0[i] == '&')
				fprintf(cOutput, "*");
		fprintf(cOutput, "*reinterpret_cast<%s*>(__%s)", pType, pName);

		if (idx++ < count - 1)
			fprintf(cOutput, ", \n");
		else
			fprintf(cOutput, "\n");
	}
	if (count)
		fprintf(cOutput, INDENT ");\n");
	else
		fprintf(cOutput, ");\n");

	if (!retVoid)
		fprintf(cOutput, INDENT "return *reinterpret_cast<%s*>(&" RESULT ");\n", prefixUserType(metType));

	fprintf(cOutput, "}\n\n");
}

static void genMethods(yyjson_val* master) {
	static const char* ignore[] = {
	    "SetDualSenseTriggerEffect", "ISteamNetworkingSockets",	"SteamDatagramHostedAddress",
	    "ISteamGameServer",		 "ISteamNetworkingFakeUDPPort", "ISteamHTML",
	};

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(yyjson_obj_get(master, "methods"), &iter);

	yyjson_val* met = NULL;
	while ((met = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(met, "methodname_flat"));
		for (size_t i = 0; i < LENGTH(ignore); i++)
			if (strstr(name, ignore[i]) != NULL)
				goto next;
		genWrapper(master, met);
	next:
		continue;
	}
}

static void genConsts() {
	yyjson_arr_iter iter;

	yyjson_val* cnst = NULL;
	yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, "consts"), &iter);

	while ((cnst = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(cnst, "constname"));
		const char* type = yyjson_get_str(yyjson_obj_get(cnst, "consttype"));
		const char* value = yyjson_get_str(yyjson_obj_get(cnst, "constval"));
		fprintf(hOutput, "#define %s ((%s)(%s))\n", name, type, value);
	}

	fprintf(hOutput, "\n");
}

static void genTypedefs() {
	genEnums(yyjson_obj_get(ROOT_OBJ, "enums"), NULL);

#define SPECIAL (2)
	yyjson_arr_iter iter;
	yyjson_val* sources[] = {
	    yyjson_obj_get(ROOT_OBJ, "structs"),
	    yyjson_obj_get(ROOT_OBJ, "callback_structs"),
	    [SPECIAL] = yyjson_obj_get(ROOT_OBJ, "interfaces"),
	};

	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_val* struc = NULL;
		yyjson_arr_iter_init(sources[i], &iter);

		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			const char* parent = yyjson_get_str(yyjson_obj_get(struc, "struct"));
			if (i == SPECIAL) {
				parent = yyjson_get_str(yyjson_obj_get(struc, "classname"));
				fprintf(hOutput, "typedef void* %s;\n", parent);
			} else {
				fprintf(hOutput, "struct %s;\n", parent);
				fprintf(hOutput, "#ifndef __cplusplus\n");
				fprintf(hOutput, "typedef struct %s %s;\n", parent, parent);
				fprintf(hOutput, "#endif\n");
			}
			genEnums(yyjson_obj_get(struc, "enums"), parent);
		}

		fprintf(hOutput, "\n");
	}
#undef SPECIAL

	yyjson_val* typeDef = NULL;
	yyjson_val* typeDefs = yyjson_obj_get(ROOT_OBJ, "typedefs");
	yyjson_arr_iter_init(typeDefs, &iter);

	while ((typeDef = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(typeDef, "typedef"));
		const char* type = yyjson_get_str(yyjson_obj_get(typeDef, "type"));
		fprintf(hOutput, "typedef ");
		writeDecl(hOutput, name, type, false);
		fprintf(hOutput, ";\n");
	}
	fprintf(hOutput, "\n");
}

static void genStruct(yyjson_val* struc) {
	const char* name = yyjson_get_str(yyjson_obj_get(struc, "struct"));
	if (name == NULL)
		name = yyjson_get_str(yyjson_obj_get(struc, "classname"));

	yyjson_val* fields = yyjson_obj_get(struc, "fields");

	if (yyjson_get_len(fields)) {
		fprintf(hOutput, "struct %s {\n", name);
		writeFields(hOutput, fields);
		fprintf(hOutput, "};\n");
	}

	yyjson_val* methods = yyjson_obj_get(struc, "methods");
	if (yyjson_get_len(methods)) {
		fprintf(hOutput, "\n");
		genMethods(struc);
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
			genStruct(struc);
			fprintf(hOutput, "\n");
		}
	}
}

static void genInterfaces() {
	yyjson_val* intr = NULL;
	yyjson_val* interfaces = yyjson_obj_get(ROOT_OBJ, "interfaces");

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(interfaces, &iter);

	while ((intr = yyjson_arr_iter_next(&iter)) != NULL) {
		genStruct(intr);
		fprintf(hOutput, "\n");
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
	fprintf(hOutput, "#include <stddef.h>\n");
	fprintf(hOutput, "#include <stdint.h>\n\n");

	fprintf(hOutput, "typedef uint32_t enum32_t;\n");
	fprintf(hOutput, "typedef enum32_t SteamInputActionEvent_t__AnalogAction_t;\n"); // :(
	fprintf(hOutput, "typedef uint64_t CSteamID, CGameID;\n");
	fprintf(hOutput, "typedef void (*SteamAPIWarningMessageHook_t)(int, const char*);\n\n");
	fprintf(hOutput, "#ifndef __cplusplus\ntypedef uint8_t bool;\n#endif\n\n");

	fprintf(cOutput, "#include \"steam_api.h\"\n\n");

	fprintf(cOutput, "namespace caulk { extern \"C\" { \n");
	fprintf(cOutput, INDENT "#include \"__gen.h\"\n");
	fprintf(cOutput, "} }\n\n");

	fprintf(cOutput, "extern \"C\" {\n\n");

	genConsts();
	genTypedefs();
	genStructs();
	genInterfaces();

	fprintf(cOutput, "\n}\n\n");

	yyjson_doc_free(gDoc);
	fclose(hOutput);
	fclose(cOutput);

	return EXIT_SUCCESS;
}
