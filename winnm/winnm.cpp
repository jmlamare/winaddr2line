// winnm.cpp : Defines the entry point for the console application.
//

#ifdef UNICODE
#define DBGHELP_TRANSLATE_TCHAR
#endif

#include "stdafx.h"
#include <windows.h>
#include "dbghelp.h"

#define    VERSION                TEXT("1.0.1")

#define OPT_PRINT_FILE_NAME   TEXT("-A")
#define OPT_PRINT_FILE_NAME_L TEXT("--print-file-name")
#define OPT_LINENUMBERS        TEXT("-l")
#define OPT_LINENUMBERS_L        TEXT("--line-numbers")
#define OPT_DEMANGLE        TEXT("-C")
#define OPT_DEMANGLE_L        TEXT("--demangle")
#define OPT_HELP            TEXT("-h")
#define OPT_HELP_L            TEXT("--help")
#define OPT_VERSION            TEXT("-v")
#define OPT_VERSION_L        TEXT("--version")
#define OPT_SYMBOL_PATH		TEXT("-y")
#define OPT_SYMBOL_PATH_L	TEXT("--symbol-path")

struct file_node
{
	TCHAR *file;
	file_node *next;

	file_node(TCHAR* f): file(f), next(NULL)
	{}
};

struct option
{
	file_node *file_list;
	BOOL demangle;
	BOOL help;
	BOOL version;
	BOOL print_file_name;
	BOOL line_numbers;
	PTSTR symbol_path;
	int file_count;
	int argc;
	TCHAR **argv;

	option(int ac, TCHAR* av[]) : argc(ac), argv(av),
		line_numbers(FALSE), print_file_name(FALSE), demangle(FALSE),
		help(FALSE), version(FALSE), file_list(NULL),
		file_count(0), symbol_path(NULL)
	{}

	~option()
	{
		file_node *node;
		while(NULL != file_list)
		{
			node = file_list;
			file_list = node->next;
			delete node;
		}
	}
};

void print_version()
{
	_tprintf(TEXT("winnm %s\r\n"), VERSION);
}

void print_help()
{
	printf("%s\r\n", 
		"Usage: winnm [option(s)] [file(s)]\n\
		List symbols in [file(s)] (a.exe by default).\n\
		The options are:\n\
		-A, --print-file-name  Print name of the input file before every symbol\n\
		-C, --demangle         Decode low-level symbol names into user-level names\n\
		-l, --line-numbers     Use debugging information to find a filename and\n\
							line number for each symbol\n\
		-n, --numeric-sort     Sort symbols numerically by address\n\
		-o                     Same as -A\n\
		-h, --help             Display this information\n\
		-V, --version          Display this program's version number\n");
}

int parse_option(option& opt)
{
	int argc = opt.argc;
	TCHAR** argv = opt.argv;
	file_node *node = NULL, *prev = NULL;
	if(argc < 2)
	{
		print_help();
		return 1;
	}
	for(int i = 1; i < argc; ++i)
	{
		if(0 == _tcsncmp(OPT_PRINT_FILE_NAME, argv[i], _tcslen(OPT_PRINT_FILE_NAME))
			|| 0 == _tcsncmp(OPT_PRINT_FILE_NAME_L, argv[i], _tcslen(OPT_PRINT_FILE_NAME_L)))
		{
			opt.print_file_name = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_LINENUMBERS, argv[i], _tcslen(OPT_LINENUMBERS))
			|| 0 == _tcsncmp(OPT_LINENUMBERS_L, argv[i], _tcslen(OPT_LINENUMBERS_L)))
		{
			opt.line_numbers = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_DEMANGLE, argv[i], _tcslen(OPT_DEMANGLE))
			|| 0 == _tcsncmp(OPT_DEMANGLE_L, argv[i], _tcslen(OPT_DEMANGLE_L)))
		{
			opt.demangle = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_HELP, argv[i], _tcslen(OPT_HELP))
			|| 0 == _tcsncmp(OPT_HELP_L, argv[i], _tcslen(OPT_HELP_L)))
		{
			opt.help = TRUE;
			print_help();
			return 1;
		}
		if(0 == _tcsncmp(OPT_VERSION, argv[i], _tcslen(OPT_VERSION))
			|| 0 == _tcsncmp(OPT_VERSION_L, argv[i], _tcslen(OPT_VERSION_L)))
		{
			opt.version = TRUE;
			print_version();
			return 1;
		}
		if(0 == _tcsncmp(OPT_SYMBOL_PATH, argv[i], _tcslen(OPT_SYMBOL_PATH))
			|| 0 == _tcsncmp(OPT_SYMBOL_PATH_L, argv[i], _tcslen(OPT_SYMBOL_PATH_L)))
		{		
			if(i + 1 >= argc)
			{
				_tprintf(TEXT("option '%s' requires an argument\r\n"), argv[i]);
				return 1;
			}	
			++i;
			opt.symbol_path = argv[i];
			continue;
		}
		// if argv isn't a known argument, it's assumed to be file names in hex format
		++opt.file_count; // count the number of addresses supplied in argument
		node = new file_node(argv[i]);

		if(NULL == opt.file_list)
		{
			opt.file_list = node;
			prev = opt.file_list;
		}
		else
		{
			prev->next = node;
			prev = node;
		}
	}
	return 0;
}

