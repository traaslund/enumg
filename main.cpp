#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdarg>

// string trimming
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

// filesystem
#include <filesystem>

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

#define ENUMG_VERSION "0.9.2"



extern "C"
{
	#define INI_MAX_LINE 2048
	#include "ini.h"
}

bool g_silentMode = true;


void logf(const char *fmt, ...)
{
	if (!g_silentMode)
	{
		va_list args;
		
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}


// trim from start
static inline std::string ltrim(const std::string &spar) 
{
	std::string s = spar;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string rtrim(const std::string &spar) 
{
	std::string s = spar;
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string trim(const std::string &spar) 
{
	std::string s = spar;
    return ltrim(rtrim(s));
}

void enumExtractName(const std::string &input, std::string &output);
std::string enumGetThraitsData(const std::string &input, std::string &output);

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
	Entry(std::string fullText) 
	{
		fullText = enumGetThraitsData(fullText, m_thraits);
		enumExtractName(fullText, m_name);
		
		m_fullText = trim(fullText);
		
		m_name = trim(m_name);
		m_thraits = trim(m_thraits);
	}

	const std::string &name() const { return m_name; }
	const std::string &fullText() const { return m_fullText; }
	const std::string &thraits() const { return m_thraits; }

private:
	std::string m_name;
	std::string m_fullText;
	std::string m_thraits;
};

class Section
{
public:
	Section(const std::string &name) 
	{
		m_name = trim(name);
	}

	const std::string &name() const { return m_name; }
	
	const std::string &type() const { return m_type; }
	void type(const std::string &val) { m_type = val; }
	
	const std::string &thraitsName() const { return m_thraitsName; }
	void thraitsName(const std::string &val) { m_thraitsName = val; }
	
	const std::string &thraitsEnableMacro() const { return m_thraitsEnableMacro; }
	void thraitsEnableMacro(const std::string &val) { m_thraitsEnableMacro = val; }
	
	const std::vector<Entry> &entries() const { return m_entries; }
	std::vector<Entry> &entries() { return m_entries; }
	
private:
	std::string m_name;
	std::string m_type;
	std::string m_thraitsName;
	std::string m_thraitsEnableMacro;
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
	std::vector<std::string> includeFiles;
};
/////////////////////

void printHelp(const char *cmdName)
{
	logf("%s version %s\n", cmdName, ENUMG_VERSION);
	logf("Usage: \n");
	logf("  %s [file_1 [file_2 ... [file_n]]] [options]\n", cmdName);
	logf("\n");
	logf("options:\n");
	logf("  -v        : print version and exit\n");
	logf("  -V        : verbose output\n");
	logf("  -h        : this screen\n");
	logf("            : this screen (no arguments)\n");
}



bool isParam(const char *str, const char *param)
{
	bool res = 0 == strncmp(str, param, strlen(param));
	return res;
}

int strxpos(const char *str, int ch, int flags)
{
	if (str == nullptr)
	{
		fprintf(stderr, "nullptr string* to strxpos\n");
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
	if (strtok(buf, ":") == nullptr)
	{
		return false;
	}
	
	strcpy(buf, param);
	const char *delim = ",";
	const char *pch = strtok(buf, delim);
	int pos = 0;
	while (pch != nullptr && pos < paramIndex)
	{
		++pos;
		pch = strtok(nullptr, delim);
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
		opts.verbose = true;
		opts.help = true;
	}
	else if (isParam(param, "-V"))
	{
		opts.verbose = true;
	}
	else if (isParam(param, "-v"))
	{
		opts.verbose = true;
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

std::string enumGetThraitsData(const std::string &input, std::string &output)
{
	int posP = strfpos(input.c_str(), '(');
	
	if (posP >= 0)
	{
		output = input.substr(posP);
		return input.substr(0, posP);
	}
	else
	{
		return input;
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
	logf("[%s]%s=%s\n", section, name, value);
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
	
	std::string sectionStr = ((section)); 
	std::string nameStr = ((name));
	std::string valueStr = ((value));
	sectionStr = trim(sectionStr);
	nameStr = trim(nameStr);
	valueStr = trim(valueStr);
	
	section = sectionStr.c_str();
	name = nameStr.c_str();
	value = valueStr.c_str();
	
	
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
	else if (strcmp(name, "thraits") == 0)
	{
		S.currentSection().thraitsName(value);
	}
	else if (strcmp(name, "thraits-enable-macro") == 0)
	{
		S.currentSection().thraitsEnableMacro(value);
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
	else if (strcmp(name, "include-file") == 0)
	{
		S.includeFiles.push_back(value);
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

void ltrim(std::string &s) 
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

void rtrim(std::string &s) 
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void trim(std::string &s) 
{
    ltrim(s);
    rtrim(s);
}

#define HEADER_GUARD_START_TOKEN "//$$enumg_hg_macro_name="

std::string tryReadOldHeaderGuardMacroName(const std::string &filename)
{
	FILE *pf = fopen(filename.c_str(), "r");
	if (!pf) return std::string();
	
	size_t bufLen = 2048;
	char buf[bufLen];
	const char *startToken = HEADER_GUARD_START_TOKEN;
	size_t startTokenLen = strlen(startToken);
	
	std::string result;
	
	while ( fgets(buf, bufLen, pf) ) {
		if (strncmp(startToken, buf, startTokenLen) == 0) {
			result = buf + startTokenLen;
			trim(result);
			break;
		}
	}
	
	fclose(pf);
	
	return result;
}


int file_compare(const std::string &file1, const std::string &file2)
{
	FILE *pf1 = fopen(file1.c_str(), "r");
	FILE *pf2 = fopen(file2.c_str(), "r");
	
	int result = 0;
	bool haveResult = false;
	
	if (pf1 == nullptr && pf2 != nullptr) { result = -1; haveResult = true; }
	else if (pf1 != nullptr && pf2 == nullptr) { result = 1; haveResult = true; }
	else if (pf1 == nullptr && pf2 == nullptr) { result = 0; haveResult = true; }
	
	while (!haveResult) {
		int ch1 = fgetc(pf1);
		int ch2 = fgetc(pf2);
		
		if (ch1 == EOF && ch2 != EOF) { result = -1; haveResult = true; }
		else if (ch1 != EOF && ch2 == EOF) { result = 1; haveResult = true; }
		else if (ch1 == EOF && ch2 == EOF) { result = 0; haveResult = true; }
		else if (ch1 < ch2) { result = -1; haveResult = true; }
		else if (ch1 > ch2) { result = 1; haveResult = true; }
	}
	
	if (pf1 != nullptr) fclose(pf1);
	if (pf2 != nullptr) fclose(pf2);
	
	return result;
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
	
	std::string cHeaderActualFileName = tmpnam(nullptr);
	std::string cSourceFileName = tmpnam(nullptr);
	
	std::string final_cHeaderActualFileName = S.includeDir + cHeaderFileName;
	std::string final_cSourceFileName = S.srcDir + title + "." + S.cSource;
	
	std::string oldHGMacroName = tryReadOldHeaderGuardMacroName(final_cHeaderActualFileName);
	if (oldHGMacroName.size() > 0) {
		S.headerGuard = oldHGMacroName;
	}
	
	FILE *cHeaderFP = fopen(cHeaderActualFileName.c_str(), "w");
	FILE *cSourceFP = fopen(cSourceFileName.c_str(), "w");
	
	fprintf(cHeaderFP, "%s%s\n", HEADER_GUARD_START_TOKEN, S.headerGuard.c_str());
	
	fprintf(cHeaderFP, "%s\n", S.introComment.c_str());
	fprintf(cSourceFP, "%s\n", S.introComment.c_str());
	
	fprintf(cHeaderFP, "#ifndef __enumg_HeaderGuard_%s_INCLUDED__\n", S.headerGuard.c_str());
	fprintf(cHeaderFP, "#define __enumg_HeaderGuard_%s_INCLUDED__\n", S.headerGuard.c_str());
	
	fprintf(cSourceFP, "#include \"%s\"\n", cHeaderFileName.c_str());
	
	for (auto line : S.topExprs)
	{
		fprintf(cHeaderFP, "%s\n", line.c_str());
	}
	
	for (auto line : S.includeFiles)
	{
		fprintf(cHeaderFP, "#include %s\n", line.c_str());
	}
	
	if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
	fprintf(cSourceFP, "#if defined(__cplusplus)\n");
	fprintf(cSourceFP, "\t#include <cstring>\n");
	fprintf(cSourceFP, "\t#include <cctype>\n");
	fprintf(cSourceFP, "#else\n");
	fprintf(cSourceFP, "\t#include <string.h>\n");
	fprintf(cSourceFP, "\t#include <ctype.h>\n");
	fprintf(cSourceFP, "#endif\n");
	if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
	
	
	
	
	for (auto section : S.sections)
	{
		logf("write enum %s\n", section.name().c_str());
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
		fprintf(cSourceFP, "\t\treturn nullptr;\n");
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "}\n");
		
		
		
		
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
		
		fprintf(cHeaderFP, "};\n");
		
		if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#if defined(%s)\n", S.stringifyDefine.c_str());
		fprintf(cHeaderFP, "const char *%sToString(%s value);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "%s %sFromString(const char *str);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "unsigned %sValueCount();\n", section.name().c_str());
		fprintf(cHeaderFP, "%s %sFromIndex(unsigned index);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "int %sFromString(const char *str, %s *presult, bool ignoreCase = false, int ignorePrefixLen = 0);\n", section.name().c_str(), section.name().c_str());
		fprintf(cHeaderFP, "int %sToIndex(%s value);\n", section.name().c_str(), section.name().c_str());
		if (S.stringifyDefine.size() > 0) fprintf(cHeaderFP, "#endif\n");
		
		if (section.thraitsName().size() > 0)
		{
			if (section.thraitsEnableMacro().size() > 0) {
				fprintf(cHeaderFP, "#if defined(%s)\n", section.thraitsEnableMacro().c_str());
			}
			
			fprintf(cHeaderFP, "const %s *%s_GetThraits(%s value, const %s *defaultResult = nullptr);\n", section.thraitsName().c_str(), section.name().c_str(), section.name().c_str(), section.thraitsName().c_str());
			
			if (section.thraitsEnableMacro().size() > 0) {
				fprintf(cHeaderFP, "#endif // %s\n", section.thraitsEnableMacro().c_str());
			}
		}
		
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
		fprintf(cSourceFP, "int %sFromString(const char *str, %s *presult, bool ignoreCase, int ignorePrefixLen)\n{\n", section.name().c_str(), section.name().c_str());
		
		fprintf(cSourceFP, "\tunsigned len = %sValueCount();\n", section.name().c_str());
		fprintf(cSourceFP, "\tfor(unsigned i = 0; i < len; ++i)\n\t{\n");
		fprintf(cSourceFP, "\t\t%s value = %sFromIndex(i);\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "\t\tconst char *checkStr = %sToString(value);\n", section.name().c_str());
		fprintf(cSourceFP, "\t\tbool equal = true;\n");
		fprintf(cSourceFP, "\t\tfor (const char *a = str, *b = checkStr + ignorePrefixLen; ; ++a, ++b)\n");
		fprintf(cSourceFP, "\t\t{\n");
		fprintf(cSourceFP, "\t\t\tif (*a == (char)0 || *b == (char)0)\n");
		fprintf(cSourceFP, "\t\t\t{\n");
		fprintf(cSourceFP, "\t\t\t\tequal = (*a == *b);\n");
		fprintf(cSourceFP, "\t\t\t\tbreak;\n");
		fprintf(cSourceFP, "\t\t\t}\n");
		fprintf(cSourceFP, "\t\t\tchar x, y;\n");
		fprintf(cSourceFP, "\t\t\tif (ignoreCase) { x = tolower(*a); y = tolower(*b); }\n");
		fprintf(cSourceFP, "\t\t\telse { x = *a; y = *b; }\n");
		fprintf(cSourceFP, "\t\t\tif (x != y)\n");
		fprintf(cSourceFP, "\t\t\t{\n");
		fprintf(cSourceFP, "\t\t\t\tequal = false;\n");
		fprintf(cSourceFP, "\t\t\t\tbreak;\n");
		fprintf(cSourceFP, "\t\t\t}\n");
		fprintf(cSourceFP, "\t\t}\n");
		////////fprintf(cSourceFP, "\t\tif (0 == strcmp(str, checkStr))\n\t\t{\n");
		fprintf(cSourceFP, "\t\tif (equal)\n\t\t{\n");
		fprintf(cSourceFP, "\t\t\t*presult = value; return 0;\n");
		fprintf(cSourceFP, "\t\t}\n");
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "\treturn -1;\n");
		fprintf(cSourceFP, "}\n");
		
		//
		// To Index
		//
		fprintf(cSourceFP, "int %sToIndex(%s value)\n", section.name().c_str(), section.name().c_str());
		fprintf(cSourceFP, "{\n");
		fprintf(cSourceFP, "\tunsigned count = %sValueCount();\n", section.name().c_str());
		fprintf(cSourceFP, "\tfor (unsigned i = 0; i < count; ++i) {\n");
		fprintf(cSourceFP, "\t\tif (value == g_%sValueArray[i]) { return i; }\n", section.name().c_str());
		fprintf(cSourceFP, "\t}\n");
		fprintf(cSourceFP, "\treturn -1;\n");
		fprintf(cSourceFP, "}\n");
		
		if (S.stringifyDefine.size() > 0) fprintf(cSourceFP, "#endif\n");
		
		
		
		//
		// thraits
		//
		if (section.thraitsName().size() > 0) 
		{
			if (section.thraitsEnableMacro().size() > 0) {
				fprintf(cSourceFP, "#if defined(%s)\n", section.thraitsEnableMacro().c_str());
			}
			
			std::vector<Entry> list;
			for (auto entry : section.entries())
			{
				if (entry.thraits().size() > 0)
				{
					list.push_back(entry);
				}
			}
			
			fprintf(cSourceFP, "\n");
			
			fprintf(cSourceFP, "class __%sThraitsHolder {\n", section.name().c_str());
			fprintf(cSourceFP, "public:\n");
			fprintf(cSourceFP, "\t%s key; %s *value;\n", section.name().c_str(), section.thraitsName().c_str());
			fprintf(cSourceFP, "\t__%sThraitsHolder(%s keyArg, %s *valueArg) : key(keyArg), value(valueArg) { }\n", section.name().c_str(), section.name().c_str(), section.thraitsName().c_str());
			fprintf(cSourceFP, "\t~__%sThraitsHolder() { if (value != nullptr) delete value; }\n", section.name().c_str());
			fprintf(cSourceFP, "};\n");
			
			//fprintf(cSourceFP, "%s g_%sThraitsArray[%d] = {\n", section.thraitsName().c_str(), section.name().c_str(), (int)list.size());
			fprintf(cSourceFP, "__%sThraitsHolder g_%sThraitsArray[%d] = {\n", 
				section.name().c_str(), section.name().c_str(), (int)list.size()
			);
			
			for (unsigned i = 0; i < list.size(); ++i)
			{
				std::string nameKey = list[i].name().c_str();
				std::string data = "new " + section.thraitsName() + list[i].thraits().c_str();
				fprintf(cSourceFP, "\t__%sThraitsHolder(%s, %s)", section.name().c_str(), nameKey.c_str(), data.c_str());
				
				if (i < list.size() -1) fprintf(cSourceFP, ",");
				fprintf(cSourceFP, "\n");
			} 
			fprintf(cSourceFP, "};\n");
			fprintf(cSourceFP, "const %s *%s_GetThraits(%s value, const %s *defaultResult)\n{\n", section.thraitsName().c_str(), section.name().c_str(), section.name().c_str(), section.thraitsName().c_str());
			fprintf(cSourceFP, "\tfor (int i = 0; i < %d; ++i) {\n", (int)list.size());
			fprintf(cSourceFP, "\t\tauto &item = g_%sThraitsArray[i];\n", section.name().c_str());
			fprintf(cSourceFP, "\t\tif (item.key == value) return item.value;\n");
			fprintf(cSourceFP, "\t}\n");
			fprintf(cSourceFP, "\treturn defaultResult;\n");
			fprintf(cSourceFP, "}\n");
			
			if (section.thraitsEnableMacro().size() > 0) {
				fprintf(cSourceFP, "#endif // %s\n", section.thraitsEnableMacro().c_str());
			}
		}
		
		
	}
	
	for (auto line : S.bottomExprs)
	{
		fprintf(cHeaderFP, "%s\n", line.c_str());
	}
	
	fprintf(cHeaderFP, "#endif // (header guard)\n");
	fclose(cHeaderFP);
	fclose(cSourceFP);
	
	auto copyOptions = std::filesystem::copy_options::overwrite_existing;
	
	if (file_compare(cHeaderActualFileName, final_cHeaderActualFileName) != 0) {
		std::filesystem::copy(cHeaderActualFileName, final_cHeaderActualFileName, copyOptions);
	}
	
	if (file_compare(cSourceFileName, final_cSourceFileName) != 0) {
		std::filesystem::copy(cSourceFileName, final_cSourceFileName, copyOptions);
	}
	
	remove(cHeaderActualFileName.c_str());
	remove(cSourceFileName.c_str());
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
	
	if (opts.verbose)
	{
		g_silentMode = false;
	}
	
	if (opts.version_out)
	{
		logf("%s\n", ENUMG_VERSION);
	}
	else if (opts.help || inputFiles.size() == 0)
	{
		printHelp(argv[0]);
	}
	else
	{
		std::string introComment = 
			"// ==============================\n"
			"// file auto-generated by [enumg]\n"
			"// ==============================\n"
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
			logf("================================\n");
			logf("input: %s\n", file.c_str());
			logf("--------------parse-------------\n");
			process(S, opts, file.c_str());
			logf("------------write file----------\n");
			makeEnumFiles(S);
			logf("================================\n");
		}
		
		
	}
	
	return 0;
}

