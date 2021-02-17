// winaddr2line.cpp : Defines the entry point for the console application.
//

#ifdef UNICODE
#define DBGHELP_TRANSLATE_TCHAR
#endif

#include "stdafx.h"
#include <windows.h>
#include "dbghelp.h"

#define	VERSION				TEXT("1.0.1")

#define OPT_ADDRESSES		TEXT("-a")
#define OPT_ADDRESSES_L		TEXT("--address")
#define OPT_PRETTY_PRINT	TEXT("-p")
#define OPT_PRETTY_PRINT_L	TEXT("--pretty-print")
#define OPT_BASENAME		TEXT("-s")
#define OPT_BASENAME_L		TEXT("--basename")
#define OPT_EXE				TEXT("-e")
#define OPT_EXE_L			TEXT("--exe")
#define OPT_FUNCTIONS		TEXT("-f")
#define OPT_FUNCTIONS_L		TEXT("--functions")
#define OPT_DEMANGLE		TEXT("-C")
#define OPT_DEMANGLE_L		TEXT("--demangle")
#define OPT_HELP			TEXT("-h")
#define OPT_HELP_L			TEXT("--help")
#define OPT_VERSION			TEXT("-v")
#define OPT_VERSION_L		TEXT("--version")
#define OPT_SYMBOL_PATH		TEXT("-y")
#define OPT_SYMBOL_PATH_L	TEXT("--symbol-path")

struct address_node
{
	TCHAR *address;
	address_node *next;

	address_node(TCHAR* addr): address(addr), next(NULL)
	{}
};

struct option 
{
	struct address_node *addr_list;
	BOOL addresses; 
	BOOL pretty_print;
	BOOL basename;
	BOOL functions;
	BOOL demangle;
	BOOL help;
	BOOL version;
	PTSTR exe;
	PTSTR symbol_path;
	int addr_count;
	int argc;
	TCHAR** argv;

	option(int ac, TCHAR* av[]) : addresses(FALSE), pretty_print(FALSE),
		basename(FALSE), functions(FALSE), demangle(FALSE),
		help(FALSE), version(FALSE), addr_list(NULL),
		exe(TEXT("a.exe")), symbol_path(NULL), addr_count(0),
		argc(ac), argv(av)
	{}

	~option()
	{
		address_node *node;
		while(NULL != addr_list)
		{
			node = addr_list;
			addr_list = node->next;
			delete node;
		}
	}
};

struct symbol_info
{
	PSYMBOL_INFO symbol;
	PIMAGEHLP_LINE64 line;
	BOOL success;

	symbol_info()
	{
		symbol = reinterpret_cast<PSYMBOL_INFO>(new char[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)]);
		memset(symbol, 0, sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR));
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;
		line = new IMAGEHLP_LINE64();
		memset(line, 0, sizeof(IMAGEHLP_LINE64));
		line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		success = TRUE;
	}

	~symbol_info()
	{
		delete symbol;
		delete line;
	}
};

void print_version()
{
	_tprintf(TEXT("winaddr2line %s\r\n"), VERSION);
}

void print_help()
{
	printf("%s\r\n", "Usage: winaddr2line [option(s)] [addr(s)]\n"
		" Convert addresses into line number/file name pairs.\n"
		" If no addresses are specified on the command line, they will be read from stdin\n"
		" The options are:\n"
		"  -a --addresses         Show addresses\n"
		"  -e --exe <executable>  Set the input file name (default is a.exe)\n"
		"  -p --pretty-print      Make the output easier to read for humans\n"
		"  -s --basenames         Strip directory names\n"
		"  -f --functions         Show function names\n"
		"  -C --demangle[=style]  Demangle function names\n"
		"  -y --symbol-path=<direcoty_to_symbol>    (option specific to winaddr2line)\n"
		"                         Set the directory to search for symbol file (.pdb)\n"
		"  -h --help              Display this information\n"
		"  -v --version           Display the program's version\n");
}

