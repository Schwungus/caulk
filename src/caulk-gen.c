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

static void fprintN(FILE* file, const char* str, size_t len) {
	for (size_t i = 0; i < len; i++)
		fputc(str[i], file);
}

static FILE *hOutput = NULL, *cOutput = NULL;
static yyjson_doc* gDoc;
#define ROOT_OBJ (yyjson_doc_get_root(gDoc))

static const char* fieldName(const char* field, const char* master) {
	if (master == NULL)
		return field;

	static char buf[1024] = {0};
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

static const char* sanitizeType(const char* type) {
	static char buf[1024] = {0};
	size_t i = 0;
	for (; i < strlen(type); i++)
		if (type[i] == ':')
			buf[i] = '_';
		else if (type[i] == '&')
			buf[i] = '*';
		else
			buf[i] = type[i];
	buf[i] = '\0';
	return buf;
}

static const char* prefixUserType(const char* type) {
	static char buf[1024] = {0};
	static const char* ignore[] = {
	    "unsigned ", "int ", "intptr", "int16", "int32",  "int64",
	    "char",	 "void", "bool",   "float", "double", "size_t",
	};

	if (!strcmp(type, "int"))
		goto noop;
	for (size_t i = 0; i < LENGTH(ignore); i++)
		if (strstr(type, ignore[i]))
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

static const char* structName(yyjson_val* type) {
	const char* master = yyjson_get_str(yyjson_obj_get(type, "struct"));
	if (master == NULL)
		master = yyjson_get_str(yyjson_obj_get(type, "classname"));
	return master;
}

static int isConstructor(yyjson_val* method) {
	return strstr(yyjson_get_str(yyjson_obj_get(method, "methodname_flat")), "Construct") != NULL;
}

static void declareEnum(yyjson_val* enm, const char* master) {
	const char* name = fieldName(yyjson_get_str(yyjson_obj_get(enm, "enumname")), master);
	fprintf(hOutput, "#ifndef __cplusplus\n");
	fprintf(hOutput, "typedef enum32_t %s;\n", name);
	fprintf(hOutput, "#endif\n");
}

static void defineEnum(yyjson_val* enm, const char* master) {
	const char* name = fieldName(yyjson_get_str(yyjson_obj_get(enm, "enumname")), master);
	fprintf(hOutput, "enum %s {\n", name);

	yyjson_val* val = NULL;
	yyjson_val* values = yyjson_obj_get(enm, "values");

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(values, &iter);
	while ((val = yyjson_arr_iter_next(&iter)) != NULL) {
		name = yyjson_get_str(yyjson_obj_get(val, "name"));
		const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
		fprintf(hOutput, INDENT "%s = %s,\n", name, value);
	}

	fprintf(hOutput, "};\n\n");
}

static void genEnums(yyjson_val* enums, const char* master) {
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(enums, &iter);

	if (yyjson_get_len(enums))
		fprintf(hOutput, "#ifndef CAULK_INTERNAL\n");

	yyjson_val* enm = NULL;
	while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
		declareEnum(enm, master);
		defineEnum(enm, master);
	}

	if (yyjson_get_len(enums))
		fprintf(hOutput, "#endif\n");
}

static void writeDecl(FILE* out, const char* name, const char* type, bool private) {
	char* offset = NULL;
	if ((offset = strstr(type, "(*)")) != NULL) {
		fprintN(out, type, offset + 2 - type);
		fprintf(out, "%s%s%s", private ? "__" : "", name, offset + 2);
	} else if ((offset = strstr(type, "[")) != NULL) {
		fprintN(out, type, offset - 1 - type);
		fprintf(out, " %s%s%s", private ? "__" : "", name, offset);
	} else {
		fprintf(out, "%s", type);
		fprintf(out, " %s%s", private ? "__" : "", name);
	}
}

static void genFields(yyjson_val* struc) {
	yyjson_val* fields = yyjson_obj_get(struc, "fields");
	if (!yyjson_get_len(fields))
		return;

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(fields, &iter);

	fprintf(hOutput, "#ifndef CAULK_INTERNAL\n");
	fprintf(hOutput, "struct %s {\n", structName(struc));

	yyjson_val* field = NULL;
	while ((field = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(field, "fieldname"));
		const char* type = yyjson_get_str(yyjson_obj_get(field, "fieldtype"));
		bool private = yyjson_get_bool(yyjson_obj_get(field, "private"));

		fprintf(hOutput, INDENT);
		writeDecl(hOutput, name, sanitizeType(type), private);
		fprintf(hOutput, ";\n");
	}

	fprintf(hOutput, "};\n");
	fprintf(hOutput, "#endif\n");
}

static void writeParams(FILE* out, yyjson_val* params) {
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(params, &iter);

	yyjson_val* arg = NULL;
	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* type0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char type[1024] = {0};
		strcpy(type, sanitizeType(type0));
		if (out == cOutput)
			strcpy(type, prefixUserType(type));

		fprintf(out, "%s %s", type, name);
		if (yyjson_arr_iter_has_next(&iter))
			fprintf(out, ", ");
	}
}

