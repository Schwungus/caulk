// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <https://unlicense.org>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

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

static FILE *hOutput = NULL, *cppOutput = NULL;
static yyjson_doc* gDoc;
#define ROOT_OBJ (yyjson_doc_get_root(gDoc))

static const char* fieldName(const char* field, const char* master) {
	if (!master)
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
	static const char* ignore[] = {"unsigned ", "int ", "intptr", "int16", "int32", "int64", "char", "void", "bool",
		"float", "double", "size_t"};

	if (!strcmp(type, "int"))
		goto noop;
	for (size_t i = 0; i < LENGTH(ignore); i++)
		if (strstr(type, ignore[i]))
			goto noop;

	if (strstr(type, "const "))
		snprintf(buf, sizeof(buf), "const " NS_PREFIX "%s", type + strlen("const "));
	else
		snprintf(buf, sizeof(buf), NS_PREFIX "%s", type);
	return buf;

noop:
	snprintf(buf, sizeof(buf), "%s", type);
	return buf;
}

static const char* structName(yyjson_val* type) {
	const char* master = yyjson_get_str(yyjson_obj_get(type, "struct"));
	if (!master)
		master = yyjson_get_str(yyjson_obj_get(type, "classname"));
	return master;
}

static bool isConstructor(yyjson_val* method) {
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
	while ((val = yyjson_arr_iter_next(&iter))) {
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
	while ((enm = yyjson_arr_iter_next(&iter))) {
		declareEnum(enm, master);
		defineEnum(enm, master);
	}

	if (yyjson_get_len(enums))
		fprintf(hOutput, "#endif\n");
}

static void writeDecl(FILE* out, const char* name, const char* type, bool private) {
	char* offset = NULL;
	if ((offset = strstr(type, "(*)"))) {
		fprintN(out, type, offset + 2 - type);
		fprintf(out, "%s%s%s", private ? "__" : "", name, offset + 2);
	} else if ((offset = strstr(type, "["))) {
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
	while ((field = yyjson_arr_iter_next(&iter))) {
		const char *name = yyjson_get_str(yyjson_obj_get(field, "fieldname")),
			   *type = yyjson_get_str(yyjson_obj_get(field, "fieldtype"));
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
	while ((arg = yyjson_arr_iter_next(&iter))) {
		const char *name = yyjson_get_str(yyjson_obj_get(arg, "paramname")),
			   *type0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char type[1024] = {0};
		snprintf(type, sizeof(type), "%s", sanitizeType(type0));
		if (out == cppOutput)
			snprintf(type, sizeof(type), "%s", prefixUserType(type));

		fprintf(out, "%s %s", type, name);
		if (yyjson_arr_iter_has_next(&iter))
			fprintf(out, ", ");
	}
}

static const char* ignoreForMethods[]
	= {"SetDualSenseTriggerEffect", "ISteamNetworkingSockets", "SteamDatagramHostedAddress", "ISteamGameServer",
		"ISteamNetworkingFakeUDPPort", "ISteamHTML", "SteamGameServer_v", "SteamGameServerStats_v"};

enum {
	methStruct,
	methInterface,
};

static const char* normalizeMethodName(yyjson_val* method) {
	const char *metName = yyjson_get_str(yyjson_obj_get(method, "methodname_flat")),
		   *metStem = metName + strlen("SteamAPI_");

	if (!strncmp(metStem, "ISteam", strlen("ISteam")))
		metStem++;

	static char buf[1024] = {0};
	snprintf(buf, sizeof(buf), METHOD_PREFIX "%s", metStem);
	return buf;
}

static void writeMethodSignature(FILE* out, yyjson_val* tMaster, yyjson_val* method, int kind) {
	const char* metName = normalizeMethodName(method);
	static char deezType[1024] = {0}, deezPtr[1024] = {0}, retType[1024] = {0};

	snprintf(deezType, sizeof(deezType), "%s", structName(tMaster));
	if (isConstructor(method))
		snprintf(retType, sizeof(retType), "%s", structName(tMaster));
	else
		snprintf(retType, sizeof(retType), "%s", yyjson_get_str(yyjson_obj_get(method, "returntype")));

	if (out == cppOutput) {
		snprintf(deezType, sizeof(deezType), "%s", prefixUserType(deezType));
		snprintf(retType, sizeof(retType), "%s", prefixUserType(retType));
	}
	snprintf(deezPtr, sizeof(deezPtr), "%s*", deezType);

	fprintf(out, "%s %s(", retType, metName);
	if (kind == methStruct)
		writeDecl(out, THIS, deezPtr, false);

	yyjson_val* params = yyjson_obj_get(method, "params");
	if (kind == methStruct && yyjson_get_len(params))
		fprintf(out, ", ");
	writeParams(out, params);

	fprintf(out, ")");
}

static void wrapMethod(yyjson_val* master, yyjson_val* method, int kind) {
	const char *masterName = structName(master),
		   *methodNameFlat = yyjson_get_str(yyjson_obj_get(method, "methodname_flat")),
		   *returnType = yyjson_get_str(yyjson_obj_get(method, "returntype"));

	writeMethodSignature(hOutput, master, method, kind);
	fprintf(hOutput, ";\n");

	writeMethodSignature(cppOutput, master, method, kind);
	fprintf(cppOutput, " {\n");

	yyjson_val* params = yyjson_obj_get(method, "params");
	if (isConstructor(method))
		returnType = masterName;

	yyjson_val* arg = NULL;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(params, &iter);

	while ((arg = yyjson_arr_iter_next(&iter))) {
		const char *pName = yyjson_get_str(yyjson_obj_get(arg, "paramname")),
			   *pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		snprintf(pType, sizeof(pType), "%s", prefixUserType(sanitizeType(pType0)));

		fprintf(cppOutput, INDENT "%s* __%s = &%s;\n", pType, pName, pName);
	}

	yyjson_arr_iter_init(params, &iter);
	size_t count = yyjson_get_len(params), idx = 0;
	int retVoid = !strcmp(returnType, "void");

	fprintf(cppOutput, INDENT);
	if (!retVoid)
		fprintf(cppOutput, "%s " RESULT " = ", returnType);

	if (isConstructor(method)) {
		fprintf(cppOutput, "%s(", returnType);
	} else if (kind == methInterface) {
		static char ctor[512] = {0};
		snprintf(ctor, sizeof(ctor), "%s", masterName + 1);
		fprintf(cppOutput, "%s(\n" INDENT INDENT "%s()", methodNameFlat, ctor);
	} else {
		fprintf(cppOutput, "%s(\n" INDENT INDENT "reinterpret_cast<%s*>(" THIS ")", methodNameFlat, masterName);
	}

	if (count)
		fprintf(cppOutput, ",\n");
	while ((arg = yyjson_arr_iter_next(&iter))) {
		const char *pName = yyjson_get_str(yyjson_obj_get(arg, "paramname")),
			   *pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		snprintf(pType, sizeof(pType), "%s", sanitizeType(pType0));

		fprintf(cppOutput, INDENT INDENT);
		for (size_t i = 0; i < strlen(pType0); i++)
			if (pType0[i] == '&')
				fprintf(cppOutput, "*");
			// HACK: `CSteamID` & `CGameID` are used as integers instead of the usual classes in
			// `steam_api_flat.h`. So here we use them as-is instead of type-casting.
			else if (!strcmp(pType, "CSteamID") || !strcmp(pType, "CGameID")) {
				fprintf(cppOutput, "%s", pName);
				goto skip_reinterpret;
			}
		fprintf(cppOutput, "*reinterpret_cast<%s*>(__%s)", pType, pName);
	skip_reinterpret:
		if (++idx < count)
			fprintf(cppOutput, ",\n");
	}
	fprintf(cppOutput, "\n" INDENT ");\n");

	if (!retVoid)
		fprintf(cppOutput, INDENT "return *reinterpret_cast<%s*>(&" RESULT ");\n", prefixUserType(returnType));

	fprintf(cppOutput, "}\n\n");
}

static void wrapStructMethod(yyjson_val* master, yyjson_val* method) {
	wrapMethod(master, method, methStruct);
}

static void wrapInterfaceMethod(yyjson_val* master, yyjson_val* method) {
	wrapMethod(master, method, methInterface);
}

static const char* normalizeAccessorName(yyjson_val* accessor) {
	const char* accName = yyjson_get_str(yyjson_obj_get(accessor, "name_flat"));

	static char buf[1024] = {0};
	snprintf(buf, sizeof(buf), METHOD_PREFIX "%s", accName + strlen("SteamAPI_"));

	char* suffix = strstr(buf, "_v0");
	if (suffix)
		*suffix = '\0';

	return buf;
}

static void writeAccessorSignature(FILE* out, yyjson_val* tMaster, yyjson_val* accessor) {
	static char deezType[1024] = {0};
	if (out == cppOutput)
		snprintf(deezType, sizeof(deezType), "%s", prefixUserType(structName(tMaster)));
	else
		snprintf(deezType, sizeof(deezType), "%s", structName(tMaster));
	fprintf(out, "%s* %s()", deezType, normalizeAccessorName(accessor));
}

static void wrapAccessor(yyjson_val* tMaster, yyjson_val* acc) {
	const char *mastName = structName(tMaster), *accName = yyjson_get_str(yyjson_obj_get(acc, "name"));
	writeAccessorSignature(hOutput, tMaster, acc);
	fprintf(hOutput, ";\n");

	writeAccessorSignature(cppOutput, tMaster, acc);
	fprintf(cppOutput, " {\n");
	fprintf(cppOutput, INDENT "%s* " RESULT " = %s();\n", mastName, accName);
	fprintf(cppOutput, INDENT "return *reinterpret_cast<%s**>(&" RESULT ");\n", prefixUserType(mastName));
	fprintf(cppOutput, "}\n\n");
}

typedef struct {
	const char *arrayName, *flatnameField;
	void (*wrap)(yyjson_val*, yyjson_val*);
} Wrapper;

static const char* nonApiInterfaces[] = {"SteamMatchmakingServerListResponse", "SteamMatchmakingPingResponse",
	"SteamMatchmakingPlayersResponse", "SteamMatchmakingRulesResponse"};

static void genMethods(yyjson_val* master, bool isInterface) {
	const char* masterName = yyjson_get_str(yyjson_obj_get(master, "classname"));
	for (size_t i = 0; masterName && i < LENGTH(nonApiInterfaces); i++)
		if (strstr(masterName, nonApiInterfaces[i])) {
			isInterface = false;
			break;
		}

	const Wrapper wrappers[] = {
		{"methods",   "methodname_flat", isInterface ? wrapInterfaceMethod : wrapStructMethod},
		{"accessors", "name_flat",       wrapAccessor                                        },
	};

	for (size_t i = 0; i < LENGTH(wrappers); i++) {
		const Wrapper* wrapper = &wrappers[i];

		yyjson_val* methods = yyjson_obj_get(master, wrapper->arrayName);
		if (!yyjson_get_len(methods))
			continue;
		fprintf(hOutput, "\n");

		yyjson_arr_iter iter;
		yyjson_arr_iter_init(yyjson_obj_get(master, wrapper->arrayName), &iter);

		yyjson_val* method = NULL;
		while ((method = yyjson_arr_iter_next(&iter))) {
			const char* name = yyjson_get_str(yyjson_obj_get(method, wrapper->flatnameField));
			for (size_t i = 0; i < LENGTH(ignoreForMethods); i++)
				if (strstr(name, ignoreForMethods[i]))
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

	while ((cnst = yyjson_arr_iter_next(&iter))) {
		const char *name = yyjson_get_str(yyjson_obj_get(cnst, "constname")),
			   *type = yyjson_get_str(yyjson_obj_get(cnst, "consttype")),
			   *value = yyjson_get_str(yyjson_obj_get(cnst, "constval"));
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
		while ((struc = yyjson_arr_iter_next(&iter))) {
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
	while ((typeDef = yyjson_arr_iter_next(&iter))) {
		const char *name = yyjson_get_str(yyjson_obj_get(typeDef, "typedef")),
			   *type = yyjson_get_str(yyjson_obj_get(typeDef, "type"));
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
#define SPECIAL (2)
	static const char* sources[] = {"structs", "callback_structs", [SPECIAL] = "interfaces"};
	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_arr_iter iter;
		yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, sources[i]), &iter);

		yyjson_val* struc = NULL;
		while ((struc = yyjson_arr_iter_next(&iter))) {
			genFields(struc);
			genMethods(struc, i == SPECIAL);
			genCallbackId(struc);
		}
	}
#undef SPECIAL
}

int main(int argc, char* argv[]) {
	if (argc != 4)
		return EXIT_FAILURE;

	hOutput = fopen(argv[1], "wt"), cppOutput = fopen(argv[2], "wt");
	if (!hOutput || !cppOutput)
		return EXIT_FAILURE;

	yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
	yyjson_read_err err;
	if (!(gDoc = yyjson_read_file(argv[3], flg, NULL, &err)))
		return EXIT_FAILURE;

	fprintf(hOutput, "#pragma once\n\n");

	fprintf(hOutput, "#include <stddef.h>\n");
	fprintf(hOutput, "#include <stdint.h>\n");
	fprintf(hOutput, "#include <stdbool.h>\n\n");

	fprintf(hOutput, "#if !defined(caulk_Malloc) || !defined(caulk_Free)\n");
	fprintf(hOutput, "#  ifdef __cplusplus\n");
	fprintf(hOutput, "#    include <cstdlib>\n");
	fprintf(hOutput, "#  else\n");
	fprintf(hOutput, "#    include <stdlib.h>\n");
	fprintf(hOutput, "#  endif\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(hOutput, "#ifndef caulk_Malloc\n");
	fprintf(hOutput, "#  define caulk_Malloc malloc\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(hOutput, "#ifndef caulk_Free\n");
	fprintf(hOutput, "#  define caulk_Free free\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(hOutput, "#ifndef CAULK_INTERNAL\n");
	fprintf(hOutput, "typedef uint32_t enum32_t;\n");
	fprintf(hOutput, "typedef enum32_t SteamInputActionEvent_t__AnalogAction_t;\n"); // :(
	fprintf(hOutput, "typedef uint64_t CSteamID, CGameID;\n");
	fprintf(hOutput, "typedef void (*SteamAPIWarningMessageHook_t)(int, const char*);\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(hOutput, "#ifdef __cplusplus\n");
	fprintf(hOutput, "extern \"C\" {\n");
	fprintf(hOutput, "#endif\n\n");

	fprintf(cppOutput, "#include \"steam_api_flat.h\"\n\n");

	fprintf(cppOutput, "namespace caulk { extern \"C\" { \n");
	fprintf(cppOutput, INDENT "#include \"__gen.h\"\n");
	fprintf(cppOutput, "} }\n\n");

	fprintf(cppOutput, "extern \"C\" {\n\n");

	genConsts();
	genTypedefs();
	genStructs();

	fprintf(cppOutput, "}\n");

	fprintf(hOutput, "typedef void (*caulk_ResultHandler)(void*, bool);\n");
	fprintf(hOutput, "typedef void (*caulk_CallbackHandler)(void*);\n\n");

	fprintf(hOutput, "bool caulk_Init();\n");
	fprintf(hOutput, "void caulk_Shutdown();\n");
	fprintf(hOutput, "void caulk_Resolve(SteamAPICall_t, caulk_ResultHandler);\n");
	fprintf(hOutput, "void caulk_Register(uint32_t, caulk_CallbackHandler);\n");
	fprintf(hOutput, "void caulk_Dispatch();\n\n");

	fprintf(hOutput, "#ifdef __cplusplus\n");
	fprintf(hOutput, "}\n");
	fprintf(hOutput, "#endif\n\n");

	yyjson_doc_free(gDoc);
	fclose(cppOutput);
	fclose(hOutput);

	return EXIT_SUCCESS;
}
