#include	<windows.h>
#include	<winreg.h>
#include	<tchar.h>
#include	<tlhelp32.h>
#include	<shellapi.h>
#include	<strsafe.h>

typedef struct _option{
	BOOL AsCUI;
	BOOL IsCUI;
	BOOL ShowHelp;
	BOOL DispKey;
	LPCTSTR Machine;
	BOOL DispAsGMT;
	BOOL DispAsUnicode;
} OPTIONS, *LPOPTIONS;

VOID (*my_puts)(HANDLE, LPCTSTR);
HANDLE hStderr = INVALID_HANDLE_VALUE;
HANDLE hStdout = INVALID_HANDLE_VALUE;


DWORD GetParentProcessName( LPTSTR lpszParentName, DWORD cchSize )
{
	HANDLE hSnapshot; 
	PROCESSENTRY32 pe32;
	DWORD pid, ppid = 0;
	DWORD r = 0;
	size_t w;

	pid = GetCurrentProcessId();

	hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	__try{
		if( hSnapshot == INVALID_HANDLE_VALUE ) __leave;

		ZeroMemory( &pe32, sizeof( pe32 ) );
		pe32.dwSize = sizeof( pe32 );
		if( !Process32First( hSnapshot, &pe32 ) ) __leave;

		// find my process and get parent's pid
		do{
			if( pe32.th32ProcessID == pid ){
				ppid = pe32.th32ParentProcessID;
				break;
			}
		}while( Process32Next( hSnapshot, &pe32 ) );

		if( ppid == 0 ) __leave;		// not found

		// rewind
		ZeroMemory( &pe32, sizeof( pe32 ) );
		pe32.dwSize = sizeof( pe32 );
		if( !Process32First( hSnapshot, &pe32 ) ) __leave;

		// find parrent process and get process name
		do{
			if( pe32.th32ProcessID == ppid ){
				StringCchCopy( lpszParentName, cchSize, pe32.szExeFile );
				if( StringCchLength( pe32.szExeFile, 32767, &w ) == NO_ERROR ){
					r = (DWORD)w;
				}
				break;
			}
		}while( Process32Next( hSnapshot, &pe32 ) );
	}
	__finally{
		if( hSnapshot != INVALID_HANDLE_VALUE ) CloseHandle( hSnapshot );
	}
	return r;


}

/* return TRUE if parent process is CMD.EXE */
BOOL IsCommandLine()
{
	TCHAR buf[ MAX_PATH ];
	TCHAR *p;
	DWORD r;

	r = GetParentProcessName( buf, _countof( buf ) );
	if( r < _countof( buf ) ){
		p = buf;
		do{
			if( CompareString( LOCALE_USER_DEFAULT, SORT_STRINGSORT | NORM_IGNORECASE, p, -1, _T("cmd.exe"), -1 ) == CSTR_EQUAL ){
				return TRUE;
			}
			while( *p && *p != _T('\\') && *p != _T('/' ) )p++;
			if( *p == _T('\\') || *p == _T('/') ) p++;
		}while( *p );
	}
	return FALSE;
}

VOID my_putsG( HANDLE h, LPCTSTR s )
{
	UINT n = MB_OK;
	if( h == hStderr && h != INVALID_HANDLE_VALUE ){
		n |= MB_ICONSTOP;
	}else{
		n |= MB_ICONINFORMATION;
	}
	MessageBox( HWND_DESKTOP, s, _T("regtime"), n );

}