int parse_option(option& opt)
{
	int argc = opt.argc;
	TCHAR** argv = opt.argv;
	address_node *node = NULL, *prev = NULL;
	if(argc < 2)
	{
		print_help();
		return 1;
	}
	for(int i = 1; i < argc; ++i)
	{
		if(0 == _tcsncmp(OPT_ADDRESSES, argv[i], _tcslen(OPT_ADDRESSES))
			|| 0 == _tcsncmp(OPT_ADDRESSES_L, argv[i], _tcslen(OPT_ADDRESSES_L)))
		{
			opt.addresses = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_PRETTY_PRINT, argv[i], _tcslen(OPT_PRETTY_PRINT))
			|| 0 == _tcsncmp(OPT_PRETTY_PRINT_L, argv[i], _tcslen(OPT_PRETTY_PRINT_L)))
		{
			opt.pretty_print = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_BASENAME, argv[i], _tcslen(OPT_BASENAME))
			|| 0 == _tcsncmp(OPT_BASENAME_L, argv[i], _tcslen(OPT_BASENAME_L)))
		{
			opt.basename = TRUE;
			continue;
		}
		if(0 == _tcsncmp(OPT_FUNCTIONS, argv[i], _tcslen(OPT_FUNCTIONS))
			|| 0 == _tcsncmp(OPT_FUNCTIONS_L, argv[i], _tcslen(OPT_FUNCTIONS_L)))
		{
			opt.functions = TRUE;
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
		if(0 == _tcsncmp(OPT_EXE, argv[i], _tcslen(OPT_EXE))
			|| 0 == _tcsncmp(OPT_EXE_L, argv[i], _tcslen(OPT_EXE_L)))
		{		
			if(i + 1 >= argc)
			{
				_tprintf(TEXT("option '%s' requires an argument\r\n"), argv[i]);
				return 1;
			}	
			++i;
			opt.exe = argv[i];
			continue;
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
		// if argv isn't a known argument, it's assumed to be address in hex format
		++opt.addr_count; // count the number of addresses supplied in argument
		node = new address_node(argv[i]);
		
		if(NULL == opt.addr_list)
		{
			opt.addr_list = node;
			prev = opt.addr_list;
		}
		else
		{
			prev->next = node;
			prev = node;
		}
	}
	return 0;
}