static const char* ignoreForMethods[] = {
    "SetDualSenseTriggerEffect", "ISteamNetworkingSockets",	"SteamDatagramHostedAddress",
    "ISteamGameServer",		 "ISteamNetworkingFakeUDPPort", "ISteamHTML",
    "SteamGameServer_v",	 "SteamGameServerStats_v",
};

static const char* normalizeMethodName(yyjson_val* method) {
	const char* metName = yyjson_get_str(yyjson_obj_get(method, "methodname_flat"));

	static char buf[1024] = {0};
	strcpy(buf, METHOD_PREFIX);
	strcpy(buf + strlen(METHOD_PREFIX), metName + strlen("SteamAPI_"));

	return buf;
}

static void writeMethodSignature(FILE* out, yyjson_val* tMaster, yyjson_val* method) {
	const char* metName = normalizeMethodName(method);
	static char deezType[1024] = {0}, deezPtr[1024] = {0}, retType[1024] = {0};

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

static void wrapMethod(yyjson_val* master, yyjson_val* method) {
	const char* mastName = structName(master);
	const char* metName = yyjson_get_str(yyjson_obj_get(method, "methodname"));
	const char* metType = yyjson_get_str(yyjson_obj_get(method, "returntype"));

	writeMethodSignature(hOutput, master, method);
	fprintf(hOutput, ";\n");

	writeMethodSignature(cOutput, master, method);
	fprintf(cOutput, " {\n");

	yyjson_val* params = yyjson_obj_get(method, "params");
	if (isConstructor(method))
		metType = mastName;

	yyjson_val* arg = NULL;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(params, &iter);

	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		strcpy(pType, prefixUserType(sanitizeType(pType0)));

		fprintf(cOutput, INDENT "%s* __%s = &%s;\n", pType, pName, pName);
	}

	yyjson_arr_iter_init(params, &iter);
	size_t count = yyjson_get_len(params), idx = 0;
	int retVoid = !strcmp(metType, "void");

	fprintf(cOutput, INDENT);
	if (!retVoid)
		fprintf(cOutput, "%s " RESULT " = ", metType);

	if (isConstructor(method))
		fprintf(cOutput, "%s(", metType);
	else
		fprintf(cOutput, "reinterpret_cast<%s*>(" THIS ")->%s(", mastName, metName);

	if (count)
		fprintf(cOutput, "\n");
	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		strcpy(pType, sanitizeType(pType0));

		fprintf(cOutput, INDENT INDENT);
		for (size_t i = 0; i < strlen(pType0); i++)
			if (pType0[i] == '&')
				fprintf(cOutput, "*");
		fprintf(cOutput, "*reinterpret_cast<%s*>(__%s)", pType, pName);

		if (++idx < count)
			fprintf(cOutput, ", ");
		fprintf(cOutput, "\n");
	}
	if (count)
		fprintf(cOutput, INDENT);
	fprintf(cOutput, ");\n");

	if (!retVoid)
		fprintf(cOutput, INDENT "return *reinterpret_cast<%s*>(&" RESULT ");\n", prefixUserType(metType));

	fprintf(cOutput, "}\n\n");
}

static const char* normalizeAccessorName(yyjson_val* accessor) {
	const char* accName = yyjson_get_str(yyjson_obj_get(accessor, "name_flat"));

	static char buf[1024] = {0};
	strcpy(buf, METHOD_PREFIX);
	strcpy(buf + strlen(METHOD_PREFIX), accName + strlen("SteamAPI_"));

	char* suffix = strstr(buf, "_v0");
	if (suffix != NULL)
		*suffix = '\0';

	return buf;
}

static void writeAccessorSignature(FILE* out, yyjson_val* tMaster, yyjson_val* accessor) {
	const char* accName = normalizeAccessorName(accessor);
	static char deezType[1024] = {0}, deezPtr[1024] = {0};

	strcpy(deezType, structName(tMaster));
	if (out == cOutput)
		strcpy(deezType, prefixUserType(deezType));

	strcpy(deezPtr, deezType);
	size_t i = strlen(deezPtr);
	deezPtr[i++] = '*';
	deezPtr[i++] = '\0';

	fprintf(out, "%s* %s()", deezType, accName);
}

