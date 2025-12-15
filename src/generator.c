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

static FILE *glueOutput = NULL, *cppOutput = NULL;
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
	static const char* ignore[] = {"unsigned ", "int ", "intptr", "int16", "int32", "int64", "char", "void", "bool",
		"float", "double", "size_t"};

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
	fprintf(glueOutput, "#ifndef __cplusplus\n");
	fprintf(glueOutput, "typedef enum32_t %s;\n", name);
	fprintf(glueOutput, "#endif\n");
}

static void defineEnum(yyjson_val* enm, const char* master) {
	const char* name = fieldName(yyjson_get_str(yyjson_obj_get(enm, "enumname")), master);
	fprintf(glueOutput, "enum %s {\n", name);

	yyjson_val* val = NULL;
	yyjson_val* values = yyjson_obj_get(enm, "values");

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(values, &iter);
	while ((val = yyjson_arr_iter_next(&iter)) != NULL) {
		name = yyjson_get_str(yyjson_obj_get(val, "name"));
		const char* value = yyjson_get_str(yyjson_obj_get(val, "value"));
		fprintf(glueOutput, INDENT "%s = %s,\n", name, value);
	}

	fprintf(glueOutput, "};\n\n");
}

static void genEnums(yyjson_val* enums, const char* master) {
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(enums, &iter);

	if (yyjson_get_len(enums))
		fprintf(glueOutput, "#ifndef CAULK_INTERNAL\n");

	yyjson_val* enm = NULL;
	while ((enm = yyjson_arr_iter_next(&iter)) != NULL) {
		declareEnum(enm, master);
		defineEnum(enm, master);
	}

	if (yyjson_get_len(enums))
		fprintf(glueOutput, "#endif\n");
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

	fprintf(glueOutput, "#ifndef CAULK_INTERNAL\n");
	fprintf(glueOutput, "struct %s {\n", structName(struc));

	yyjson_val* field = NULL;
	while ((field = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(field, "fieldname"));
		const char* type = yyjson_get_str(yyjson_obj_get(field, "fieldtype"));
		bool private = yyjson_get_bool(yyjson_obj_get(field, "private"));

		fprintf(glueOutput, INDENT);
		writeDecl(glueOutput, name, sanitizeType(type), private);
		fprintf(glueOutput, ";\n");
	}

	fprintf(glueOutput, "};\n");
	fprintf(glueOutput, "#endif\n");
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
		if (out == cppOutput)
			strcpy(type, prefixUserType(type));

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
	const char* metName = yyjson_get_str(yyjson_obj_get(method, "methodname_flat"));
	const char* metStem = metName + strlen("SteamAPI_");
	if (!strncmp(metStem, "ISteam", strlen("ISteam")))
		metStem++;

	static char buf[1024] = {0};
	strcpy(buf, METHOD_PREFIX);
	strcat(buf, metStem);

	return buf;
}

static void writeMethodSignature(FILE* out, yyjson_val* tMaster, yyjson_val* method, int kind) {
	const char* metName = normalizeMethodName(method);
	static char deezType[1024] = {0}, deezPtr[1024] = {0}, retType[1024] = {0};

	strcpy(deezType, structName(tMaster));
	if (isConstructor(method))
		strcpy(retType, deezType);
	else
		strcpy(retType, yyjson_get_str(yyjson_obj_get(method, "returntype")));

	if (out == cppOutput) {
		strcpy(deezType, prefixUserType(deezType));
		strcpy(retType, prefixUserType(retType));
	}

	strcpy(deezPtr, deezType);
	size_t i = strlen(deezPtr);
	deezPtr[i++] = '*';
	deezPtr[i++] = '\0';

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
	const char* masterName = structName(master);
	const char* methodNameFlat = yyjson_get_str(yyjson_obj_get(method, "methodname_flat"));
	const char* returnType = yyjson_get_str(yyjson_obj_get(method, "returntype"));

	writeMethodSignature(glueOutput, master, method, kind);
	fprintf(glueOutput, ";\n");

	writeMethodSignature(cppOutput, master, method, kind);
	fprintf(cppOutput, " {\n");

	yyjson_val* params = yyjson_obj_get(method, "params");
	if (isConstructor(method))
		returnType = masterName;

	yyjson_val* arg = NULL;
	yyjson_arr_iter iter;
	yyjson_arr_iter_init(params, &iter);

	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		strcpy(pType, prefixUserType(sanitizeType(pType0)));

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
		strcpy(ctor, masterName + 1);
		fprintf(cppOutput, "%s(\n" INDENT INDENT "%s()", methodNameFlat, ctor);
	} else {
		fprintf(cppOutput, "%s(\n" INDENT INDENT "reinterpret_cast<%s*>(" THIS ")", methodNameFlat, masterName);
	}

	if (count)
		fprintf(cppOutput, ",\n");
	while ((arg = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* pName = yyjson_get_str(yyjson_obj_get(arg, "paramname"));
		const char* pType0 = yyjson_get_str(yyjson_obj_get(arg, "paramtype"));

		static char pType[1024] = {0};
		strcpy(pType, sanitizeType(pType0));

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
	if (out == cppOutput)
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

	writeAccessorSignature(glueOutput, tMaster, acc);
	fprintf(glueOutput, ";\n");

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
	for (size_t i = 0; masterName != NULL && i < LENGTH(nonApiInterfaces); i++)
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
		fprintf(glueOutput, "\n");

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
		fprintf(glueOutput, "#define %s ((%s)(%s))\n", name, type, value);
	}

	fprintf(glueOutput, "\n");
}