// Console, ANSI
VOID my_putsCA( HANDLE h, LPCTSTR s )
{
	DWORD n1, n2;
	DWORD len = 0;
	LPSTR p;

#ifdef UNICODE
	UINT cp = GetConsoleOutputCP();

	if( ( len = WideCharToMultiByte( cp, 0, s, -1, NULL, 0, NULL, NULL ) ) == 0 ) return;
	if( ( p = (LPSTR)LocalAlloc( LMEM_FIXED, len ) ) == NULL ) return;
	len = WideCharToMultiByte( cp, 0, s, -1, p, len, NULL, NULL );
#else
	size_t n;
	p = (LPTSTR)s;
	if( StringCbLength( p, 4096, &n ) != S_OK ) len = 0;
	else len = n;
#endif

	n1 = len ? len -1 : 0;
	while( n1 ){
		if( !WriteFile( h, p, n1, &n2, NULL ) )  break;
		n1 -= n2;
	}
#ifdef UNICODE
	LocalFree( p );
#endif
}

// Console, Wide
VOID my_putsCW( HANDLE h, LPCTSTR s )
{
	DWORD n1, n2;
	DWORD len = 0;
	LPWSTR p;
	size_t n;

#ifdef UNICODE
	p = (LPWSTR)s;
	if( len = StringCbLength( p, 4096, &n ) != S_OK ) len = 0;
	else len = n;
#else
	if( ( len = MultiByteToWideChar( CP_ACP, 0, s, -1, NULL, 0 ) ) == 0 ) return;
	if( ( p = (LPWSTR)LocalAlloc( LMEM_FIXED, len ) ) == NULL ) return;
	len = MultiByteToWideChar( CP_ACP, 0, s, -1, p, len );
#endif
	n1 = len ? len - 1 : 0;
	while( n1 ){
		if( !WriteFile( h, p, n1, &n2, NULL ) )  break;
		n1 -= n2;
	}
#ifndef UNICODE
	LocalFree( p );
#endif
}

VOID my_printf( HANDLE h, LPCTSTR lpszFormat, ... )
{
	va_list ap;
	LPTSTR buf;
	DWORD r;

	va_start( ap, lpszFormat );
	r = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
			(LPCVOID)lpszFormat,
			0, 0, (LPTSTR)&buf, 0,
			&ap );
	if( r ) my_puts( h, buf );

	va_end( ap );
	LocalFree( buf );
}

void ShowError(DWORD dwErrorCode, LPCTSTR s)
{
	LPVOID p1 = NULL, p2 = NULL;
	DWORD r;
	TCHAR buf[ 1024 ];
	DWORD_PTR pArgs[ 2 ];

	__try{
		r = FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
			NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &p1, 0,NULL);
		if( r ){
			if( s ){
				pArgs[ 0 ] = (DWORD_PTR)s;
				pArgs[ 1 ] = (DWORD_PTR)p1;
				r = FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
					(LPCVOID)_T("%1!s!:%2!s!"),
					0,
					0,
					(LPTSTR)&p2,
					0,
					(va_list*)pArgs );
				if( r ){
					my_puts( hStderr, (LPCTSTR)p2 );
				}else{
					my_puts( hStderr, (LPCTSTR)p1 );
				}
			}else{
				my_puts( hStderr, (LPCTSTR)p1 );
			}
		}else{
			StringCchPrintf( buf, _countof( buf ), s ? _T("Error:%lu\n%s\n") : _T("Error:%lu\n"), dwErrorCode, s );
			my_puts( hStderr, buf ); 
		}
	}
	__finally{
		if( p1 ) LocalFree( p1 );
		if( p2 ) LocalFree( p2 );
	}
}

VOID ShowHelp( VOID )
{
	LPCTSTR s = 
		_T( "\nregtime - show timestamp of registry key.\n\n" )
		_T( "Usage:  regtime [-c|-w] [-k] [\\\\machine] name-of-RegistryKey\n\n" )
		_T( "        -c : run as console program.\n" )
		_T( "        -w : run as GUI program.\n" )
		_T( "        -k : display registry key name prior to  timestamp.\n" )
		_T( "        -g : display timestamp as GMT.\n" )
		_T( "        -u : display with Unicode.\n" );
	my_puts( hStdout, s );

	return ;
}

