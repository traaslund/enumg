#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#include <cstdio>

//
// POSIX
//
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
//
#include <execinfo.h>
#include <signal.h>
#include <cstdlib>
#include <unistd.h>

#define ENUMG_VERSION "0.9.1"

extern "C"
{
	#include "ini.h"
}

void enumExtractName(const std::string &input, std::string &output);

int iniFieldHandler(void* data, const char* section, const char* name, const char* value);

/////////////////
struct options 
{
	bool all;
	bool help;
	bool version_out;
	bool verbose;
	std::string wdir;
	
	options()
	{
		all = false;
		help = false;
		version_out = false;
		verbose = false;
		wdir = ".";
	}
};

enum strposflags
{
	STRPOSFLAGS_FIRST, STRPOSFLAGS_LAST
};

class Entry
{
public:
	Entry(const std::string &fullText) :
		m_fullText(fullText)
	{
		enumExtractName(fullText, m_name);
	}

	const std::string &name() const { return m_name; }
	const std::string &fullText() const { return m_fullText; }

private:
	std::string m_name;
	std::string m_fullText;
};

class Section
{
public:
	Section(const std::string &name) :
		m_name(name)
	{
	}

	const std::string &name() const { return m_name; }
	const std::string &type() const { return m_type; }
	
	void type(const std::string &val) { m_type = val; }
	
	const std::vector<Entry> &entries() const { return m_entries; }
	std::vector<Entry> &entries() { return m_entries; }
	
private:
	std::string m_name;
	std::string m_type;
	std::vector<Entry> m_entries;
};

struct statefields
{
	std::string fileName;
	std::string curSection;
	std::string cSource;
	std::string cHeader;
	std::string enumType;
	std::string stringifyDefine;
	std::string headerGuard;
	std::string introComment;
	std::string srcDir;
	std::string includeDir;
	bool firstField;
	bool cppStringifyDisable;
	
	std::vector<Section> sections;
	
	Section &currentSection() { return sections[sections.size()-1]; }

	std::vector<std::string> topExprs;
	std::vector<std::string> bottomExprs;
};
/////////////////////

void printHelp(const char *cmdName)
{
	printf("%s version %s\n", cmdName, ENUMG_VERSION);
	printf("Usage: \n");
	printf("  %s [file_1 [file_2 ... [file_n]]] [options]\n", cmdName);
	printf("\n");
	printf("options:\n");
	printf("  -v        : print version and exit\n");
	printf("  -V        : verbose output\n");
	printf("  -h        : this screen\n");
	printf("            : this screen (no arguments)\n");
}



bool isParam(const char *str, const char *param)
{
	bool res = 0 == strncmp(str, param, strlen(param));
	return res;
}

int strxpos(const char *str, int ch, int flags)
{
	if (str == NULL)
	{
		fprintf(stderr, "NULL string to strxpos\n");
		exit(1);
	}
	
	int off = 0;
	int pos = -1;
	while (*str != '\0')
	{
		if (*str == ch) 
		{
			pos = off;
			
			if (flags & STRPOSFLAGS_FIRST) break;
		}
		
		++off;
		++str;
	} 
	
	return pos;
}

int strfpos(const char *str, int ch)
{
	return strxpos(str, ch, STRPOSFLAGS_FIRST);
}

int strlpos(const char *str, int ch)
{
	return strxpos(str, ch, STRPOSFLAGS_LAST);
}

void process(struct statefields &S, struct options &opts, const char *file)
{
	ini_parse(file, iniFieldHandler, &S);
}

bool getSubParam(const char *param, std::string &valOut, int paramIndex)
{
	int len = strlen(param);
	char buf[len+1];
	strcpy(buf, param);
	if (strtok(buf, ":") == NULL)
	{
		return false;
	}
	
	strcpy(buf, param);
	const char *delim = ",";
	const char *pch = strtok(buf, delim);
	int pos = 0;
	while (pch != NULL && pos < paramIndex)
	{
		++pos;
		pch = strtok(NULL, delim);
	}
	
	if (pos == paramIndex)
	{
		valOut = pch;
		return true;
	}
	
	return false;
}