void print(DWORD64 addr, option &opt, symbol_info &sym)
{
	if(!sym.success)
	{
		if(opt.pretty_print)
		{
			if(opt.addresses)
				_tprintf(TEXT("0x%08x: "), addr);
			if(opt.functions)
				_tprintf(TEXT("??\n"), addr);
			_tprintf(TEXT("??:0\n"));
		}
		else
		{
			if(opt.addresses)
				_tprintf(TEXT("0x%08x\n"), addr);
			if(opt.functions)
				_tprintf(TEXT("??\n"), addr);
			_tprintf(TEXT("??:0\n"));
		}
		return;
	}
	if(opt.pretty_print)
	{
		if(opt.addresses)
			_tprintf(TEXT("0x%08x: "), addr);

		if(opt.functions)
		{
			_tprintf(TEXT("%s at "), sym.symbol->Name);
		}

		if(opt.basename)
		{
			TCHAR fname[_MAX_FNAME] = {0}, ext[_MAX_EXT] = {0};
			_tsplitpath_s(sym.line->FileName, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
			_tprintf(TEXT("%s%s:%d\r\n"), fname, ext, sym.line->LineNumber);
		}
		else
			_tprintf(TEXT("%s:%d\r\n"), sym.line->FileName, sym.line->LineNumber);
	} // if(opt.pretty_print) 
	else
	{
		if(opt.addresses)
			_tprintf(TEXT("0x%08x\r\n"), addr);

		if(opt.functions)
		{
			_tprintf(TEXT("%s\r\n"), sym.symbol->Name);
		}

		if(opt.basename)
		{
			TCHAR fname[_MAX_FNAME] = {0}, ext[_MAX_EXT] = {0};
			_tsplitpath_s(sym.line->FileName, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
			_tprintf(TEXT("%s%s:%d\r\n"), fname, ext, sym.line->LineNumber);
		}
		else
			_tprintf(TEXT("%s:%d\r\n"), sym.line->FileName, sym.line->LineNumber);
	} // if(opt.pretty_print) 
}

DWORD64 get_next_address(const option &opt, int index)
{	
	int i = 0;
	if(opt.addr_count > 0)
	{
		address_node *node = opt.addr_list;
		while(i++ < index)
		{
			
			node = node->next;
		}
		DWORD64 SymAddr = (DWORD64)-1;
		if (NULL != node && _stscanf(node->address, _T("%I64x"), &SymAddr)!=1) {
			SymAddr = (DWORD64)-1;
		}
		return SymAddr;
	}
	else
	{
		TCHAR buf[256];
		if(_fgetts(buf, 256, stdin))
			return _tcstoul(buf, NULL, 16);
		else
			return (DWORD64)-1;
	}
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

int addr2line(option &opt)
{
	HANDLE proc;
	symbol_info result;

	proc = GetCurrentProcess();

	DWORD sym_opt = SymGetOptions();
	// SYMOPT_DEBUG option asks DbgHelp to print additional troubleshooting 
	// messages to debug output - use the debugger's Debug Output window 
	// to view the messages 
	sym_opt |= SYMOPT_DEBUG;

	// SYMOPT_LOAD_LINES option asks DbgHelp to load line information 
	sym_opt |= SYMOPT_LOAD_LINES; // load line information 
	if(opt.demangle)
		sym_opt |= SYMOPT_UNDNAME;
	else
		sym_opt &= ~SYMOPT_UNDNAME;
	::SymSetOptions(sym_opt);

	if (!SymInitialize(proc, opt.symbol_path, FALSE))
	{
		int errorno = GetLastError();
		_ftprintf(stderr, TEXT("SymInitialize failed, error code: %d\n"), errorno);
		return errorno;
	}

	DWORD64 base_addr = 0, load_addr = 0;
	DWORD file_size = 0;

	if(!get_file_attr(opt.exe, base_addr, file_size))
		return 1;
	load_addr = SymLoadModuleEx(proc, NULL, opt.exe, NULL, base_addr, file_size, NULL, 0);
	// load_addr = SymLoadModule64(proc, NULL, opt.exe, NULL, base_addr, file_size);
	if(0 == load_addr)
	{
		int errorno = GetLastError();
		_ftprintf(stderr, TEXT("SymLoadModuleEx failed, error code: %d\n"), errorno);
		return errorno;
	}
	else {
		_ftprintf(stderr, TEXT("Load address: %I64x\n"), load_addr);
	}

	int i = 0;
	DWORD64 func_addr;
	while((func_addr = get_next_address(opt, i++)) != -1)
	{ // if function address isn't given as argument, read from stdio
		result.success = TRUE;
		// _ftprintf(stderr, TEXT("Looking for symbol at address %I64x\n"), func_addr);
		if(opt.functions)
		{
			DWORD64  displacement = 0;
			if (!SymFromAddr(proc, func_addr, &displacement, result.symbol))
			{
				int errorno = GetLastError();
				_ftprintf(stderr, TEXT("SymFromAddr failed at address %I64x, error code: %d\n"), func_addr, errorno);
				result.success = FALSE;
			}
		}
		{
			DWORD displacement;
			if (!SymGetLineFromAddr64(proc, func_addr, &displacement, result.line))
			{
				int errorno = GetLastError();
				_ftprintf(stderr, TEXT("SymGetLineFromAddr64 failed at address %I64x, error code: %d\n"), func_addr, errorno);
				result.success = FALSE;
			}
		}
		print(func_addr, opt, result);
	}
	SymUnloadModule64(proc, load_addr);
	SymCleanup(proc);
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	option opt(argc, argv);
	if(0 == parse_option(opt))
		return addr2line(opt);
	return 0;
}