BOOL StringToHive( LPCTSTR lpszRegKey, PHKEY phkHive, LPCTSTR *lplpszKey )
{
	LPCTSTR s[] = {
		_T("HKEY_CLASSES_ROOT\\"),
		_T("HKEY_CURRENT_USER\\"),
		_T("HKEY_LOCAL_MACHINE\\"),
		_T("HKEY_USERS\\"),
		_T("HKEY_PERFORMANCE_DATA\\"),
		_T("HKEY_CURRENT_CONFIG\\"),
		_T("HKCR\\"),
		_T("HKCU\\"),
		_T("HKLM\\"),
		_T("HKU\\"),

	};
	HKEY h[] = {
		HKEY_CLASSES_ROOT,
		HKEY_CURRENT_USER,
		HKEY_LOCAL_MACHINE,
		HKEY_USERS,
		HKEY_PERFORMANCE_DATA,
		HKEY_CURRENT_CONFIG,
		HKEY_CLASSES_ROOT,
		HKEY_CURRENT_USER,
		HKEY_LOCAL_MACHINE,
		HKEY_USERS,
	};
	int i;
	size_t len1;
	size_t len2;

	int n;

	StringCchLength( lpszRegKey, 32767, &len1 );
	for( i = 0; i < _countof( h ); i++ ){
		StringCchLength( s[ i ], 24, &len2 );
		n = CompareString( LOCALE_USER_DEFAULT, SORT_STRINGSORT | NORM_IGNORECASE, lpszRegKey, min( len1, len2 ), s[ i ], len2 );

		if( CompareString( LOCALE_USER_DEFAULT, SORT_STRINGSORT | NORM_IGNORECASE, lpszRegKey, min( len1, len2 ), s[ i ], len2 ) == CSTR_EQUAL ){
			*lplpszKey = lpszRegKey + len2;
			*phkHive = h[ i ];
			return TRUE;
		}
	}
	return FALSE;

}

BOOL GetRegTimestamp( LPCTSTR lpszMachine, LPCTSTR lpszRegKey, LPFILETIME lpTime )
{
	DWORD r;
	HKEY hHive;
	HKEY hReg = NULL, hSubKey = NULL;
	LPCTSTR lpszKey;
	BOOL Result = FALSE;

	if( !StringToHive( lpszRegKey, &hHive, &lpszKey ) ) {
		ShowError( ERROR_INVALID_PARAMETER, lpszRegKey );
		return FALSE;
	}

	__try{
		r = RegConnectRegistry( lpszMachine, hHive, &hReg );
		if( r != ERROR_SUCCESS ){
			ShowError( r, lpszMachine ? lpszMachine : _T("RegConnectRegistry") );
			__leave;
		}

		r = RegOpenKeyEx( hReg, lpszKey, 0, KEY_READ, &hSubKey );
		if( r != ERROR_SUCCESS ){
			ShowError( r, lpszKey );
			__leave;
		}

		r = RegQueryInfoKey( hSubKey, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL, NULL, NULL, NULL, lpTime );
		if( r != ERROR_SUCCESS ){
			ShowError( r, lpszKey );
			__leave;
		}

		Result = TRUE;
	}
	__finally{
		if( hSubKey != NULL ) RegCloseKey( hSubKey );
		if( hReg != NULL ) RegCloseKey( hReg );
	}
	return Result;
}