static void genTypedefs() {
	genEnums(yyjson_obj_get(ROOT_OBJ, "enums"), NULL);

#define SPECIAL (2)
	static const char* sources[] = {"structs", "callback_structs", [SPECIAL] = "interfaces"};
	yyjson_arr_iter iter;

	fprintf(glueOutput, "#ifndef CAULK_INTERNAL\n\n");

	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, sources[i]), &iter);

		yyjson_val* struc = NULL;
		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			const char* parent = yyjson_get_str(yyjson_obj_get(struc, "struct"));
			if (i == SPECIAL) {
				parent = yyjson_get_str(yyjson_obj_get(struc, "classname"));
				fprintf(glueOutput, "typedef void* %s;\n", parent);
			} else {
				fprintf(glueOutput, "struct %s;\n", parent);
				fprintf(glueOutput, "#ifndef __cplusplus\n");
				fprintf(glueOutput, "typedef struct %s %s;\n", parent, parent);
				fprintf(glueOutput, "#endif\n");
			}
			genEnums(yyjson_obj_get(struc, "enums"), parent);
		}

		fprintf(glueOutput, "\n");
	}
#undef SPECIAL

	yyjson_val* typeDefs = yyjson_obj_get(ROOT_OBJ, "typedefs");
	yyjson_arr_iter_init(typeDefs, &iter);

	yyjson_val* typeDef = NULL;
	while ((typeDef = yyjson_arr_iter_next(&iter)) != NULL) {
		const char* name = yyjson_get_str(yyjson_obj_get(typeDef, "typedef"));
		const char* type = yyjson_get_str(yyjson_obj_get(typeDef, "type"));
		fprintf(glueOutput, "typedef ");
		writeDecl(glueOutput, name, type, false);
		fprintf(glueOutput, ";\n");
	}

	fprintf(glueOutput, "\n#endif\n\n");
}

static void genCallbackId(yyjson_val* struc) {
	int id = yyjson_get_int(yyjson_obj_get(struc, "callback_id"));
	if (id)
		fprintf(glueOutput, "#define %s_iCallback %d\n", structName(struc), id);
}

static void genStructs() {
#define SPECIAL (2)
	static const char* sources[] = {"structs", "callback_structs", [SPECIAL] = "interfaces"};
	for (size_t i = 0; i < LENGTH(sources); i++) {
		yyjson_arr_iter iter;
		yyjson_arr_iter_init(yyjson_obj_get(ROOT_OBJ, sources[i]), &iter);

		yyjson_val* struc = NULL;
		while ((struc = yyjson_arr_iter_next(&iter)) != NULL) {
			genFields(struc);
			genMethods(struc, i == SPECIAL);
			genCallbackId(struc);
		}
	}
#undef SPECIAL
}

int main(int argc, char* argv[]) {
	if (argc != 5)
		return EXIT_FAILURE;

	glueOutput = fopen(argv[1], "wt");
	cppOutput = fopen(argv[3], "wt");
	if (glueOutput == NULL || cppOutput == NULL)
		return EXIT_FAILURE;

	yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
	yyjson_read_err err;
	gDoc = yyjson_read_file(argv[4], flg, NULL, &err);
	if (gDoc == NULL)
		return EXIT_FAILURE;

	fprintf(glueOutput, "#ifndef CAULK_INTERNAL\n");
	fprintf(glueOutput, "typedef uint32_t enum32_t;\n");
	fprintf(glueOutput, "typedef enum32_t SteamInputActionEvent_t__AnalogAction_t;\n");
	fprintf(glueOutput, "typedef uint64_t CSteamID, CGameID;\n");
	fprintf(glueOutput, "typedef void (*SteamAPIWarningMessageHook_t)(int, const char*);\n");
	fprintf(glueOutput, "#endif\n\n");

	fprintf(cppOutput, "#include \"steam_api_flat.h\"\n\n");

	fprintf(cppOutput, "namespace caulk { extern \"C\" { \n");
	fprintf(cppOutput, INDENT "#include \"__gen.h\"\n");
	fprintf(cppOutput, "} }\n\n");

	fprintf(cppOutput, "extern \"C\" {\n\n");

	genConsts();
	genTypedefs();
	genStructs();

	fprintf(cppOutput, "}\n");
	fclose(glueOutput);

	FILE* hOutput = fopen(argv[2], "wt");
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

	char buf[4096] = {0};
	size_t count = 0;
	FILE* glueInput = fopen(argv[1], "rt");

	if (glueInput == NULL)
		return EXIT_FAILURE;
	while (!feof(glueInput)) {
		count = fread(buf, 1, sizeof(buf), glueInput);
		fwrite(buf, 1, count, hOutput);
	}
	fclose(glueInput);

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