bool checkParam(std::vector<std::string> &allFiles, struct options &opts, const char *param)
{
	if (isParam(param, "-h"))
	{
		opts.help = true;
	}
	else if (isParam(param, "-V"))
	{
		opts.verbose = true;
	}
	else if (isParam(param, "-v"))
	{
		opts.version_out = true;
	}
	else if (isParam(param, "-d"))
	{
		std::string wdir;
		if (getSubParam(param, wdir, 0))
		{
			opts.wdir = wdir;
		}
		else
		{
			return false;
		}
	}
	else
	{
		allFiles.push_back(param);
	}
	
	return true;
}

void signalHandler(int sig)
{
	void *array[10];
	size_t size;

	size = backtrace(array, 10);

	fprintf(stderr, "Error: signal %d:\n", sig);
	fprintf(stderr, "Stack trace:\n");
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

void enumExtractName(const std::string &input, std::string &output)
{
	int posE = strfpos(input.c_str(), '=');
	int posP = strfpos(input.c_str(), '(');
	
	int pos = posE > 0 ? posE : posP;
	
	if (pos > 0)
	{
		output = input.substr(0, pos);
	}
	else
	{
		output = input;
	}
}

void extractFileTitle(const std::string &input, std::string &output)
{
	int pos = strlpos(input.c_str(), '.');
	if (pos > 0)
	{
		output = input.substr(0, pos);
	}
	else
	{
		output = input;
	}
}

int iniFieldHandler(void* data, const char* section, const char* name, const char* value)
{
	printf("[%s]%s=%s\n", section, name, value);
	struct statefields &S = *reinterpret_cast<struct statefields *>(data);
	if (0 != strcmp(section, S.curSection.c_str()))
	{
		if (strlen(section) > 0)
		{
			S.sections.push_back(std::string(section));
			S.curSection = (section);
			S.firstField = false;
		}
	}
	
	if (strcmp(name, "c-header") == 0)
	{
		S.cHeader = value;
	}
	else if (strcmp(name, "c-source") == 0)
	{
		S.cSource = value;
	}
	else if (strcmp(name, "header-guard") == 0)
	{
		S.headerGuard = value;
	}
	else if (strcmp(name, "type") == 0)
	{
		S.currentSection().type(value);
	}
	else if (strcmp(name, "stringify-define") == 0)
	{
		S.stringifyDefine = value;
	}
	else if (strcmp(name, "cpp-stringify") == 0)
	{
		S.cppStringifyDisable = strcmp(value, "yes") != 0;
	}
	else if (strcmp(name, "top") == 0)
	{
		S.topExprs.push_back(value);
	}
	else if (strcmp(name, "bottom") == 0)
	{
		S.bottomExprs.push_back(value);
	}
	else if (strcmp(name, "src-dir") == 0)
	{
		S.srcDir = value;
	}
	else if (strcmp(name, "include-dir") == 0)
	{
		S.includeDir = value;
	}
	else if (strcmp(name, "field") == 0)
	{
		if (S.sections.size() < 1)
		{
			fprintf(stderr, "FIELD without SECTION\n");
			exit(1);
		}
		
		S.currentSection().entries().push_back(Entry(value));
	}
	
	return 0;
}

void makeEnumFiles(struct statefields &S)
{
	srand(time(0));
	std::string title;
	extractFileTitle(S.fileName, title);
	
	unsigned titleBufSize = title.size();
	char titleBuf[titleBufSize +1];
	memset(titleBuf, 0, titleBufSize +1);
	for (unsigned i = 0, titleBufPos = 0; i < titleBufSize; ++i)
	{
		char ch = title[i];
		if (isalnum(ch) || ch == '_')
		{
			titleBuf[titleBufPos++] = ch;
		}
	}
	
	if (S.headerGuard.empty())
	{
		S.headerGuard = titleBuf;
		
		unsigned rndLen = 16;
		char rndBuf[rndLen +1];
		memset(rndBuf, 0, rndLen +1);
		for (unsigned i = 0; i < rndLen; ++i)
		{
			rndBuf[i] = ('a' + (rand() % ('z'-'a')));
		}
		
		S.headerGuard += std::string("_") + std::string(rndBuf);
	}
	
	
	
	std::string cHeaderFileName = title + "." + S.cHeader;
	std::string cHeaderActualFileName = S.includeDir + cHeaderFileName;
	std::string cSourceFileName = S.srcDir + title + "." + S.cSource;
	FILE *cHeaderFP = fopen(cHeaderActualFileName.c_str(), "w");
	FILE *cSourceFP = fopen(cSourceFileName.c_str(), "w");
	
	fprintf(cHeaderFP, "%s\n", S.introComment.c_str());
	fprintf(cSourceFP, "%s\n", S.introComment.c_str());
	
	fprintf(cHeaderFP, "#ifndef __enumg_HeaderGuard_%s_INCLUDED__\n", S.headerGuard.c_str());
	fprintf(cHeaderFP, "#define __enumg_HeaderGuard_%s_INCLUDED__\n", S.headerGuard.c_str());
	
	if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
	fprintf(cSourceFP, "#if defined(__cplusplus)\n");
	fprintf(cSourceFP, "\t#include <cstring>\n");
	fprintf(cSourceFP, "#else\n");
	fprintf(cSourceFP, "\t#include <string.h>\n");
	fprintf(cSourceFP, "#endif\n");
	if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
	fprintf(cSourceFP, "#include \"%s\"\n", cHeaderFileName.c_str());
	
	for (auto line : S.topExprs)
	{
		fprintf(cHeaderFP, "%s\n", line.c_str());
	}
	
	for (auto section : S.sections)
	{
		printf("write enum %s\n", section.name().c_str());
		fprintf(cHeaderFP, "%s %s\n{\n", section.type().c_str(), section.name().c_str());
		
		for (auto entry : section.entries())
		{
			// fill out header enum decl
			fprintf(cHeaderFP, "\t%s,\n", entry.fullText().c_str());
		}
		
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
		
		//
		// enum field name string array
		//
		fprintf(cSourceFP, "const char *g_%sStringArray[] = {\n", section.name().c_str());
		for (auto entry : section.entries())
		{	
			fprintf(cSourceFP, "\t\"%s\",\n", entry.name().c_str());
		}
		fprintf(cSourceFP, "};\n");
		
		//
		// value array
		//
		fprintf(cSourceFP, "%s g_%sValueArray[] = \n{\n", section.name().c_str(), section.name().c_str());
		for (auto entry : section.entries())
		{
			fprintf(cSourceFP, "\t%s,\n", entry.name().c_str());
		}
		fprintf(cSourceFP, "};\n");
		
		//
		// to c-string
		//
		fprintf(cSourceFP, "const char *%sToString(%s value)\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "{\n");
		fprintf(cSourceFP, "\tint ix = %sToIndex(value);\n", section.name().c_str());
		fprintf(cSourceFP, "\tif (ix >= 0) {\n");
		fprintf(cSourceFP, "\t\treturn g_%sStringArray[ix];\n", section.name().c_str());
		fprintf(cSourceFP, "\t} else {\n");
		fprintf(cSourceFP, "\t\treturn NULL;\n");
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "}\n");
		
		
		
		fprintf(cSourceFP, "int %sToIndex(%s value)\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "{\n");
		fprintf(cSourceFP, "\tunsigned count = %sValueCount();\n", section.name().c_str());
		fprintf(cSourceFP, "\tfor (unsigned i = 0; i < count; ++i) {\n");
		fprintf(cSourceFP, "\t\tif (value == g_%sValueArray[i]) { return i; }\n", section.name().c_str());
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "\treturn -1;\n");
		fprintf(cSourceFP, "}\n");
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
		
		fprintf(cHeaderFP, "};\n");
		
		if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
		fprintf(cHeaderFP, "const char *%sToString(%s value);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "int %sToIndex(%s value);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "%s %sFromString(const char *str);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "unsigned %sValueCount();\n", section.name().c_str());
		fprintf(cHeaderFP, "%s %sFromIndex(unsigned index);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "int %sFromString(const char *str, %s *presult);\n", section.name().c_str(), section.name().c_str());
		if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#endif\n");
		
//		if (!S.cppStringifyDisable)
//		{
//			if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#if defined(%s) && defined(__cplusplus)\n", S.stringifyDefine.c_str());
//			fprintf(cHeaderFP, "void %sToCppString(std::string &out, %s value);\n", section.name().c_str(), section.name().c_str());
//			if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#endif\n");
//
//
//			if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#if defined(%s) && defined(__cplusplus)\n", S.stringifyDefine.c_str());
//			fprintf(cSourceFP, "void %sToCppString(std::string &out, %s value)\n{\n", section.name().c_str(), section.name().c_str());
//			fprintf(cSourceFP, "\tout = %sToString(value);\n", section.name().c_str());
//			fprintf(cSourceFP, "}\n");
//			if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
//		}
		
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
		//
		// from index
		//
		fprintf(cSourceFP, "%s %sFromIndex(unsigned index)\n{\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "\treturn g_%sValueArray[index];\n", section.name().c_str());
		fprintf(cSourceFP, "}\n");
		fprintf(cSourceFP, "unsigned %sValueCount()\n{\n", section.name().c_str());
		fprintf(cSourceFP, "\treturn %u;\n", (unsigned)section.entries().size());
		fprintf(cSourceFP, "}\n");
		
		//
		// from string 
		//
		fprintf(cSourceFP, "int %sFromString(const char *str, %s *presult)\n{\n", section.name().c_str(), section.name().c_str());
		
		fprintf(cSourceFP, "\tunsigned len = %sValueCount();\n", section.name().c_str());
		fprintf(cSourceFP, "\tfor(unsigned i = 0; i < len; ++i)\n\t{\n");
		fprintf(cSourceFP, "\t\t%s value = %sFromIndex(i);\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "\t\tconst char *checkStr = %sToString(value);\n", section.name().c_str());
		fprintf(cSourceFP, "\t\tif (0 == strcmp(str, checkStr))\n\t\t{\n");
		fprintf(cSourceFP, "\t\t\t*presult = value; return 0;\n");
		fprintf(cSourceFP, "\t\t}\n");
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "\treturn -1;\n");
		fprintf(cSourceFP, "}\n");
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
		
	}
	
	for (auto line : S.bottomExprs)
	{
		fprintf(cHeaderFP, "%s\n", line.c_str());
	}
	
	fprintf(cHeaderFP, "#endif // (header guard)\n");
	fclose(cHeaderFP);
	fclose(cSourceFP);
}

int main(int argc, char **argv)
{
	signal(SIGSEGV, signalHandler);
	
	struct options opts;
	std::vector<std::string> inputFiles;

	for (int i = 1; i < argc; ++i)
	{
		checkParam(inputFiles, opts, argv[i]);
	}
	
	if (opts.version_out)
	{
		printf("%s\n", ENUMG_VERSION);
	}
	else if (opts.help || inputFiles.size() == 0)
	{
		printHelp(argv[0]);
	}
	else
	{
		std::string introComment = 
			"// file auto-generated by [enumg]\n"
			"// at " __DATE__ " " __TIME__ "\n"
			"//\n"
			"//     http://github.com/traaslund/enumg\n"
			"//\n"
			"// command: ";
		
		for (int i = 0; i < argc; ++i)
		{
			introComment += std::string(argv[i]) + std::string(" ");
		}
		
		introComment += "\n";
		
		for (auto file : inputFiles)
		{
			struct statefields S;
			S.introComment = introComment;
			S.fileName = file;
			printf("================================\n");
			printf("input: %s\n", file.c_str());
			printf("--------------parse-------------\n");
			process(S, opts, file.c_str());
			printf("------------write file----------\n");
			makeEnumFiles(S);
			printf("================================\n");
		}
		
		
	}
	
	return 0;
}