BOOL PutTimestamp( LPCTSTR lpszKey, const LPFILETIME lpTime, LPOPTIONS lpOpt )
{
	FILETIME ftLocal;
	SYSTEMTIME stTime;

	if( lpOpt->DispAsGMT ){
		if( !FileTimeToSystemTime( lpTime, &stTime ) ){
			ShowError( GetLastError(), _T("FileTimeToSystemTime") );
				return FALSE;
		}
	}else{
		if( !FileTimeToLocalFileTime( lpTime, &ftLocal ) ){
			ShowError( GetLastError(), _T("FileTimeToLocalFileTime") );
			return FALSE;
		}
		if( !FileTimeToSystemTime( &ftLocal, &stTime ) ){
			ShowError( GetLastError(), _T("FileTimeToSystemTime") );
			return FALSE;
		}
	}

	my_printf( hStdout, 
			lpOpt->DispKey ? 
			_T("%8!s! - %1!4.4d!-%2!2.2d!-%3!2.2d! %4!2.2d!:%5!2.2d!:%6!2.2d!.%7!3.3d!\n") : 
			_T("%1!4.4d!-%2!2.2d!-%3!2.2d! %4!2.2d!:%5!2.2d!:%6!2.2d!.%7!3.3d!\n"),
			stTime.wYear,
			stTime.wMonth,
			stTime.wDay,
			stTime.wHour,
			stTime.wMinute,
			stTime.wSecond,
			stTime.wMilliseconds,
			lpszKey
			);
	return TRUE;

}

int _tmain( int argc, TCHAR**argv )
//int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	int nArgs;
	LPWSTR *lplpszArgs;
	int i, n = 0;
	LPTSTR p;
	FILETIME ft;
	OPTIONS opt;

	ZeroMemory( &opt, sizeof( opt ) );
	opt.Machine = NULL;

	opt.IsCUI = opt.AsCUI = IsCommandLine();
	lplpszArgs = CommandLineToArgvW( GetCommandLineW(), &nArgs );
	if( lplpszArgs ){
		for( i = 1; i < nArgs; i++ ){
			p = lplpszArgs[ i ];
			if( *p == _T('-') || *p == _T('/') ){
				*p = '\0';
				p++;
				while( *p ){
					switch( *p ){
						case _T('h'):
						case _T('H'):
						case _T('?'):
							opt.ShowHelp = TRUE;
							break;
						case _T('c'):
						case _T('C'):
							opt.AsCUI = TRUE;
							break;
						case _T('w'):
						case _T('W'):
							opt.AsCUI = FALSE;
							break;
						case _T('k'):
						case _T('K'):
							opt.DispKey = TRUE;
							break;
						case _T('g'):
						case _T('G'):
							opt.DispAsGMT = TRUE;
							break;
						case _T('u'):
						case _T('U'):
							opt.DispAsUnicode = TRUE;
							break;
					}
					p++;
				}
			}else if( *p == '\\' && *(p+1) == '\\' ){
				opt.Machine = lplpszArgs[ i ];
				lplpszArgs[ i ] = NULL;
			}else{
				n++;
			}
		}
	}else{
		return -1;
	}
	if( n == 0 ) opt.ShowHelp = TRUE;

	if( opt.AsCUI ){
		/*
		if( !AttachConsole( ATTACH_PARENT_PROCESS ) ){
			MessageBox( 0, _T("cannot attach to console."), _T("regtime"), MB_OK | MB_ICONSTOP );
			return -1;
		}
		hStdout = CreateFile( _T("CONOUT$"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		hStderr = CreateFile( _T("CONOUT$"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		my_puts = my_putsC;
		*/
		hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
		hStderr = GetStdHandle( STD_ERROR_HANDLE );
		if( opt.DispAsUnicode ) my_puts = my_putsCW;
		else my_puts = my_putsCA;
	}else{
		hStderr = GetStdHandle( STD_ERROR_HANDLE );
		hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
		my_puts = my_putsG;
	}

	if( opt.ShowHelp ){
		ShowHelp();
		return 0;
	}
	if( !opt.IsCUI )
		ShowWindow( GetConsoleWindow(), SW_HIDE );

	for( i = 1; i < nArgs; i++ ){
		p = lplpszArgs[ i ];
		if( !p || !*p ) continue;
		if( GetRegTimestamp( opt.Machine, p, &ft ) ){
			PutTimestamp( p, &ft, &opt );
		}
	}
	if( lplpszArgs ) LocalFree( lplpszArgs );

	return 0;
}