static void wrapAccessor(yyjson_val* tMaster, yyjson_val* acc) {
	const char* mastName = structName(tMaster);
	const char* accName = yyjson_get_str(yyjson_obj_get(acc, "name"));

	writeAccessorSignature(hOutput, tMaster, acc);
	fprintf(hOutput, ";\n");

	writeAccessorSignature(cOutput, tMaster, acc);
	fprintf(cOutput, " {\n");
	fprintf(cOutput, INDENT "%s* " RESULT " = %s();\n", mastName, accName);
	fprintf(cOutput, INDENT "return *reinterpret_cast<%s**>(&" RESULT ");\n", prefixUserType(mastName));
	fprintf(cOutput, "}\n\n");
}

struct wrapper {
	const char* arrayName;
	void (*wrap)(yyjson_val*, yyjson_val*);
	const char* flatnameField;
};

static void genMethods(yyjson_val* master) {
	static const struct wrapper wrappers[] = {
	    {"methods", wrapMethod, "methodname_flat"},
	    {"accessors", wrapAccessor, "name_flat"},
	};

	for (size_t i = 0; i < LENGTH(wrappers); i++) {
		const struct wrapper* wrapper = &wrappers[i];

		yyjson_val* methods = yyjson_obj_get(master, wrapper->arrayName);
		if (!yyjson_get_len(methods))
			continue;
		fprintf(hOutput, "\n");

		yyjson_arr_iter iter;
		yyjson_arr_iter_init(yyjson_obj_get(master, wrapper->arrayName), &iter);

		yyjson_val* method = NULL;
		while ((method = yyjson_arr_iter_next(&iter)) != NULL) {
			const char* name = yyjson_get_str(yyjson_obj_get(method, wrapper->flatnameField));
			for (size_t i = 0; i < LENGTH(ignoreForMethods); i++)
				if (strstr(name, ignoreForMethods[i]) != NULL)
					goto next;
			wrapper->wrap(master, method);
		next:
			continue;
		}
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
	static const char* sources[] = {"structs", "callback_structs", [SPECIAL] = "interfaces"};
	yyjson_arr_iter iter;

	fprintf(hOutput, "#ifndef CAULK_INTERNAL\n\n");

	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, sources[i]), &iter);

		yyjson_val* struc = NULL;
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

	yyjson_val* typeDefs = yyjson_obj_get(ROOT_OBJ, "typedefs");
	yyjson_arr_iter_init(typeDefs, &iter);

	yyjson_val* typeDef = NULL;
	while ((typeDef = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(typeDef, "typedef"));
		const char* type = yyjson_get_str(yyjson_obj_get(typeDef, "type"));
		fprintf(hOutput, "typedef ");
		writeDecl(hOutput, name, type, false);
		fprintf(hOutput, ";\n");
	}

	fprintf(hOutput, "\n#endif\n\n");
}

static void genCallbackId(yyjson_val* struc) {
	int id = yyjson_get_int(yyjson_obj_get(struc, "callback_id"));
	if (id)
		fprintf(hOutput, "#define %s_iCallback %d\n", structName(struc), id);
}

static void genStructs() {
	static const char* sources[] = {"structs", "callback_structs", "interfaces"};
	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_arr_iter iter;
		yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, sources[i]), &iter);

		yyjson_val* struc = NULL;
		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			genFields(struc);
			genMethods(struc);
			genCallbackId(struc);
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

	yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
	yyjson_read_err err;
	gDoc = yyjson_read_file(argv[3], flg, NULL, &err);
	if (gDoc == NULL)
		return EXIT_FAILURE;

	fprintf(hOutput, "#pragma once\n\n");
	fprintf(hOutput, "#include <stddef.h>\n");
	fprintf(hOutput, "#include <stdint.h>\n");
	fprintf(hOutput, "#include <stdbool.h>\n\n");

	fprintf(hOutput, "#ifndef CAULK_INTERNAL\n");
	fprintf(hOutput, "typedef uint32_t enum32_t;\n");
	fprintf(hOutput, "typedef enum32_t SteamInputActionEvent_t__AnalogAction_t;\n"); // :(
	fprintf(hOutput, "typedef uint64_t CSteamID, CGameID;\n");
	fprintf(hOutput, "typedef void (*SteamAPIWarningMessageHook_t)(int, const char*);\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(cOutput, "#include \"steam_api.h\"\n\n");

	fprintf(cOutput, "namespace caulk { extern \"C\" { \n");
	fprintf(cOutput, INDENT "#include \"__gen.h\"\n");
	fprintf(cOutput, "} }\n\n");

	fprintf(cOutput, "extern \"C\" {\n\n");

	genConsts();
	genTypedefs();
	genStructs();

	fprintf(cOutput, "}\n");

	yyjson_doc_free(gDoc);
	fclose(hOutput);
	fclose(cOutput);

	return EXIT_SUCCESS;
}