int get_file_attr(const TCHAR* exe, DWORD64 &base, DWORD &file_size)
{
	HANDLE f = CreateFile(exe, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL );

	if(f == INVALID_HANDLE_VALUE)
	{
		_ftprintf(stderr, TEXT("'%s': No such file\n"), exe);
		return 0;
	}

	TCHAR ext[_MAX_EXT] = {0};
	_tsplitpath_s(exe, NULL, 0, NULL, 0, NULL, 0, ext, _MAX_EXT);
	if(_tcsicmp(TEXT(".pdb"), ext) == 0)
		base = 0x10000000; // base address can't be null if exe is a .pdb symbol file
	file_size = GetFileSize(f, NULL);
	return 1;
}

TCHAR* get_next_file(option &opt, int index)
{
	if(opt.file_count < 1)
		return TEXT("a.exe");
	if(index >= opt.file_count)
		return NULL;
	file_node* node = opt.file_list;
	int i = 0;
		//_tprintf(TEXT("%d %d\n"), i, index);
	while(i < index)
	{
		node = node->next;
		++i;
	}
	return node->file;
}

enum SymTagEnum { 
	SymTagNull,
	SymTagExe,
	SymTagCompiland,
	SymTagCompilandDetails,
	SymTagCompilandEnv,
	SymTagFunction,
	SymTagBlock,
	SymTagData,
	SymTagAnnotation,
	SymTagLabel,
	SymTagPublicSymbol,
	SymTagUDT,
	SymTagEnum,
	SymTagFunctionType,
	SymTagPointerType,
	SymTagArrayType, 
	SymTagBaseType, 
	SymTagTypedef, 
	SymTagBaseClass,
	SymTagFriend,
	SymTagFunctionArgType, 
	SymTagFuncDebugStart, 
	SymTagFuncDebugEnd,
	SymTagUsingNamespace, 
	SymTagVTableShape,
	SymTagVTable,
	SymTagCustom,
	SymTagThunk,
	SymTagCustomType,
	SymTagManagedType,
	SymTagDimension
};

static TCHAR get_symbol_type(PSYMBOL_INFO pSymInfo)
{
	switch(pSymInfo->Tag)
	{
	case SymTagFunction:
		return 'T';
	case SymTagData:
		return 'D';
	default:
		return ' ';
	}
}

struct user_context
{
	option *opt;
	TCHAR* module;

	user_context(option *o, TCHAR* m) : opt(o), module(m)
	{
	}
};

static BOOL CALLBACK on_enum_symbol(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
	user_context* uc = (user_context*)UserContext;
	option *opt = uc->opt;;
	if(opt->print_file_name)
		_tprintf(TEXT("%s:"), uc->module);

	_tprintf(TEXT("%08llx %c %s"), pSymInfo->Address, //pSymInfo->Tag,
		get_symbol_type(pSymInfo), pSymInfo->Name);

	PIMAGEHLP_LINE64 line = new IMAGEHLP_LINE64();
	memset(line, 0, sizeof(IMAGEHLP_LINE64));
	line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD disp;
	if (opt->line_numbers &&
		0 != SymGetLineFromAddr64(GetCurrentProcess(), pSymInfo->Address, &disp, line))
	{
		_tprintf(TEXT("  %s:%d"), line->FileName, line->LineNumber);
	}
	delete line;
	_tprintf(TEXT("\n"));
	return TRUE;
}

int nm(option &opt)
{
	HANDLE proc;

	proc = GetCurrentProcess();

	DWORD sym_opt = SymGetOptions();
	if(opt.demangle)
		sym_opt |= SYMOPT_UNDNAME;
	else
		sym_opt &= ~SYMOPT_UNDNAME;
	SymSetOptions(sym_opt);

	if (!SymInitialize(proc, opt.symbol_path, FALSE))
	{
		_tprintf(TEXT("%d error %d\n"), __LINE__, GetLastError());
		return GetLastError();
	}

	DWORD64 base_addr = 0, load_addr = 0;
	DWORD file_size = 0;

	int i = 0;
	TCHAR* module = get_next_file(opt, i);
	while(NULL != module)
	{
		if(!get_file_attr(module, base_addr, file_size))
		{
			_tprintf(TEXT("get_file_attr %d error %d  %s\n"), __LINE__, i, module);
			return 1;
		}
		load_addr = SymLoadModuleEx(proc, NULL, module, NULL, base_addr,
			file_size, NULL, 0);
		if(0 == load_addr)
		{
			return GetLastError();
		}
		user_context uc(&opt, module);
		if(!SymEnumSymbols(proc, base_addr, "*!*", on_enum_symbol, &uc))       
			_tprintf(TEXT("SymEnumSymbols failed. errorno: %u\n"), __LINE__, GetLastError());

		SymUnloadModule64(proc, load_addr);
		module = get_next_file(opt, ++i);
	}
	SymCleanup(proc);
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	option opt(argc, argv);
	if(0 == parse_option(opt))
		return nm(opt);
	return 0;
}
